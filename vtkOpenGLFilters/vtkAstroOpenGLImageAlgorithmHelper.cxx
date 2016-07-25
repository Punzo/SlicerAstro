// vtkOpenGLFilters includes
#include "vtkAstroOpenGLImageAlgorithmHelper.h"

// VTK includes
#include "vtkDataArray.h"
#include "vtkFrameBufferObject.h"
#include "vtkImageData.h"
#include "vtkImageCast.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLShaderCache.h"
#include "vtkOpenGLVertexArrayObject.h"
#include "vtkPixelBufferObject.h"
#include "vtkPixelTransfer.h"
#include "vtkPointData.h"
#include "vtkShaderProgram.h"
#include "vtkTextureObject.h"
#include "vtk_glew.h"

vtkStandardNewMacro(vtkAstroOpenGLImageAlgorithmHelper);

// ----------------------------------------------------------------------------
vtkAstroOpenGLImageAlgorithmHelper::vtkAstroOpenGLImageAlgorithmHelper()
{
  this->RenderWindow = 0;
}

// ----------------------------------------------------------------------------
vtkAstroOpenGLImageAlgorithmHelper::~vtkAstroOpenGLImageAlgorithmHelper()
{
  this->SetRenderWindow(0);
}

void vtkAstroOpenGLImageAlgorithmHelper::SetRenderWindow(vtkRenderWindow *renWin)
{
  if (renWin == this->RenderWindow.Get())
    {
    return;
    }

  vtkOpenGLRenderWindow *orw  = NULL;
  if (renWin)
    {
    orw = vtkOpenGLRenderWindow::SafeDownCast(renWin);
    }

  this->RenderWindow = orw;
  this->Modified();
}

