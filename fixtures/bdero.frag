uniform FrameInfo {
  float current_time;
  vec2 cursor_position;
  vec2 window_size;
} frame;

in vec2 interporlated_texture_coordinates;

out vec4 frag_color;

#define PI acos(-1.)

uniform samplerCube iChannel0; // Reflection map
uniform sampler2D iChannel1; // Blue noise

const float EPSILON = 0.0001;
const int MAX_STEPS = 80;
const float MAX_DIST = 300.;
const vec4 GLOW_COLOR = vec4(0.96, 0.98, 1.0, 1);
const vec4 FRESNEL_COLOR = vec4(0.97, 0.8, 0.8, 1);
const float GLOW_BLEND = 1.0;

const float FOCAL_LENGTH = 6.0;
const float APERTURE_SIZE = 0.3;
const int RAYS_PER_FRAG = 10;

vec3 hash3(uint n) {
    // Integer hash copied from Hugo Elias
    n = (n << 13U) ^ n;
    n = n*(n*n*15731U + 789221U) + 1376312589U;
    uvec3 k = n*uvec3(n,n*16807U,n*48271U);
    return vec3( k & uvec3(0x7fffffffU))/float(0x7fffffff);
}

vec4 blueNoise(vec2 position) {
    return texture(iChannel1, position);
}

vec3 opRep(vec3 p, vec3 c) {
    return mod(p + 0.5*c, c) - 0.5*c;
}

float opUnion(float d1, float d2) {
    return min(d1, d2);
}

float opSubtraction(float d1, float d2) {
    return max(d1, -d2);
}

float sminPoly(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b - a)/k, 0.0, 1.0);
    return mix(b, a, h) - k*h*(1.0 - h);
}

float sphereField(vec3 s, vec3 spherePos, float size) {
    return length(s - spherePos) - size;
}

float sceneField(vec3 pos, vec3 cameraPos) {
    vec3 sneksPos = vec3(pos.x + cos(pos.z + frame.current_time*2.)/4., pos.y + sin(pos.z + frame.current_time*2.)/4., pos.z);
    float dist = 0.4;
    float snekDist = 4.;
    sneksPos = opRep(sneksPos, vec3(snekDist, snekDist, dist));
    float sphere1 = sphereField(sneksPos, vec3(0, 0, 0), 0.7);
    float sphere2 = sphereField(sneksPos, vec3(0, 0, dist), 0.7);
    float sphere3 = sphereField(sneksPos, vec3(0, 0, -dist), 0.7);
    float sneks1 = sminPoly(sminPoly(sphere1, sphere2, 0.08), sphere3, 0.1);

    sneksPos = vec3(pos.x, pos.y + sin(pos.z + frame.current_time*3.)/4., pos.z + cos(pos.z + frame.current_time*3.)/4.);
    sneksPos = opRep(sneksPos, vec3(dist, snekDist, snekDist));
    sphere1 = sphereField(sneksPos, vec3(0, 0, 0), 0.7);
    sphere2 = sphereField(sneksPos, vec3(dist, 0, 0), 0.7);
    sphere3 = sphereField(sneksPos, vec3(-dist, 0, 0), 0.7);
    float sneks2 = sminPoly(sminPoly(sphere1, sphere2, 0.08), sphere3, 0.1);

    float sneks = sminPoly(sneks1, sneks2, 0.4);

    float cameraNearClip = sphereField(pos, cameraPos, 0.5);
    return opSubtraction(sneks, cameraNearClip);
}

vec3 sceneGradient(vec3 pos, vec3 cameraPos) {
    return normalize(vec3(
        sceneField(pos+vec3(EPSILON,0,0), cameraPos) - sceneField(pos+vec3(-EPSILON,0,0), cameraPos),
        sceneField(pos+vec3(0,EPSILON,0), cameraPos) - sceneField(pos+vec3(0,-EPSILON,0), cameraPos),
        sceneField(pos+vec3(0,0,EPSILON), cameraPos) - sceneField(pos+vec3(0,0,-EPSILON), cameraPos)
    ));
}

float march(vec3 pos, vec3 dir, out int stepsTaken) {
    float depth = 0.;
    for (int i = 0; i < MAX_STEPS; i++) {
        if (depth > MAX_DIST) {
            stepsTaken = i;
            return MAX_DIST;
        }

        float dist = sceneField(pos + dir*depth, pos);
        if (dist < EPSILON) {
            stepsTaken = i;
            return depth;
        }

        depth += dist;
    }
    stepsTaken = MAX_STEPS;
    return MAX_DIST;
}
vec3 getFragDir(vec2 fragCoord, vec3 camForward) {
    vec2 uv = (fragCoord - 0.5*frame.window_size.xy)/frame.window_size.xx;
    vec3 camRight = cross(camForward, vec3(0, 1, 0));
    vec3 camUp = cross(camRight, camForward);

    float fov = 60.*3.141592/180.;
    return normalize(
        camForward*cos(fov) +
        camRight*uv.x*sin(fov) +
        camUp*uv.y*sin(fov));
}

vec4 environmentColor(vec3 fragDir) {
    return texture(iChannel0, fragDir);
}

vec4 surfaceColor(vec3 fragDir, vec3 surfacePos, vec3 surfaceNormal) {
    vec3 reflectionDir = reflect(fragDir, surfaceNormal);
    vec4 reflectionColor = texture(iChannel0, reflectionDir);

    return mix(FRESNEL_COLOR, reflectionColor, dot(-fragDir, surfaceNormal) - 0.5f);
}

vec4 combinedColor(vec3 camPos, vec3 fragDir) {
    int stepsTaken;
    float dist = march(camPos, fragDir, stepsTaken);

    vec4 resultColor;
    if (dist >= MAX_DIST) {
    	resultColor = environmentColor(fragDir);
    } else {
        vec3 surfacePos = camPos + fragDir*dist;
        vec3 surfaceNormal = sceneGradient(surfacePos, camPos);

        resultColor = surfaceColor(fragDir, surfacePos, surfaceNormal);
    }

    float glowPercentage = float(stepsTaken)/float(MAX_STEPS);
    return mix(resultColor, GLOW_COLOR, glowPercentage*GLOW_BLEND);
}

void main() {
    vec2 fragCoord = interporlated_texture_coordinates * frame.window_size;

    vec3 camPos = vec3(sin(frame.current_time + .3)*6.25, cos(frame.current_time + 0.3)*3., cos(frame.current_time - .1)*5.4);
    //vec3 camPos = vec3(0, 3, 5);
    vec3 camDir = normalize(-camPos);
    //camPos.z += frame.current_time*PI*4.;

    vec3 fragDir = getFragDir(fragCoord, camDir);
    vec3 focalPos = camPos + fragDir*FOCAL_LENGTH;

    for (int i = 0; i < RAYS_PER_FRAG; i++) {
        vec3 rayStart = camPos + blueNoise(hash3(uint(RAYS_PER_FRAG + i)).xy).xyz*APERTURE_SIZE;
        vec3 rayDir = normalize(focalPos - rayStart);

        vec4 resultColor = combinedColor(rayStart, rayDir);
        frag_color += resultColor/float(RAYS_PER_FRAG);
    }
}
