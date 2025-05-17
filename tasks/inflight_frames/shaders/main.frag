#version 450
#extension GL_GOOGLE_include_directive : require


#include "UniformParams.h"
layout(std140, binding = 0, set = 0) uniform AppData
{
  UniformParams params;
};

layout(binding = 1) uniform sampler2D proceduralTexture;
layout(binding = 2) uniform samplerCube cubemapTexture;
layout(binding = 3) uniform sampler2D cupTexture;

layout (location = 0) out vec4 fragColor;

/* ===== SHADERTOY BEGIN ===== */

const int MAXSTEP = 500;
const float MAXDIST = 200.0;
const float TOL = 0.001;
const float PI = 3.1415;

vec3 getTriplanarWeights ( in vec3 n )
{
	vec3 triW = abs(n);
	return triW / (triW.x + triW.y + triW.z);
}

mat2 rotate2d(float theta) {
  float s = sin(theta), c = cos(theta);
  return mat2(c, -s, s, c);
}

// Rotation matrix around the X axis.
mat3 rotateX(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(1, 0, 0),
        vec3(0, c, -s),
        vec3(0, s, c)
    );
}

// Rotation matrix around the Y axis.
mat3 rotateY(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(c, 0, s),
        vec3(0, 1, 0),
        vec3(-s, 0, c)
    );
}

// Rotation matrix around the Z axis.
mat3 rotateZ(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(c, -s, 0),
        vec3(s, c, 0),
        vec3(0, 0, 1)
    );
}

// Identity matrix.
mat3 identity() {
    return mat3(
        vec3(1, 0, 0),
        vec3(0, 1, 0),
        vec3(0, 0, 1)
    );
}

// Camera matrix.
mat3 camera(vec3 cameraPos, vec3 lookAtPoint) {
	vec3 cd = normalize(cameraPos - lookAtPoint); // camera direction
	vec3 cr = normalize(cross(vec3(0, 1, 0), cd)); // camera right
	vec3 cu = normalize(cross(cd, cr)); // camera up

	return mat3(cr, cu, cd);
}

struct sdfCol
{
    float sdf;
    vec4 col;
};

float sdSphere( in vec3 p, in float r, in vec3 offset )
{
    return length( p - offset ) - r;
}

float sdFloor( in vec3 p )
{
    return p.y + 3.0;
}

float sdTorus( vec3 p, vec2 t )
{
  vec2 q = vec2(length(p.xy)-t.x, p.z);
  return length(q)-t.y;
}

float opSmoothUnion( float d1, float d2, float k ) {
    float h = clamp( 0.5 + 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) - k*h*(1.0-h); }

float opSmoothSubtraction( float d1, float d2, float k ) {
    float h = clamp( 0.5 - 0.5*(d2+d1)/k, 0.0, 1.0 );
    return mix( d2, -d1, h ) + k*h*(1.0-h); }

float opSmoothIntersection( float d1, float d2, float k ) {
    float h = clamp( 0.5 - 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) + k*h*(1.0-h); }

float length2( vec3 p ) { p=p*p; return sqrt(p.x+p.y+p.z); }
float length2( vec2 p ) { p=p*p; return sqrt(p.x+p.y); }

float length4( vec3 p ) { p=p*p; p=p*p; return pow(p.x+p.y+p.z, 1.0/4.0); }
float length4( vec2 p ) { p=p*p; p=p*p; return pow(p.x+p.y, 1.0/4.0); }

float length6( vec3 p ) { p=p*p*p; p=p*p; return pow(p.x+p.y+p.z, 1.0/6.0); }
float length6( vec2 p ) { p=p*p*p; p=p*p; return pow(p.x+p.y, 1.0/6.0); }

float length8( vec3 p ) { p=p*p; p=p*p; p=p*p; return pow(p.x+p.y+p.z, 1.0/8.0); }
float length8( vec2 p ) { p=p*p; p=p*p; p=p*p; return pow(p.x+p.y, 1.0/8.0); }

float sdCappedCylinder( vec3 p, float h, float r )
{
  vec2 d = abs(vec2(length4(p.xz),p.y)) - vec2(r,h);
  return min(max(d.x,d.y),0.0) + length4(max(d,0.0)) - 0.05;
}

float sdf( in vec3 p )
{
    float cyl1 = sdCappedCylinder( p + vec3(0.0, 0.0, 1.0), 0.5, 0.5 );
    float cyl2 = sdCappedCylinder( p + vec3(0.0, -0.1, 1.0), 0.38, 0.4 );

    float cyl3 = sdCappedCylinder( p + vec3(0.0, -0.1, 1.0), 0.2, 0.4 );
    float water = cyl3 + 0.02*sin(2.0*p.x + 3.0*p.y + 4.0*p.z + params.iTime*2.0);

    float cup = opSmoothSubtraction(cyl2, cyl1, 0.1);

    float handle = sdTorus( p + vec3(-0.5, 0.0, 1.0), vec2(0.38, 0.08) );
    handle = opSmoothSubtraction(cyl1, handle, 0.08);

    float contents = max(water, cyl2);

    float bgfloor = sdFloor(p);

    return min(min(opSmoothUnion(cup, handle, 0.08), contents), bgfloor);
}

