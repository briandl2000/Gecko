from build import module, shader

module(
    name="debug_renderer",
    unity="gecko_debug_renderer.cpp",
    requires=["../core", "../math", "../platform", "../graphics"],
    shader_namespace="gecko::debug_renderer::shaders",
    shaders=[
        shader("DebugLineVertex", "shaders/debug_line.vert.hlsl"),
        shader("DebugLinePixel", "shaders/debug_line.frag.hlsl"),
    ],
)
