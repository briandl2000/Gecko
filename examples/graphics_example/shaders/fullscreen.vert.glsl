// glslc -fshader-stage=vert fullscreen.vert.glsl -o fullscreen.vert.spv
#version 450

// Fullscreen triangle trick: one triangle covering the NDC quad,
// driven by gl_VertexIndex 0..2 — no vertex buffer required.
layout(location = 0) out vec2 v_UV;

void main()
{
    v_UV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(v_UV * 2.0 - 1.0, 0.0, 1.0);
}
