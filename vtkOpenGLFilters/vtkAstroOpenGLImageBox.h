// .NAME vtkAstroOpenGLImageBox - Compute Box using the GPU

#ifndef vtkAstroOpenGLImageBox_h
#define vtkAstroOpenGLImageBox_h

// VTK includes
#include <vtkImageGradient.h>
#include <vtkSmartPointer.h>

// VTK decleration
class vtkAstroOpenGLImageAlgorithmHelper;
class vtkRenderWindow;
#include "vtkImageGaussianSmooth.h"
#include "vtkOpenGLFiltersWin32Header.h"

class VTK_OPENGLFILTERS_EXPORT vtkAstroOpenGLImageBox : public vtkImageGradient
{
public:
  static vtkAstroOpenGLImageBox *New();
  vtkTypeMacro(vtkAstroOpenGLImageBox,vtkImageGradient);

  // Description:
  // Set the render window to get the OpenGL resources from
  void SetRenderWindow(vtkRenderWindow *);

  // Description:
  // Sets/Gets the kernels dimensions in pixels unit.
  vtkSetVector3Macro(KernelLength, int);
  vtkGetVector3Macro(KernelLength, int);

protected:
  void PrintSelf(ostream& os, vtkIndent indent);
  vtkAstroOpenGLImageBox();
  ~vtkAstroOpenGLImageBox();

  vtkSmartPointer<vtkAstroOpenGLImageAlgorithmHelper> Helper;

  void ThreadedRequestData(vtkInformation *request,
                           vtkInformationVector **inputVector,
                           vtkInformationVector *outputVector,
                           vtkImageData ***inData, vtkImageData **outData,
                           int extent[6], int id);

  int KernelLength[3];

private:
  vtkAstroOpenGLImageBox(const vtkAstroOpenGLImageBox&);  // Not implemented.
  void operator=(const vtkAstroOpenGLImageBox&);  // Not implemented.
};

#endif
