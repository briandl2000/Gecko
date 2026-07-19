#!/usr/bin/env python3

"""Build Gecko and Gecko projects with Python's standard library only.

The file intentionally contains both the small ``module.py`` declaration API
and the build implementation. Keeping one driver makes a source checkout and a
downloaded SDK behave the same without shipping a Python package.
"""

from __future__ import annotations

import argparse
import hashlib
import json
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


# ---------------------------------------------------------------------------
# Data model and public module.py API
# ---------------------------------------------------------------------------

@dataclass
class Shader:
    """One shader declaration owned and namespaced by a module."""

    name: str
    path: str
    stage: str | None = None
    entry: str = "main"


@dataclass
class WaylandProtocol:
    """One Wayland protocol generated for a module on Linux."""

    name: str
    path: str


@dataclass
class BuildOptions:
    """Platform-specific compile and link requirements for a module."""

    defines: list[str] = field(default_factory=list)
    libraries: list[str] = field(default_factory=list)
    packages: list[str] = field(default_factory=list)
    wayland_protocols: list[WaylandProtocol] = field(default_factory=list)


@dataclass
class Module:
    """Loaded representation of one ``module.py`` declaration."""

    root: Path
    name: str
    output: str = "sources"
    output_name: str | None = None
    unity: str | None = None
    sources: list[str] = field(default_factory=list)
    requires: list[str] = field(default_factory=list)
    include_dirs: list[str] = field(default_factory=list)
    defines: list[str] = field(default_factory=list)
    windows: BuildOptions = field(default_factory=BuildOptions)
    linux: BuildOptions = field(default_factory=BuildOptions)
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


@dataclass(frozen=True)
class BuildDirectories:
    objects: Path
    generated: Path
    binary: Path
    libraries: Path

    @staticmethod
    def create(output_base: Path, config: str) -> BuildDirectories:
        root = output_base / platform_name() / config_name(config)
        result = BuildDirectories(root / "obj", root / "generated", root / "bin", root / "lib")
        for path in (result.objects, result.generated, result.binary):
            path.mkdir(parents=True, exist_ok=True)
        return result


_loading_root: Path | None = None
_loaded_module: Module | None = None
_compile_commands: dict[Path, list[str]] = {}
_compile_commands_path: Path | None = None


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
    windows: BuildOptions | None = None,
    linux: BuildOptions | None = None,
    shaders: list[Shader] | None = None,
    shader_namespace: str | None = None,
) -> None:
    """Declare the single build node owned by the current directory.

    Prefer one unity source per source module. Declare each module whose public
    API this module uses; dependencies of that module are inherited through the
    graph and do not need to be repeated.

    Args:
        name: Stable project-wide name. Names must be unique in a build graph.
        output: ``sources`` or ``headers`` for a contributing module;
            ``executable`` or ``plugin`` for a final project; ``engine`` for
            Gecko's source root. ``prebuilt`` is reserved for the staged SDK.
        output_name: Filename stem for an executable or plugin. Defaults to
            ``name``.
        unity: C++ unity source relative to this ``module.py``. This is the
            recommended source-module entrypoint.
        sources: Additional C++ sources relative to this directory. Use when a
            source genuinely should remain a separate translation unit.
        requires: Direct dependency directories relative to this module. Use
            the special name ``gecko`` when building against the engine.
        include_dirs: Additional include roots relative to this module.
        defines: Preprocessor definitions applied to this module's consumer.
        windows: Windows-only requirements produced by :func:`build_options`.
        linux: Linux-only requirements produced by :func:`build_options`.
        shaders: Shader declarations produced by :func:`shader`.
        shader_namespace: C++ namespace for generated shader symbols. A stable,
            module-owned namespace is recommended for public code.
    """
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
        windows=windows or BuildOptions(),
        linux=linux or BuildOptions(),
        shaders=shaders or [],
        shader_namespace=shader_namespace,
    )


