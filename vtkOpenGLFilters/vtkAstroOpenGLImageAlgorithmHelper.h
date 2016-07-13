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

  void Execute(
    vtkOpenGLImageAlgorithmCallback *cb,
    vtkImageData *inImage, vtkDataArray *inArray,
    vtkImageData *outImage, int outExt[6],
    const char *vertexCode,
    const char *fragmentCode,
    const char *geometryCode
    );

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
