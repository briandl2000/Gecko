# GeckoShaders.cmake — gecko_add_shaders()
#
# Compile HLSL → SPIR-V with glslc and emit a single header that exposes each
# blob as `inline constexpr unsigned char <Var>[]` via C++26 `#embed`. Any
# target (example, library, app) can include the generated header and use the
# bytes directly with `sizeof()`.
#
# Usage:
#
#   gecko_add_shaders(
#     TARGET    my_app
#     NAMESPACE my_app::shaders          # optional, defaults to no namespace
#     HEADER    shaders.h                 # generated under build/<target>/
#     SHADERS
#       triangle.vert.hlsl                # auto var name: TriangleVert
#       triangle.frag.hlsl=TriangleFrag   # explicit var name
#       plasma.comp.hlsl=PlasmaComp
#   )
#
# Stage is inferred from the second-to-last extension:
#   .vert .frag .comp .geom .tesc .tese .mesh .task → glslc -fshader-stage=...
#
# After the call, `${TARGET}` has the header's directory on its private include
# path so `#include "shaders.h"` resolves, and the C++26 `--embed-dir=` flag is
# set so `#embed "foo.spv"` finds the compiled blobs.
#
# If glslc is unavailable, the call is a no-op with a STATUS message and the
# target is left untouched (the ticket calls for "silently skip" — caller is
# expected to gate or handle absence).

if(DEFINED _GECKO_SHADERS_INCLUDED)
  return()
endif()
set(_GECKO_SHADERS_INCLUDED TRUE)

find_program(GECKO_GLSLC glslc HINTS $ENV{VULKAN_SDK}/bin)

# Convert "triangle.vert" → "TriangleVert", "fullscreen.frag" → "FullscreenFrag".
function(_gecko_shader_default_var out_var basename)
  string(REPLACE "." ";" _parts "${basename}")
  set(_out "")
  foreach(_p IN LISTS _parts)
    string(SUBSTRING "${_p}" 0 1 _first)
    string(SUBSTRING "${_p}" 1 -1 _rest)
    string(TOUPPER "${_first}" _first)
    set(_out "${_out}${_first}${_rest}")
  endforeach()
  set(${out_var} "${_out}" PARENT_SCOPE)
endfunction()

