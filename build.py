#!/usr/bin/env python3

"""Direct Gecko build driver. Python 3 standard library only."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import platform
import re
import runpy
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from typing import NoReturn

sys.dont_write_bytecode = True
sys.modules.setdefault("build", sys.modules[__name__])


@dataclass
class Shader:
    name: str
    path: str
    stage: str | None = None
    entry: str = "main"


@dataclass
class Module:
    root: Path
    name: str
    output: str = "sources"
    output_name: str | None = None
    unity: str | None = None
    sources: list[str] = field(default_factory=list)
    requires: list[str] = field(default_factory=list)
    include_dirs: list[str] = field(default_factory=list)
    defines: list[str] = field(default_factory=list)
    system_libraries: list[str] = field(default_factory=list)
    shaders: list[Shader] = field(default_factory=list)
    shader_namespace: str | None = None

    def source_paths(self) -> list[Path]:
        paths = list(self.sources)
        if self.unity is not None:
            paths.insert(0, self.unity)
        return [(self.root / path).resolve() for path in paths]


@dataclass
class GeckoArtifact:
    include_dir: Path
    link_library: Path
    runtime_library: Path


_loading_root: Path | None = None
_loaded_module: Module | None = None


def module(
    *,
    name: str,
    output: str = "sources",
    output_name: str | None = None,
    unity: str | None = None,
    sources: list[str] | None = None,
    requires: list[str] | None = None,
    include_dirs: list[str] | None = None,
    defines: list[str] | None = None,
    system_libraries: list[str] | None = None,
    shaders: list[Shader] | None = None,
    shader_namespace: str | None = None,
) -> None:
    """Declare the module in the current module.py file."""
    global _loaded_module
    if _loading_root is None:
        raise RuntimeError("module() may only be called while loading module.py")
    if _loaded_module is not None:
        fail(f"{_loading_root / 'module.py'} declares more than one module")
    _loaded_module = Module(
        root=_loading_root,
        name=name,
        output=output,
        output_name=output_name,
        unity=unity,
        sources=sources or [],
        requires=requires or [],
        include_dirs=include_dirs or [],
        defines=defines or [],
        system_libraries=system_libraries or [],
        shaders=shaders or [],
        shader_namespace=shader_namespace,
    )


def shader(name: str, path: str, *, stage: str | None = None, entry: str = "main") -> Shader:
    return Shader(name=name, path=path, stage=stage, entry=entry)


def load_module(module_root: Path) -> Module:
    global _loading_root, _loaded_module
    module_root = module_root.resolve()
    description = module_root / "module.py"
    if not description.is_file():
        fail(f"module description not found: {description}")

    previous_root = _loading_root
    previous_module = _loaded_module
    _loading_root = module_root
    _loaded_module = None
    try:
        runpy.run_path(str(description), run_name=f"gecko_module_{safe_name(str(module_root))}")
        result = _loaded_module
    finally:
        _loading_root = previous_root
        _loaded_module = previous_module
    if result is None:
        fail(f"{description} must call module(...) exactly once")
    value = result
    if value.output not in {"engine", "executable", "plugin", "sources", "headers", "prebuilt"}:
        fail(f"{description}: unknown output kind {value.output!r}")
    if value.output not in {"headers", "prebuilt"} and not value.source_paths():
        fail(f"{description}: a {value.output} module needs unity= or sources=")
    return value


def fail(message: str) -> NoReturn:
    raise SystemExit(message)


def run(label: str, description: str, arguments: list[str], *, cwd: Path | None = None) -> None:
    print(f"[{label:<5}] {description}", flush=True)
    subprocess.run(arguments, cwd=cwd, check=True)


def capture(arguments: list[str]) -> str:
    return subprocess.check_output(arguments, text=True).strip()


def require_tool(name: str) -> str:
    result = shutil.which(name)
    if result is None:
        fail(f"required build tool not found: {name}")
    return result


def platform_name() -> str:
    machine = platform.machine().lower()
    if machine in {"amd64", "x86_64"}:
        machine = "x86_64"
    return f"{'Windows' if os.name == 'nt' else 'Linux'}-{machine}"


def config_name(config: str) -> str:
    return "Debug" if config == "debug" else "Release"


def write_if_changed(path: Path, text: str) -> None:
    if path.is_file() and path.read_text(encoding="utf-8") == text:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def files_under(root: Path, suffixes: tuple[str, ...]) -> list[Path]:
    if not root.exists():
        return []
    return [path for path in root.rglob("*") if path.is_file() and path.suffix.lower() in suffixes]


def stale(output: Path, inputs: list[Path]) -> bool:
    if not output.is_file():
        return True
    output_time = output.stat().st_mtime_ns
    return any(path.is_file() and path.stat().st_mtime_ns > output_time for path in inputs)


def common_defines() -> list[str]:
    if os.name == "nt":
        return [
            "GECKO_BUILD_SHARED=1",
            "GECKO_PLATFORM_WINDOWS=1",
            "GECKO_GRAPHICS_VULKAN=1",
            "GECKO_GRAPHICS_VULKAN_WIN32=1",
            "_CRT_SECURE_NO_WARNINGS",
        ]
    return [
        "GECKO_BUILD_SHARED=1",
        "GECKO_PLATFORM_LINUX=1",
        "GECKO_PLATFORM_LINUX_X11=1",
        "GECKO_PLATFORM_LINUX_WAYLAND=1",
        "GECKO_HAS_XKBCOMMON=1",
        "GECKO_HAVE_XDG_DECORATION=1",
        "GECKO_GRAPHICS_VULKAN=1",
        "GECKO_GRAPHICS_VULKAN_XLIB=1",
        "GECKO_GRAPHICS_VULKAN_WAYLAND=1",
    ]


def linux_cpp_flags(config: str, includes: list[Path], defines: list[str]) -> list[str]:
    flags = [
        "-std=c++26",
        "-fPIC",
        "-fno-exceptions",
        "-fno-rtti",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-Wno-unused-parameter",
        "-Wno-pedantic",
    ]
    flags += ["-O0", "-g3", "-D_DEBUG=1"] if config == "debug" else ["-O2", "-g", "-DNDEBUG=1"]
    flags += [f"-D{value}" for value in defines]
    flags += [f"-I{path}" for path in includes]
    return flags


def windows_cpp_flags(config: str, includes: list[Path], defines: list[str]) -> list[str]:
    flags = [
        "/nologo",
        "/std:c++latest",
        "/W4",
        "/WX",
        "/wd4201",
        "/wd4251",
        "/wd4324",
        "/Zc:preprocessor",
        "/EHs-c-",
        "/GR-",
        "/D_HAS_EXCEPTIONS=0",
    ]
    flags += ["/Od", "/Z7", "/D_DEBUG=1"] if config == "debug" else ["/O2", "/Z7", "/DNDEBUG=1"]
    flags += [f"/D{value}" for value in defines]
    flags += [f"/I{path}" for path in includes]
    return flags


def locate_glslc() -> str:
    configured = os.environ.get("GLSLC")
    if configured:
        return configured
    found = shutil.which("glslc")
    if found:
        return found
    sdk = os.environ.get("VULKAN_SDK")
    if sdk:
        candidate = Path(sdk) / ("Bin" if os.name == "nt" else "bin") / ("glslc.exe" if os.name == "nt" else "glslc")
        if candidate.is_file():
            return str(candidate)
    fail("glslc was not found; install the Vulkan SDK or set GLSLC")


def shader_stage(path: Path, explicit: str | None) -> str:
    if explicit is not None:
        return explicit
    stages = {".vert.hlsl": "vert", ".frag.hlsl": "frag", ".comp.hlsl": "comp", ".geom.hlsl": "geom",
              ".tesc.hlsl": "tesc", ".tese.hlsl": "tese"}
    lower = path.name.lower()
    for suffix, stage in stages.items():
        if lower.endswith(suffix):
            return stage
    fail(f"cannot infer shader stage from {path}; pass stage= explicitly")


def safe_name(value: str) -> str:
    result = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if not result or result[0].isdigit():
        result = f"_{result}"
    return result


def build_shaders(module_value: Module, generated_root: Path) -> tuple[list[Path], Path | None]:
    if not module_value.shaders:
        return [], None
    compiler = locate_glslc()
    module_name = safe_name(module_value.name)
    module_generated = generated_root / module_name
    shader_generated = module_generated / "shaders"
    shader_generated.mkdir(parents=True, exist_ok=True)
    symbols: set[str] = set()
    outputs: list[Path] = []

    for value in module_value.shaders:
        symbol = safe_name(value.name)
        if symbol != value.name:
            fail(f"shader name must be a C++ identifier: {value.name}")
        if symbol in symbols:
            fail(f"duplicate shader name in {module_value.name}: {symbol}")
        symbols.add(symbol)
        source = (module_value.root / value.path).resolve()
        if not source.is_file():
            fail(f"shader not found: {source}")
        output = shader_generated / f"{symbol}.inc"
        dependencies = [source, module_value.root / "module.py"] + files_under(source.parent, (".hlsl", ".hlsli"))
        if stale(output, dependencies):
            run("GLSLC", f"{module_value.name}/{source.name}", [
                compiler,
                "-x", "hlsl",
                f"-fshader-stage={shader_stage(source, value.stage)}",
                f"-fentry-point={value.entry}",
                "-mfmt=c",
                f"-I{source.parent}",
                str(source),
                "-o", str(output),
            ])
        outputs.append(output)

    namespace = module_value.shader_namespace or f"gecko_generated::{module_name}::shaders"
    namespace_parts = namespace.split("::")
    if any(safe_name(part) != part for part in namespace_parts):
        fail(f"invalid shader namespace in {module_value.name}: {namespace}")
    lines = ["#pragma once", "", '#include "gecko/core/types.h"', "", f"namespace {namespace} {{", ""]
    for value in module_value.shaders:
        lines += [
            f"alignas(4) inline constexpr gecko::u32 {value.name}[] =",
            f'#include "shaders/{value.name}.inc"',
            "    ;",
            "",
        ]
    lines += [f"}}  // namespace {namespace}", ""]
    header = module_generated / "Shaders.generated.h"
    write_if_changed(header, "\n".join(lines))
    outputs.append(header)
    return outputs, header


def engine_directories(root: Path, config: str) -> tuple[Path, Path, Path, Path]:
    build = root / "out" / platform_name() / config_name(config)
    return build, build / "obj", build / "generated", build / "bin"


def build_engine(root: Path, config: str) -> GeckoArtifact:
    if not (root / "src/gecko_engine.cpp").is_file():
        fail("this SDK contains a prebuilt Gecko engine; the engine target requires a source checkout")
    modules = load_graph(root)
    engine_module = modules[-1]
    if engine_module.output != "engine":
        fail(f"{root / 'module.py'} must declare output='engine'")
    build, objects, generated, binary = engine_directories(root, config)
    library_dir = build / "lib"
    objects.mkdir(parents=True, exist_ok=True)
    generated.mkdir(parents=True, exist_ok=True)
    binary.mkdir(parents=True, exist_ok=True)
    library_dir.mkdir(parents=True, exist_ok=True)

    if os.name == "nt":
        return build_engine_windows(root, config, modules, objects, generated, binary, library_dir)
    return build_engine_linux(root, config, modules, objects, generated, binary)


def engine_includes(root: Path, modules: list[Module], generated: Path) -> list[Path]:
    result = [root / "include", generated]
    for value in modules:
        result.append(value.root)
        result += [(value.root / path).resolve() for path in value.include_dirs]
    return list(dict.fromkeys(result))


def engine_compile_units(modules: list[Module]) -> list[tuple[Module, int, Path]]:
    result: list[tuple[Module, int, Path]] = []
    for value in modules:
        if value.output == "headers":
            continue
        for index, source in enumerate(value.source_paths()):
            result.append((value, index, source))
    return result


def engine_inputs(root: Path, value: Module, generated_inputs: list[Path], modules: list[Module]) -> list[Path]:
    inputs = [root / "build.py", value.root / "module.py", root / "include/gecko/api.h", *generated_inputs]
    if value.root == root:
        inputs += files_under(root / "include", (".h", ".hpp"))
        inputs += files_under(root / "src", (".h", ".hpp"))
        inputs += value.source_paths()
        inputs.append(root / "src/gecko.cpp")
    else:
        inputs += module_inputs(value)
        by_root = {module_value.root: module_value for module_value in modules}
        dependencies: set[Path] = set()

        def collect(module_value: Module) -> None:
            if module_value.root in dependencies:
                return
            dependencies.add(module_value.root)
            for requirement in module_value.requires:
                if requirement != "gecko":
                    collect(by_root[(module_value.root / requirement).resolve()])

        collect(value)
        for dependency_root in dependencies:
            dependency = by_root[dependency_root]
            inputs += files_under(root / "include/gecko" / dependency.name, (".h", ".hpp"))
    return list(dict.fromkeys(inputs))


def build_engine_linux(root: Path, config: str, modules: list[Module], objects: Path, generated: Path,
                       binary: Path) -> GeckoArtifact:
    cxx = os.environ.get("CXX", "g++")
    cc = os.environ.get("CC", "gcc")
    require_tool(cxx)
    require_tool(cc)
    require_tool("pkg-config")
    scanner = require_tool("wayland-scanner")
    protocol_dir = Path(capture(["pkg-config", "--variable=pkgdatadir", "wayland-protocols"]))
    protocols = {
        "xdg-shell": protocol_dir / "stable/xdg-shell/xdg-shell.xml",
        "xdg-decoration": protocol_dir / "unstable/xdg-decoration/xdg-decoration-unstable-v1.xml",
    }
    protocol_objects: list[Path] = []
    for name, source in protocols.items():
        header = generated / f"{name}-client-protocol.h"
        code = generated / f"{name}-protocol.c"
        if stale(header, [source]):
            run("WAYL", name, [scanner, "client-header", str(source), str(header)])
            run("WAYL", name, [scanner, "private-code", str(source), str(code)])
        output = objects / f"{name}-protocol.o"
        if stale(output, [code]):
            run("CC", name, [cc, "-fPIC", "-Wall", "-Wextra", "-Werror", f"-I{generated}", "-c", str(code), "-o", str(output)])
        protocol_objects.append(output)

    generated_inputs: dict[Path, list[Path]] = {}
    for value in modules:
        shader_outputs, _ = build_shaders(value, generated)
        generated_inputs[value.root] = shader_outputs

    includes = engine_includes(root, modules, generated)
    defines = common_defines() + ["GECKO_BUILDING=1"]
    for value in modules:
        defines += value.defines
    flags = linux_cpp_flags(config, includes, list(dict.fromkeys(defines)))
    signature = hashlib.sha256("\0".join([cxx, *flags]).encode()).hexdigest()
    signature_file = objects / "engine.signature"
    signature_changed = not signature_file.is_file() or signature_file.read_text() != signature
    engine_objects: list[Path] = []
    for value, index, source in engine_compile_units(modules):
        output = objects / f"{safe_name(value.name)}_{index}.o"
        inputs = engine_inputs(root, value, generated_inputs[value.root], modules)
        if signature_changed or stale(output, inputs):
            run("CXX", value.name, [cxx, *flags, "-c", str(source), "-o", str(output)])
        engine_objects.append(output)
    write_if_changed(signature_file, signature)

    library = binary / "libGecko.so"
    link_inputs = [*engine_objects, *protocol_objects]
    if stale(library, link_inputs):
        linker = ["-nostdlib++"]
        if shutil.which("ld.lld"):
            linker += ["-fuse-ld=lld"]
        platform_libraries = capture(["pkg-config", "--libs", "wayland-client", "wayland-cursor", "xkbcommon", "x11", "xrandr", "vulkan"]).split()
        run("LINK", library.name, [cxx, *linker, "-shared", "-Wl,-soname,libGecko.so", *map(str, engine_objects),
                                    *map(str, protocol_objects), *platform_libraries, "-pthread", "-ldl", "-lm", "-o", str(library)])
    return GeckoArtifact(root / "include", library, library)


def build_engine_windows(root: Path, config: str, modules: list[Module], objects: Path, generated: Path,
                         binary: Path, library_dir: Path) -> GeckoArtifact:
    cl = require_tool("cl")
    generated_inputs: dict[Path, list[Path]] = {}
    for value in modules:
        shader_outputs, _ = build_shaders(value, generated)
        generated_inputs[value.root] = shader_outputs

    includes = engine_includes(root, modules, generated)
    sdk = os.environ.get("VULKAN_SDK")
    if sdk:
        includes.append(Path(sdk) / "Include")
    defines = common_defines() + ["GECKO_BUILDING=1"]
    for value in modules:
        defines += value.defines
    flags = windows_cpp_flags(config, includes, list(dict.fromkeys(defines)))
    signature = hashlib.sha256("\0".join([cl, *flags]).encode()).hexdigest()
    signature_file = objects / "engine.signature"
    signature_changed = not signature_file.is_file() or signature_file.read_text() != signature
    engine_objects: list[Path] = []
    for value, index, source in engine_compile_units(modules):
        output = objects / f"{safe_name(value.name)}_{index}.obj"
        inputs = engine_inputs(root, value, generated_inputs[value.root], modules)
        if signature_changed or stale(output, inputs):
            run("CXX", value.name, [cl, *flags, "/c", str(source), f"/Fo{output}"])
        engine_objects.append(output)
    write_if_changed(signature_file, signature)

    runtime = binary / "Gecko.dll"
    import_library = library_dir / "Gecko.lib"
    pdb = binary / "Gecko.pdb"
    if stale(runtime, engine_objects):
        vulkan_library = "vulkan-1.lib"
        link_flags: list[str] = []
        if sdk:
            link_flags.append(f"/LIBPATH:{Path(sdk) / 'Lib'}")
        run("LINK", runtime.name, [cl, "/nologo", "/LD", *map(str, engine_objects), f"/Fe{runtime}", "/link",
                                     f"/IMPLIB:{import_library}", "/DEBUG", f"/PDB:{pdb}", *link_flags,
                                     "user32.lib", "shell32.lib", "shcore.lib", "ole32.lib", "winmm.lib", vulkan_library])
    return GeckoArtifact(root / "include", import_library, runtime)


def sdk_artifact(sdk_root: Path, config: str) -> GeckoArtifact:
    config_dir = sdk_root / "lib" / config_name(config)
    if os.name == "nt":
        artifact = GeckoArtifact(sdk_root / "include", config_dir / "Gecko.lib", config_dir / "Gecko.dll")
    else:
        library = config_dir / "libGecko.so"
        artifact = GeckoArtifact(sdk_root / "include", library, library)
    if not artifact.link_library.is_file() or not artifact.runtime_library.is_file():
        fail(f"Gecko SDK does not contain a {config_name(config)} engine for {platform_name()}")
    return artifact


def resolve_gecko(driver_root: Path, config: str) -> GeckoArtifact:
    if (driver_root / "src/gecko_engine.cpp").is_file():
        return build_engine(driver_root, config)
    return sdk_artifact(driver_root, config)


def load_graph(root_module: Path) -> list[Module]:
    ordered: list[Module] = []
    visiting: set[Path] = set()
    loaded: dict[Path, Module] = {}

    def visit(module_root: Path) -> None:
        key = module_root.resolve()
        if key in loaded:
            return
        if key in visiting:
            fail(f"module dependency cycle at {key}")
        visiting.add(key)
        value = load_module(key)
        for requirement in value.requires:
            if requirement != "gecko":
                visit((value.root / requirement).resolve())
        visiting.remove(key)
        loaded[key] = value
        ordered.append(value)

    visit(root_module)
    return ordered


def module_inputs(value: Module) -> list[Path]:
    return [value.root / "module.py"] + files_under(value.root, (".h", ".hpp", ".cpp", ".hlsl", ".hlsli"))


def build_project(driver_root: Path, module_root: Path, config: str, output_base: Path) -> Path:
    modules = load_graph(module_root)
    if not any("gecko" in value.requires for value in modules):
        fail("the project does not require the gecko module")
    gecko = resolve_gecko(driver_root, config)
    build = output_base / platform_name() / config_name(config)
    objects = build / "obj"
    generated = build / "generated"
    binary = build / "bin"
    objects.mkdir(parents=True, exist_ok=True)
    generated.mkdir(parents=True, exist_ok=True)
    binary.mkdir(parents=True, exist_ok=True)

    built_outputs: dict[Path, Path] = {}
    modules_by_root = {value.root: value for value in modules}

    def compile_dependencies(root_value: Module) -> list[Module]:
        result: list[Module] = []
        seen: set[Path] = set()

        def collect(value: Module) -> None:
            for requirement in value.requires:
                if requirement == "gecko":
                    continue
                dependency = modules_by_root[(value.root / requirement).resolve()]
                if dependency.output in {"sources", "headers"}:
                    collect(dependency)
                    if dependency.root not in seen:
                        seen.add(dependency.root)
                        result.append(dependency)

        collect(root_value)
        result.append(root_value)
        return result

    for value in modules:
        if value.output not in {"plugin", "executable"}:
            continue
        source_modules = compile_dependencies(value)
        sources: list[Path] = []
        includes = [gecko.include_dir, generated]
        defines = common_defines()
        libraries: list[str] = []
        generated_inputs: list[Path] = []
        for dependency in source_modules:
            if dependency.output != "headers":
                sources += dependency.source_paths()
            includes.append(dependency.root)
            includes += [(dependency.root / path).resolve() for path in dependency.include_dirs]
            defines += dependency.defines
            libraries += dependency.system_libraries
            shader_outputs, _ = build_shaders(dependency, generated)
            generated_inputs += shader_outputs

        output_name = value.output_name or value.name
        if value.output == "plugin":
            output_name = f"{'' if os.name == 'nt' else 'lib'}{output_name}{'.dll' if os.name == 'nt' else '.so'}"
        elif os.name == "nt":
            output_name += ".exe"
        output = binary / output_name
        inputs = [gecko.link_library, *generated_inputs]
        for dependency in source_modules:
            inputs += module_inputs(dependency)

        if stale(output, inputs):
            if os.name == "nt":
                flags = windows_cpp_flags(config, includes, defines)
                command = [require_tool("cl"), *flags]
                if value.output == "plugin":
                    command.append("/LD")
                object_dir = objects / safe_name(value.name)
                object_dir.mkdir(parents=True, exist_ok=True)
                command += [*map(str, sources), str(gecko.link_library), f"/Fo{object_dir}{os.sep}", f"/Fe{output}",
                            "/link", "/DEBUG", f"/PDB:{output.with_suffix('.pdb')}", *libraries]
            else:
                flags = linux_cpp_flags(config, includes, defines)
                command = [os.environ.get("CXX", "g++"), *flags, "-nostdlib++"]
                if shutil.which("ld.lld"):
                    command.append("-fuse-ld=lld")
                if value.output == "plugin":
                    command.append("-shared")
                command += [*map(str, sources), f"-L{gecko.link_library.parent}", "-lGecko", "-Wl,-rpath,$ORIGIN", "-lm"]
                command += libraries + ["-o", str(output)]
            run("LINK", output.name, command)
        built_outputs[value.root] = output

    runtime_target = binary / gecko.runtime_library.name
    if gecko.runtime_library.resolve() != runtime_target.resolve():
        shutil.copy2(gecko.runtime_library, runtime_target)
    root = load_module(module_root)
    if root.root not in built_outputs:
        fail(f"root module {root.name} must produce an executable or plugin")
    return built_outputs[root.root]


def stage_sdk(root: Path, config: str, destination: Path) -> None:
    artifact = build_engine(root, config)
    shutil.copytree(root / "include", destination / "include", dirs_exist_ok=True)
    shutil.copy2(root / "build.py", destination / "build.py")
    shutil.copy2(root / "LICENSE", destination / "LICENSE")
    shutil.copy2(root / "README.md", destination / "README.md")
    module_dir = destination / "modules" / "gecko"
    module_dir.mkdir(parents=True, exist_ok=True)
    write_if_changed(module_dir / "module.py", 'from build import module\n\nmodule(name="gecko", output="prebuilt")\n')
    config_dir = destination / "lib" / config_name(config)
    config_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(artifact.runtime_library, config_dir / artifact.runtime_library.name)
    if artifact.link_library.resolve() != artifact.runtime_library.resolve():
        shutil.copy2(artifact.link_library, config_dir / artifact.link_library.name)
    if os.name == "nt":
        pdb = artifact.runtime_library.with_suffix(".pdb")
        if pdb.is_file():
            shutil.copy2(pdb, config_dir / pdb.name)


def build_sdk(root: Path) -> Path:
    destination = root / "out" / "sdk"
    if destination.exists():
        shutil.rmtree(destination)
    for config in ("debug", "release"):
        stage_sdk(root, config, destination)
    print(f"[DONE ] {destination}")
    return destination


def sdk_test(root: Path, config: str) -> None:
    test_root = root / "out" / "sdk-test"
    sdk_root = test_root / "sdk"
    consumer = test_root / "consumer"
    if test_root.exists():
        shutil.rmtree(test_root)
    stage_sdk(root, config, sdk_root)
    command = [sys.executable, str(sdk_root / "build.py"), config, str(root / "projects/launcher"), "--output", str(consumer)]
    run("SDK", "external launcher + plugin", command, cwd=root)
    binary = consumer / platform_name() / config_name(config) / "bin"
    launcher = binary / ("gecko_launcher.exe" if os.name == "nt" else "gecko_launcher")
    run("RUN", "headless SDK consumer", [str(launcher), "--backend=null", "--graphics=null", "--frames=2"], cwd=binary)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build Gecko or a Gecko module")
    parser.add_argument("first", nargs="?", default="debug")
    parser.add_argument("second", nargs="?")
    parser.add_argument("--output", type=Path, default=None)
    return parser.parse_args()


def main() -> None:
    arguments = parse_arguments()
    root = Path(__file__).resolve().parent
    if arguments.first == "clean":
        output = root / "out"
        if output.exists():
            shutil.rmtree(output)
        print(f"[CLEAN] {output}")
        return
    if arguments.first == "sdk":
        if not (root / "src/gecko_engine.cpp").is_file():
            fail("the sdk target requires a Gecko source checkout")
        build_sdk(root)
        return
    if arguments.first not in {"debug", "release"}:
        fail("usage: python build.py [debug|release] [engine|sandbox|sdk-test|module-path]\n       python build.py [sdk|clean]")

    config = arguments.first
    target = arguments.second or "sandbox"
    if target == "engine":
        build_engine(root, config)
    elif target == "sandbox":
        build_project(root, root / "projects/launcher", config, root / "out")
    elif target == "sdk-test":
        sdk_test(root, config)
    else:
        module_root = Path(target).resolve()
        output = arguments.output.resolve() if arguments.output else Path.cwd() / "out"
        build_project(root, module_root, config, output)
    print(f"[DONE ] {config_name(config)} {target}")


if __name__ == "__main__":
    main()
