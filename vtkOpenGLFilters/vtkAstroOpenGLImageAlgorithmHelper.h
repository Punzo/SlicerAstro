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

// .NAME vtkAstroOpenGLImageAlgorithmHelper - Help image algorithms use the GPU
// .SECTION Description
// Designed to make it easier to accelerate an image algorithm on the GPU

#ifndef vtkAstroOpenGLImageAlgorithmHelper_h
#define vtkAstroOpenGLImageAlgorithmHelper_h

#include "vtkObject.h"
#include "vtkOpenGLImageAlgorithmHelper.h"
#include "vtkOpenGLFiltersWin32Header.h"

class vtkTextureObject;

#include <algorithm>

/// Restores N values on scope exit (the GPU filters derive shader
/// parameters in place: this keeps the filter parameters unchanged).
template <typename T, int N>
struct vtkAstroRestoreOnExit
{
  explicit vtkAstroRestoreOnExit(T* values) : Values(values) { std::copy(values, values + N, this->Saved); }
  ~vtkAstroRestoreOnExit() { std::copy(this->Saved, this->Saved + N, this->Values); }
  T* Values;
  T Saved[N];
};

class VTK_OPENGLFILTERS_EXPORT vtkAstroOpenGLImageAlgorithmHelper : public vtkOpenGLImageAlgorithmHelper
{
public:
  static vtkAstroOpenGLImageAlgorithmHelper *New();
  vtkTypeMacro(vtkAstroOpenGLImageAlgorithmHelper, vtkOpenGLImageAlgorithmHelper);

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

 /// Values outside the image: zero (true, default: zero padding as the CPU
  /// filters) or the nearest edge voxel (false, e.g. for the gradient filter).
  vtkSetMacro(ZeroBorder, bool);
  vtkGetMacro(ZeroBorder, bool);

 protected:
  bool ZeroBorder{true};
  void ConfigureBorder(vtkTextureObject* texture);
  vtkAstroOpenGLImageAlgorithmHelper();
  ~vtkAstroOpenGLImageAlgorithmHelper();

 private:
  vtkAstroOpenGLImageAlgorithmHelper(const vtkAstroOpenGLImageAlgorithmHelper&);
  void operator=(const vtkAstroOpenGLImageAlgorithmHelper&);
};

#endif
