/*=========================================================================
  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkOpenGLTextureImage.h,v $
  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.
     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.
=========================================================================*/
// .NAME vtkOpenGLTextureImage - OpenGL actor
// .SECTION Description
// vtkOpenGLTextureImage is an interface between an vtkImageData and and
// an OpenGL 3D texture object.
// This is a work in progress, but when complete it should be possible
// to easily move data back and forth between C++/Python and GLSL
// to support volume rendering and computation when used with
// the vtkOpenGLShaderComputation class.

#ifndef __vtkOpenGLAstroTextureImage_h
#define __vtkOpenGLAstroTextureImage_h

#include "vtkOpenGLAstroShaderComputation.h"

#include "vtkImageData.h"

#include "vtkAddon.h"

#include "vtkSlicerAstroSmoothingModuleLogicExport.h"

/// \ingroup Slicer_QtModules_AstroSmoothing
class VTK_SLICER_ASTROSMOOTHING_MODULE_LOGIC_EXPORT vtkOpenGLAstroTextureImage : public vtkObject
{
protected:

public:
  static vtkOpenGLAstroTextureImage *New();
  vtkTypeMacro(vtkOpenGLAstroTextureImage,vtkObject);
  virtual void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // The ShaderComputation used to manage the OpenGL context and shaders
  vtkGetObjectMacro(ShaderComputation, vtkOpenGLAstroShaderComputation);
  vtkSetObjectMacro(ShaderComputation, vtkOpenGLAstroShaderComputation);

  // Description:
  // The image data that corresponds to the texture.
  // Not all sizes and types of image data are supported, only
  // those that map cleanly to textures.  All are treated as
  // 3D textures no matter the dimensions.
  vtkGetObjectMacro(ImageData, vtkImageData);
  vtkSetObjectMacro(ImageData, vtkImageData);

  // Description:
  // The id provided by glGenTextures.
  // It is actually in integer that is
  // an opaque mapping to a hardware structure.
  // Non-zero means the texture has been generated.
  // Exposed here for introspection.
  vtkGetMacro(TextureName, vtkTypeUInt32);

  // Description:
  // True (default) to interpolate samples
  vtkGetMacro(Interpolate, int);
  vtkSetMacro(Interpolate, int);

  // Description:
  // Make the image data available as GL_TEXTUREn
  // where n is the texture unit.  There are at least
  // GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, which is variable
  // by driver but must be at least 48.
  // This method also checks the modification time of
  // the image data to ensure the texture up to date and
  // initiates the transfer if not.
  void Activate(vtkTypeUInt32 unit);

  // Description:
  // Creates/transfers image data to texture if needed.
  bool UpdateTexture();

  // Description:
  // Make the specified layer (slice) be the draw target.
  // This is used to direct the output of the shading into
  // the specified slice of the texture and can be used to
  // implemement volumetric algorithms.  Iterated algorithms
  // can be done fully on the GPU by swapping textures between
  // active units and draw targets.
  // Parameters:
  // attachmentIndex is which color attachment to use (only valid for color)
  // level is z slice to target
  // attachment is 0 (color), 1 (depth), 2 (stencil), 3 (depth-stencil)
  enum AttachmentPoints
    {
    ColorAttachmentPoint = 0,
    DepthAttachmentPoint,
    StencilAttachmentPoint,
    DepthStencilAttachmentPoint
    };
  void AttachAsDrawTarget(int layer=0, int attachement=0, int attachmentIndex=0);

  // Description:
  // Read the texture data back into the image data
  // (assumes it has been written as a target)
  // Warning: probably best to only use this to read back into the same buffer that was used
  // when the data was uploaded (i.e. this will assume that the vtkImageData buffer pointer
  // is the right size for the data).
  void ReadBack();

  // Description:
  // TODO: options for min and mag filter, wrapping...

protected:
  vtkOpenGLAstroTextureImage();
  ~vtkOpenGLAstroTextureImage();

private:
  vtkOpenGLAstroTextureImage(const vtkOpenGLAstroTextureImage&);  // Not implemented.
  void operator=(const vtkOpenGLAstroTextureImage&);  // Not implemented.

  vtkOpenGLAstroShaderComputation *ShaderComputation;
  vtkImageData *ImageData;
  vtkTypeUInt32 TextureName;
  int Interpolate;
  unsigned long TextureMTime;

};

#endif
