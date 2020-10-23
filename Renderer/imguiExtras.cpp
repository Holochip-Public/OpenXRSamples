//
// Created by swinston on 9/30/20.
//
#include <imgui.h>
#include <imgui_internal.h>

#include "Vulkan/CommonHelper.h"
#include "imguiExtras.h"

// Definition (.cpp file. Not sure if it needs "imgui_internal.h" or not)
namespace ImGui {
    bool ImageButtonWithText(ImTextureID texId,const char* label,const ImVec2& imageSize, const ImVec2 &uv0, const ImVec2 &uv1, int frame_padding, const ImVec4 &bg_col, const ImVec4 &tint_col) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImVec2 size = imageSize;
        if (size.x<=0 && size.y<=0) {size.x=size.y=ImGui::GetTextLineHeightWithSpacing();}
        else {
            if (size.x<=0)          size.x=size.y;
            else if (size.y<=0)     size.y=size.x;
            size.x *= window->FontWindowScale*ImGui::GetIO().FontGlobalScale;
            size.y *= window->FontWindowScale*ImGui::GetIO().FontGlobalScale;
        }

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;

        const ImGuiID id = window->GetID(label);
        const ImVec2 textSize = ImGui::CalcTextSize(label,nullptr,true);
        const bool hasText = textSize.x>0;

        const float innerSpacing = hasText ? ((frame_padding >= 0) ? (float)frame_padding : (style.ItemInnerSpacing.x)) : 0.f;
        const ImVec2 padding = (frame_padding >= 0) ? ImVec2((float)frame_padding, (float)frame_padding) : style.FramePadding;
        const ImVec2 totalSizeWithoutPadding(size.x+innerSpacing+textSize.x,size.y>textSize.y ? size.y : textSize.y);
        const ImRect bb(window->DC.CursorPos, {window->DC.CursorPos.x + totalSizeWithoutPadding.x + padding.x*2, window->DC.CursorPos.y + totalSizeWithoutPadding.y + padding.y*2});
        ImVec2 start(0,0);
        start.x = window->DC.CursorPos.x + padding.x;
        start.y = window->DC.CursorPos.y + padding.y;

        if (size.y<textSize.y) start.y+=(textSize.y-size.y)*.5f;
        const ImRect image_bb(start, {start.x + size.x, start.y + size.y});
        start.x = window->DC.CursorPos.x + padding.x;
        start.y = window->DC.CursorPos.y + padding.y;

        start.x+=size.x+innerSpacing;

        if (size.y>textSize.y) start.y+=(size.y-textSize.y)*.5f;
        ItemSize(bb);
        if (!ItemAdd(bb, id))
            return false;

        bool hovered=false, held=false;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held);

        // Render
        const ImU32 col = GetColorU32((hovered && held) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
        RenderFrame(bb.Min, bb.Max, col, true, ImClamp((float)ImMin(padding.x, padding.y), 0.0f, style.FrameRounding));
        if (bg_col.w > 0.0f)
            window->DrawList->AddRectFilled(image_bb.Min, image_bb.Max, GetColorU32(bg_col));

        window->DrawList->AddImage(texId, image_bb.Min, image_bb.Max, uv0, uv1, GetColorU32(tint_col));

        if (textSize.x>0) ImGui::RenderText(start,label);
        return pressed;
    }

    ImTextureID AddTexture(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool vkDescriptorPool, VkSampler sampler, VkImageView image_view, VkImageLayout image_layout){
        VkResult err;

        VkDescriptorSet descriptor_set;
        // Create Descriptor Set:
        {
            VkDescriptorSetAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool = vkDescriptorPool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &descriptorSetLayout;
            err = vkAllocateDescriptorSets(device, &alloc_info, &descriptor_set);
            VK_CHECK_RESULT(err);
        }

        // Update the Descriptor Set:
        {
            VkDescriptorImageInfo desc_image[1] = {};
            desc_image[0].sampler = sampler;
            desc_image[0].imageView = image_view;
            desc_image[0].imageLayout = image_layout;
            VkWriteDescriptorSet write_desc[1] = {};
            write_desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_desc[0].dstSet = descriptor_set;
            write_desc[0].descriptorCount = 1;
            write_desc[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_desc[0].pImageInfo = desc_image;
            vkUpdateDescriptorSets(device, 1, write_desc, 0, nullptr);
        }

        return (ImTextureID)descriptor_set;
    }
} // namespace ImGui
