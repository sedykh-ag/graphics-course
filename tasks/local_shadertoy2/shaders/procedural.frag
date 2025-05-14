#version 450
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"
layout(std140, binding = 0, set = 0) uniform AppData
{
  UniformParams params;
};

layout(binding = 1) uniform sampler2D textureImage;

layout(location = 0) out vec4 fragColor;

void main()
{
  vec2 uv = gl_FragCoord.xy / params.iResolution.xy;
  uv.x -= params.iTime * 0.5;

  vec3 col = texture(textureImage, uv).rgb;

  fragColor = vec4(col, 1.0);
}