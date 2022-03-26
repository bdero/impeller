// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <variant>
#include <vector>

#include "impeller/entity/contents/filters/filter_input.h"
#include "impeller/entity/entity.h"
#include "impeller/renderer/formats.h"

namespace impeller {

class Pipeline;

class FilterContents : public Contents {
 public:
  static std::shared_ptr<FilterContents> MakeBlend(Entity::BlendMode blend_mode,
                                                   FilterInput::Vector inputs);

  static std::shared_ptr<FilterContents> MakeDirectionalGaussianBlur(
      FilterInput::Ref input,
      Vector2 blur_vector);

  static std::shared_ptr<FilterContents>
  MakeGaussianBlur(FilterInput::Ref input, Scalar sigma_x, Scalar sigma_y);

  FilterContents();

  ~FilterContents() override;

  /// @brief The input texture sources for this filter. Each input's emitted
  ///        texture is expected to have premultiplied alpha colors.
  ///
  ///        The number of required or optional textures depends on the
  ///        particular filter's implementation.
  void SetInputs(FilterInput::Vector inputs);

  // |Contents|
  bool Render(const ContentContext& renderer,
              const Entity& entity,
              RenderPass& pass) const override;

  // |Contents|
  Rect GetBounds(const Entity& entity) const override;

  // |Contents|
  virtual std::optional<Snapshot> RenderToTexture(
      const ContentContext& renderer,
      const Entity& entity) const override;

 private:
  /// @brief Takes a set of zero or more input textures and writes to an output
  ///        texture.
  virtual bool RenderFilter(const std::vector<Snapshot>& input_textures,
                            const ContentContext& renderer,
                            RenderPass& pass,
                            const Matrix& transform) const = 0;

  FilterInput::Vector inputs_;
  Rect destination_;

  FML_DISALLOW_COPY_AND_ASSIGN(FilterContents);
};

}  // namespace impeller
