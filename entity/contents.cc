// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents.h"

#include <memory>

#include "flutter/fml/logging.h"
#include "impeller/entity/content_context.h"
#include "impeller/entity/entity.h"
#include "impeller/geometry/path_builder.h"
#include "impeller/geometry/vector.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/sampler_library.h"
#include "impeller/renderer/surface.h"
#include "impeller/renderer/tessellator.h"
#include "impeller/renderer/vertex_buffer.h"
#include "impeller/renderer/vertex_buffer_builder.h"

namespace impeller {

static ContentContext::Options OptionsFromPass(const RenderPass& pass) {
  ContentContext::Options opts;
  opts.sample_count = pass.GetRenderTarget().GetSampleCount();
  return opts;
}

/*******************************************************************************
 ******* Contents
 ******************************************************************************/

Contents::Contents() = default;

Contents::~Contents() = default;

/*******************************************************************************
 ******* Linear Gradient Contents
 ******************************************************************************/

LinearGradientContents::LinearGradientContents() = default;

LinearGradientContents::~LinearGradientContents() = default;

void LinearGradientContents::SetEndPoints(Point start_point, Point end_point) {
  start_point_ = start_point;
  end_point_ = end_point;
}

void LinearGradientContents::SetColors(std::vector<Color> colors) {
  colors_ = std::move(colors);
  if (colors_.empty()) {
    colors_.push_back(Color::Black());
    colors_.push_back(Color::Black());
  } else if (colors_.size() < 2u) {
    colors_.push_back(colors_.back());
  }
}

const std::vector<Color>& LinearGradientContents::GetColors() const {
  return colors_;
}

bool LinearGradientContents::Render(const ContentContext& renderer,
                                    const Entity& entity,
                                    RenderPass& pass) const {
  using VS = GradientFillPipeline::VertexShader;
  using FS = GradientFillPipeline::FragmentShader;

  auto vertices_builder = VertexBufferBuilder<VS::PerVertexData>();
  {
    auto result = Tessellator{entity.GetPath().GetFillType()}.Tessellate(
        entity.GetPath().CreatePolyline(), [&vertices_builder](Point point) {
          VS::PerVertexData vtx;
          vtx.vertices = point;
          vertices_builder.AppendVertex(vtx);
        });
    if (!result) {
      return false;
    }
  }

  VS::FrameInfo frame_info;
  frame_info.mvp = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransformation();

  FS::GradientInfo gradient_info;
  gradient_info.start_point = start_point_;
  gradient_info.end_point = end_point_;
  gradient_info.start_color = colors_[0];
  gradient_info.end_color = colors_[1];

  Command cmd;
  cmd.label = "LinearGradientFill";
  cmd.pipeline = renderer.GetGradientFillPipeline(OptionsFromPass(pass));
  cmd.stencil_reference = entity.GetStencilDepth();
  cmd.BindVertices(
      vertices_builder.CreateVertexBuffer(pass.GetTransientsBuffer()));
  cmd.primitive_type = PrimitiveType::kTriangle;
  FS::BindGradientInfo(
      cmd, pass.GetTransientsBuffer().EmplaceUniform(gradient_info));
  VS::BindFrameInfo(cmd, pass.GetTransientsBuffer().EmplaceUniform(frame_info));
  return pass.AddCommand(std::move(cmd));
}

/*******************************************************************************
 ******* SolidColorContents
 ******************************************************************************/

SolidColorContents::SolidColorContents() = default;

SolidColorContents::~SolidColorContents() = default;

void SolidColorContents::SetColor(Color color) {
  color_ = color;
}

const Color& SolidColorContents::GetColor() const {
  return color_;
}

static VertexBuffer CreateSolidFillVertices(const Path& path,
                                            HostBuffer& buffer) {
  using VS = SolidFillPipeline::VertexShader;

  VertexBufferBuilder<VS::PerVertexData> vtx_builder;

  auto tesselation_result = Tessellator{path.GetFillType()}.Tessellate(
      path.CreatePolyline(), [&vtx_builder](auto point) {
        VS::PerVertexData vtx;
        vtx.vertices = point;
        vtx_builder.AppendVertex(vtx);
      });
  if (!tesselation_result) {
    return {};
  }

  return vtx_builder.CreateVertexBuffer(buffer);
}

bool SolidColorContents::Render(const ContentContext& renderer,
                                const Entity& entity,
                                RenderPass& pass) const {
  if (color_.IsTransparent()) {
    return true;
  }

  using VS = SolidFillPipeline::VertexShader;

  Command cmd;
  cmd.label = "SolidFill";
  cmd.pipeline = renderer.GetSolidFillPipeline(OptionsFromPass(pass));
  cmd.stencil_reference = entity.GetStencilDepth();
  cmd.BindVertices(
      CreateSolidFillVertices(entity.GetPath(), pass.GetTransientsBuffer()));

  VS::FrameInfo frame_info;
  frame_info.mvp = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransformation();
  frame_info.color = color_;
  VS::BindFrameInfo(cmd, pass.GetTransientsBuffer().EmplaceUniform(frame_info));

  cmd.primitive_type = PrimitiveType::kTriangle;

  if (!pass.AddCommand(std::move(cmd))) {
    return false;
  }

  return true;
}

std::unique_ptr<SolidColorContents> SolidColorContents::Make(Color color) {
  auto contents = std::make_unique<SolidColorContents>();
  contents->SetColor(color);
  return contents;
}

/*******************************************************************************
 ******* TextureContents
 ******************************************************************************/

TextureContents::TextureContents() = default;

TextureContents::~TextureContents() = default;

void TextureContents::SetTexture(std::shared_ptr<Texture> texture) {
  texture_ = std::move(texture);
}

std::shared_ptr<Texture> TextureContents::GetTexture() const {
  return texture_;
}

void TextureContents::SetOpacity(Scalar opacity) {
  opacity_ = opacity;
}

bool TextureContents::Render(const ContentContext& renderer,
                             const Entity& entity,
                             RenderPass& pass) const {
  if (texture_ == nullptr) {
    return true;
  }

  using VS = TextureFillVertexShader;
  using FS = TextureFillFragmentShader;

  const auto coverage_rect = entity.GetPath().GetBoundingBox();

  if (!coverage_rect.has_value()) {
    return true;
  }

  if (coverage_rect->size.IsEmpty()) {
    return true;
  }

  const auto texture_size = texture_->GetSize();
  if (texture_size.IsEmpty()) {
    return true;
  }

  if (source_rect_.IsEmpty()) {
    return true;
  }

  VertexBufferBuilder<VS::PerVertexData> vertex_builder;
  {
    const auto tess_result =
        Tessellator{entity.GetPath().GetFillType()}.Tessellate(
            entity.GetPath().CreatePolyline(),
            [this, &vertex_builder, &coverage_rect, &texture_size](Point vtx) {
              VS::PerVertexData data;
              data.vertices = vtx;
              auto coverage_coords =
                  (vtx - coverage_rect->origin) / coverage_rect->size;
              data.texture_coords =
                  (source_rect_.origin + source_rect_.size * coverage_coords) /
                  texture_size;
              vertex_builder.AppendVertex(data);
            });
    if (!tess_result) {
      return false;
    }
  }

  if (!vertex_builder.HasVertices()) {
    return true;
  }

  auto& host_buffer = pass.GetTransientsBuffer();

  VS::FrameInfo frame_info;
  frame_info.mvp = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransformation();
  frame_info.alpha = opacity_;

  Command cmd;
  cmd.label = "TextureFill";
  cmd.pipeline = renderer.GetTexturePipeline(OptionsFromPass(pass));
  cmd.stencil_reference = entity.GetStencilDepth();
  cmd.BindVertices(vertex_builder.CreateVertexBuffer(host_buffer));
  VS::BindFrameInfo(cmd, host_buffer.EmplaceUniform(frame_info));
  FS::BindTextureSampler(
      cmd, texture_,
      renderer.GetContext()->GetSamplerLibrary()->GetSampler({}));
  pass.AddCommand(std::move(cmd));

  return true;
}

void TextureContents::SetSourceRect(const IRect& source_rect) {
  source_rect_ = source_rect;
}

const IRect& TextureContents::GetSourceRect() const {
  return source_rect_;
}

/*******************************************************************************
 ******* SolidStrokeContents
 ******************************************************************************/

SolidStrokeContents::SolidStrokeContents() = default;

SolidStrokeContents::~SolidStrokeContents() = default;

void SolidStrokeContents::SetColor(Color color) {
  color_ = color;
}

const Color& SolidStrokeContents::GetColor() const {
  return color_;
}

static void CreateCap(
    VertexBufferBuilder<SolidStrokeVertexShader::PerVertexData>& vtx_builder,
    const Point& position,
    const Point& normal) {}

static void CreateJoin(
    VertexBufferBuilder<SolidStrokeVertexShader::PerVertexData>& vtx_builder,
    const Point& position,
    const Point& start_normal,
    const Point& end_normal) {
  SolidStrokeVertexShader::PerVertexData vtx;
  vtx.vertex_position = position;
  vtx.pen_down = 1.0;
  vtx.vertex_normal = {};
  vtx_builder.AppendVertex(vtx);

  // A simple bevel join to start with.
  Scalar dir = start_normal.Cross(end_normal) > 0 ? -1 : 1;
  vtx.vertex_normal = start_normal * dir;
  vtx_builder.AppendVertex(vtx);
  vtx.vertex_normal = end_normal * dir;
  vtx_builder.AppendVertex(vtx);
}

static VertexBuffer CreateSolidStrokeVertices(const Path& path,
                                              HostBuffer& buffer) {
  using VS = SolidStrokeVertexShader;

  VertexBufferBuilder<VS::PerVertexData> vtx_builder;
  auto polyline = path.CreatePolyline();

  size_t point_i = 0;
  if (polyline.points.size() < 2) {
    return {};  // Nothing to render.
  }

  VS::PerVertexData vtx;

  // Cursor state.
  Point direction;
  Point normal;
  Point previous_normal;  // Used for computing joins.
  auto compute_normals = [&](size_t point_i) {
    previous_normal = normal;
    direction =
        (polyline.points[point_i] - polyline.points[point_i - 1]).Normalize();
    normal = {-direction.y, direction.x};
  };
  compute_normals(1);

  // Break state.
  auto breaks_it = polyline.breaks.begin();
  size_t break_end =
      breaks_it != polyline.breaks.end() ? *breaks_it : polyline.points.size();

  while (point_i < polyline.points.size()) {
    if (point_i > 0) {
      compute_normals(point_i);

      // This branch only executes when we've just finished drawing a contour
      // and are switching to a new one.
      // We're drawing a triangle strip, so we need to "pick up the pen" by
      // appending transparent vertices between the end of the previous contour
      // and the beginning of the new contour.
      vtx.vertex_position = polyline.points[point_i - 1];
      vtx.vertex_normal = {};
      vtx.pen_down = 0.0;
      vtx_builder.AppendVertex(vtx);
      vtx.vertex_position = polyline.points[point_i];
      vtx_builder.AppendVertex(vtx);
    }

    // Generate start cap.
    CreateCap(vtx_builder, polyline.points[point_i], -direction);

    // Generate contour geometry.
    size_t contour_point_i = 0;
    while (point_i < break_end) {
      if (contour_point_i > 0) {
        if (contour_point_i > 1) {
          // Generate join from the previous line to the current line.
          CreateJoin(vtx_builder, polyline.points[point_i - 1], previous_normal,
                     normal);
        } else {
          compute_normals(point_i);
        }

        // Generate line rect.
        vtx.vertex_position = polyline.points[point_i - 1];
        vtx.pen_down = 1.0;
        vtx.vertex_normal = normal;
        vtx_builder.AppendVertex(vtx);
        vtx.vertex_normal = -normal;
        vtx_builder.AppendVertex(vtx);
        vtx.vertex_position = polyline.points[point_i];
        vtx.vertex_normal = normal;
        vtx_builder.AppendVertex(vtx);
        vtx.vertex_normal = -normal;
        vtx_builder.AppendVertex(vtx);

        compute_normals(point_i + 1);
      }

      ++contour_point_i;
      ++point_i;
    }

    // Generate end cap.
    CreateCap(vtx_builder, polyline.points[point_i - 1], -direction);

    if (break_end < polyline.points.size()) {
      ++breaks_it;
      break_end = breaks_it != polyline.breaks.end() ? *breaks_it
                                                     : polyline.points.size();
    }
  }

  return vtx_builder.CreateVertexBuffer(buffer);
}

bool SolidStrokeContents::Render(const ContentContext& renderer,
                                 const Entity& entity,
                                 RenderPass& pass) const {
  if (color_.IsTransparent() || stroke_size_ <= 0.0) {
    return true;
  }

  using VS = SolidStrokeVertexShader;

  VS::FrameInfo frame_info;
  frame_info.mvp = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransformation();

  VS::StrokeInfo stroke_info;
  stroke_info.color = color_;
  stroke_info.size = stroke_size_;

  Command cmd;
  cmd.primitive_type = PrimitiveType::kTriangleStrip;
  cmd.label = "SolidStroke";
  cmd.pipeline = renderer.GetSolidStrokePipeline(OptionsFromPass(pass));
  cmd.stencil_reference = entity.GetStencilDepth();
  cmd.BindVertices(
      CreateSolidStrokeVertices(entity.GetPath(), pass.GetTransientsBuffer()));
  VS::BindFrameInfo(cmd, pass.GetTransientsBuffer().EmplaceUniform(frame_info));
  VS::BindStrokeInfo(cmd,
                     pass.GetTransientsBuffer().EmplaceUniform(stroke_info));

  pass.AddCommand(std::move(cmd));

  return true;
}

void SolidStrokeContents::SetStrokeSize(Scalar size) {
  stroke_size_ = size;
}

Scalar SolidStrokeContents::GetStrokeSize() const {
  return stroke_size_;
}

void SolidStrokeContents::SetStrokeMiter(Scalar miter) {
  miter_ = miter;
}

Scalar SolidStrokeContents::GetStrokeMiter(Scalar miter) {
  return miter_;
}

void SolidStrokeContents::SetStrokeCap(Cap cap) {
  cap_ = cap;
}

SolidStrokeContents::Cap SolidStrokeContents::GetStrokeCap() {
  return cap_;
}

void SolidStrokeContents::SetStrokeJoin(Join join) {
  join_ = join;
}

SolidStrokeContents::Join SolidStrokeContents::GetStrokeJoin() {
  return join_;
}

/*******************************************************************************
 ******* ClipContents
 ******************************************************************************/

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
  cmd.BindVertices(
      CreateSolidFillVertices(entity.GetPath(), pass.GetTransientsBuffer()));

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

}  // namespace impeller
