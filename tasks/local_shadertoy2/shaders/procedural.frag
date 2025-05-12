#version 450
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"
layout(std140, binding = 0, set = 0) uniform AppData
{
  UniformParams params;
};

layout(location = 0) out vec4 fragColor;

void main()
{
  fragColor = vec4(1.0, 0.0, 1.0, 1.0);
}