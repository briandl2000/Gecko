from build import module

module(
    name="gecko",
    output="engine",
    unity="src/gecko_engine.cpp",
    requires=["src/debug_renderer"],
)
