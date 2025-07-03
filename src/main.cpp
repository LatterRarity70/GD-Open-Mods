#include <imgui-cocos.hpp>
#include "imgui-cocos-key-inputs.hpp"

#include "imgui_markdown.h"
static ImFont* H1;
static ImFont* H2;
static ImFont* H3;
void Markdown(const std::string& markdown_) {
    static ImGui::MarkdownConfig mdConfig;
    mdConfig.headingFormats[0] = { H1, true };
    mdConfig.headingFormats[1] = { H2, true };
    mdConfig.headingFormats[2] = { H3, false };
    ImGui::Markdown(markdown_.c_str(), markdown_.length(), mdConfig);
}

$on_mod(Loaded) {
    ImGuiCocos::get().setup(
        [] {
            ImGuiIO& io = ImGui::GetIO();

            io.Fonts->AddFontDefault();

            ImFontConfig config;
            config.MergeMode = false;
            config.SizePixels = 24.0f;

            H1 = io.Fonts->AddFontDefault(&config);

            config.SizePixels = 20.0f;
            H2 = io.Fonts->AddFontDefault(&config);

            config.SizePixels = 16.0f;
            H3 = io.Fonts->AddFontDefault(&config);

            io.Fonts->Build();

            io.FontGlobalScale = 2.0f;
        }
    ).draw(
        [] {
            ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
            ImGui::SetNextWindowPos({ 0, 0 });
            static bool open;
            if (ImGui::Begin(geode::getMod()->getName().c_str(), nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
                Markdown(geode::getMod()->getDetails().value_or("xd"));
                ImGui::ShowMetricsWindow();
            } ImGui::End();
        }
    );
}
