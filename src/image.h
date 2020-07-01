// Copyright 2019 The Amber Authors.
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

#ifndef SRC_IMAGE_H_
#define SRC_IMAGE_H_

#include "buffer.h"

namespace amber {

class Buffer;
class Sampler;

enum class ImageDimension : int8_t { kUnknown = -1, k1D = 0, k2D = 1, k3D = 2 };

enum class ImageType : int8_t {
  /// Unknown image type
  kUnknown = -1,
  /// A color buffer.
  kColor = 0,
  /// A depth/stencil buffer.
  kDepthStencil,
  /// A sampled image.
  kSampledImage,
  /// A combined image sampler.
  kCombinedImageSampler,
  /// A storage image.
  kStorageImage,
  /// A uniform texel buffer.
  kUniformTexelBuffer,
  /// A storage texel buffer.
  kStorageTexelBuffer
};

class Image {
 public:
  /// Create an image of unknown type.
  Image();

  ~Image();

  /// Sets the image |name|.
  void SetName(const std::string& name) { name_ = name; }

  /// Returns the name of the image.
  std::string GetName() const { return name_; }

  /// Sets the image format to |format|.
  void SetFormat(Format* format) { format_ = format; }
  /// Returns the image format.
  Format* GetFormat() const { return format_; }

  /// Sets the sampler used with combined image sampler image type.
  void SetSampler(Sampler* sampler) { sampler_ = sampler; }
  /// Returns the sampler of combined image sampler image.
  Sampler* GetSampler() const { return sampler_; }

  /// Gets the number of elements this image is wide.
  uint32_t GetWidth() const { return width_; }
  /// Set the number of elements wide for the image.
  void SetWidth(uint32_t width) { width_ = width; }
  /// Get the number of elements this image is high.
  uint32_t GetHeight() const { return height_; }
  /// Set the number of elements high for the image.
  void SetHeight(uint32_t height) { height_ = height; }
  /// Get the number of elements this image is deep.
  uint32_t GetDepth() const { return depth_; }
  /// Set the number of elements this image is deep.
  void SetDepth(uint32_t depth) { depth_ = depth; }
  /// Get the image dimensionality.
  ImageDimension GetImageDimension() const { return image_dim_; }
  /// Set the image dimensionality.
  void SetImageDimension(ImageDimension dim) { image_dim_ = dim; }
  /// Sets the number of mip levels for an image used as a color buffer
  /// or a texture.
  void SetMipLevels(uint32_t mip_levels) { mip_levels_ = mip_levels; }
  /// Returns the number of mip levels.
  uint32_t GetMipLevels() const { return mip_levels_; }
  /// Sets the number of samples.
  void SetSamples(uint32_t samples) { samples_ = samples; }
  /// Returns the number of samples.
  uint32_t GetSamples() const { return samples_; }

  // TODO Ari: This is the new functionality where the image is linked to
  // buffers
  void AddBuffer(amber::Buffer* buffer) { buffers_.push_back(buffer); }
  std::vector<amber::Buffer*> GetBuffers() { return buffers_; }

 private:
  std::string name_;
  std::vector<amber::Buffer*> buffers_;
  uint32_t width_ = 1;
  uint32_t height_ = 1;
  uint32_t depth_ = 1;
  uint32_t mip_levels_ = 1;
  uint32_t samples_ = 1;
  Format* format_ = nullptr;
  Sampler* sampler_ = nullptr;
  ImageDimension image_dim_ = ImageDimension::kUnknown;
};

}  // namespace amber

#endif  // SRC_IMAGE_H_
