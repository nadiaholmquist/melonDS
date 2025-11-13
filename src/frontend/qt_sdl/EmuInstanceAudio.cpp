/*
    Copyright 2016-2025 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include "Config.h"
#include "NDS.h"
#include "SPU.h"
#include "Platform.h"
#include "main.h"

#include "mic_blow.h"

using namespace melonDS;

#define INTERNAL_FRAME_RATE 59.8260982880808f

// --- AUDIO OUTPUT -----------------------------------------------------------


void EmuInstance::audioInit()
{
    audioVolume = localCfg.GetInt("Audio.Volume");
    audioDSiVolumeSync = localCfg.GetBool("Audio.DSiVolumeSync");

    audioMuted = false;
    audioSyncCond = SDL_CreateCondition();
    audioSyncLock = SDL_CreateMutex();

    audioFreq = 48000;
    audioBufSize = 1024;

    SDL_AudioSpec deviceSpec;
    int res = SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &deviceSpec, &audioBufSize);
    if (res)
        audioFreq = deviceSpec.freq;

    SDL_AudioSpec audioSpec = {};
    audioSpec.freq = audioFreq;
    audioSpec.format = SDL_AUDIO_S16LE;
    audioSpec.channels = 2;

    audioDevice = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec, audioCallback, this);

    if (!audioDevice)
    {
        Platform::Log(Platform::LogLevel::Error, "Audio init failed: %s\n", SDL_GetError());
    }
    else
    {
        Platform::Log(Platform::LogLevel::Info, "Audio output frequency: %d Hz\n", audioFreq);
        Platform::Log(Platform::LogLevel::Info, "Audio output buffer size: %d samples\n", audioBufSize);
        SDL_ResumeAudioStreamDevice(audioDevice);
    }

    audioSampleFrac = 0;

    micStarted = false;
    micDevice = 0;
    micWavBuffer = nullptr;
    micBuffer = nullptr;

    micLock = SDL_CreateMutex();

    setupMicInputData();
}

void EmuInstance::audioCallback(void* data, SDL_AudioStream* stream, int additional, int total)
{
    EmuInstance* inst = (EmuInstance*) data;
    if (inst == nullptr || inst->nds == nullptr || additional == 0) return;

    double skew = std::clamp(inst->targetFPS / INTERNAL_FRAME_RATE, 0.995, 1.005);
    inst->nds->SPU.SetOutputSkew(skew);

    int n = inst->nds->SPU.GetOutputSize() * (2 * sizeof(s16));
    int toRead = std::min(additional, n);
    s16 samples[toRead];
    inst->nds->SPU.ReadOutput(samples, toRead / (2 * sizeof(s16)));
    SDL_PutAudioStreamData(stream, samples, toRead);
}


void EmuInstance::audioDeInit()
{
    if (audioDevice) SDL_DestroyAudioStream(audioDevice);
    audioDevice = nullptr;
    micClose();
    micStarted = false;

    if (audioSyncCond) SDL_DestroyCondition(audioSyncCond);
    audioSyncCond = nullptr;

    if (audioSyncLock) SDL_DestroyMutex(audioSyncLock);
    audioSyncLock = nullptr;

    if (micWavBuffer) delete[] micWavBuffer;
    micWavBuffer = nullptr;

    if (micLock) SDL_DestroyMutex(micLock);
    micLock = nullptr;
}

void EmuInstance::audioMute()
{
    audioMuted = false;
    if (numEmuInstances() < 2) return;

    switch (mpAudioMode)
    {
        case 1: // only instance 1
            if (instanceID > 0) audioMuted = true;
            break;

        case 2: // only currently focused instance
            audioMuted = true;
            for (int i = 0; i < kMaxWindows; i++)
            {
                if (!windowList[i]) continue;
                if (windowList[i]->isFocused())
                {
                    audioMuted = false;
                    break;
                }
            }
            break;
    }
}

void EmuInstance::audioSync()
{
    if (audioDevice)
    {
        SDL_LockMutex(audioSyncLock);
        while (nds->SPU.GetOutputSize() > audioBufSize)
        {
            if (!SDL_WaitConditionTimeout(audioSyncCond, audioSyncLock, 500))
                break;
        }
        SDL_UnlockMutex(audioSyncLock);
    }
}

int EmuInstance::audioGetNumSamplesOut(int outlen)
{
    float f_len_in = outlen * (curFPS/targetFPS);
    f_len_in += audioSampleFrac;
    int len_in = (int)floor(f_len_in);
    audioSampleFrac = f_len_in - len_in;

    return len_in;
}

// --- MIC INPUT --------------------------------------------------------------

void EmuInstance::micOpen()
{
    memset(micExtBuffer, 0, sizeof(micExtBuffer));
    micExtBufferWritePos = 0;
    micExtBufferCount = 0;
    micBufferReadPos = 0;

    if (micDevice) return;

    if (micInputType != micInputType_External)
    {
        micDevice = 0;
        return;
    }

    micFreq = 47743;
    SDL_AudioSpec micSpec = { SDL_AUDIO_S16LE, 2, micFreq };
    micDevice = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &micSpec, nullptr, this);

    if (!micDevice)
    {
        Platform::Log(Platform::LogLevel::Error, "Mic init failed: %s\n", SDL_GetError());
    }
    else
    {
        Platform::Log(Platform::LogLevel::Info, "Mic output frequency: %d Hz\n", micFreq);
        Platform::Log(Platform::LogLevel::Info, "Mic output buffer size: %d samples\n", micBufSize);
        SDL_ResumeAudioStreamDevice(micDevice);
    }

    micSampleFrac = 0;
}

void EmuInstance::micClose()
{
    if (micDevice)
        SDL_DestroyAudioStream(micDevice);

    micDevice = 0;
}

void EmuInstance::micStart()
{
    micStarted = true;
    micOpen();
}

void EmuInstance::micStop()
{
    micClose();
    micStarted = false;
}

void EmuInstance::micLoadWav(const std::string& name)
{
    SDL_AudioSpec format = {};

    if (micWavBuffer) SDL_free(micWavBuffer);
    micWavBuffer = nullptr;
    micWavLength = 0;

    u8* buf;
    u32 len;
    if (!SDL_LoadWAV(name.c_str(), &format, &buf, &len))
        return;

    if (len > 0x4000000)
    {
        SDL_free(buf);
        return;
    }

    s16* out;
    int outLen;
    SDL_AudioSpec inSpec = { SDL_AUDIO_S16LE, 1, audioFreq };
    bool cvtres = SDL_ConvertAudioSamples(&format, buf, len, &inSpec, (u8**) &out, &outLen);

    if (cvtres)
    {
        micWavBuffer = out;
        micWavLength = outLen;
    }

    SDL_free(buf);
}

void EmuInstance::setupMicInputData()
{
    if (micWavBuffer != nullptr)
    {
        SDL_free(micWavBuffer);
        micWavBuffer = nullptr;
        micWavLength = 0;
    }

    micInputType = globalCfg.GetInt("Mic.InputType");
    micDeviceName = globalCfg.GetString("Mic.Device");
    micWavPath = globalCfg.GetString("Mic.WavPath");

    switch (micInputType)
    {
        case micInputType_Silence:
            micBuffer = nullptr;
            micBufferLength = 0;
            break;
        case micInputType_External:
            micBuffer = nullptr;
            micBufferLength = 0;
            break;
        case micInputType_Noise:
            micBuffer = (s16*)&mic_blow[0];
            micBufferLength = sizeof(mic_blow) / sizeof(s16);
            break;
        case micInputType_Wav:
            micLoadWav(micWavPath);
            micBuffer = micWavBuffer;
            micBufferLength = micWavLength;
            break;
    }

    micBufferReadPos = 0;
}

int EmuInstance::micReadInput(s16* data, int maxlength)
{
    int type = micInputType;

    bool cmd = hotkeyDown(HK_Mic);

    if ((!micBuffer) ||
        ((type != micInputType_External) && (!cmd)))
    {
        type = micInputType_Silence;
    }

    if (type == micInputType_Silence)
    {
        micBufferReadPos = 0;
        memset(data, 0, maxlength * sizeof(s16));
        return maxlength;
    }

    if (type == micInputType_External)
    {
        return SDL_GetAudioStreamData(micDevice, data, maxlength * 2) / 2;
    }
    else
    {
        int readlength = 0;
        while (readlength < maxlength)
        {
            int thislen = maxlength - readlength;
            if ((micBufferReadPos + thislen) > micBufferLength)
                thislen = micBufferLength - micBufferReadPos;

            if (!thislen)
                break;

            memcpy(data, &micBuffer[micBufferReadPos], thislen * sizeof(s16));
            data += thislen;
            micBufferReadPos += thislen;
            if (micBufferReadPos >= micBufferLength)
                micBufferReadPos -= micBufferLength;

            readlength += thislen;
        }

        return readlength;
    }
}

void EmuInstance::audioUpdateSettings()
{
    if (micStarted) micClose();

    if (nds != nullptr)
    {
        int audiointerp = globalCfg.GetInt("Audio.Interpolation");
        nds->SPU.SetInterpolation(static_cast<AudioInterpolation>(audiointerp));
    }

    setupMicInputData();
    if (micStarted) micOpen();
}

void EmuInstance::audioEnable()
{
    if (audioDevice) SDL_ResumeAudioStreamDevice(audioDevice);
    if (micStarted) micOpen();
}

void EmuInstance::audioDisable()
{
    if (audioDevice) SDL_PauseAudioStreamDevice(audioDevice);
    if (micStarted) micClose();
}
