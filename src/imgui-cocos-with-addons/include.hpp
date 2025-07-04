#define IMGUI_IMPLEMENTATION
#include <imgui-cocos.hpp>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include "_key-inputs-support-hook.hpp"
#include "_markdown.h"
namespace ImGui {
    inline auto BetterInputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0) {
        auto g = ImGui::GetCurrentContext();
        auto io = ImGui::GetIO();
        auto style = g->Style;
        int lines_count = std::count(str->begin(), str->end(), '\n') + 1;
        const auto size = CalcItemSize({ 0, 0 }, CalcItemWidth(), (g->FontSize * lines_count) + style.FramePadding.y * 2.0f);
        auto textunput_rtn = InputTextMultiline(label, str, size, flags);
        return textunput_rtn;
    }
};