void vtkAstroOpenGLImageAlgorithmHelper::Execute(
  vtkOpenGLImageAlgorithmCallback *cb,
  vtkImageData *inImage, vtkDataArray *inArray,
  vtkImageData *outImage, int outExt[6],
  const char *vertexCode,
  const char *fragmentCode,
  const char *geometryCode
  )
{
  // make sure it is initialized
  if (!this->RenderWindow)
    {
    this->SetRenderWindow(vtkRenderWindow::New());
    this->RenderWindow->SetOffScreenRendering(true);
    this->RenderWindow->UnRegister(this);
    }
  this->RenderWindow->Initialize();

  // Is it a 2D or 3D image
  int dims[3];
  inImage->GetDimensions(dims);
  int dimensions = 0;
  for (int i = 0; i < 3; i ++)
    {
    if (dims[i] > 1)
      {
      dimensions++;
      }
    }

  // no 1d or 2D supprt yet
  if (dimensions < 3)
    {
    vtkErrorMacro("no 1D or 2D processing support yet");
    return;
    }

  // send vector data to a texture
  int inputExt[6];
  inImage->GetExtent(inputExt);
  void *inPtr = inArray->GetVoidPointer(0);

  if (inArray->GetArrayType() != VTK_FLOAT)
      {
      vtkNew<vtkImageCast> castFilter;
      castFilter->SetInputData(inImage);
      castFilter->SetOutputScalarTypeToFloat();
      castFilter->Update();
      inImage->DeepCopy(castFilter->GetOutput());
      inPtr = inImage->GetScalarPointer();
      }

  vtkNew<vtkTextureObject> inputTex;
  inputTex->SetContext(this->RenderWindow);
  inputTex->SetInternalFormat(GL_R32F);
  inputTex->Create3DFromRaw(
    dims[0], dims[1], dims[2],
    inArray->GetNumberOfComponents(),
    VTK_FLOAT, inPtr);

  // now create the framebuffer for the output
  int outDims[3];
  outDims[0] = outExt[1] - outExt[0] + 1;
  outDims[1] = outExt[3] - outExt[2] + 1;
  outDims[2] = outExt[5] - outExt[4] + 1;

  vtkNew<vtkTextureObject> outputTex;
  outputTex->SetContext(this->RenderWindow);

  vtkNew<vtkFrameBufferObject> fbo;
  fbo->SetContext(this->RenderWindow);

  outputTex->SetInternalFormat(GL_R32F);
  outputTex->Create2D(outDims[0], outDims[1], 1, VTK_FLOAT, false);
  fbo->SetNumberOfRenderTargets(1);
  fbo->SetColorBuffer(0, outputTex.GetPointer());

  // because the same FBO can be used in another pass but with several color
  // buffers, force this pass to use 1, to avoid side effects from the
  // render of the previous frame.
  fbo->SetActiveBuffer(0);

  fbo->SetDepthBufferNeeded(false);
  fbo->StartNonOrtho(outDims[0], outDims[1], false);
  glViewport(0, 0, outDims[0], outDims[1]);
  glScissor(0, 0, outDims[0], outDims[1]);
  glDisable(GL_DEPTH_TEST);

  vtkShaderProgram *prog =
    this->RenderWindow->GetShaderCache()->ReadyShaderProgram(
      vertexCode, fragmentCode, geometryCode);
  if (prog != this->Quad.Program)
    {
    this->Quad.Program = prog;
    this->Quad.VAO->ShaderProgramChanged();
    }
  cb->InitializeShaderUniforms(prog);

  inputTex->Activate();
  int inputTexId = inputTex->GetTextureUnit();
  this->Quad.Program->SetUniformi("inputTex1", inputTexId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // for each zslice in the output
  //vtkPixelExtent outputPixelExt(outExt);
  for (int i = outExt[4]; i <= outExt[5]; i++)
    {
    this->Quad.Program->SetUniformf("zPos", (i - outExt[4] + 0.5) / (outDims[2]));

    fbo->RenderQuad(
      0, outDims[0] - 1,
      0, outDims[1] - 1,
      this->Quad.Program, this->Quad.VAO);

    vtkPixelBufferObject *outPBO = outputTex->Download();

    unsigned int newDims[2];
    newDims[0] = dims[0];
    newDims[1] = dims[1];
    vtkIdType increments[2];
    increments[0] = 0;
    increments[1] = 0;
    outPBO->Download2D(VTK_DOUBLE, outImage->GetScalarPointer(outExt[0], outExt[2], i),
                       newDims, 1, increments);
    outPBO->Delete();
    }
  inputTex->Deactivate();

  vtkNew<vtkImageCast> castFilter;
  castFilter->SetInputData(outImage);
  castFilter->SetOutputScalarTypeToFloat();
  castFilter->Update();
  outImage->DeepCopy(castFilter->GetOutput());
}

// ----------------------------------------------------------------------------
void vtkAstroOpenGLImageAlgorithmHelper::Execute(
  vtkOpenGLImageAlgorithmCallback *cb,
  vtkImageData *inImage, vtkDataArray *inArray,
  vtkImageData *outImage, int outExt[],
  const char *vertexCode,
  const char *fragmentCodeX,
  const char *fragmentCodeY,
  const char *fragmentCodeZ,
  const char *geometryCode
  )
{
  // make sure it is initialized
  if (!this->RenderWindow)
    {
    this->SetRenderWindow(vtkRenderWindow::New());
    this->RenderWindow->SetOffScreenRendering(true);
    this->RenderWindow->UnRegister(this);
    }
  this->RenderWindow->Initialize();

  // Is it a 2D or 3D image
  int dims[3];
  inImage->GetDimensions(dims);
  int dimensions = 0;
  for (int i = 0; i < 3; i ++)
    {
    if (dims[i] > 1)
      {
      dimensions++;
      }
    }

  // no 1d or 2D supprt yet
  if (dimensions < 3)
    {
    vtkErrorMacro("no 1D or 2D processing support yet");
    return;
    }

  // send vector data to a texture
  int inputExt[6];
  inImage->GetExtent(inputExt);
  void *inPtr = inArray->GetVoidPointer(0);

  if (inArray->GetArrayType() != VTK_FLOAT)
      {
      vtkNew<vtkImageCast> castFilter;
      castFilter->SetInputData(inImage);
      castFilter->SetOutputScalarTypeToFloat();
      castFilter->Update();
      inImage->DeepCopy(castFilter->GetOutput());
      inPtr = inImage->GetScalarPointer();
      }

  vtkNew<vtkTextureObject> inputTex;
  inputTex->SetContext(this->RenderWindow);
  inputTex->SetInternalFormat(GL_R32F);
  inputTex->Create3DFromRaw(
    dims[0], dims[1], dims[2],
    inArray->GetNumberOfComponents(),
    VTK_FLOAT, inPtr);

  // now create the framebuffer for the output
  int outDims[3];
  outDims[0] = outExt[1] - outExt[0] + 1;
  outDims[1] = outExt[3] - outExt[2] + 1;
  outDims[2] = outExt[5] - outExt[4] + 1;

  vtkNew<vtkTextureObject> outputTex;
  outputTex->SetContext(this->RenderWindow);

  vtkNew<vtkFrameBufferObject> fbo;
  fbo->SetContext(this->RenderWindow);

  outputTex->SetInternalFormat(GL_R32F);
  outputTex->Create3D(outDims[0], outDims[1], outDims[2], 1, VTK_FLOAT, false);

  fbo->SetNumberOfRenderTargets(1);
  fbo->SetDepthBufferNeeded(false);
  glViewport(0, 0, outDims[0], outDims[1]);
  glScissor(0, 0, outDims[0], outDims[1]);
  glDisable(GL_DEPTH_TEST);

  inputTex->Activate();
  outputTex->Activate();

  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Along X
  vtkShaderProgram *prog =
    this->RenderWindow->GetShaderCache()->ReadyShaderProgram(
      vertexCode, fragmentCodeX, geometryCode);
  if (prog != this->Quad.Program)
    {
    this->Quad.Program = prog;
    this->Quad.VAO->ShaderProgramChanged();
    }

  int inputTexId = inputTex->GetTextureUnit();
  this->Quad.Program->SetUniformi("inputTex1", inputTexId);
  cb->InitializeShaderUniforms(prog);

  // for each zslice in the output
  for (int i = outExt[4]; i <= outExt[5]; i++)
    {
    int slice = i - outExt[4];
    fbo->SetColorBuffer(0, outputTex.GetPointer(), slice);
    fbo->SetActiveBuffer(0);
    fbo->StartNonOrtho(outDims[0], outDims[1], false);

    this->Quad.Program->SetUniformf("zPos", (slice + 0.5) / (outDims[2]));

    fbo->RenderQuad(
      0, outDims[0] - 1,
      0, outDims[1] - 1,
      this->Quad.Program, this->Quad.VAO);
    }

  // Along Y
  prog = this->RenderWindow->GetShaderCache()->ReadyShaderProgram(
      vertexCode, fragmentCodeY, geometryCode);
  if (prog != this->Quad.Program)
    {
    this->Quad.Program = prog;
    this->Quad.VAO->ShaderProgramChanged();
    }

  inputTexId = outputTex->GetTextureUnit();
  this->Quad.Program->SetUniformi("inputTex1", inputTexId);
  cb->InitializeShaderUniforms(prog);

  // for each zslice in the output
  for (int i = outExt[4]; i <= outExt[5]; i++)
    {
    int slice = i - outExt[4];
    fbo->SetColorBuffer(0, inputTex.GetPointer(), slice);
    fbo->SetActiveBuffer(0);
    fbo->StartNonOrtho(outDims[0], outDims[1], false);

    this->Quad.Program->SetUniformf("zPos", (slice + 0.5) / (outDims[2]));

    fbo->RenderQuad(
      0, outDims[0] - 1,
      0, outDims[1] - 1,
      this->Quad.Program, this->Quad.VAO);
    }

  // Along Z
  prog = this->RenderWindow->GetShaderCache()->ReadyShaderProgram(
      vertexCode, fragmentCodeZ, geometryCode);
  if (prog != this->Quad.Program)
    {
    this->Quad.Program = prog;
    this->Quad.VAO->ShaderProgramChanged();
    }

  inputTexId = inputTex->GetTextureUnit();
  this->Quad.Program->SetUniformi("inputTex1", inputTexId);
  cb->InitializeShaderUniforms(prog);

  // for each zslice in the output
  for (int i = outExt[4]; i <= outExt[5]; i++)
    {
    int slice = i - outExt[4];
    fbo->SetColorBuffer(0, outputTex.GetPointer(), slice);
    fbo->SetActiveBuffer(0);
    fbo->StartNonOrtho(outDims[0], outDims[1], false);

    this->Quad.Program->SetUniformf("zPos", (slice + 0.5) / (outDims[2]));

    fbo->RenderQuad(
      0, outDims[0] - 1,
      0, outDims[1] - 1,
      this->Quad.Program, this->Quad.VAO);
    }

  fbo->RemoveAllColorBuffers();

  vtkPixelBufferObject *outPBO = outputTex->Download();

  unsigned int newDims[3];
  newDims[0] = dims[0];
  newDims[1] = dims[1];
  newDims[2] = dims[2];
  vtkIdType increments[3];
  increments[0] = 0;
  increments[1] = 0;
  increments[2] = 0;
  outPBO->Download3D(VTK_DOUBLE, outImage->GetScalarPointer(),
                     newDims, 1, increments);
  outPBO->Delete();
  vtkNew<vtkImageCast> castFilter;
  castFilter->SetInputData(outImage);
  castFilter->SetOutputScalarTypeToFloat();
  castFilter->Update();
  outImage->DeepCopy(castFilter->GetOutput());
}

// ----------------------------------------------------------------------------
void vtkAstroOpenGLImageAlgorithmHelper::Execute(
  vtkOpenGLImageAlgorithmCallback *cb,
  vtkImageData *inImage, vtkDataArray *inArray,
  vtkImageData *outImage, int outExt[6],
  const char *vertexCode,
  const char *fragmentCode,
  const char *geometryCode,
  const int n
  )
{
  // make sure it is initialized
  if (!this->RenderWindow)
    {
    this->SetRenderWindow(vtkRenderWindow::New());
    this->RenderWindow->SetOffScreenRendering(true);
    this->RenderWindow->UnRegister(this);
    }
  this->RenderWindow->Initialize();

  // Is it a 2D or 3D image
  int dims[3];
  inImage->GetDimensions(dims);
  int dimensions = 0;
  for (int i = 0; i < 3; i ++)
    {
    if (dims[i] > 1)
      {
      dimensions++;
      }
    }

  // no 1d or 2D supprt yet
  if (dimensions < 3)
    {
    vtkErrorMacro("no 1D or 2D processing support yet");
    return;
    }

  // send vector data to a texture
  int inputExt[6];
  inImage->GetExtent(inputExt);
  void *inPtr = inArray->GetVoidPointer(0);

  if (inArray->GetArrayType() != VTK_FLOAT)
      {
      vtkNew<vtkImageCast> castFilter;
      castFilter->SetInputData(inImage);
      castFilter->SetOutputScalarTypeToFloat();
      castFilter->Update();
      inImage->DeepCopy(castFilter->GetOutput());
      inPtr = inImage->GetScalarPointer();
      }

  vtkNew<vtkTextureObject> inputTex;
  inputTex->SetContext(this->RenderWindow);
  inputTex->SetInternalFormat(GL_R32F);
  inputTex->Create3DFromRaw(
    dims[0], dims[1], dims[2],
    inArray->GetNumberOfComponents(),
    VTK_FLOAT, inPtr);

  // now create the framebuffer for the output
  int outDims[3];
  outDims[0] = outExt[1] - outExt[0] + 1;
  outDims[1] = outExt[3] - outExt[2] + 1;
  outDims[2] = outExt[5] - outExt[4] + 1;

  vtkNew<vtkTextureObject> outputTex;
  outputTex->SetContext(this->RenderWindow);

  vtkNew<vtkFrameBufferObject> fbo;
  fbo->SetContext(this->RenderWindow);

  outputTex->SetInternalFormat(GL_R32F);
  outputTex->Create3D(outDims[0], outDims[1], outDims[2], 1, VTK_FLOAT, false);

  fbo->SetNumberOfRenderTargets(1);
  fbo->SetDepthBufferNeeded(false);
  glViewport(0, 0, outDims[0], outDims[1]);
  glScissor(0, 0, outDims[0], outDims[1]);
  glDisable(GL_DEPTH_TEST);

  vtkShaderProgram *prog =
    this->RenderWindow->GetShaderCache()->ReadyShaderProgram(
      vertexCode, fragmentCode, geometryCode);
  if (prog != this->Quad.Program)
    {
    this->Quad.Program = prog;
    this->Quad.VAO->ShaderProgramChanged();
    }

  cb->InitializeShaderUniforms(prog);

  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  inputTex->Activate();
  outputTex->Activate();

  for (int ii = 0; ii < n; ii++)
    {
    int inputTexId;

    if ((ii % 2) < 0.001)
      {
      inputTexId = inputTex->GetTextureUnit();
      }
    else
      {
      inputTexId = outputTex->GetTextureUnit();
      }

    this->Quad.Program->SetUniformi("inputTex1", inputTexId);

    // for each zslice in the output
    for (int i = outExt[4]; i <= outExt[5]; i++)
      {
      int slice = i - outExt[4];

      if ((ii % 2) < 0.001)
        {
        fbo->SetColorBuffer(0, outputTex.GetPointer(), slice);
        }
      else
        {
        fbo->SetColorBuffer(0, inputTex.GetPointer(), slice);
        }

      fbo->SetActiveBuffer(0);
      fbo->StartNonOrtho(outDims[0], outDims[1], false);

      this->Quad.Program->SetUniformf("zPos", (slice + 0.5) / (outDims[2]));

      fbo->RenderQuad(
        0, outDims[0] - 1,
        0, outDims[1] - 1,
        this->Quad.Program, this->Quad.VAO);
      }
    }
  fbo->RemoveAllColorBuffers();

  vtkPixelBufferObject *outPBO = outputTex->Download();

  unsigned int newDims[3];
  newDims[0] = dims[0];
  newDims[1] = dims[1];
  newDims[2] = dims[2];
  vtkIdType increments[3];
  increments[0] = 0;
  increments[1] = 0;
  increments[2] = 0;
  outPBO->Download3D(VTK_DOUBLE, outImage->GetScalarPointer(),
                     newDims, 1, increments);
  outPBO->Delete();
  vtkNew<vtkImageCast> castFilter;
  castFilter->SetInputData(outImage);
  castFilter->SetOutputScalarTypeToFloat();
  castFilter->Update();
  outImage->DeepCopy(castFilter->GetOutput());
}

// ----------------------------------------------------------------------------
void vtkAstroOpenGLImageAlgorithmHelper::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "RenderWindow:";
  if(this->RenderWindow != 0)
    {
    this->RenderWindow->PrintSelf(os,indent);
    }
  else
    {
    os << "(none)" <<endl;
    }
}
