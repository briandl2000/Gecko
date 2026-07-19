from build import module

module(
    name="platform",
    unity="gecko_platform.cpp",
    requires=["../core", "../math"],
)
