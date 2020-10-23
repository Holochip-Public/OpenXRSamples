//
// Created by swinston on 9/30/20.
//

#ifndef OPENXR_IMGUIEXTRAS_H
#define OPENXR_IMGUIEXTRAS_H

#include <imgui.h>

// Declaration (.h file)
namespace ImGui {
// label is used as id
// <0 frame_padding uses default frame padding settings. 0 for no padding
    IMGUI_API bool ImageButtonWithText(ImTextureID texId,const char* label,const ImVec2& imageSize=ImVec2(0,0), const ImVec2& uv0 = ImVec2(0,0),  const ImVec2& uv1 = ImVec2(1,1), int frame_padding = -1, const ImVec4& bg_col = ImVec4(0,0,0,0), const ImVec4& tint_col = ImVec4(1,1,1,1));
    IMGUI_API ImTextureID AddTexture(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool vkDescriptorPool, VkSampler sampler, VkImageView image_view, VkImageLayout image_layout);
} // namespace ImGui


#endif //OPENXR_IMGUIEXTRAS_H