sdfCol sdfWithCol( in vec3 p )
{
    float cyl1 = sdCappedCylinder( p + vec3(0.0, 0.0, 1.0), 0.5, 0.5 );
    float cyl2 = sdCappedCylinder( p + vec3(0.0, -0.1, 1.0), 0.38, 0.4 );

    float cyl3 = sdCappedCylinder( p + vec3(0.0, -0.1, 1.0), 0.2, 0.4 );
    float water = cyl3 + 0.02*sin(2.0*p.x + 3.0*p.y + 4.0*p.z + params.iTime*2.0);

    float cup = opSmoothSubtraction(cyl2, cyl1, 0.1);

    float handle = sdTorus( p + vec3(-0.5, 0.0, 1.0), vec2(0.38, 0.08) );
    handle = opSmoothSubtraction(cyl1, handle, 0.08);

    // three main components
    float cuphandle = opSmoothUnion(cup, handle, 0.08);
    float contents = max(water, cyl2);
    float bgfloor = sdFloor(p);

    vec4 col = vec4( 0.0, 0.0, 0.0, 1.0 );
    float curMin = 0.0;
    // step 1
    if (cuphandle < contents)
    {
        col = vec4( 1.0, 0.0, 0.0, 1.0 );
        curMin = cuphandle;
    }
    else
    {
        col = vec4( 0.0, 0.0, 1.0, 1.0 );
        curMin = contents;
    }

    return sdfCol( curMin, col );
}

vec3 calcNormal( in vec3 p )
{
    float e = 0.0005;
    float dx = sdf(vec3( p.x + e, p.y, p.z )) - sdf(vec3( p.x - e, p.y, p.z ));
    float dy = sdf(vec3( p.x, p.y + e, p.z )) - sdf(vec3( p.x, p.y - e, p.z ));
    float dz = sdf(vec3( p.x, p.y, p.z + e )) - sdf(vec3( p.x, p.y, p.z - e ));
    return normalize(vec3( dx, dy, dz ));
}

vec3 trace( in vec3 org, in vec3 dir, out bool hit )
{
    hit = false;
    float totalDist = 0.0;
    float dist = 0.0;
    vec3 p = org;
    for ( int step = 0; step < MAXSTEP; step++ )
    {
        dist = sdf( p );
        if ( dist < TOL )
        {
            hit = true;
            break;
        }

        p += dir * dist;
        totalDist += dist;
        if ( totalDist > MAXDIST )
        {
            break;
        }
    }

    return p;
}

vec3 traceWithCol( in vec3 org, in vec3 dir, out bool hit, out vec4 col )
{
    hit = false;
    float totalDist = 0.0;
    sdfCol dist;
    vec3 p = org;
    for ( int step = 0; step < MAXSTEP; step++ )
    {
        sdfCol dist = sdfWithCol( p );
        if ( dist.sdf < TOL )
        {
            hit = true;
            col = dist.col;
            break;
        }

        p += dir * dist.sdf;
        totalDist += dist.sdf;
        if ( totalDist > MAXDIST )
        {
            col = vec4( 1.0, 0.0, 1.0, 1.0 );
            break;
        }
    }

    return p;
}


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = (fragCoord - 0.5 * params.iResolution.xy) / params.iResolution.y;
    uv.y = -uv.y;
    vec4 mouse = params.iMouse;
    mouse.xy = (mouse.xy / params.iResolution.xy);


    vec4 col = vec4( 0.0, 0.0, 0.0, 1.0 );
    vec3 lightPos = vec3( 2.0, 4.0, 4.0 );

    vec3 lp = vec3( 0.0, 0.0, -1.0 );
    vec3 ro = vec3( 1.0, 1.5, 1.0 );

    // Camera rotation with mouse
    if ( mouse.z > 0.0 )
    {
        ro = vec3( 0.0, 0.0, 3.5 );
        ro.xz = lp.xz + rotate2d(mix(-PI/2.0, PI/2.0, mouse.x)) * ro.xz;
        ro.yz = lp.yz + rotate2d(mix(-PI/6.0, PI/2.0, mouse.y)) * ro.yz;
    }


    vec3 dir = camera( ro, lp ) * normalize( vec3( uv.xy, -1.0 ) );
    bool hit = false;
    float todist = 0.0;
    // vec3 p = trace( ro, dir, hit );
    vec3 p = traceWithCol( ro, dir, hit, col );
    if ( hit )
    {

        // lighting
        vec3 lightDir = normalize( lightPos - p );
        vec3 normal = calcNormal( p );
        float dif = clamp( dot( normal, lightDir ), 0.2, 1.0 );

        vec3 tx = getTriplanarWeights( normal );
        if ( col == vec4( 0.0, 0.0, 1.0, 1.0 ) )
        {
            // water texture
            vec4 cx = texture( proceduralTexture, p.yz );
            vec4 cy = texture( proceduralTexture, p.zx );
            vec4 cz = texture( proceduralTexture, p.xy );
            col = dif * (tx.x * cx + tx.y + cy + tx.z * cz);
        }
        else
        {
            // cup texture
            vec4 cx = texture( cupTexture, p.yz );
            vec4 cy = texture( cupTexture, p.zx );
            vec4 cz = texture( cupTexture, p.xy );
            col = dif * (tx.x * cx + tx.y + cy + tx.z * cz);
            // col = vec4(0.95, 0.7, 0.1, 1.0);
        }
    }
    else
        col = texture( cubemapTexture, dir );


    fragColor = col;

}

/* ===== SHADERTOY END ===== */

void main() {
  mainImage(fragColor, gl_FragCoord.xy);
}