function(gecko_add_shaders)
  # Map shader filename extension → glslc stage flag. Defined inside the
  # function so it is reachable regardless of which directory scope this
  # module was first included from.
  set(_GECKO_SHADER_STAGE_vert "vertex")
  set(_GECKO_SHADER_STAGE_frag "fragment")
  set(_GECKO_SHADER_STAGE_comp "compute")
  set(_GECKO_SHADER_STAGE_geom "geometry")
  set(_GECKO_SHADER_STAGE_tesc "tesscontrol")
  set(_GECKO_SHADER_STAGE_tese "tesseval")
  set(_GECKO_SHADER_STAGE_mesh "mesh")
  set(_GECKO_SHADER_STAGE_task "task")
  set(_options)
  set(_one_value TARGET NAMESPACE HEADER SOURCE_DIR)
  set(_multi_value SHADERS)
  cmake_parse_arguments(GAS "${_options}" "${_one_value}" "${_multi_value}"
                        ${ARGN})

  if(NOT GAS_TARGET)
    message(FATAL_ERROR "gecko_add_shaders: TARGET is required")
  endif()
  if(NOT GAS_HEADER)
    set(GAS_HEADER "shaders.h")
  endif()
  if(NOT GAS_SOURCE_DIR)
    set(GAS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
  endif()
  if(NOT GAS_SHADERS)
    message(FATAL_ERROR "gecko_add_shaders: SHADERS list is empty")
  endif()

  if(NOT GECKO_GLSLC)
    message(STATUS "gecko_add_shaders(${GAS_TARGET}): glslc not found — "
                   "skipping shader compilation. Install the Vulkan SDK or "
                   "set VULKAN_SDK to enable.")
    return()
  endif()

  set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}/gecko_shaders/${GAS_TARGET}")
  file(MAKE_DIRECTORY "${_out_dir}")

  # Body of the generated header — built up as a list of #embed statements.
  set(_embed_decls "")
  set(_spv_outputs "")

  foreach(_entry IN LISTS GAS_SHADERS)
    # Split optional "=VarName" override.
    if(_entry MATCHES "^([^=]+)=(.+)$")
      set(_src_name "${CMAKE_MATCH_1}")
      set(_var_name "${CMAKE_MATCH_2}")
    else()
      set(_src_name "${_entry}")
      set(_var_name "")
    endif()

    # basename = filename without final ".hlsl" extension.
    if(NOT _src_name MATCHES "\\.hlsl$")
      message(FATAL_ERROR
        "gecko_add_shaders(${GAS_TARGET}): '${_src_name}' must end in .hlsl")
    endif()
    string(REGEX REPLACE "\\.hlsl$" "" _basename "${_src_name}")

    # Stage = the second-to-last extension (e.g. "vert" in "triangle.vert").
    if(NOT _basename MATCHES "\\.([^.]+)$")
      message(FATAL_ERROR
        "gecko_add_shaders(${GAS_TARGET}): '${_src_name}' missing stage "
        "suffix (e.g. .vert.hlsl, .frag.hlsl, .comp.hlsl)")
    endif()
    set(_stage_ext "${CMAKE_MATCH_1}")
    set(_stage "${_GECKO_SHADER_STAGE_${_stage_ext}}")
    if(NOT _stage)
      message(FATAL_ERROR
        "gecko_add_shaders(${GAS_TARGET}): unknown shader stage '.${_stage_ext}' "
        "in '${_src_name}'. Supported: vert frag comp geom tesc tese mesh task")
    endif()

    if(NOT _var_name)
      _gecko_shader_default_var(_var_name "${_basename}")
    endif()

    set(_in  "${GAS_SOURCE_DIR}/${_src_name}")
    set(_spv "${_out_dir}/${_basename}.spv")

    add_custom_command(
      OUTPUT  "${_spv}"
      COMMAND "${GECKO_GLSLC}"
              -x hlsl
              -fshader-stage=${_stage}
              -fentry-point=main
              "${_in}"
              -o "${_spv}"
      DEPENDS "${_in}"
      COMMENT "glslc ${_src_name} -> ${_basename}.spv"
      VERBATIM
    )
    list(APPEND _spv_outputs "${_spv}")

    string(APPEND _embed_decls
      "alignas(4) inline constexpr unsigned char ${_var_name}[] = {\n"
      "#embed \"${_basename}.spv\"\n"
      "};\n")
  endforeach()

  # Build the header text. Optional namespace wrapper.
  set(_header_path "${_out_dir}/${GAS_HEADER}")
  set(_header_body "// Auto-generated by gecko_add_shaders — do not edit.\n")
  string(APPEND _header_body
    "// Shader SPIR-V blobs embedded via C++26 #embed.\n"
    "#pragma once\n"
    "\n")
  if(GAS_NAMESPACE)
    string(APPEND _header_body "namespace ${GAS_NAMESPACE} {\n\n")
  endif()
  string(APPEND _header_body
    "#if defined(__clang__)\n"
    "#  pragma clang diagnostic push\n"
    "#  pragma clang diagnostic ignored \"-Wc23-extensions\"\n"
    "#endif\n"
    "\n"
    "${_embed_decls}"
    "\n"
    "#if defined(__clang__)\n"
    "#  pragma clang diagnostic pop\n"
    "#endif\n")
  if(GAS_NAMESPACE)
    string(APPEND _header_body "\n}  // namespace ${GAS_NAMESPACE}\n")
  endif()

  # Write the header at configure time. This is fine: it depends only on the
  # SHADERS list, not on the contents of the .spv files (those are pulled in
  # at compile time via #embed). If the caller changes the list, CMake re-runs
  # and rewrites the header.
  file(WRITE "${_header_path}" "${_header_body}")

  # Drive the build: a custom target that depends on every .spv ensures
  # glslc runs before the consumer compiles.
  add_custom_target(${GAS_TARGET}_shaders DEPENDS ${_spv_outputs})
  add_dependencies(${GAS_TARGET} ${GAS_TARGET}_shaders)

  target_include_directories(${GAS_TARGET} PRIVATE "${_out_dir}")
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${GAS_TARGET} PRIVATE --embed-dir=${_out_dir})
  endif()
endfunction()
