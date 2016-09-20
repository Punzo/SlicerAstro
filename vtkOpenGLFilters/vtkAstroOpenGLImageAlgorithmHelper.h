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

/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkAstroOpenGLImageAlgorithmHelper.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkAstroOpenGLImageAlgorithmHelper - Help image algorithms use the GPU
// .SECTION Description
// Designed to make it easier to accelerate an image algorithm on the GPU

#ifndef vtkAstroOpenGLImageAlgorithmHelper_h
#define vtkAstroOpenGLImageAlgorithmHelper_h

#include "vtkRenderingOpenGL2Module.h" // For export macro
#include "vtkObject.h"

#include "vtkOpenGLHelper.h" // used for ivars
#include "vtkSmartPointer.h" // for ivar

#include "vtkOpenGLFiltersWin32Header.h"

class vtkOpenGLRenderWindow;
class vtkRenderWindow;
class vtkImageData;
class vtkDataArray;

class vtkOpenGLImageAlgorithmCallback
{
public:
  virtual void InitializeShaderUniforms(vtkShaderProgram * /* program */) {};
  virtual void UpdateShaderUniforms(
    vtkShaderProgram * /* program */, int /* zExtent */) {};
  virtual ~vtkOpenGLImageAlgorithmCallback() {};
};

class VTK_OPENGLFILTERS_EXPORT vtkAstroOpenGLImageAlgorithmHelper : public vtkObject
{
public:
  static vtkAstroOpenGLImageAlgorithmHelper *New();
  vtkTypeMacro(vtkAstroOpenGLImageAlgorithmHelper,vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Execute one-pass filter
  void Execute(
    vtkOpenGLImageAlgorithmCallback *cb,
    vtkImageData *inImage, vtkDataArray *inArray,
    vtkImageData *outImage, int outExt[6],
    const char *vertexCode,
    const char *fragmentCode,
    const char *geometryCode
    );

  // Description:
  // Execute 3-pass filter with different fragmentCode kernels
  void Execute(
    vtkOpenGLImageAlgorithmCallback *cb,
    vtkImageData *inImage, vtkDataArray *inArray,
    vtkImageData *outImage, int outExt[6],
    const char *vertexCode,
    const char *fragmentCodeX,
    const char *fragmentCodeY,
    const char *fragmentCodeZ,
    const char *geometryCode
    );

  // Description:
  // Execute n-pass filter with the same fragmentCode kernel
  void Execute(
    vtkOpenGLImageAlgorithmCallback *cb,
    vtkImageData *inImage, vtkDataArray *inArray,
    vtkImageData *outImage, int outExt[6],
    const char *vertexCode,
    const char *fragmentCode,
    const char *geometryCode,
    const int n
    );

  // Description:
  // Set the render window to get the OpenGL resources from
  void SetRenderWindow(vtkRenderWindow *renWin);

 protected:
  vtkAstroOpenGLImageAlgorithmHelper();
  virtual ~vtkAstroOpenGLImageAlgorithmHelper();

  vtkSmartPointer<vtkOpenGLRenderWindow> RenderWindow;
  vtkOpenGLHelper Quad;

 private:
  vtkAstroOpenGLImageAlgorithmHelper(const vtkAstroOpenGLImageAlgorithmHelper&);  // Not implemented.
  void operator=(const vtkAstroOpenGLImageAlgorithmHelper&);  // Not implemented.
};

#endif
