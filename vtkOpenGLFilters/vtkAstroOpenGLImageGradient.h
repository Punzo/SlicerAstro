// .NAME vtkAstroOpenGLImageGradient - Compute Box using the GPU

#ifndef vtkAstroOpenGLImageGradient_h
#define vtkAstroOpenGLImageGradient_h

// VTK includes
#include <vtkImageGradient.h>
#include <vtkSmartPointer.h>

// VTK decleration
class vtkAstroOpenGLImageAlgorithmHelper;
class vtkRenderWindow;
#include "vtkImageGaussianSmooth.h"
#include "vtkOpenGLFiltersWin32Header.h"

class VTK_OPENGLFILTERS_EXPORT vtkAstroOpenGLImageGradient : public vtkImageGradient
{
public:
  static vtkAstroOpenGLImageGradient *New();
  vtkTypeMacro(vtkAstroOpenGLImageGradient,vtkImageGradient);

  // Description:
  // Set the render window to get the OpenGL resources from
  void SetRenderWindow(vtkRenderWindow *);

  vtkSetVector3Macro(Cl, double);
  vtkGetVector3Macro(Cl, double);

  vtkSetMacro(K,double);
  vtkGetMacro(K,double);

  vtkSetMacro(TimeStep,double);
  vtkGetMacro(TimeStep,double);

  vtkSetMacro(RMS,double);
  vtkGetMacro(RMS,double);

  vtkSetMacro(Accuracy,int);
  vtkGetMacro(Accuracy,int);

protected:
  void PrintSelf(ostream& os, vtkIndent indent);
  vtkAstroOpenGLImageGradient();
  ~vtkAstroOpenGLImageGradient();

  vtkSmartPointer<vtkAstroOpenGLImageAlgorithmHelper> Helper;

  void ThreadedRequestData(vtkInformation *request,
                           vtkInformationVector **inputVector,
                           vtkInformationVector *outputVector,
                           vtkImageData ***inData, vtkImageData **outData,
                           int extent[6], int id);

  double Cl[3];
  int Accuracy;
  double K, TimeStep, RMS;

private:
  vtkAstroOpenGLImageGradient(const vtkAstroOpenGLImageGradient&);  // Not implemented.
  void operator=(const vtkAstroOpenGLImageGradient&);  // Not implemented.
};

#endif
