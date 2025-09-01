#version 460
#extension GL_EXT_ray_tracing : require

layout(location=0) rayPayloadInEXT uint hit;
hitAttributeEXT vec2 attribs; // unused
void main(){ hit = 1u; }

