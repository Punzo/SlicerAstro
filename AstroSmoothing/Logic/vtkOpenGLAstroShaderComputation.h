/*=========================================================================
  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkOpenGLShaderComputation.h,v $
  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.
     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.
=========================================================================*/
// .NAME vtkOpenGLShaderComputation - OpenGL actor
// .SECTION Description
// vtkOpenGLShaderComputation is a way to perform GPU computations on vtk data.
// vtkOpenGLShaderComputation interfaces to the OpenGL rendering library.

#ifndef __vtkOpenGLAstroShaderComputation_h
#define __vtkOpenGLAstroShaderComputation_h

#include "vtkImageData.h"
#include "vtkRenderWindow.h"

class vtkImageData;

#include "vtkAddon.h"

#include "vtkSlicerAstroSmoothingModuleLogicExport.h"

/// \ingroup Slicer_QtModules_AstroSmoothing
class VTK_SLICER_ASTROSMOOTHING_MODULE_LOGIC_EXPORT vtkOpenGLAstroShaderComputation : public vtkObject
{
protected:

public:
  static vtkOpenGLAstroShaderComputation *New();
  vtkTypeMacro(vtkOpenGLAstroShaderComputation,vtkObject);
  virtual void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Loads the required extensions
  void Initialize(vtkRenderWindow *renderWindow);

  // Description:
  // Rebuild the shader program if needed
  bool UpdateProgram();

  // Description:
  // Manage the OpenGL offscreen rendering framebuffer for computing
  // Select this mode to render into a buffer that matches the ResultImageData
  // and can be read back with ReadResult.  Otherwise use
  // vtkOpenGLTextureImage::AttachAsDrawTarget to set a texture
  // as the draw target.
  bool AcquireResultRenderbuffer();
  void ReleaseResultRenderbuffer();

  // Description:
  // Perform the actual computation
  // Updates the program if needed and then
  // renders to the current framebuffer configuration
  // Slice will be passed as a uniform float
  void Compute(float slice=0.);

  // Description:
  // Copy the framebuffer pixels into the result image
  void ReadResult();

  // Description:
  // The strings defining the shaders
  vtkGetStringMacro(VertexShaderSource);
  vtkSetStringMacro(VertexShaderSource);
  vtkGetStringMacro(FragmentShaderSource);
  vtkSetStringMacro(FragmentShaderSource);

  // Description:
  // The results of the computation.
  // Must be set with the desired dimensions before calling Compute.
  vtkGetObjectMacro(ResultImageData, vtkImageData);
  vtkSetObjectMacro(ResultImageData, vtkImageData);

  // Description:
  // Used internally to manage OpenGL context and extensions
  vtkGetObjectMacro(RenderWindow, vtkRenderWindow);
  vtkSetObjectMacro(RenderWindow, vtkRenderWindow);

  // Description:
  // Has the context been set up with a render window?
  vtkGetMacro(Initialized, bool);

protected:
  vtkOpenGLAstroShaderComputation();
  ~vtkOpenGLAstroShaderComputation();

private:
  vtkOpenGLAstroShaderComputation(const vtkOpenGLAstroShaderComputation&);  // Not implemented.
  void operator=(const vtkOpenGLAstroShaderComputation&);  // Not implemented.

  bool Initialized;
  char *VertexShaderSource;
  char *FragmentShaderSource;
  vtkImageData *ResultImageData;

  vtkTypeUInt32 ProgramObject; // vtkTypeUInt32 same as GLuint: https://www.opengl.org/wiki/OpenGL_Type
  unsigned long ProgramObjectMTime;
  vtkTypeUInt32 FramebufferID;
  vtkTypeUInt32 ColorRenderbufferID;
  vtkTypeUInt32 DepthRenderbufferID;

  vtkRenderWindow *RenderWindow;
};

#endif
