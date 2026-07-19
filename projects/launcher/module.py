module(
    name="launcher",
    output="executable",
    output_name="gecko_launcher",
    unity="main.cpp",
    requires=["gecko", "../sandbox"],
)
