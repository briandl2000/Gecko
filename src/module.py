from build import module

# The src directory is the root engine project. Its dependencies compile as
# separate unity objects and are linked with this glue unit into one library.
module(
    name="gecko",
    output="engine",
    unity="gecko_engine.cpp",
    requires=["debug_renderer"],
)
