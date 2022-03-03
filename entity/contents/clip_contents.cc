// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "clip_contents.h"

#include <functional>
#include <memory>
#include <vector>

#include "flutter/fml/macros.h"
#include "impeller/entity/content_context.h"
#include "impeller/entity/contents/contents.h"
#include "impeller/entity/contents/solid_color_contents.h"
#include "impeller/entity/entity.h"
#include "impeller/entity/solid_stroke.vert.h"
#include "impeller/geometry/color.h"
#include "impeller/geometry/path_component.h"
#include "impeller/geometry/point.h"
#include "impeller/geometry/rect.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/texture.h"
#include "impeller/typographer/glyph_atlas.h"
#include "impeller/typographer/text_frame.h"

namespace impeller {

ClipContents::ClipContents() = default;

ClipContents::~ClipContents() = default;

bool ClipContents::Render(const ContentContext& renderer,
                          const Entity& entity,
                          RenderPass& pass) const {
  using VS = ClipPipeline::VertexShader;

  Command cmd;
  cmd.label = "Clip";
  cmd.pipeline = renderer.GetClipPipeline(OptionsFromPass(pass));
  cmd.stencil_reference = entity.GetStencilDepth();
  cmd.BindVertices(SolidColorContents::CreateSolidFillVertices(
      entity.GetPath(), pass.GetTransientsBuffer()));

  VS::FrameInfo info;
  // The color really doesn't matter.
  info.color = Color::SkyBlue();
  info.mvp = Matrix::MakeOrthographic(pass.GetRenderTargetSize());

  VS::BindFrameInfo(cmd, pass.GetTransientsBuffer().EmplaceUniform(info));

  pass.AddCommand(std::move(cmd));
  return true;
}

/*******************************************************************************
 ******* ClipRestoreContents
 ******************************************************************************/

ClipRestoreContents::ClipRestoreContents() = default;

ClipRestoreContents::~ClipRestoreContents() = default;

bool ClipRestoreContents::Render(const ContentContext& renderer,
                                 const Entity& entity,
                                 RenderPass& pass) const {
  using VS = ClipPipeline::VertexShader;

  Command cmd;
  cmd.label = "Clip Restore";
  cmd.pipeline = renderer.GetClipRestorePipeline(OptionsFromPass(pass));
  cmd.stencil_reference = entity.GetStencilDepth();

  // Create a rect that covers the whole render target.
  auto size = pass.GetRenderTargetSize();
  VertexBufferBuilder<VS::PerVertexData> vtx_builder;
  vtx_builder.AddVertices({
      {Point(0.0, 0.0)},
      {Point(size.width, 0.0)},
      {Point(size.width, size.height)},
      {Point(0.0, 0.0)},
      {Point(size.width, size.height)},
      {Point(0.0, size.height)},
  });
  cmd.BindVertices(vtx_builder.CreateVertexBuffer(pass.GetTransientsBuffer()));

  VS::FrameInfo info;
  // The color really doesn't matter.
  info.color = Color::SkyBlue();
  info.mvp = Matrix::MakeOrthographic(pass.GetRenderTargetSize());

  VS::BindFrameInfo(cmd, pass.GetTransientsBuffer().EmplaceUniform(info));

  pass.AddCommand(std::move(cmd));
  return true;
}

};  // namespace impeller