def build_options(
    *,
    defines: list[str] | None = None,
    libraries: list[str] | None = None,
    packages: list[str] | None = None,
    wayland_protocols: list[WaylandProtocol] | None = None,
) -> BuildOptions:
    """Declare platform-specific requirements owned by a module.

    ``defines`` are added while compiling a graph containing the module.
    ``libraries`` are native linker arguments such as ``user32.lib`` or
    ``-pthread``. On Linux, ``packages`` provide both compile and link flags
    through ``pkg-config``. ``wayland_protocols`` are generated and compiled
    by the driver. Keep these requirements beside the module that uses them
    instead of adding subsystem knowledge to the build driver.
    """
    return BuildOptions(
        defines=defines or [],
        libraries=libraries or [],
        packages=packages or [],
        wayland_protocols=wayland_protocols or [],
    )


def wayland_protocol(name: str, path: str) -> WaylandProtocol:
    """Declare a protocol XML path relative to the wayland-protocols data directory."""
    return WaylandProtocol(name=name, path=path)


def shader(name: str, path: str, *, stage: str | None = None, entry: str = "main") -> Shader:
    """Describe one HLSL shader embedded in its module's final binary.

    ``name`` becomes the generated C++ symbol and must be an identifier. The
    stage is inferred from names such as ``.vert.hlsl`` and ``.frag.hlsl``;
    pass ``stage`` only for a non-standard filename. ``path`` is relative to
    the declaring ``module.py``.
    """
    return Shader(name=name, path=path, stage=stage, entry=entry)


# ---------------------------------------------------------------------------
# Generic build helpers
# ---------------------------------------------------------------------------

def fail(message: str) -> NoReturn:
    raise SystemExit(message)


def run(label: str, description: str, arguments: list[str], *, cwd: Path | None = None) -> None:
    print(f"[{label:<5}] {description}", flush=True)
    result = subprocess.run(arguments, cwd=cwd, check=False)
    if result.returncode != 0:
        fail(f"[FAIL ] {description} ({label}, exit {result.returncode})")


