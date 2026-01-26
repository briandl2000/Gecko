# Gecko Engine Tests

Unit tests for the Gecko engine using [Catch2 v3](https://github.com/catchorg/Catch2).

## Running Tests

```bash
# After sourcing scripts/setup.sh
gk test              # Build and run (Debug)
gk test release      # Release tests
gk test --build-only # Build without running
```

### Direct Execution

```bash
./out/bin/Debug/tests/math_tests
./out/bin/Debug/tests/math_tests --list-tests
./out/bin/Debug/tests/math_tests "[vector]"  # Specific tag
```

## Structure

```
test/
├── math/
│   ├── test_vector.cpp  # Vector operations
│   ├── test_matrix.cpp  # Matrix operations  
│   ├── test_aabb.cpp    # Bounding boxes
│   └── test_rotor.cpp   # Geometric algebra rotors
└── CMakeLists.txt       # Catch2 setup
```

## Adding Tests

Create `test/yourmodule/test_feature.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "gecko/yourmodule/feature.h"

TEST_CASE("Feature works", "[yourmodule]")
{
  REQUIRE(YourFunction() == expected);
}
```

Add to `test/yourmodule/CMakeLists.txt`:

```cmake
add_executable(yourmodule_tests test_feature.cpp)
target_link_libraries(yourmodule_tests PRIVATE Catch2::Catch2WithMain)
```

## Notes

- Tests **not built by default** (only when running `gk test`)
- Catch2 downloaded once on first test build (~20s), then cached
- Output: `out/bin/{Debug,Release}/tests/`

[Catch2 Docs](https://github.com/catchorg/Catch2/blob/devel/docs/tutorial.md)
