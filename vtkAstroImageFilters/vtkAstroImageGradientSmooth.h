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
/// \brief Intensity-driven gradient filter (adaptive anisotropic diffusion).
///
/// Implements the intensity-driven gradient filter of SlicerAstro
/// (Punzo et al. 2016, Astronomy and Computing, arXiv:1609.03782, Sect. 3.3):
/// the Perona & Malik (1990) diffusion equation dI/dt = S * Laplacian(I)
/// (the grad(S) . grad(I) term neglected), with an intensity-driven diffusion
/// coefficient instead of a gradient-driven one:
///
///   S = 1 / (1 + I^2 / (K^2 rms^2))
///
/// so that faint (low signal-to-noise) emission is smoothed more than bright
/// emission. Each iteration (Perona & Malik discretization):
///
///   I' = I + TimeStep * (Cx * Lx + Cy * Ly + Cz * Lz) * S
///
/// where L_axis is the sum of the differences with the two neighbours along
/// the axis (reflecting borders). Voxels with a blank (NaN) neighbour are not
/// modified. The filter does not conserve the flux scale: results are meant
/// for visualization, not for quantitative analysis.
/// Defaults are the paper values: K = 1.5, TimeStep = 0.0325, 20 iterations,
/// Cx = Cy = Cz = 5. Two image buffers are used (no copy per iteration).
class VTK_ASTRO_IMAGE_FILTERS_EXPORT vtkAstroImageGradientSmooth : public vtkAstroImageSmoothBase
{
public:
  static vtkAstroImageGradientSmooth* New();
  vtkTypeMacro(vtkAstroImageGradientSmooth, vtkAstroImageSmoothBase);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Diffusion conductances along x, y, z (Cx, Cy, Cz).
  vtkSetVector3Macro(Conductance, double);
  vtkGetVector3Macro(Conductance, double);

  /// Integration time step.
  vtkSetMacro(TimeStep, double);
  vtkGetMacro(TimeStep, double);

  /// Intensities K times above the noise (rms) diffuse less.
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

  double Conductance[3]{5., 5., 5.};
  double TimeStep{0.0325};
  double K{1.5};
  double Noise{1.};
  int NumberOfIterations{20};

private:
  vtkAstroImageGradientSmooth(const vtkAstroImageGradientSmooth&) = delete;
  void operator=(const vtkAstroImageGradientSmooth&) = delete;
};

#endif
