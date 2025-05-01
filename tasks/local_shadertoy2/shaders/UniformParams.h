#ifndef UNIFORM_PARAMS_H_INCLUDED
#define UNIFORM_PARAMS_H_INCLUDED

#ifdef __cplusplus
  #define ALIGN_IF_CPP(n) alignas(n)
#else
  #define ALIGN_IF_CPP(n)
#endif

#include "cpp_glsl_compat.h"


struct UniformParams
{
  ALIGN_IF_CPP(4) shader_float iTime;       // current time in seconds
  ALIGN_IF_CPP(8) shader_uvec2 iResolution; // viewport resolution (in pixels)
  ALIGN_IF_CPP(16) shader_vec4 iMouse;      // xy = current pixel coords (if LMB is down). zw = click pixel
};


#endif // UNIFORM_PARAMS_H_INCLUDED
