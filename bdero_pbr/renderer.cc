#include "renderer.h"

#include <algorithm>

#include "bdero_pbr/imgui/imgui.h"
#include "bdero_pbr/imgui_particle_text.h"
#include "imgui/imgui_impl_impeller.h"
#include "imgui/imgui_shaders.h"
#include "shaders.h"

#include "flutter/fml/logging.h"
#include "flutter/fml/trace_event.h"
#include "impeller/base/validation.h"
#include "impeller/renderer/command_buffer.h"
#include "impeller/renderer/surface.h"

namespace bdero {

std::vector<std::shared_ptr<fml::Mapping>> Renderer::GetShaderMappings() {
  return {
      std::make_shared<fml::NonOwnedMapping>(impeller_shaders_data,
                                             impeller_shaders_length),
      std::make_shared<fml::NonOwnedMapping>(impeller_imgui_shaders_data,
                                             impeller_imgui_shaders_length),
  };
}

Renderer::Renderer(std::shared_ptr<impeller::Context> context,
                   size_t max_frames_in_flight)
    : frames_in_flight_sema_(std::make_shared<fml::Semaphore>(
          std::max<std::size_t>(1u, max_frames_in_flight))),
      context_(std::move(context)) {
  if (!context_->IsValid()) {
    return;
  }

  is_valid_ = true;

  ImGui_ImplImpeller_Init(context_);

  ImGuiIO& im_io = ImGui::GetIO();
  im_io.IniSavingRate = -1.0f;

  ImGuiStyle& im_style = ImGui::GetStyle();
  im_style.WindowBorderSize = 1;
  im_style.WindowRounding = 4;
  im_style.FrameRounding = 2;
  im_style.ScrollbarRounding = 12;
  im_style.GrabRounding = 2;
  im_style.TabRounding = 2;
  im_style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

  ImVec4* colors = im_style.Colors;
  colors[ImGuiCol_Text] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
  colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.29f, 0.48f, 0.54f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.38f, 0.24f, 0.21f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.29f, 0.49f, 0.28f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
  colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.35f, 0.58f, 0.86f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
  colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
  colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
  colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
  colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
  colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
  colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
  colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
  colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
  colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
  colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
  colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}

Renderer::~Renderer() {
  ImGui_ImplImpeller_Shutdown();
}

bool Renderer::IsValid() const {
  return is_valid_;
}

bool Renderer::Render(std::unique_ptr<impeller::Surface> surface) const {
  TRACE_EVENT0("bdero", "Renderer::Render");
  if (!IsValid()) {
    return false;
  }

  if (!surface || !surface->IsValid()) {
    return false;
  }

  auto command_buffer = context_->CreateRenderCommandBuffer();

  if (!command_buffer) {
    return false;
  }

  command_buffer->SetLabel("Onscreen Command Buffer");

  auto render_pass = command_buffer->CreateRenderPass(
      surface->GetTargetRenderPassDescriptor());
  if (!render_pass) {
    return false;
  }

  render_pass->SetLabel("Main pass");

  // ImGui stuff
  ImGui::NewFrame();

  bool show_demo_window = true;
  ImGui::ShowDemoWindow(&show_demo_window);
  ShowParticleTextWindow();

  ImGui::Render();
  ImGui_ImplImpeller_RenderDrawData(ImGui::GetDrawData(), *render_pass);

  if (!render_pass->EncodeCommands(*GetContext()->GetTransientsAllocator())) {
    return false;
  }

  if (!frames_in_flight_sema_->Wait()) {
    return false;
  }

  if (!command_buffer->SubmitCommands(
          [sema =
               frames_in_flight_sema_](impeller::CommandBuffer::Status result) {
            sema->Signal();
            if (result != impeller::CommandBuffer::Status::kCompleted) {
              VALIDATION_LOG << "Could not commit command buffer.";
            }
          })) {
    return false;
  }

  return surface->Present();
}

std::shared_ptr<impeller::Context> Renderer::GetContext() const {
  return context_;
}

}  // namespace bdero
