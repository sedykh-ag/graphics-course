#version 450
#extension GL_GOOGLE_include_directive : require


#include "UniformParams.h"
layout(std140, binding = 0, set = 0) uniform AppData
{
  UniformParams params;
};


layout (location = 0) out vec4 fragColor;

void main() {

  vec2 uv = gl_FragCoord.xy / params.iResolution.xy;

  vec3 col = 0.5 + 0.5*cos(params.iTime+uv.xyx+vec3(0,2,4));

  fragColor = vec4(col, 1.0);
}
