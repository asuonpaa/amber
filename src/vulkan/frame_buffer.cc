// Copyright 2018 The Amber Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/vulkan/frame_buffer.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

#include "src/make_unique.h"
#include "src/vulkan/command_buffer.h"
#include "src/vulkan/device.h"

namespace amber {
namespace vulkan {

FrameBuffer::FrameBuffer(
    Device* device,
    const std::vector<const amber::Pipeline::ImageInfo*>& color_attachments,
    amber::Pipeline::ImageInfo depth_stencil_attachment,
    uint32_t width,
    uint32_t height)
    : device_(device),
      color_attachments_(color_attachments),
      depth_stencil_attachment_(depth_stencil_attachment),
      width_(width),
      height_(height) {}

FrameBuffer::~FrameBuffer() {
  if (frame_ != VK_NULL_HANDLE) {
    device_->GetPtrs()->vkDestroyFramebuffer(device_->GetVkDevice(), frame_,
                                             nullptr);
  }
}

Result FrameBuffer::Initialize(VkRenderPass render_pass) {
  std::vector<VkImageView> attachments;

  if (!color_attachments_.empty()) {
    std::vector<int32_t> seen_idx(color_attachments_.size(), -1);
    for (auto* info : color_attachments_) {
      if (info->location >= color_attachments_.size())
        return Result("color attachment locations must be sequential from 0");
      if (seen_idx[info->location] != -1) {
        return Result("duplicate attachment location: " +
                      std::to_string(info->location));
      }
      seen_idx[info->location] = static_cast<int32_t>(info->location);
    }

    attachments.resize(color_attachments_.size());
    for (auto* info : color_attachments_) {
      color_images_.push_back(MakeUnique<TransferImage>(
          device_, *info->image->GetFormat(), VK_IMAGE_ASPECT_COLOR_BIT,
          VK_IMAGE_TYPE_2D, width_ << info->base_mip_level,
          height_ << info->base_mip_level, depth_, info->image->GetMipLevels(),
          info->base_mip_level, 1u, 1u));

      Result r = color_images_.back()->Initialize(
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
      if (!r.IsSuccess())
        return r;

      attachments[info->location] = color_images_.back()->GetVkImageView();
    }
  }

  if (depth_stencil_attachment_.image &&
      depth_stencil_attachment_.image->GetFormat()->IsFormatKnown()) {
    VkImageAspectFlags aspect = 0;
    if (depth_stencil_attachment_.image->GetFormat()->HasDepthComponent())
      aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
    if (depth_stencil_attachment_.image->GetFormat()->HasStencilComponent())
      aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    assert(aspect != 0);

    depth_stencil_image_ = MakeUnique<TransferImage>(
        device_, *depth_stencil_attachment_.image->GetFormat(), aspect,
        VK_IMAGE_TYPE_2D, width_, height_, depth_, 1u, 0u, 1u, 1u);

    Result r = depth_stencil_image_->Initialize(
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    if (!r.IsSuccess())
      return r;

    attachments.push_back(depth_stencil_image_->GetVkImageView());
  }

  VkFramebufferCreateInfo frame_buffer_info = VkFramebufferCreateInfo();
  frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  frame_buffer_info.renderPass = render_pass;
  frame_buffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
  frame_buffer_info.pAttachments = attachments.data();
  frame_buffer_info.width = width_;
  frame_buffer_info.height = height_;
  frame_buffer_info.layers = 1;

  if (device_->GetPtrs()->vkCreateFramebuffer(device_->GetVkDevice(),
                                              &frame_buffer_info, nullptr,
                                              &frame_) != VK_SUCCESS) {
    return Result("Vulkan::Calling vkCreateFramebuffer Fail");
  }

  return {};
}

void FrameBuffer::ChangeFrameLayout(CommandBuffer* command,
                                    VkImageLayout color_layout,
                                    VkPipelineStageFlags color_stage,
                                    VkImageLayout depth_layout,
                                    VkPipelineStageFlags depth_stage) {
  for (auto& img : color_images_)
    img->ImageBarrier(command, color_layout, color_stage);

  if (depth_stencil_image_)
    depth_stencil_image_->ImageBarrier(command, depth_layout, depth_stage);
}

void FrameBuffer::ChangeFrameToDrawLayout(CommandBuffer* command) {
  ChangeFrameLayout(command,
                    // Color attachments
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    // Depth attachment
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
}

void FrameBuffer::ChangeFrameToProbeLayout(CommandBuffer* command) {
  ChangeFrameLayout(
      command,
      // Color attachments
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
      // Depth attachments
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT);
}

void FrameBuffer::ChangeFrameToWriteLayout(CommandBuffer* command) {
  ChangeFrameLayout(
      command,
      // Color attachments
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
      // Depth attachments
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT);
}

void FrameBuffer::TransferImagesToHost(CommandBuffer* command) {
  for (auto& img : color_images_)
    img->CopyToHost(command);

  if (depth_stencil_image_)
    depth_stencil_image_->CopyToHost(command);
}

void FrameBuffer::CopyImagesToBuffers() {
  for (size_t i = 0; i < color_images_.size(); ++i) {
    auto& img = color_images_[i];
    auto* info = color_attachments_[i];
    auto buf = info->image->GetBuffers();
    assert(buf.size() == 1);
    auto* values = buf[0]->ValuePtr();
    values->resize(buf[0]->GetSizeInBytes());
    std::memcpy(values->data(), img->HostAccessibleMemoryPtr(),
                buf[0]->GetSizeInBytes());
  }

  if (depth_stencil_image_) {
    auto buf = depth_stencil_attachment_.image->GetBuffers();
    assert(buf.size() == 1);
    auto* values = buf[0]->ValuePtr();
    values->resize(buf[0]->GetSizeInBytes());
    std::memcpy(values->data(), depth_stencil_image_->HostAccessibleMemoryPtr(),
                buf[0]->GetSizeInBytes());
  }
}

void FrameBuffer::TransferImagesToDevice(CommandBuffer* command) {
  for (auto& img : color_images_)
    img->CopyToDevice(command);

  if (depth_stencil_image_)
    depth_stencil_image_->CopyToDevice(command);
}

void FrameBuffer::CopyBuffersToImages() {
  for (size_t i = 0; i < color_images_.size(); ++i) {
    auto& img = color_images_[i];
    auto* info = color_attachments_[i];
    auto buf = info->image->GetBuffers();
    assert(buf.size() == 1);
    auto* values = buf[0]->ValuePtr();
    // Nothing to do if our local buffer is empty
    if (values->empty())
      continue;

    std::memcpy(img->HostAccessibleMemoryPtr(), values->data(),
                buf[0]->GetSizeInBytes());
  }

  if (depth_stencil_image_) {
    auto buf = depth_stencil_attachment_.image->GetBuffers();
    assert(buf.size() == 1);
    auto* values = buf[0]->ValuePtr();
    // Nothing to do if our local buffer is empty
    if (!values->empty()) {
      std::memcpy(depth_stencil_image_->HostAccessibleMemoryPtr(),
                  values->data(), buf[0]->GetSizeInBytes());
    }
  }
}

}  // namespace vulkan
}  // namespace amber
