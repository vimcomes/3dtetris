#pragma once

#include "math.h"

struct RenderPalette
{
    Vec3 clear{0.024f, 0.012f, 0.059f};
    Vec3 grid{1.0f, 1.0f, 1.0f};
    Vec3 outline{0.92f, 0.95f, 0.98f};
    Vec3 grad_bottom{0.06f, 0.02f, 0.14f};
    Vec3 grad_top{0.02f, 0.01f, 0.06f};
};