def capture(arguments: list[str]) -> str:
    result = subprocess.run(arguments, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        if result.stderr:
            print(result.stderr.rstrip(), file=sys.stderr)
        fail(f"[FAIL ] {Path(arguments[0]).name} exited with code {result.returncode}")
    return result.stdout.strip()


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


def safe_name(value: str) -> str:
    result = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if not result or result[0].isdigit():
        result = f"_{result}"
    return result


def record_compile(source: Path, arguments: list[str]) -> None:
    """Record the real compiler command used by clangd/Zed navigation."""
    _compile_commands.setdefault(source.resolve(), arguments)


def write_compile_commands() -> None:
    if _compile_commands_path is None or not _compile_commands:
        return
    entries_by_source: dict[Path, dict[str, object]] = {}
    if _compile_commands_path.is_file():
        try:
            previous_entries = json.loads(_compile_commands_path.read_text(encoding="utf-8"))
            for entry in previous_entries:
                source = Path(entry["file"]).resolve()
                if source.is_file():
                    entries_by_source[source] = entry
        except (json.JSONDecodeError, KeyError, TypeError):
            pass
    for source, arguments in _compile_commands.items():
        entries_by_source[source] = {
            "directory": str(_compile_commands_path.parent),
            "file": str(source),
            "arguments": arguments,
        }
    entries = [entries_by_source[source] for source in sorted(entries_by_source, key=str)]
    write_if_changed(_compile_commands_path, json.dumps(entries, indent=2) + "\n")


# ---------------------------------------------------------------------------
# Module loading and dependency graph
# ---------------------------------------------------------------------------

def load_module(module_root: Path) -> Module:
    """Load exactly one declaration from ``module_root/module.py``."""
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
    if result.output not in {"engine", "executable", "plugin", "sources", "headers", "prebuilt"}:
        fail(f"{description}: unknown output kind {result.output!r}")
    if result.output not in {"headers", "prebuilt"} and not result.source_paths():
        fail(f"{description}: a {result.output} module needs unity= or sources=")
    return result


def load_graph(root_module: Path) -> list[Module]:
    """Return dependencies before consumers and reject dependency cycles."""
    ordered: list[Module] = []
    visiting: list[Path] = []
    loaded: dict[Path, Module] = {}

    def visit(module_root: Path) -> None:
        key = module_root.resolve()
        if key in loaded:
            return
        if key in visiting:
            cycle = visiting[visiting.index(key):] + [key]
            fail("module dependency cycle:\n  " + "\n  -> ".join(str(path) for path in cycle))
        visiting.append(key)
        value = load_module(key)
        for requirement in value.requires:
            if requirement != "gecko":
                visit(value.root / requirement)
        visiting.pop()
        loaded[key] = value
        ordered.append(value)

    visit(root_module)
    names: dict[str, Path] = {}
    for value in ordered:
        previous = names.get(value.name)
        if previous is not None:
            fail(f"duplicate module name {value.name!r}:\n  {previous}\n  {value.root}")
        names[value.name] = value.root
    return ordered


def module_inputs(value: Module) -> list[Path]:
    return [value.root / "module.py"] + files_under(value.root, (".h", ".hpp", ".cpp", ".hlsl", ".hlsli"))


def print_graph(modules: list[Module]) -> None:
    """Print the build order and direct dependencies for human review."""
    by_root = {value.root: value for value in modules}
    print("[GRAPH] dependency order")
    for index, value in enumerate(modules, start=1):
        dependency_names = [
            "gecko" if requirement == "gecko" else by_root[(value.root / requirement).resolve()].name
            for requirement in value.requires
        ]
        dependencies = ", ".join(dependency_names) if dependency_names else "none"
        unity = value.unity or "headers only"
        print(f"  {index}. {value.name:<16} {value.output:<8} <- {dependencies:<36} {unity}")


# ---------------------------------------------------------------------------
# Compiler configuration
# ---------------------------------------------------------------------------


def common_defines() -> list[str]:
    if os.name == "nt":
        return [
            "GECKO_BUILD_SHARED=1",
            "GECKO_PLATFORM_WINDOWS=1",
            "_CRT_SECURE_NO_WARNINGS",
        ]
    return [
        "GECKO_BUILD_SHARED=1",
        "GECKO_PLATFORM_LINUX=1",
    ]


def active_options(value: Module) -> BuildOptions:
    return value.windows if os.name == "nt" else value.linux


def module_defines(modules: list[Module]) -> list[str]:
    result = common_defines()
    for value in modules:
        result += value.defines
        result += active_options(value).defines
    return list(dict.fromkeys(result))


def module_libraries(modules: list[Module]) -> list[str]:
    """Resolve native libraries for the active platform in graph order."""
    libraries: list[str] = []
    packages: list[str] = []
    for value in modules:
        options = active_options(value)
        libraries += options.libraries
        packages += options.packages
    libraries = list(dict.fromkeys(libraries))
    packages = list(dict.fromkeys(packages))
    if packages:
        pkg_config = require_tool("pkg-config")
        libraries = capture([pkg_config, "--libs", *packages]).split() + libraries
    return list(dict.fromkeys(libraries))


def module_compile_options(modules: list[Module]) -> list[str]:
    """Resolve compiler flags exported by packages on the active platform."""
    if os.name == "nt":
        return []
    packages: list[str] = []
    for value in modules:
        packages += active_options(value).packages
    packages = list(dict.fromkeys(packages))
    if not packages:
        return []
    pkg_config = require_tool("pkg-config")
    return list(dict.fromkeys(capture([pkg_config, "--cflags", *packages]).split()))


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


# ---------------------------------------------------------------------------
# Shader compilation and embedding
# ---------------------------------------------------------------------------

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


# ---------------------------------------------------------------------------
# Gecko engine build
# ---------------------------------------------------------------------------

def build_engine(root: Path, config: str) -> GeckoArtifact:
    if not (root / "src/gecko_engine.cpp").is_file():
        fail("this SDK contains a prebuilt Gecko engine; the engine target requires a source checkout")
    engine_root = root / "src"
    modules = load_graph(engine_root)
    engine_module = modules[-1]
    if engine_module.output != "engine":
        fail(f"{engine_root / 'module.py'} must declare output='engine'")
    directories = BuildDirectories.create(root / "out", config)

    if os.name == "nt":
        return build_engine_windows(root, config, modules, directories)
    return build_engine_linux(root, config, modules, directories)


def engine_includes(root: Path, modules: list[Module], generated: Path) -> list[Path]:
    result = [root / "include", generated]
    for value in modules:
        result.append(value.root)
        result += [(value.root / path).resolve() for path in value.include_dirs]
    return list(dict.fromkeys(result))


def engine_compile_units(modules: list[Module]) -> list[tuple[Module, int, Path]]:
    """Use one unity object per source module for useful incremental builds.

    These objects are implementation units inside one Gecko shared library;
    they are not separately distributed libraries.
    """
    result: list[tuple[Module, int, Path]] = []
    for value in modules:
        if value.output == "headers":
            continue
        for index, source in enumerate(value.source_paths()):
            result.append((value, index, source))
    return result


def engine_inputs(root: Path, value: Module, generated_inputs: list[Path], modules: list[Module]) -> list[Path]:
    inputs = [root / "build.py", value.root / "module.py", root / "include/gecko/api.h", *generated_inputs]
    if value.output == "engine":
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


def build_wayland_protocols(modules: list[Module], directories: BuildDirectories, cc: str) -> list[Path]:
    """Generate protocol sources declared by modules and return their objects."""
    protocols = [protocol for value in modules for protocol in active_options(value).wayland_protocols]
    if not protocols:
        return []
    scanner = require_tool("wayland-scanner")
    pkg_config = require_tool("pkg-config")
    protocol_dir = Path(capture([pkg_config, "--variable=pkgdatadir", "wayland-protocols"]))
    objects: list[Path] = []
    names: set[str] = set()
    for protocol in protocols:
        name = protocol.name
        if re.fullmatch(r"[A-Za-z0-9_-]+", name) is None:
            fail(f"invalid Wayland protocol name: {name}")
        if name in names:
            fail(f"duplicate Wayland protocol name: {name}")
        names.add(name)
        source = protocol_dir / protocol.path
        if not source.is_file():
            fail(f"Wayland protocol not found: {source}")
        header = directories.generated / f"{name}-client-protocol.h"
        code = directories.generated / f"{name}-protocol.c"
        if stale(header, [source]) or stale(code, [source]):
            run("WAYL", name, [scanner, "client-header", str(source), str(header)])
            run("WAYL", name, [scanner, "private-code", str(source), str(code)])
        output = directories.objects / f"{name}-protocol.o"
        if stale(output, [code]):
            run("CC", name, [cc, "-fPIC", "-Wall", "-Wextra", "-Werror", f"-I{directories.generated}",
                              "-c", str(code), "-o", str(output)])
        objects.append(output)
    return objects


def build_engine_linux(root: Path, config: str, modules: list[Module], directories: BuildDirectories) -> GeckoArtifact:
    cxx = require_tool(os.environ.get("CXX", "g++"))
    cc = require_tool(os.environ.get("CC", "gcc"))
    protocol_objects = build_wayland_protocols(modules, directories, cc)

    generated_inputs: dict[Path, list[Path]] = {}
    for value in modules:
        shader_outputs, _ = build_shaders(value, directories.generated)
        generated_inputs[value.root] = shader_outputs

    includes = engine_includes(root, modules, directories.generated)
    defines = module_defines(modules) + ["GECKO_BUILDING=1"]
    flags = linux_cpp_flags(config, includes, defines) + module_compile_options(modules)
    signature = hashlib.sha256("\0".join([cxx, *flags]).encode()).hexdigest()
    signature_file = directories.objects / "engine.signature"
    signature_changed = not signature_file.is_file() or signature_file.read_text() != signature
    engine_objects: list[Path] = []
    for value, index, source in engine_compile_units(modules):
        output = directories.objects / f"{safe_name(value.name)}_{index}.o"
        inputs = engine_inputs(root, value, generated_inputs[value.root], modules)
        command = [cxx, *flags, "-c", str(source), "-o", str(output)]
        record_compile(source, command)
        if signature_changed or stale(output, inputs):
            run("CXX", value.name, command)
        engine_objects.append(output)
    write_if_changed(signature_file, signature)

    library = directories.binary / "libGecko.so"
    link_inputs = [*engine_objects, *protocol_objects]
    if stale(library, link_inputs):
        linker = ["-nostdlib++"]
        if shutil.which("ld.lld"):
            linker += ["-fuse-ld=lld"]
        libraries = module_libraries(modules)
        run("LINK", library.name, [cxx, *linker, "-shared", "-Wl,-soname,libGecko.so", *map(str, engine_objects),
                                    *map(str, protocol_objects), *libraries, "-o", str(library)])
    return GeckoArtifact(root / "include", library, library)


def build_engine_windows(root: Path, config: str, modules: list[Module],
                         directories: BuildDirectories) -> GeckoArtifact:
    cl = require_tool("cl")
    directories.libraries.mkdir(parents=True, exist_ok=True)
    generated_inputs: dict[Path, list[Path]] = {}
    for value in modules:
        shader_outputs, _ = build_shaders(value, directories.generated)
        generated_inputs[value.root] = shader_outputs

    includes = engine_includes(root, modules, directories.generated)
    sdk = os.environ.get("VULKAN_SDK")
    if sdk:
        includes.append(Path(sdk) / "Include")
    defines = module_defines(modules) + ["GECKO_BUILDING=1"]
    flags = windows_cpp_flags(config, includes, defines)
    signature = hashlib.sha256("\0".join([cl, *flags]).encode()).hexdigest()
    signature_file = directories.objects / "engine.signature"
    signature_changed = not signature_file.is_file() or signature_file.read_text() != signature
    engine_objects: list[Path] = []
    for value, index, source in engine_compile_units(modules):
        output = directories.objects / f"{safe_name(value.name)}_{index}.obj"
        inputs = engine_inputs(root, value, generated_inputs[value.root], modules)
        command = [cl, *flags, "/c", str(source), f"/Fo{output}"]
        record_compile(source, command)
        if signature_changed or stale(output, inputs):
            run("CXX", value.name, command)
        engine_objects.append(output)
    write_if_changed(signature_file, signature)

    runtime = directories.binary / "Gecko.dll"
    import_library = directories.libraries / "Gecko.lib"
    pdb = directories.binary / "Gecko.pdb"
    if stale(runtime, engine_objects):
        link_flags: list[str] = []
        if sdk:
            link_flags.append(f"/LIBPATH:{Path(sdk) / 'Lib'}")
        libraries = module_libraries(modules)
        run("LINK", runtime.name, [cl, "/nologo", "/LD", *map(str, engine_objects), f"/Fe{runtime}", "/link",
                                     f"/IMPLIB:{import_library}", "/DEBUG", f"/PDB:{pdb}", *link_flags,
                                     *libraries])
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


# ---------------------------------------------------------------------------
# Executable and plugin projects
# ---------------------------------------------------------------------------

def build_project(driver_root: Path, module_root: Path, config: str, output_base: Path) -> Path:
    modules = load_graph(module_root)
    if not any("gecko" in value.requires for value in modules):
        fail("the project does not require the gecko module")
    gecko = resolve_gecko(driver_root, config)
    directories = BuildDirectories.create(output_base, config)

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
        includes = [gecko.include_dir, directories.generated]
        defines = module_defines(source_modules)
        generated_inputs: list[Path] = []
        for dependency in source_modules:
            if dependency.output != "headers":
                sources += dependency.source_paths()
            includes.append(dependency.root)
            includes += [(dependency.root / path).resolve() for path in dependency.include_dirs]
            shader_outputs, _ = build_shaders(dependency, directories.generated)
            generated_inputs += shader_outputs
        libraries = module_libraries(source_modules)
        protocol_objects: list[Path] = []
        if os.name != "nt" and any(active_options(item).wayland_protocols for item in source_modules):
            cc = require_tool(os.environ.get("CC", "gcc"))
            protocol_objects = build_wayland_protocols(source_modules, directories, cc)

        output_name = value.output_name or value.name
        if value.output == "plugin":
            output_name = f"{'' if os.name == 'nt' else 'lib'}{output_name}{'.dll' if os.name == 'nt' else '.so'}"
        elif os.name == "nt":
            output_name += ".exe"
        output = directories.binary / output_name
        inputs = [driver_root / "build.py", gecko.link_library, *generated_inputs, *protocol_objects]
        for dependency in source_modules:
            inputs += module_inputs(dependency)

        includes = list(dict.fromkeys(includes))
        defines = list(dict.fromkeys(defines))
        if os.name == "nt":
            flags = windows_cpp_flags(config, includes, defines)
            compiler = require_tool("cl")
            for source in sources:
                record_compile(source, [compiler, *flags, "/c", str(source)])
            command = [compiler, *flags]
            if value.output == "plugin":
                command.append("/LD")
            object_dir = directories.objects / safe_name(value.name)
            object_dir.mkdir(parents=True, exist_ok=True)
            command += [*map(str, sources), str(gecko.link_library), f"/Fo{object_dir}{os.sep}", f"/Fe{output}",
                        "/link", "/DEBUG", f"/PDB:{output.with_suffix('.pdb')}", *libraries]
        else:
            flags = linux_cpp_flags(config, includes, defines) + module_compile_options(source_modules)
            compiler = require_tool(os.environ.get("CXX", "g++"))
            for source in sources:
                record_compile(source, [compiler, *flags, "-c", str(source)])
            command = [compiler, *flags, "-nostdlib++"]
            if shutil.which("ld.lld"):
                command.append("-fuse-ld=lld")
            if value.output == "plugin":
                command.append("-shared")
            command += [*map(str, sources), *map(str, protocol_objects), f"-L{gecko.link_library.parent}",
                        "-lGecko", "-Wl,-rpath,$ORIGIN"]
            command += libraries + ["-o", str(output)]
        signature = hashlib.sha256("\0".join(map(str, command)).encode()).hexdigest()
        signature_file = directories.objects / f"{safe_name(value.name)}.signature"
        signature_changed = not signature_file.is_file() or signature_file.read_text() != signature
        if signature_changed or stale(output, inputs):
            run("LINK", output.name, command)
            write_if_changed(signature_file, signature)
        built_outputs[value.root] = output

    runtime_target = directories.binary / gecko.runtime_library.name
    if gecko.runtime_library.resolve() != runtime_target.resolve():
        shutil.copy2(gecko.runtime_library, runtime_target)
    root = load_module(module_root)
    if root.root not in built_outputs:
        fail(f"root module {root.name} must produce an executable or plugin")
    return built_outputs[root.root]


# ---------------------------------------------------------------------------
# Downloadable SDK and public-consumer smoke test
# ---------------------------------------------------------------------------

def stage_sdk(root: Path, config: str, destination: Path) -> None:
    artifact = build_engine(root, config)
    shutil.copytree(root / "include", destination / "include", dirs_exist_ok=True)
    docs = destination / "docs"
    docs.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / "docs/sdk-project.md", docs / "sdk-project.md")
    shutil.copy2(root / "docs/build-system.md", docs / "build-system.md")
    shutil.copy2(root / "build.py", destination / "build.py")
    shutil.copy2(root / "LICENSE", destination / "LICENSE")
    shutil.copy2(root / "docs/third-party-notices.txt", destination / "THIRD_PARTY_NOTICES.txt")
    write_if_changed(
        destination / "README.md",
        "# Gecko SDK\n\n"
        "This package contains Gecko's public headers, Debug and Release shared libraries, and the "
        "single-file project build driver. Start with [the project tutorial](docs/sdk-project.md).\n",
    )
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
    command = [sys.executable, str(sdk_root / "build.py"), str(root / "projects/launcher"),
               "--config", config, "--output", str(consumer)]
    run("SDK", "external launcher + plugin", command, cwd=root)
    binary = consumer / platform_name() / config_name(config) / "bin"
    launcher = binary / ("gecko_launcher.exe" if os.name == "nt" else "gecko_launcher")
    run("RUN", "headless SDK consumer", [str(launcher), "--backend=null", "--graphics=null", "--frames=2"], cwd=binary)


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------

