#include "profiler.h"

#include <vector>
#include <stdio.h>
#include <assert.h>

#include <switch.h>

#include "imgui/imgui.h"

namespace profiler
{

std::vector<Section*> sections;
std::vector<Counter*> counters;
std::vector<Section*> stack;

int frames = 0;

Counter::Counter(const char* name) : name_(name)
{}

Section::Section(const char* name) : name_(name)
{}

void Counter::Execute()
{
    if (!registered_)
    {
        counters.push_back(this);
        registered_ = true;
    }

    hit_++;
}

void Section::Enter()
{
    if (!registered_)
    {
        sections.push_back(this);
        registered_ = true;
    }

    if (start_ == 0) // rekursive funktionen
        start_ = armGetSystemTick();
    recursive_++;
    stack.push_back(this);
}

void Section::Leave()
{
    assert(start_ != 0);
    assert(stack.size() > 0);
    assert(stack[stack.size() - 1] == this);
    hit_++;
    if (--recursive_ == 0)
    {
        timeSpend_ += armGetSystemTick() - start_;
        start_ = 0;
    }
}

void Frame()
{
    assert(stack.size() == 0);
    u64 freq = armGetSystemTickFreq();
    for (int i = 0; i < sections.size(); i++)
    {
        for (int j = 0; j < 31; j++)
        {
            sections[i]->History[j] = sections[i]->History[j + 1];
        }
        sections[i]->History[31] = (double)sections[i]->TimeSpend()*1000/(double)freq;
        //sections[i]->History[31] = (double)armTicksToNs(sections[i]->TimeSpend())/(1000*1000.f);
        sections[i]->LastHit = sections[i]->Hit();
        sections[i]->Reset();
    }
    for (int i = 0; i < counters.size(); i++)
    {
        counters[i]->HistoryMax = 0.f;
        for (int j = 0; j < 31; j++)
        {
            counters[i]->HistoryMax = std::max(counters[i]->HistoryMax, counters[i]->History[j + 1]);
            counters[i]->History[j] = counters[i]->History[j + 1];
        }
        counters[i]->History[31] = counters[i]->Hit();
        counters[i]->Reset();
    }
}

void Render()
{
    if (ImGui::TreeNode("Sections"))
    {
        for (int i = 0; i < sections.size(); i++)
        {
            if(ImGui::TreeNode(sections[i]->Name(), "%s: %fms hit %dx", sections[i]->Name(), sections[i]->History[31], sections[i]->LastHit))
            {
                ImGui::PlotHistogram("History", sections[i]->History, 32, 0, NULL, 0.f, 15.f, ImVec2(0, 50.f));
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Counters"))
    {
        for (int i = 0; i < counters.size(); i++)
        {
            if(ImGui::TreeNode(counters[i]->Name(), "%s: %fx", counters[i]->Name(), counters[i]->History[31]))
            {
                ImGui::PlotHistogram("History", counters[i]->History, 32, 0, NULL, 0.f, counters[i]->HistoryMax, ImVec2(0, 50.f));
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

void EndSection()
{
    stack[stack.size() - 1]->Leave();
    stack.pop_back();
}

}