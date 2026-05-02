# Bench harness library (static).
#
# Provides:
#   Gecko::Bench      -- registry, runner, JSON writer, GraphicsFixture
#   Gecko::BenchMain  -- minimal main() that calls gecko::bench::Main

add_library(Bench STATIC)
add_library(Gecko::Bench ALIAS Bench)

target_sources(Bench
  PRIVATE
    bench.cpp
    graphics_fixture.cpp
)

target_include_directories(Bench
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(Bench
  PUBLIC
    Gecko::Core
    Gecko::CoreServices
    Gecko::Runtime
    Gecko::Platform
    Gecko::Graphics
    Gecko::DebugRenderer
)

add_library(BenchMain STATIC bench_main.cpp)
add_library(Gecko::BenchMain ALIAS BenchMain)
target_link_libraries(BenchMain PUBLIC Gecko::Bench)
