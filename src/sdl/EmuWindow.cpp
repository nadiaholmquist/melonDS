
#include <tuple>
#include <SDL2/SDL.h>

#include "../types.h"
#include "EmuWindow.h"

EmuWindow::EmuWindow() :
	window(), gl_window(), rend(), emu_texture(), fullscreen()
{
	window = SDL_CreateWindow(
		"melonDS", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		256, 384, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
	);

	SDL_SetWindowMinimumSize(window, 256, 384);

	rend = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED);

	emu_texture = SDL_CreateTexture(
		rend, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING,
		256, 384
	);

	SDL_RenderSetLogicalSize(rend, 256, 384);
	SDL_RenderClear(rend);
	SDL_RenderPresent(rend);
}

EmuWindow::~EmuWindow() {
	SDL_DestroyTexture(emu_texture);
	SDL_DestroyRenderer(rend);
	SDL_DestroyWindow(window);
}

void EmuWindow::render() {
	SDL_Rect rect;
	rect.w = 256;
	rect.h = 384;
	rect.x = 0;
	rect.y = 0;

	SDL_RenderClear(rend);
	SDL_RenderCopy(rend, emu_texture, &rect, &rect);
	SDL_RenderPresent(rend);
}

void EmuWindow::update(u32* top, u32* bottom) {
	void* texture;
	int pitch;

	const int fbsize = 256 * 192 * 4;

	SDL_LockTexture(emu_texture, NULL, &texture, &pitch);
	memcpy(texture, top, fbsize);
	memcpy((u8*) texture + fbsize, bottom, fbsize);
	SDL_UnlockTexture(emu_texture);

	this->render();
}

auto EmuWindow::get_window_id() -> u32 {
	return SDL_GetWindowID(window);
}

auto EmuWindow::set_fullscreen(bool fs) -> void {
	SDL_SetWindowFullscreen(window, fs ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

auto EmuWindow::get_fullscreen() -> bool {
	return SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
}

auto EmuWindow::get_size(int& w, int& h) -> void {
	SDL_GetWindowSize(window, &w, &h);
}

auto EmuWindow::set_size(u32 w, u32 h) -> void {
	SDL_SetWindowSize(window, w, h);
}

auto EmuWindow::get_content_size(int& w, int& h) -> void {
	SDL_GetRendererOutputSize(rend, &w, &h);
}

auto EmuWindow::set_integer_size(u32 factor) -> void {
	SDL_SetWindowSize(window, 256 * factor, 384 * factor);
}