def parse_arguments(source_checkout: bool) -> argparse.Namespace:
    if source_checkout:
        default_target = "sandbox"
        target_help = "engine, sandbox, sdk-test, graph, sdk, clean, or a module directory"
        examples = """examples:
  python3 build.py                         build the Debug sandbox
  python3 build.py engine --config release
  python3 build.py sdk-test --config release
  python3 build.py ../MyGame --output ../MyGame/out
  python3 build.py graph                   show engine dependency order
  python3 build.py sdk                     stage Debug and Release SDKs
"""
    else:
        default_target = "."
        target_help = "clean or a project module directory (default: current directory)"
        examples = """examples:
  python3 /path/to/GeckoSDK/build.py       build the project in the current directory
  python3 /path/to/GeckoSDK/build.py . --config release
  python3 /path/to/GeckoSDK/build.py clean
"""
    parser = argparse.ArgumentParser(
        description="Build Gecko or a Gecko project.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=examples,
    )
    parser.add_argument("target", nargs="?", default=default_target, help=target_help)
    parser.add_argument("-c", "--config", choices=("debug", "release"), default="debug",
                        help="build configuration (default: debug)")
    parser.add_argument("-o", "--output", type=Path, default=None,
                        help="output root for a project (default: the project's out directory)")
    return parser.parse_args()


