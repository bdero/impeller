#pragma once

#include <dispatch/dispatch.h>

#include <functional>
#include <memory>

#include "flutter/fml/macros.h"
#include "flutter/fml/mapping.h"
#include "flutter/fml/synchronization/semaphore.h"
#include "impeller/geometry/size.h"
#include "impeller/renderer/context.h"

namespace impeller {
class Surface;
class RenderPass;
}  // namespace impeller

namespace bdero {

class Renderer {
 public:
  static constexpr size_t kDefaultMaxFramesInFlight = 3u;

  static std::vector<std::shared_ptr<fml::Mapping>> GetShaderMappings();

  Renderer(std::shared_ptr<impeller::Context> context,
           size_t max_frames_in_flight = kDefaultMaxFramesInFlight);

  ~Renderer();

  bool IsValid() const;

  bool Render(std::unique_ptr<impeller::Surface> surface) const;

  std::shared_ptr<impeller::Context> GetContext() const;

 private:
  std::shared_ptr<fml::Semaphore> frames_in_flight_sema_;
  std::shared_ptr<impeller::Context> context_;
  bool is_valid_ = false;

  FML_DISALLOW_COPY_AND_ASSIGN(Renderer);
};

}  // namespace bdero
