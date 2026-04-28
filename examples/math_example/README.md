# math_example

Tour of the `gecko::math` module — vectors, matrices, quaternions, AABBs,
and the helper free functions in `gecko/math/math.h`.

## Run

```bash
gk run math_example debug
```

The example links only against `Gecko::Math` (which is header-only) and
prints results to stdout. No window, no services — the math module has
no dependencies.

## What's covered

| Method                | Topic                                                  |
|-----------------------|--------------------------------------------------------|
| `RunVectorBasics`     | `Float2/3` arithmetic, indexing, `Length`/`Dot`/`Cross`/`Normalized` |
| `RunMatrixTransforms` | `Float4x4` translation/scale/rotation, `TransformPoint`/`TransformVector` |
| `RunCameraMatrices`   | `LookAt` and `Perspective` view/projection matrices    |
| `RunAabbAndRect`      | `Aabb2`/`RectI`, `Contains`/`Intersects`/`Expand`/`Union`/`Clamp`/`Center`/`Size` |
| `RunQuaternions`      | `Quat::AxisAngle`/`FromTo`, composition (`*`), `Rotate`, `Slerp` |
| `RunUtilities`        | `ToRadians`, `Min`/`Max`/`Lerp` on vectors             |

## Where to look next

- Public headers: [include/gecko/math/](../../include/gecko/math/)
- Single-include facade: [include/gecko/math/math.h](../../include/gecko/math/math.h)
- Module overview: [docs/architecture.md](../../docs/architecture.md)
