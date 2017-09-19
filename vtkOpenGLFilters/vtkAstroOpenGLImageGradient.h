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
  and was supported through the European Research Consil grant nr. 291531.

==============================================================================*/

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
                           int extent[6], int id) VTK_OVERRIDE;

  double Cl[3];
  int Accuracy;
  double K, TimeStep, RMS;

private:
  vtkAstroOpenGLImageGradient(const vtkAstroOpenGLImageGradient&);
  void operator=(const vtkAstroOpenGLImageGradient&);
};

#endif
