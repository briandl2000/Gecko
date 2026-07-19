from build import module

module(
    name="graphics",
    unity="gecko_graphics.cpp",
    requires=["../core", "../math", "../platform"],
)
