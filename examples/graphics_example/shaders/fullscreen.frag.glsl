// glslc -fshader-stage=frag fullscreen.frag.glsl -o fullscreen.frag.spv
#version 450

layout(set = 0, binding = 0) uniform sampler2D u_Tex;

layout(location = 0) in  vec2 v_UV;
layout(location = 0) out vec4 o_Color;

void main()
{
    o_Color = texture(u_Tex, v_UV);
}
