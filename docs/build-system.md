# Build system

Gecko uses one Python-standard-library driver for source development and the downloadable SDK. Run `python3 build.py --help` for the complete command list; the common loop is:

```sh
python3 build.py engine
python3 build.py sandbox
python3 build.py sdk-test
```

Debug is the default. Add `--config release` when needed. `graph`, `sdk`, and `clean` inspect the engine graph, stage both SDK configurations, and remove generated state.

## Projects and modules

A project is the root module selected for a build. A module is one directory containing one `module.py`. Dependencies are relative directory paths; `gecko` is the special dependency for projects linking the engine.

```text
src/module.py                  engine project
src/core/module.py             contributing source module
src/platform/module.py         contributing source module
projects/launcher/module.py    executable project
projects/sandbox/module.py     plugin project
```

A typical module is deliberately small. Platform-specific requirements stay
with the module that uses them:

```python
from build import build_options, module, shader

module(
    name="debug_renderer",
    unity="gecko_debug_renderer.cpp",
    requires=["../core", "../graphics"],
    windows=build_options(libraries=["example.lib"]),
    linux=build_options(packages=["example"]),
    shaders=[shader("DebugLineVertex", "shaders/debug_line.vert.hlsl")],
)
```

Hover `module()`, `build_options()`, or `shader()` in Zed for every field. The important output kinds are:

- `sources`: contributes compiled implementation.
- `headers`: dependency and include boundary with no translation unit.
- `executable`: produces a program.
- `plugin`: produces a shared library loaded by a host.
- `engine`: the `src/module.py` root that produces Gecko.

Recommended conventions:

- Use one subsystem directory and one unity source per source module.
- Declare modules whose API you use directly. Their dependencies are inherited;
  a module requiring Platform does not repeat Core unless it also uses Core.
- Keep the graph acyclic; duplicate module names are rejected.
- Use `sources=` only when code must remain a separate translation unit.
- Give shaders logical names and let the driver own generated paths.
- Put optional backend defines, native libraries, and `pkg-config` packages in
  their owning module. The driver only resolves and invokes the tools.

Shader declarations remain in their owning module, while compilation and
embedding remain generic driver operations. Graphics consumes shader bytes at
runtime; it does not need to know how a project chose to produce them.

## What a build does

The driver loads the selected project, resolves dependencies in topological order, embeds declared shaders, and checks timestamps. Engine source modules compile into separate unity objects for useful incremental builds, then link into the single Gecko shared library. Executables and plugins link that same library.

Generated files live under `out/<platform>/<configuration>/`: `obj` contains intermediate objects, `generated` contains protocols and embedded shader headers, and `bin` contains runtime outputs. A successful build also refreshes the ignored `compile_commands.json` used by clangd. No Python packages, CMake state, or tracked editor flags are involved.

`sdk-test` is the public-boundary check: it stages headers, `build.py`, and the engine libraries, then uses that staged package to rebuild and run the launcher/plugin headlessly. A passing source build alone does not replace this check.
