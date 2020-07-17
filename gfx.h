#pragma once

#include "ctk/ctk.h"
#include "ctk/math.h"

struct gfx_transform
{
    ctk::vec3<f32> Position;
    ctk::vec3<f32> Rotation;
    ctk::vec3<f32> Scale;
};
