from build import module

# Root engine project. Its transitive source modules compile as separate unity
# objects and are linked together into the single Gecko shared library.
module(
    name="gecko",
    output="engine",
    unity="src/gecko_engine.cpp",
    requires=["src/debug_renderer"],
)
