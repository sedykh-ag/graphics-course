#version 450
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"
layout(std140, binding = 0, set = 0) uniform AppData
{
  UniformParams params;
};

layout(location = 0) out vec4 fragColor;

/* ===== SHADERTOY BEGIN ===== */

// Water caustics by David Hoskins.
// Original water turbulence effect by joltz0r

#define TAU 6.28318530718
#define MAX_ITER 5

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	float time = params.iTime * .3;
	vec2 uv = fragCoord.xy / params.iResolution.xy;
  uv.y = -uv.y;

  vec2 p = mod(uv*TAU*1.0, TAU)-250.0;

	vec2 i = vec2(p);
	float c = 1.0;
	float inten = .005;

	for (int n = 0; n < MAX_ITER; n++)
	{
		float t = time * (1.0 - (3.5 / float(n+1)));
		i = p + vec2(cos(t - i.x) + sin(t + i.y), sin(t - i.y) + cos(t + i.x));
		c += 1.0/length(vec2(p.x / (sin(i.x+t)/inten),p.y / (cos(i.y+t)/inten)));
	}
	c /= float(MAX_ITER);
	c = 1.17-pow(c, 1.4);
	vec3 colour = vec3(pow(abs(c), 8.0));
    colour = clamp(colour + vec3(0.0, 0.35, 0.6), 0.0, 1.0);

	fragColor = vec4(colour, 1.0);
}

/* ===== SHADERTOY END ===== */

void main()
{
  mainImage(fragColor, gl_FragCoord.xy);
}