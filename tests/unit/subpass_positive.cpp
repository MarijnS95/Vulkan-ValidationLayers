/*
 * Copyright (c) 2023-2025 Valve Corporation
 * Copyright (c) 2023-2025 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "../framework/layer_validation_tests.h"
#include "../framework/pipeline_helper.h"
#include "../framework/render_pass_helper.h"
#include "utils/convert_utils.h"

class PositiveSubpass : public VkLayerTest {};

TEST_F(PositiveSubpass, SubpassImageBarrier) {
    TEST_DESCRIPTION("Subpass with image barrier (self-dependency)");
    SetTargetApiVersion(VK_API_VERSION_1_3);
    AddRequiredFeature(vkt::Feature::synchronization2);
    RETURN_IF_SKIP(Init());

    const VkAttachmentDescription attachment = {0,
                                                VK_FORMAT_R8G8B8A8_UNORM,
                                                VK_SAMPLE_COUNT_1_BIT,
                                                VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_GENERAL};
    const VkSubpassDependency dependency = {0,
                                            0,
                                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                            VK_DEPENDENCY_BY_REGION_BIT};
    const VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_GENERAL};
    const VkSubpassDescription subpass = {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 1, &ref, 1, &ref, nullptr, nullptr, 0, nullptr};

    VkRenderPassCreateInfo rpci = vku::InitStructHelper();
    rpci.attachmentCount = 1;
    rpci.pAttachments = &attachment;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dependency;
    vkt::RenderPass render_pass(*m_device, rpci);
    vkt::Image image(*m_device, 32, 32, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vkt::ImageView image_view = image.CreateView();
    vkt::Framebuffer framebuffer(*m_device, render_pass, 1, &image_view.handle());

    // VkImageMemoryBarrier
    VkImageMemoryBarrier barrier = vku::InitStructHelper();
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    // VkDependencyInfo with VkImageMemoryBarrier2
    const auto safe_barrier2 = ConvertVkImageMemoryBarrierToV2(barrier, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    // Test vkCmdPipelineBarrier subpass barrier
    m_command_buffer.Begin();
    m_command_buffer.BeginRenderPass(render_pass, framebuffer, 32, 32);
    vk::CmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1,
                           &barrier);
    vk::CmdEndRenderPass(m_command_buffer);
    m_command_buffer.End();

    // Test vkCmdPipelineBarrier2 subpass barrier
    m_command_buffer.Begin();
    m_command_buffer.BeginRenderPass(render_pass, framebuffer, 32, 32);
    m_command_buffer.Barrier(*safe_barrier2.ptr(), VK_DEPENDENCY_BY_REGION_BIT);
    vk::CmdEndRenderPass(m_command_buffer);
    m_command_buffer.End();
}

TEST_F(PositiveSubpass, SubpassWithEventWait) {
    TEST_DESCRIPTION("Subpass waits for the event set outside of this subpass");
    SetTargetApiVersion(VK_API_VERSION_1_3);
    AddRequiredFeature(vkt::Feature::synchronization2);
    RETURN_IF_SKIP(Init());

    const VkAttachmentDescription attachment = {0,
                                                VK_FORMAT_R8G8B8A8_UNORM,
                                                VK_SAMPLE_COUNT_1_BIT,
                                                VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_GENERAL};
    const VkSubpassDependency dependency = {VK_SUBPASS_EXTERNAL,
                                            0,
                                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                            0};
    const VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_GENERAL};
    const VkSubpassDescription subpass = {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 1, &ref, 1, &ref, nullptr, nullptr, 0, nullptr};

    VkRenderPassCreateInfo rpci = vku::InitStructHelper();
    rpci.attachmentCount = 1;
    rpci.pAttachments = &attachment;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dependency;
    vkt::RenderPass render_pass(*m_device, rpci);
    vkt::Image image(*m_device, 32, 32, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vkt::ImageView image_view = image.CreateView();
    vkt::Framebuffer framebuffer(*m_device, render_pass, 1, &image_view.handle());

    // VkImageMemoryBarrier
    VkImageMemoryBarrier barrier = vku::InitStructHelper();
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    // VkDependencyInfo with VkImageMemoryBarrier2
    const auto safe_barrier2 = ConvertVkImageMemoryBarrierToV2(barrier, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    VkDependencyInfo dependency_info = vku::InitStructHelper();
    dependency_info.dependencyFlags = 0;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = safe_barrier2.ptr();

    // vkCmdWaitEvents inside render pass
    {
        vkt::Event event(*m_device);
        m_command_buffer.Begin();
        vk::CmdSetEvent(m_command_buffer, event, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        m_command_buffer.BeginRenderPass(render_pass, framebuffer, 32, 32);
        vk::CmdWaitEvents(m_command_buffer, 1, &event.handle(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, nullptr, 0, nullptr, 1, &barrier);
        vk::CmdEndRenderPass(m_command_buffer);
        m_command_buffer.End();
    }

    // vkCmdWaitEvents2 inside render pass.
    // It's also a regression test for https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/4258
    {
        vkt::Event event2(*m_device);
        m_command_buffer.Begin();
        vk::CmdSetEvent2(m_command_buffer, event2, &dependency_info);
        m_command_buffer.BeginRenderPass(render_pass, framebuffer, 32, 32);
        vk::CmdWaitEvents2(m_command_buffer, 1, &event2.handle(), &dependency_info);
        vk::CmdEndRenderPass(m_command_buffer);
        m_command_buffer.End();
    }
}

// This is not working because of a bug in the Spec Constant logic
// https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/5911
TEST_F(PositiveSubpass, DISABLED_InputAttachmentMissingSpecConstant) {
    RETURN_IF_SKIP(Init());
    InitRenderTarget();

    char const *fsSource = R"glsl(
        #version 450
        layout (constant_id = 0) const int index = 4; // over VkDescriptorSetLayoutBinding::descriptorCount
        layout(input_attachment_index=0, set=0, binding=0) uniform subpassInput xs[index];
        layout(location=0) out vec4 color;
        void main() {
           color = subpassLoad(xs[0]);
        }
    )glsl";

    uint32_t data = 2;
    VkSpecializationMapEntry entry = {0, 0, sizeof(uint32_t)};
    VkSpecializationInfo specialization_info = {1, &entry, sizeof(uint32_t), &data};
    const VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_GLSL, &specialization_info);

    const auto set_info = [&](CreatePipelineHelper &helper) {
        helper.shader_stages_ = {helper.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
        helper.dsl_bindings_[0] = {0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    };
    CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
}

TEST_F(PositiveSubpass, AccessFlags3) {
    SetTargetApiVersion(VK_API_VERSION_1_2);
    AddRequiredExtensions(VK_KHR_MAINTENANCE_8_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    AddRequiredFeature(vkt::Feature::maintenance8);
    AddRequiredFeature(vkt::Feature::synchronization2);
    RETURN_IF_SKIP(Init());
    InitRenderTarget();

    VkMemoryBarrierAccessFlags3KHR memory_barrier_access_flags = vku::InitStructHelper();
    memory_barrier_access_flags.srcAccessMask3 = VK_ACCESS_3_NONE_KHR;
    memory_barrier_access_flags.dstAccessMask3 = VK_ACCESS_3_NONE_KHR;

    VkMemoryBarrier2 memory_barrier = vku::InitStructHelper(&memory_barrier_access_flags);

    VkSubpassDependency2 subpass_dependency = vku::InitStructHelper(&memory_barrier);
    subpass_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkAttachmentDescription2 attach_desc = vku::InitStructHelper();
    attach_desc.format = VK_FORMAT_R32_UINT;
    attach_desc.samples = VK_SAMPLE_COUNT_1_BIT;
    attach_desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attach_desc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentReference2 reference = vku::InitStructHelper();
    reference.attachment = 0;
    reference.layout = VK_IMAGE_LAYOUT_GENERAL;
    reference.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkSubpassDescription2 subpass = vku::InitStructHelper();
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.viewMask = 0;
    subpass.inputAttachmentCount = 1;
    subpass.pInputAttachments = &reference;

    auto rpci2 =
        vku::InitStruct<VkRenderPassCreateInfo2>(nullptr, 0u, 1u, &attach_desc, 1u, &subpass, 1u, &subpass_dependency, 0u, nullptr);

    PositiveTestRenderPass2KHRCreate(*m_device, rpci2);
}

TEST_F(PositiveSubpass, AllCommandsInSubpassDependency) {
    TEST_DESCRIPTION("Test ALL_COMMANDS_BIT is allowed in subpass dependency");
    RETURN_IF_SKIP(Init());

    VkSubpassDependency subpass_dep{};
    subpass_dep.srcSubpass = 0;
    subpass_dep.dstSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dep.srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;  // Here!
    subpass_dep.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    subpass_dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpass_dep.dstAccessMask = VK_ACCESS_NONE;

    RenderPassSingleSubpass rp(*this);
    rp.AddSubpassDependency(subpass_dep);
    rp.CreateRenderPass();
}

TEST_F(PositiveSubpass, TopOfPipeInSubpassDependency) {
    TEST_DESCRIPTION("Test TOP_OF_PIPE is allowed in subpass dependency");
    RETURN_IF_SKIP(Init());

    VkSubpassDependency subpass_dep{};
    subpass_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dep.dstSubpass = 0;
    subpass_dep.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    subpass_dep.dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;  // Here!
    subpass_dep.srcAccessMask = VK_ACCESS_NONE;
    subpass_dep.dstAccessMask = VK_ACCESS_NONE;

    RenderPassSingleSubpass rp(*this);
    rp.AddSubpassDependency(subpass_dep);
    rp.CreateRenderPass();
}

TEST_F(PositiveSubpass, BottomOfPipeInSubpassDependency) {
    TEST_DESCRIPTION("Test BOTTOM_OF_PIPE is allowed in subpass dependency");
    RETURN_IF_SKIP(Init());

    VkSubpassDependency subpass_dep{};
    subpass_dep.srcSubpass = 0;
    subpass_dep.dstSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dep.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;  // Here!
    subpass_dep.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    subpass_dep.srcAccessMask = VK_ACCESS_NONE;
    subpass_dep.dstAccessMask = VK_ACCESS_NONE;

    RenderPassSingleSubpass rp(*this);
    rp.AddSubpassDependency(subpass_dep);
    rp.CreateRenderPass();
}
