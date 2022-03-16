// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 1D gaussian blur in the X direction.
//
// Paths for future optimization:
//   * Remove the bounds check branch in SampleColor by adding sampler support
//     SamplerAddressMode::ClampToBorder for texture samplers.
//   * Sample from higher mipmap levels when the blur radius is high enough.

uniform sampler2D texture_sampler;

in vec2 v_texture_coords;
in vec2 v_texture_size;
in vec2 v_blur_direction;
in float v_blur_radius;

out vec4 frag_color;

const float TWO_PI = 6.283185307179586;

float Gaussian(float x) {
  float stddev = v_blur_radius * 0.5;
  float xnorm = x / stddev;
  return exp(-0.5 * xnorm * xnorm) / (TWO_PI * stddev * stddev);
}

// Emulate SamplerAddressMode::ClampToBorder.
vec4 SampleWithBorder(vec2 uv) {
  if (uv.x > 0 && uv.y > 0 && uv.x < 1 && uv.y < 1) {
    return texture(texture_sampler, uv);
  }
  return vec4(0);
}

void main() {
  vec2 blur_radius_uv = vec2(v_blur_radius) / v_texture_size;

  vec4 total = vec4(0);
  float total_gaussian = 0;
  for (float i = -v_blur_radius; i <= v_blur_radius; i++) {
    float gaussian = Gaussian(i);
    total_gaussian += gaussian;
    total += gaussian * SampleWithBorder(v_texture_coords +
                                         v_blur_direction * i / v_texture_size);
  }
  frag_color = total / total_gaussian;
}
