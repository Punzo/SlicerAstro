/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Davide Punzo, Kapteyn Astronomical Institute,
  and was supported through the European Research Council grant nr. 291531.

==============================================================================*/

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
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

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

  // Description:
  // Sets/Gets of parameter to explicit use a 3-Pass filter (isotropic filter).
  vtkSetMacro(Iterative, bool);
  vtkGetMacro(Iterative, bool);

protected:
  vtkAstroOpenGLImageGaussian();
  ~vtkAstroOpenGLImageGaussian();

  vtkSmartPointer<vtkAstroOpenGLImageAlgorithmHelper> Helper;

  void ThreadedRequestData(vtkInformation *request,
                           vtkInformationVector **inputVector,
                           vtkInformationVector *outputVector,
                           vtkImageData ***inData, vtkImageData **outData,
                           int extent[6], int id) VTK_OVERRIDE;

  int KernelLength[3];
  double FWHM[3];
  double RotationAngles[3];
  bool Iterative;

private:
  vtkAstroOpenGLImageGaussian(const vtkAstroOpenGLImageGaussian&);
  void operator=(const vtkAstroOpenGLImageGaussian&);
};

#endif
