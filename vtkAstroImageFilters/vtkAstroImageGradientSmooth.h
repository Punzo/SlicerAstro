/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#ifndef __vtkAstroImageGradientSmooth_h
#define __vtkAstroImageGradientSmooth_h

#include "vtkAstroImageSmoothBase.h"

/// \ingroup SlicerAstro_vtkAstroImageFilters
/// \brief Intensity-driven gradient (non-linear diffusion) smoothing.
///
/// At each iteration:
///   v' = v + TimeStep * (Cx * Lx + Cy * Ly + Cz * Lz) / (1 + v^2 / (K * Noise)^2)
/// where L_axis is the sum of the differences with the two neighbours along
/// the axis (reflecting borders). Voxels with a blank (NaN) neighbour are not
/// modified. Two image buffers are used (no copy per iteration).
class VTK_ASTRO_IMAGE_FILTERS_EXPORT vtkAstroImageGradientSmooth : public vtkAstroImageSmoothBase
{
public:
  static vtkAstroImageGradientSmooth* New();
  vtkTypeMacro(vtkAstroImageGradientSmooth, vtkAstroImageSmoothBase);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Diffusion conductances along x, y, z.
  vtkSetVector3Macro(Conductance, double);
  vtkGetVector3Macro(Conductance, double);

  /// Integration time step.
  vtkSetMacro(TimeStep, double);
  vtkGetMacro(TimeStep, double);

  /// Edge-stopping: intensities K times above the noise diffuse less.
  vtkSetMacro(K, double);
  vtkGetMacro(K, double);
  vtkSetMacro(Noise, double);
  vtkGetMacro(Noise, double);

  /// Number of iterations.
  vtkSetMacro(NumberOfIterations, int);
  vtkGetMacro(NumberOfIterations, int);

protected:
  vtkAstroImageGradientSmooth();
  ~vtkAstroImageGradientSmooth() override;

  bool Smooth(vtkImageData* input, vtkImageData* output) override;

  double Conductance[3]{1., 1., 1.};
  double TimeStep{0.0325};
  double K{2.};
  double Noise{1.};
  int NumberOfIterations{10};

private:
  vtkAstroImageGradientSmooth(const vtkAstroImageGradientSmooth&) = delete;
  void operator=(const vtkAstroImageGradientSmooth&) = delete;
};

#endif
