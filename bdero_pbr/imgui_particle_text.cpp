#include "imgui_particle_text.h"
#include <math.h>

#define IMGUI_DEFINE_MATH_OPERATORS  // Access to math operators
#include "third_party/imgui/imgui_internal.h"

#define IM_CLAMP(V, MN, MX) ((V) < (MN) ? (MN) : (V) > (MX) ? (MX) : (V))

// https://github.com/ocornut/imgui/commit/7ef5b681567eb7a0c2317c8a77afbbab1cd8fcc7

// Mini "Particle Text" demo by Brandon DeRosier
// Originally done as a 1024 bytes source, as part of the "ImDrawList coding
// party" challenge:
// - https://github.com/ocornut/imgui/issues/3606#issuecomment-734636054
// - https://twitter.com/algebrandon/status/1332182010593304576
// This is a fun thing to inspire people to see the potential of using
// ImDrawList for custom drawing. Don't do this at home!
static void ShowFxParticleText(ImDrawList* draw_list,
                               ImVec2 p0,
                               ImVec2 p1,
                               float time) {
  // The base diameter (and distance apart) all of the circles should be on the
  // grid. The actual diameter is inflated a bit from this value so that the
  // circles smoosh together.
  const float diameter = 11.0f;
  const float scale = (p1.x - p0.x) / 400.0f;

  // This packed array contains a 17x13 grid (221-bit) spelling the word "DEAR
  // IMGUI" Our storage 32x7 (224-bit) so the least significant 3 bits are
  // unused (hence the -3 in the loop below).
  const unsigned int GRID[] = {0xD9000080, 0x750A2B18, 0xDC2A2A17, 0x0200025D,
                               0x5AB1E800, 0x26EAB555, 0x01800100};

  // Storage for a rotating circle (we use the index for sorting and for
  // computing the color)
  struct GridCircle {
    float x, y, z;
    int index;
  };
  GridCircle circles[221];

  // Helper functions
  struct f {
    // This is a S-curve function `1/(1+e^-t)`, but with a range of 0 to 2 PI
    // because this demo happens to only use it for computing rotations.
    // See also: https://en.wikipedia.org/wiki/Sigmoid_function
    static float Sigmoid(float T) {
      return 1.f / (1 + expf(-(T))) * 3.141592f * 2.0f;
    }

    // This is a Sine function with an exponentially decaying magnitude
    // `sin(t)`. The intended domain is 0 to ~5, and the range is -1 to 1.
    static float Bounce(float T) { return sinf((T)*3.0f) * expf(-(T)); }

    // 2D rotation formula. This demo uses Euler angles, and so this formula is
    // used three times (once for each basis axis) when computing the position
    // of each circle. See also: https://en.wikipedia.org/wiki/Rotation_matrix
    static void Rotate(float& u, float& v, float r) {
      float U = u;
      u = cosf(r) * u - sinf(r) * v;
      v = sinf(r) * U + cosf(r) * v;
    }

    // Sorting function so that circles with higher Z values (which are further
    // away from the camera) are given lower order
    static int IMGUI_CDECL GridCircleCompareByZ(const void* a, const void* b) {
      return (((const GridCircle*)b)->z - ((const GridCircle*)a)->z > 0.0f)
                 ? 1
                 : -1;
    }
  };

  // For each circle in the grid: rotate, calculate z and calculate x
  const float loop_time = fmodf(time, 6.0f) * 10.0f;
  for (int n = 3; n < 224; n++) {
    int index = (n - 3);
    float x = (float)(index % 17) * diameter - 9.0f * diameter;
    float y = (float)(index / 17) * diameter - 6.0f * diameter;
    float z = 0.0f;
    float distance = sqrtf(x * x + y * y) / 32.0f;
    f::Rotate(x, y,
              f::Sigmoid(loop_time - 20 - distance) +
                  cosf(time / 2) / 4);  // Rotate on Z axis
    f::Rotate(y, z,
              f::Sigmoid(loop_time - 4 - distance) +
                  cosf(time / 3) / 4);  // Rotate on X axis
    f::Rotate(x, z,
              f::Sigmoid(loop_time - 12 - distance) +
                  cosf(time / 7) / 4);  // Rotate on Y axis

    z -= (loop_time - distance > 28)
             ? f::Bounce((loop_time - 28 - distance) / 2) * 50
             : 0;
    z = (GRID[n / 32] & (1 << (n % 32))) ? z / 100.f + 1.f : 0.0f;

    circles[index].x = p0.x + (p1.x - p0.x) * 0.5f + scale * x / z;
    circles[index].y = p0.y + (p1.y - p0.y) * 0.5f + scale * y / z;
    circles[index].z = z;
    circles[index].index = index;
  }

  // Sort back-to-front, then draw
  qsort(&circles[0], 221, sizeof(circles[0]), f::GridCircleCompareByZ);
  for (int i = 0; i < 221; i++) {
    GridCircle* c = &circles[i];
    if (c->z == 0.0f)
      continue;
    ImU32 col = IM_COL32(c->index > 102 ? 0 : 255, c->index < 102 ? 0 : 255,
                         IM_CLAMP((int)(765 - c->z * 637), 0, 255), 255);
    float rad = diameter * .8f / c->z;
    draw_list->AddCircleFilled(ImVec2(c->x, c->y), rad * scale, col);
  }
}

void ShowParticleTextWindow() {
  ImGuiIO& io = ImGui::GetIO();
  ImGui::Begin("FX", NULL, ImGuiWindowFlags_AlwaysAutoResize);
  ImVec2 size(320.0f, 180.0f);
  ImGui::InvisibleButton("canvas", size);
  ImVec2 p0 = ImGui::GetItemRectMin();
  ImVec2 p1 = ImGui::GetItemRectMax();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->PushClipRect(p0, p1);

  ImVec4 mouse_data;
  mouse_data.x = (io.MousePos.x - p0.x) / size.x;
  mouse_data.y = (io.MousePos.y - p0.y) / size.y;
  mouse_data.z = io.MouseDownDuration[0];
  mouse_data.w = io.MouseDownDuration[1];

  ShowFxParticleText(draw_list, p0, p1, static_cast<float>(ImGui::GetTime()));
  draw_list->PopClipRect();
  ImGui::End();
}
