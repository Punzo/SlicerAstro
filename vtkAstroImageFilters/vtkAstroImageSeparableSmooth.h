/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#ifndef __vtkAstroImageSeparableSmooth_h
#define __vtkAstroImageSeparableSmooth_h

#include "vtkAstroImageSmoothBase.h"

// VTK includes
#include <vtkSmartPointer.h>

class vtkDoubleArray;

/// \ingroup SlicerAstro_vtkAstroImageFilters
/// \brief Separable smoothing: one 1D kernel per axis (e.g. box or Gaussian).
///
/// The axes are processed in the order X, Y, Z; an axis without kernel is
/// left unchanged. Along each axis out(x) = sum_k in(x + k) * kernel(k);
/// voxels outside the image and blank (NaN) voxels contribute zero.
/// Only one scratch buffer (same size as the image) is used.
class VTK_ASTRO_IMAGE_FILTERS_EXPORT vtkAstroImageSeparableSmooth : public vtkAstroImageSmoothBase
{
public:
  static vtkAstroImageSeparableSmooth* New();
  vtkTypeMacro(vtkAstroImageSeparableSmooth, vtkAstroImageSmoothBase);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// 1D kernel (odd length) for an axis (0, 1 or 2); nullptr disables the axis.
  void SetAxisKernel(int axis, vtkDoubleArray* kernel);
  vtkDoubleArray* GetAxisKernel(int axis);

  /// Convenience: normalized box kernel of the given (odd-rounded) size;
  /// size <= 0 disables the axis.
  void SetAxisBoxKernel(int axis, int size);

protected:
  vtkAstroImageSeparableSmooth();
  ~vtkAstroImageSeparableSmooth() override;

  bool Smooth(vtkImageData* input, vtkImageData* output) override;

  vtkSmartPointer<vtkDoubleArray> AxisKernels[3];

private:
  vtkAstroImageSeparableSmooth(const vtkAstroImageSeparableSmooth&) = delete;
  void operator=(const vtkAstroImageSeparableSmooth&) = delete;
};

#endif
