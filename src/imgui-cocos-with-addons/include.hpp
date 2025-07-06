#define IMGUI_IMPLEMENTATION
#include <imgui-cocos.hpp>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include "_key-inputs-support-hook.hpp"

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

    void CCNodeImage(cocos2d::CCNode* node, float maxWidth = ImGui::GetContentRegionAvail().x) {
        struct Entry {
            geode::Ref<cocos2d::CCNode> node;
            geode::Ref<cocos2d::CCRenderTexture> rt;
        };
        static std::map<cocos2d::CCNode*, Entry> created;

        auto& E = created[node];
        if (!E.node) {
            E.node = geode::createQuickPopup("fuckyou", "asd", "asd", nullptr, nullptr, false);

            auto sz = E.node->getContentSize();
            int w = static_cast<int>(sz.width);
            int h = static_cast<int>(sz.height);

            E.rt = cocos2d::CCRenderTexture::create(w, h);
        }

        GLboolean tex2d = glIsEnabled(GL_TEXTURE_2D);
        GLint     prevTex;
        GLint     prevActiveTex;
        GLint     blendSrcRGB, blendDstRGB, blendSrcAlpha, blendDstAlpha;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTex);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRGB);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);

        glActiveTexture(GL_TEXTURE0);
        if (!tex2d) glEnable(GL_TEXTURE_2D);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        //update size
        auto sz = E.node->getContentSize();
        E.rt->m_nWidth = static_cast<int>(sz.width);
        E.rt->m_nHeight = static_cast<int>(sz.height);

        E.rt->beginWithClear(0, 0, 0, 0);
        E.node->update(ImGui::GetIO().DeltaTime);
        E.node->visit();
        E.rt->end();

        if (!tex2d) glDisable(GL_TEXTURE_2D);
        glActiveTexture(prevActiveTex);
        glBindTexture(GL_TEXTURE_2D, prevTex);
        glBlendFuncSeparate(blendSrcRGB, blendDstRGB, blendSrcAlpha, blendDstAlpha);

        auto sprite = E.rt->getSprite();

        GLuint texID = sprite->getTexture()->getName();
        auto w = sprite->getTexture()->getPixelsWide();
        auto h = sprite->getTexture()->getPixelsHigh();
        float scale = maxWidth / w;

        ImVec2 itemSize = ImVec2(w * scale, h * scale);
        ImGui::SetNextItemWidth(itemSize.x);
        ImGui::Image(
            (void*)(uintptr_t)texID, itemSize,
            ImVec2(0, 1), ImVec2(1, 0)
        );

    }

};

#include "_markdown.h"