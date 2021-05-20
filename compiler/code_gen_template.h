// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

namespace impeller {
namespace compiler {

constexpr std::string_view kReflectionHeaderTemplate = R"~~(
// THIS FILE IS GENERATED BY impellerc.
// DO NOT EDIT OR CHECK THIS INTO SOURCE CONTROL

#include "shader_types.h"

namespace impeller {
namespace shader {

struct {{camel_case(shader_name)}}{{camel_case(shader_stage)}}Info {
  static constexpr std::string_view kEntrypointName = "{{entrypoint}}";
  static constexpr ShaderStage kShaderStage = {{to_shader_stage(shader_stage)}};

  // Stage Inputs
{% for stage_input in stage_inputs %}
  // Stage input {{stage_input.name}}
  static constexpr ShaderStageInput kInput{{camel_case(stage_input.name)}} = {"{{stage_input.name}}", {{stage_input.location}}};
{% endfor %}
}; // struct {{camel_case(shader_name)}}{{camel_case(shader_stage)}}Info

} // namespace shader
} // namespace impeller
)~~";

constexpr std::string_view kReflectionCCTemplate = R"~~(
// THIS FILE IS GENERATED BY impellerc.
// DO NOT EDIT OR CHECK THIS INTO SOURCE CONTROL

#include "{{header_file_name}}"

namespace impeller {
namespace shader {



} // namespace shader
} // namespace impeller
)~~";

}  // namespace compiler
}  // namespace impeller