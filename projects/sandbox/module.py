from build import module, shader

module(
    name="sandbox",
    output="plugin",
    output_name="gecko_sandbox",
    unity="plugin.cpp",
    requires=["gecko"],
    shader_namespace="gecko::sandbox::shaders",
    shaders=[
        shader("SmokeCompute", "shaders/smoke.comp.hlsl"),
    ],
)