def main() -> None:
    global _compile_commands_path
    root = Path(__file__).resolve().parent
    source_checkout = (root / "src/gecko_engine.cpp").is_file()
    arguments = parse_arguments(source_checkout)
    target = arguments.target
    config = arguments.config

    if source_checkout:
        _compile_commands_path = root / "compile_commands.json"

    if target == "clean":
        clean_root = root if source_checkout else Path.cwd()
        output = clean_root / "out"
        if output.exists():
            shutil.rmtree(output)
        compile_commands = clean_root / "compile_commands.json"
        if compile_commands.exists():
            compile_commands.unlink()
        print(f"[CLEAN] {output}")
        return
    if target == "graph":
        if not source_checkout:
            fail("the graph target requires a Gecko source checkout")
        print_graph(load_graph(root / "src"))
        return
    if target == "sdk":
        if not source_checkout:
            fail("the sdk target requires a Gecko source checkout")
        build_sdk(root)
        write_compile_commands()
        return
    if target == "engine":
        build_engine(root, config)
    elif target == "sandbox":
        if not source_checkout:
            fail("the sandbox target requires a Gecko source checkout")
        build_project(root, root / "projects/launcher", config, root / "out")
    elif target == "sdk-test":
        if not source_checkout:
            fail("the sdk-test target requires a Gecko source checkout")
        sdk_test(root, config)
    else:
        module_root = Path(target).resolve()
        _compile_commands_path = module_root / "compile_commands.json"
        output = arguments.output.resolve() if arguments.output else module_root / "out"
        build_project(root, module_root, config, output)
    write_compile_commands()
    print(f"[DONE ] {config_name(config)} {target}")


if __name__ == "__main__":
    main()
