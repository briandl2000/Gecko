# Math

Gecko math uses a right-handed world coordinate system. Angles are radians unless a function name explicitly says otherwise.

## Files

- `scalar.h`: constants and scalar functions such as `Pi`, `Clamp`, `Lerp`, `Sin`, `Cos`, `Sqrt`.
- `vector.h`: `Float2/3/4`, `Int2/3/4`, vector operators and vector helpers.
- `matrix.h`: row-major `Float2x2`, `Float3x3`, `Float4x4` and matrix transform helpers.
- `quat.h`: `Quat`/`Rotor` rotations and interpolation.
- `aabb.h`, `rect.h`: bounds and rectangle types.
- `plane.h`, `ray.h`, `transform.h`: common game-object and geometry primitives.
- `math.h`: convenience umbrella include.

## Matrix Convention

Matrices are stored row-major in memory and exposed as rows:

```cpp
Float4 row0 = matrix[0];
f32 m01 = matrix.M01;
```

Math operations use column-vector semantics:

```cpp
Float4 clip = projection * view * world * Float4 {position.X, position.Y, position.Z, 1.0f};
Float3 worldPosition = TransformPoint(world, localPosition);
```

That means transform composition reads right-to-left. In `A * B * v`, `B` is applied first, then `A`.

Translation lives in the last column (`M03`, `M13`, `M23`) because vectors multiply on the right:

```cpp
Float4x4 t = Float4x4::Translation({10.0f, 0.0f, 0.0f});
Float3 p = TransformPoint(t, {1.0f, 2.0f, 3.0f}); // {11, 2, 3}
```

## HLSL / SPIR-V / Vulkan

CPU-side storage order and shader-side layout are separate concerns.

Gecko's CPU matrices are row-major arrays, but the algebra convention is column-vector (`M * v`). In HLSL, be explicit at shader boundaries instead of relying on compiler defaults:

```hlsl
#pragma pack_matrix(row_major)

cbuffer Camera : register(b0)
{
    float4x4 ViewProjection;
};

float4 clip = mul(ViewProjection, float4(position, 1.0));
```

Use the same convention consistently in shaders: matrices on the left, vectors on the right. If a shader chooses `mul(vector, matrix)`, transpose the uploaded matrix or document that shader as an exception.

## Practical Rule

Build transforms in the order you want them applied to a point:

```cpp
Float4x4 world =
    Float4x4::Translation(position) *
    ToMatrix4(rotation) *
    Float4x4::Scale(scale);
```

For a local point, this applies scale, then rotation, then translation.
