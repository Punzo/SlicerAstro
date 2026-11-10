/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#ifndef __vtkAstroWebGPUShaders_h
#define __vtkAstroWebGPUShaders_h

// WGSL compute shaders of the SlicerAstro WebGPU smoothing filters (internal).
//
// Bindings (group 0): 0 = source cube, 1 = destination cube, 2 = kernel
// weights (box/Gaussian), 3 = parameters. The cubes are float32, x fastest.
// They implement exactly the CPU filters of vtkAstroImageFilters: blank (NaN)
// voxels and voxels outside the image do not contribute to the kernel sums;
// the gradient filter leaves voxels with a blank neighbour unchanged.
//
// NaN is detected from the bit pattern: WGSL implementations may assume
// finite floating point values, so that "v != v" can be optimized away.

namespace vtkAstroWebGPUShaders
{

static const char* Common = R"(
struct Params
{
  dims : vec4<u32>,   // x, y, z, axis (separable)
  radius : vec4<u32>, // kernel radius x, y, z, offset of the kernel weights
  coeffs : vec4<f32>, // gradient: conductance x, y, z, time step
  extra : vec4<f32>,  // gradient: (K * noise)^2
};

@group(0) @binding(0) var<storage, read_write> source : array<f32>;
@group(0) @binding(1) var<storage, read_write> destination : array<f32>;
@group(0) @binding(3) var<uniform> params : Params;

fn isBlank(value : f32) -> bool
{
  return (bitcast<u32>(value) & 0x7fffffffu) > 0x7f800000u;
}

fn inside(id : vec3<u32>) -> bool
{
  return id.x < params.dims.x && id.y < params.dims.y && id.z < params.dims.z;
}

fn voxelIndex(id : vec3<u32>) -> u32
{
  return (id.z * params.dims.y + id.y) * params.dims.x + id.x;
}
)";

//----------------------------------------------------------------------------
// Separable: 1D kernel along params.dims.w (0: x, 1: y, 2: z).
static const char* Separable = R"(
@group(0) @binding(2) var<storage, read> kernel : array<f32>;

@compute @workgroup_size(8, 8, 4)
fn axisSmooth(@builtin(global_invocation_id) id : vec3<u32>)
{
  if (!inside(id))
  {
    return;
  }
  let axis = params.dims.w;
  var stride = 1u;
  var position = id.x;
  var length = params.dims.x;
  if (axis == 1u)
  {
    stride = params.dims.x;
    position = id.y;
    length = params.dims.y;
  }
  else if (axis == 2u)
  {
    stride = params.dims.x * params.dims.y;
    position = id.z;
    length = params.dims.z;
  }
  let radius = params.radius[axis];
  // kernel range clipped to the image (zero padding)
  let kMin = min(radius, position);              // neighbours before
  let kMax = min(radius, length - 1u - position); // neighbours after
  let index = voxelIndex(id);
  var sample = index - kMin * stride;
  var weight = params.radius.w + radius - kMin;
  var sum = 0.0;
  for (var k = 0u; k <= kMin + kMax; k++)
  {
    let value = source[sample];
    if (!isBlank(value))
    {
      sum += value * kernel[weight];
    }
    sample += stride;
    weight += 1u;
  }
  destination[index] = sum;
}
)";

//----------------------------------------------------------------------------
// Full 3D kernel of size (2 rx + 1) x (2 ry + 1) x (2 rz + 1), x fastest.
static const char* Kernel = R"(
@group(0) @binding(2) var<storage, read> kernel : array<f32>;

@compute @workgroup_size(8, 8, 4)
fn kernelSmooth(@builtin(global_invocation_id) id : vec3<u32>)
{
  if (!inside(id))
  {
    return;
  }
  let dims = params.dims.xyz;
  let radius = params.radius.xyz;
  let kernelDims = 2u * radius + vec3<u32>(1u);
  // kernel range clipped to the image (zero padding)
  let before = min(radius, id);
  let after = min(radius, dims - vec3<u32>(1u) - id);
  var sum = 0.0;
  for (var k = id.z - before.z; k <= id.z + after.z; k++)
  {
    let kz = k + radius.z - id.z;
    for (var j = id.y - before.y; j <= id.y + after.y; j++)
    {
      let ky = j + radius.y - id.y;
      let row = (k * dims.y + j) * dims.x;
      let kernelRow = (kz * kernelDims.y + ky) * kernelDims.x + radius.x - id.x;
      for (var i = id.x - before.x; i <= id.x + after.x; i++)
      {
        let value = source[row + i];
        if (!isBlank(value))
        {
          sum += value * kernel[kernelRow + i];
        }
      }
    }
  }
  destination[voxelIndex(id)] = sum;
}
)";

//----------------------------------------------------------------------------
// One iteration of the intensity-driven gradient filter (Punzo et al. 2016):
// I' = I + dt * (Cx Lx + Cy Ly + Cz Lz) / (1 + I^2 / (K noise)^2),
// with reflecting borders.
static const char* Gradient = R"(
@compute @workgroup_size(8, 8, 4)
fn gradientSmooth(@builtin(global_invocation_id) id : vec3<u32>)
{
  if (!inside(id))
  {
    return;
  }
  let dims = params.dims.xyz;
  let index = voxelIndex(id);
  let nxy = dims.x * dims.y;
  let xMinus = select(index, index - 1u, id.x > 0u);
  let xPlus = select(index, index + 1u, id.x + 1u < dims.x);
  let yMinus = select(index, index - dims.x, id.y > 0u);
  let yPlus = select(index, index + dims.x, id.y + 1u < dims.y);
  let zMinus = select(index, index - nxy, id.z > 0u);
  let zPlus = select(index, index + nxy, id.z + 1u < dims.z);

  let v = source[index];
  let x1 = source[xMinus];
  let x2 = source[xPlus];
  let y1 = source[yMinus];
  let y2 = source[yPlus];
  let z1 = source[zMinus];
  let z2 = source[zPlus];
  if (isBlank(v) || isBlank(x1) || isBlank(x2) || isBlank(y1) || isBlank(y2) ||
      isBlank(z1) || isBlank(z2))
  {
    destination[index] = v; // blank voxel or blank neighbour: unchanged
    return;
  }
  let norm = 1.0 + (v * v) / params.extra.x;
  let flux = ((x1 - v) + (x2 - v)) * params.coeffs.x +
             ((y1 - v) + (y2 - v)) * params.coeffs.y +
             ((z1 - v) + (z2 - v)) * params.coeffs.z;
  destination[index] = v + params.coeffs.w * flux / norm;
}
)";

} // namespace vtkAstroWebGPUShaders

#endif
