// .NAME vtkAstroOpenGLImageGaussian - Compute Box using the GPU

#ifndef vtkAstroOpenGLImageGaussian_h
#define vtkAstroOpenGLImageGaussian_h

// VTK includes
#include <vtkImageGradient.h>
#include <vtkSmartPointer.h>

// VTK decleration
class vtkAstroOpenGLImageAlgorithmHelper;
class vtkRenderWindow;
#include "vtkImageGaussianSmooth.h"
#include "vtkOpenGLFiltersWin32Header.h"

class VTK_OPENGLFILTERS_EXPORT vtkAstroOpenGLImageGaussian : public vtkImageGradient
{
public:
  static vtkAstroOpenGLImageGaussian *New();
  vtkTypeMacro(vtkAstroOpenGLImageGaussian,vtkImageGradient);

  // Description:
  // Set the render window to get the OpenGL resources from
  void SetRenderWindow(vtkRenderWindow *);

  // Description:
  // Sets/Gets the kernels dimensions in pixels unit.
  vtkSetVector3Macro(KernelLength, int);
  vtkGetVector3Macro(KernelLength, int);

  // Description:
  // Sets/Gets the FWHM dimensions in pixels unit.
  vtkSetVector3Macro(FWHM, double);
  vtkGetVector3Macro(FWHM, double);

  // Description:
  // Sets/Gets the Euler angles, Theta, in degree unit.
  vtkSetVector3Macro(RotationAngles, double);
  vtkGetVector3Macro(RotationAngles, double);

protected:
  void PrintSelf(ostream& os, vtkIndent indent);
  vtkAstroOpenGLImageGaussian();
  ~vtkAstroOpenGLImageGaussian();

  vtkSmartPointer<vtkAstroOpenGLImageAlgorithmHelper> Helper;

  void ThreadedRequestData(vtkInformation *request,
                           vtkInformationVector **inputVector,
                           vtkInformationVector *outputVector,
                           vtkImageData ***inData, vtkImageData **outData,
                           int extent[6], int id);

  int KernelLength[3];
  double FWHM[3];
  double RotationAngles[3];

private:
  vtkAstroOpenGLImageGaussian(const vtkAstroOpenGLImageGaussian&);  // Not implemented.
  void operator=(const vtkAstroOpenGLImageGaussian&);  // Not implemented.
};

#endif
