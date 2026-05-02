# Benchmark binaries.
#
# Layout:
#   bench/<module>/CMakeLists.txt produces an executable named
#   `bench_<module>` that links Gecko::Bench + Gecko::BenchMain.
#
# Built only when GECKO_BUILD_BENCHMARKS=ON (set by `gk bench`).

if(NOT GECKO_BUILD_BENCHMARKS)
  return()
endif()

# Output bench binaries to bin/<cfg>/bench/ (gitignored).
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER ${cfg} CFGU)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFGU}
      "${GECKO_OUT_DIR}/bin/${cfg}/bench")
endforeach()

add_subdirectory(debug_renderer)
