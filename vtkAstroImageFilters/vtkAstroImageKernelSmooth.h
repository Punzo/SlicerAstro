/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#ifndef __vtkAstroImageKernelSmooth_h
#define __vtkAstroImageKernelSmooth_h

#include "vtkAstroImageSmoothBase.h"

// VTK includes
#include <vtkSmartPointer.h>

class vtkDoubleArray;

/// \ingroup SlicerAstro_vtkAstroImageFilters
/// \brief Smoothing with an arbitrary 3D kernel (e.g. box or rotated Gaussian).
///
/// out(x) = sum_k in(x + k) * kernel(k), for the kernel offsets k centered on
/// the kernel center. Voxels outside the image and blank (NaN) voxels
/// contribute zero; the kernel is not renormalized.
class VTK_ASTRO_IMAGE_FILTERS_EXPORT vtkAstroImageKernelSmooth : public vtkAstroImageSmoothBase
{
public:
  static vtkAstroImageKernelSmooth* New();
  vtkTypeMacro(vtkAstroImageKernelSmooth, vtkAstroImageSmoothBase);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Kernel values, x fastest, of size KernelDimensions[0]*[1]*[2].
  /// Dimensions should be odd (the kernel center is at (dim - 1) / 2).
  void SetKernel(vtkDoubleArray* kernel, int dimX, int dimY, int dimZ);
  vtkDoubleArray* GetKernel();
  vtkGetVector3Macro(KernelDimensions, int);

  /// Convenience: normalized box kernel (size rounded up to odd values).
  void SetBoxKernel(int sizeX, int sizeY, int sizeZ);

protected:
  vtkAstroImageKernelSmooth();
  ~vtkAstroImageKernelSmooth() override;

  bool Smooth(vtkImageData* input, vtkImageData* output) override;

  vtkSmartPointer<vtkDoubleArray> Kernel;
  int KernelDimensions[3]{1, 1, 1};

private:
  vtkAstroImageKernelSmooth(const vtkAstroImageKernelSmooth&) = delete;
  void operator=(const vtkAstroImageKernelSmooth&) = delete;
};

#endif
