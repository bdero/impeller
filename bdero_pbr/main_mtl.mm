#include <cstdlib>
#include <iostream>

#include "renderer.h"
#include "shaders.h"

#include "flutter/fml/closure.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "impeller/renderer/backend/metal/context_mtl.h"
#include "impeller/renderer/backend/metal/formats_mtl.h"
#include "impeller/renderer/backend/metal/surface_mtl.h"
#include "impeller/renderer/context.h"

#define GLFW_INCLUDE_NONE
#import "third_party/glfw/include/GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_COCOA
#import "third_party/glfw/include/GLFW/glfw3native.h"

#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

int main() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  fml::ScopedCleanupClosure destroy_imgui_context(
      []() { ImGui::DestroyContext(); });
  // ImGuiIO& io = ImGui::GetIO();

  ImGui::StyleColorsDark();

  if (::glfwInit() != GLFW_TRUE) {
    std::cerr << "Failed to initialize GLFW." << std::endl;
    return EXIT_FAILURE;
  }
  fml::ScopedCleanupClosure terminate([]() { ::glfwTerminate(); });

  ::glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  auto window = ::glfwCreateWindow(1024, 768, "bdero test", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window." << std::endl;
    return EXIT_FAILURE;
  }
  fml::ScopedCleanupClosure close_window(
      [window]() { ::glfwDestroyWindow(window); });

  ImGui_ImplGlfw_InitForOther(window, true);
  fml::ScopedCleanupClosure shutdown_imgui([]() { ImGui_ImplGlfw_Shutdown(); });

  bdero::Renderer renderer(
      impeller::ContextMTL::Create(bdero::Renderer::GetShaderMappings()));

  NSWindow* cocoa_window = ::glfwGetCocoaWindow(window);
  CAMetalLayer* layer = [CAMetalLayer layer];
  layer.device =
      impeller::ContextMTL::Cast(*renderer.GetContext()).GetMTLDevice();
  // This pixel format is one of the documented supported formats.
  layer.pixelFormat =
      impeller::ToMTLPixelFormat(impeller::PixelFormat::kDefaultColor);
  cocoa_window.contentView.layer = layer;
  cocoa_window.contentView.wantsLayer = YES;

  while (true) {
    ::glfwWaitEventsTimeout(1.0 / 30.0);

    if (::glfwWindowShouldClose(window)) {
      break;
    }

    ImGui_ImplGlfw_NewFrame();

    const auto layer_size = layer.bounds.size;
    const auto layer_scale = layer.contentsScale;
    layer.drawableSize = CGSizeMake(layer_size.width * layer_scale,
                                    layer_size.height * layer_scale);

    if (!renderer.Render(impeller::SurfaceMTL::WrapCurrentMetalLayerDrawable(
            renderer.GetContext(), layer))) {
      VALIDATION_LOG << "Could not render into the surface.";
      return EXIT_FAILURE;
    }
  }

  std::cout << "End\n";
  return EXIT_SUCCESS;
}
