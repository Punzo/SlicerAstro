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

// vtkOpenGLFilters includes
#include "vtkAstroOpenGLImageAlgorithmHelper.h"

// VTK includes
#include "vtkDataArray.h"
#include "vtkImageData.h"
#include "vtkFloatArray.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLFramebufferObject.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLShaderCache.h"
#include "vtkOpenGLVertexArrayObject.h"
#include "vtkPixelBufferObject.h"
#include "vtkPixelTransfer.h"
#include "vtkPointData.h"
#include "vtkShaderProgram.h"
#include "vtkTextureObject.h"
#include "vtkOpenGLState.h"
#include "vtk_glad.h"
#include "vtkVersionMacros.h"
#include "vtkRenderingOpenGLConfigure.h"

#if defined(VTK_OPENGL_HAS_EGL) && defined(__has_include)
#if __has_include(<vtkglad/include/glad/egl.h>)
#include "vtkEGLRenderWindow.h"
#include <vtkglad/include/glad/egl.h>
#define SLICERASTRO_OPENGL_FILTERS_EGL
#endif
#endif

#include <cstdlib>
#if defined(SLICERASTRO_OPENGL_FILTERS_EGL)
#include <dlfcn.h>
#endif

namespace
{
//----------------------------------------------------------------------------
/// A new offscreen render window with its own OpenGL context.
///
/// vtkRenderWindow::New() first probes GLX. With Qt on Wayland (and
/// XWayland with the NVIDIA driver, Xvfb) glXMakeCurrent of the probe fails
/// with BadAccess (ignored by SlicerAstro's X error handler), but VTK still
/// considers the window initialized because the OpenGL functions answer:
/// they answer from the context that is current, i.e. the one of a view of
/// the application. The filters would then run in the context of that view
/// and leave it in a state its cache does not know ("Error in cache state
/// for GL_ACTIVE_TEXTURE"). EGL gives an offscreen context of its own on
/// Linux, without X; VTK's default is used if EGL is not available, or if
/// the backend is chosen with VTK_DEFAULT_OPENGL_WINDOW.
#if defined(SLICERASTRO_OPENGL_FILTERS_EGL)
/// Whether a GLX context is current in this thread (the application draws
/// with GLX, e.g. Qt on X11 or Xvfb): an EGL context can then not be made
/// current in the same thread (EGL_BAD_ACCESS).
bool IsGLXContextCurrent()
{
  const char* libraries[2] = {"libGLX.so.0", "libGL.so.1"};
  for (int i = 0; i < 2; i++)
    {
    // only a library already loaded by the application
    void* library = dlopen(libraries[i], RTLD_LAZY | RTLD_NOLOAD);
    if (!library)
      {
      continue;
      }
    typedef void* (*GetCurrentContextFunction)();
    GetCurrentContextFunction getCurrentContext =
      reinterpret_cast<GetCurrentContextFunction>(dlsym(library, "glXGetCurrentContext"));
    const bool current = getCurrentContext && getCurrentContext() != nullptr;
    dlclose(library);
    if (current)
      {
      return true;
      }
    }
  return false;
}
#endif

vtkRenderWindow* NewFilterRenderWindow()
{
#if defined(SLICERASTRO_OPENGL_FILTERS_EGL)
  static const bool eglLoaded = gladLoaderLoadEGL(EGL_NO_DISPLAY) != 0;
  if (eglLoaded && !std::getenv("VTK_DEFAULT_OPENGL_WINDOW") && !IsGLXContextCurrent())
    {
    // eglGetCurrentContext (EGL 1.4) is loaded by glad only once an EGL
    // display is open (the first EGL window): before, no EGL context is current
    const EGLContext previous = glad_eglGetCurrentContext ? eglGetCurrentContext() : EGL_NO_CONTEXT;
    vtkEGLRenderWindow* window = vtkEGLRenderWindow::New();
    window->SetOffScreenRendering(true);
    window->Initialize();
    // initialized with its own context (and not with the previous one)
    const EGLContext context = glad_eglGetCurrentContext ? eglGetCurrentContext() : EGL_NO_CONTEXT;
    if (window->GetInitialized() && context != EGL_NO_CONTEXT && context != previous)
      {
      return window;
      }
    window->Delete();
    }
#endif
  vtkRenderWindow* window = vtkRenderWindow::New();
  window->SetOffScreenRendering(true);
  return window;
}

//----------------------------------------------------------------------------
/// The cached OpenGL state of a render window read again from its context
void ResynchronizeState(vtkRenderWindow* window)
{
  vtkOpenGLRenderWindow* glWindow = vtkOpenGLRenderWindow::SafeDownCast(window);
  if (glWindow && glWindow->GetState())
    {
    glWindow->GetState()->Reset();
    }
}

//----------------------------------------------------------------------------
/// Releases the OpenGL context of a render window when the scope ends
struct ReleaseContextOnExit
{
  explicit ReleaseContextOnExit(vtkRenderWindow* window) : Window(window) {}
  ~ReleaseContextOnExit()
    {
    if (this->Window)
      {
      this->Window->ReleaseCurrent();
      }
    }
  vtkRenderWindow* Window;
};
} // end of anonymous namespace

vtkStandardNewMacro(vtkAstroOpenGLImageAlgorithmHelper);

//----------------------------------------------------------------------------
void vtkAstroOpenGLImageAlgorithmHelper::ConfigureBorder(vtkTextureObject* texture)
{
  // ClampToBorder samples the (default, zero) border color outside the image
  const int mode = this->ZeroBorder ? vtkTextureObject::ClampToBorder : vtkTextureObject::ClampToEdge;
  texture->SetWrapS(mode);
  texture->SetWrapT(mode);
  texture->SetWrapR(mode);
}

// ----------------------------------------------------------------------------
vtkAstroOpenGLImageAlgorithmHelper::vtkAstroOpenGLImageAlgorithmHelper()
{
  this->RenderWindow = nullptr;
}

// ----------------------------------------------------------------------------
vtkAstroOpenGLImageAlgorithmHelper::~vtkAstroOpenGLImageAlgorithmHelper()
{
  this->SetRenderWindow(nullptr);
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
    this->SetRenderWindow(NewFilterRenderWindow());
    this->RenderWindow->UnRegister(this);
    }
  this->RenderWindow->Initialize();
  // the context of this window (a view of the application may have made
  // its own context current since the last use); released at the end, so
  // that the views of the application do not render with it
  this->RenderWindow->MakeCurrent();
  const ReleaseContextOnExit releaseContext(this->RenderWindow);
  // the state cache of VTK from the actual state of this context (another
  // context may have been current in between: "Error in cache state for
  // GL_ACTIVE_TEXTURE" warnings otherwise)
  ResynchronizeState(this->RenderWindow);

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

  // no 1D or 2D support yet
  if (dimensions < 3)
    {
    vtkErrorMacro("no 1D or 2D processing support yet");
    return;
    }

  // send vector data to a texture
  int inputExt[6];
  inImage->GetExtent(inputExt);
  void *inPtr = inArray->GetVoidPointer(0);

  // The GPU filters work in float: a non-float input is converted into a
  // temporary buffer (the input image is never modified)
  vtkNew<vtkFloatArray> floatInput;
  if (inArray->GetDataType() != VTK_FLOAT)
    {
    floatInput->DeepCopy(inArray);
    inPtr = floatInput->GetVoidPointer(0);
    }

  vtkNew<vtkTextureObject> inputTex;
  inputTex->SetContext(this->RenderWindow);
  inputTex->Create3DFromRaw(
    dims[0], dims[1], dims[2],
    inArray->GetNumberOfComponents(),
    VTK_FLOAT, inPtr);
  this->ConfigureBorder(inputTex);

  // now create the framebuffer for the output
  int outDims[3];
  outDims[0] = outExt[1] - outExt[0] + 1;
  outDims[1] = outExt[3] - outExt[2] + 1;
  outDims[2] = outExt[5] - outExt[4] + 1;

  vtkNew<vtkTextureObject> outputTex;
  outputTex->SetContext(this->RenderWindow);

  vtkNew<vtkOpenGLFramebufferObject> fbo;
  fbo->SetContext(this->RenderWindow);
  vtkOpenGLState* ostate = this->RenderWindow->GetState();
  ostate->PushFramebufferBindings();
  fbo->Bind();

  outputTex->Create2D(outDims[0], outDims[1], 1, VTK_FLOAT, false);

  fbo->AddColorAttachment(0, outputTex.GetPointer());

  // because the same FBO can be used in another pass but with several color
  // buffers, force this pass to use 1, to avoid side effects from the
  // render of the previous frame.
  fbo->ActivateDrawBuffer(0);

  fbo->StartNonOrtho(outDims[0], outDims[1]);
  ostate->vtkglViewport(0, 0, outDims[0], outDims[1]);
  ostate->vtkglScissor(0, 0, outDims[0], outDims[1]);
  ostate->vtkglDisable(GL_DEPTH_TEST);
  ostate->vtkglDepthMask(false);

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
  vtkPixelExtent outputPixelExt(outExt);
  for (int i = outExt[4]; i <= outExt[5]; i++)
    {
    this->Quad.Program->SetUniformf("zPos", (i - outExt[4] + 0.5) / (outDims[2]));

    fbo->RenderQuad(
      0, outDims[0] - 1,
      0, outDims[1] - 1,
      this->Quad.Program, this->Quad.VAO);

    vtkPixelBufferObject *outPBO = outputTex->Download();

    vtkPixelTransfer::Blit<float, float>(
      outputPixelExt,
      outputPixelExt,
      outputPixelExt,
      outputPixelExt,
      1,
      (float*)outPBO->MapPackedBuffer(),
      1,
      static_cast<float *>(outImage->GetScalarPointer(outExt[0], outExt[2], i)));

    outPBO->UnmapPackedBuffer();
    outPBO->Delete();
    }

  fbo->RemoveColorAttachment(0);

  inputTex->Deactivate();

  ostate->PopFramebufferBindings();
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
  // make sure RenderWindow is initialized
  if (!this->RenderWindow)
    {
    this->SetRenderWindow(NewFilterRenderWindow());
    this->RenderWindow->UnRegister(this);
    }
  this->RenderWindow->Initialize();
  // the context of this window (a view of the application may have made
  // its own context current since the last use); released at the end, so
  // that the views of the application do not render with it
  this->RenderWindow->MakeCurrent();
  const ReleaseContextOnExit releaseContext(this->RenderWindow);
  // the state cache of VTK from the actual state of this context (another
  // context may have been current in between: "Error in cache state for
  // GL_ACTIVE_TEXTURE" warnings otherwise)
  ResynchronizeState(this->RenderWindow);

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

  // no 1D or 2D support yet
  if (dimensions < 3)
    {
    vtkErrorMacro("no 1D or 2D processing support yet");
    return;
    }

  // Send vector data to a texture
  int inputExt[6];
  inImage->GetExtent(inputExt);
  void *inPtr = inArray->GetVoidPointer(0);

  // The GPU filters work in float: a non-float input is converted into a
  // temporary buffer (the input image is never modified)
  vtkNew<vtkFloatArray> floatInput;
  if (inArray->GetDataType() != VTK_FLOAT)
    {
    floatInput->DeepCopy(inArray);
    inPtr = floatInput->GetVoidPointer(0);
    }

  vtkNew<vtkTextureObject> inputTex;
  inputTex->SetContext(this->RenderWindow);
  inputTex->Create3DFromRaw(
    dims[0], dims[1], dims[2],
    inArray->GetNumberOfComponents(),
    VTK_FLOAT, inPtr);
  this->ConfigureBorder(inputTex);

  // Create the TextureObject for the output
  int outDims[3];
  outDims[0] = outExt[1] - outExt[0] + 1;
  outDims[1] = outExt[3] - outExt[2] + 1;
  outDims[2] = outExt[5] - outExt[4] + 1;

  vtkNew<vtkTextureObject> outputTex;
  outputTex->SetContext(this->RenderWindow);
  outputTex->Create3D(outDims[0], outDims[1], outDims[2], 1, VTK_FLOAT, false);
  this->ConfigureBorder(outputTex);
  // Intermediate texture for the Y pass (the input texture is not reused as
  // render target: X: input -> output, Y: output -> temp, Z: temp -> output)
  vtkNew<vtkTextureObject> tempTex;
  tempTex->SetContext(this->RenderWindow);
  tempTex->Create3D(outDims[0], outDims[1], outDims[2], 1, VTK_FLOAT, false);
  this->ConfigureBorder(tempTex);

  // Create the Framebuffer for the output
  vtkNew<vtkOpenGLFramebufferObject> fbo;
  fbo->SetContext(this->RenderWindow);
  vtkOpenGLState* ostate = this->RenderWindow->GetState();
  ostate->PushFramebufferBindings();
  fbo->Bind();

  ostate->vtkglViewport(0, 0, outDims[0], outDims[1]);
  ostate->vtkglScissor(0, 0, outDims[0], outDims[1]);
  ostate->vtkglDisable(GL_DEPTH_TEST);
  ostate->vtkglDepthMask(false);

  // Run Filter along X
  vtkShaderProgram *prog =
    this->RenderWindow->GetShaderCache()->ReadyShaderProgram(
      vertexCode, fragmentCodeX, geometryCode);
  if (prog != this->Quad.Program)
    {
    this->Quad.Program = prog;
    this->Quad.VAO->ShaderProgramChanged();
    }
  cb->InitializeShaderUniforms(prog);

  inputTex->Activate();
  outputTex->Activate();
  tempTex->Activate();

  int inputTexId = inputTex->GetTextureUnit();
  this->Quad.Program->SetUniformi("inputTex1", inputTexId);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // for each zslice in the output
  for (int i = outExt[4]; i <= outExt[5]; i++)
    {
    int slice = i - outExt[4];

    fbo->RemoveColorAttachment(0);
    fbo->AddColorAttachment(0, outputTex.GetPointer(), slice);

    fbo->ActivateDrawBuffer(0);
    fbo->StartNonOrtho(outDims[0], outDims[1]);

    this->Quad.Program->SetUniformf("zPos", (slice + 0.5) / (outDims[2]));

    fbo->RenderQuad(
      0, outDims[0] - 1,
      0, outDims[1] - 1,
      this->Quad.Program, this->Quad.VAO);
    }

  // Run Filter along Y (reads the X result in outputTex, writes tempTex)
  prog = this->RenderWindow->GetShaderCache()->ReadyShaderProgram(
      vertexCode, fragmentCodeY, geometryCode);
  if (prog != this->Quad.Program)
    {
    this->Quad.Program = prog;
    this->Quad.VAO->ShaderProgramChanged();
    }
  cb->InitializeShaderUniforms(prog);

  inputTexId = outputTex->GetTextureUnit();
  this->Quad.Program->SetUniformi("inputTex1", inputTexId);

  // for each zslice in the output
  for (int i = outExt[4]; i <= outExt[5]; i++)
    {
    int slice = i - outExt[4];
    fbo->RemoveColorAttachment(0);
    fbo->AddColorAttachment(0, tempTex.GetPointer(), slice);
    fbo->ActivateDrawBuffer(0);
    fbo->StartNonOrtho(outDims[0], outDims[1]);

    this->Quad.Program->SetUniformf("zPos", (slice + 0.5) / (outDims[2]));

    fbo->RenderQuad(
      0, outDims[0] - 1,
      0, outDims[1] - 1,
      this->Quad.Program, this->Quad.VAO);
    }

  // Run Filter along Z (reads the Y result in tempTex, writes outputTex)
  prog = this->RenderWindow->GetShaderCache()->ReadyShaderProgram(
      vertexCode, fragmentCodeZ, geometryCode);
  if (prog != this->Quad.Program)
    {
    this->Quad.Program = prog;
    this->Quad.VAO->ShaderProgramChanged();
    }
  cb->InitializeShaderUniforms(prog);

  inputTexId = tempTex->GetTextureUnit();
  this->Quad.Program->SetUniformi("inputTex1", inputTexId);

  // for each zslice in the output
  for (int i = outExt[4]; i <= outExt[5]; i++)
    {
    int slice = i - outExt[4];
    fbo->RemoveColorAttachment(0);
    fbo->AddColorAttachment(0, outputTex.GetPointer(), slice);
    fbo->ActivateDrawBuffer(0);
    fbo->StartNonOrtho(outDims[0], outDims[1]);

    this->Quad.Program->SetUniformf("zPos", (slice + 0.5) / (outDims[2]));

    fbo->RenderQuad(
      0, outDims[0] - 1,
      0, outDims[1] - 1,
      this->Quad.Program, this->Quad.VAO);
    }

  vtkPixelBufferObject *outPBO = outputTex->Download();

  unsigned int newDims[3];
  newDims[0] = dims[0];
  newDims[1] = dims[1];
  newDims[2] = dims[2];
  vtkIdType increments[3];
  increments[0] = 0;
  increments[1] = 0;
  increments[2] = 0;
  outPBO->Download3D(VTK_FLOAT, outImage->GetScalarPointer(),
                     newDims, 1, increments);
  outPBO->Delete();

  fbo->RemoveColorAttachment(0);
  inputTex->Deactivate();
  outputTex->Deactivate();
  tempTex->Deactivate();

  ostate->PopFramebufferBindings();
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
    this->SetRenderWindow(NewFilterRenderWindow());
    this->RenderWindow->UnRegister(this);
    }
  this->RenderWindow->Initialize();
  // the context of this window (a view of the application may have made
  // its own context current since the last use); released at the end, so
  // that the views of the application do not render with it
  this->RenderWindow->MakeCurrent();
  const ReleaseContextOnExit releaseContext(this->RenderWindow);
  // the state cache of VTK from the actual state of this context (another
  // context may have been current in between: "Error in cache state for
  // GL_ACTIVE_TEXTURE" warnings otherwise)
  ResynchronizeState(this->RenderWindow);

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

  // no 1D or 2D support yet
  if (dimensions < 3)
    {
    vtkErrorMacro("no 1D or 2D processing support yet");
    return;
    }

  // Send vector data to a texture
  int inputExt[6];
  inImage->GetExtent(inputExt);
  void *inPtr = inArray->GetVoidPointer(0);

  // The GPU filters work in float: a non-float input is converted into a
  // temporary buffer (the input image is never modified)
  vtkNew<vtkFloatArray> floatInput;
  if (inArray->GetDataType() != VTK_FLOAT)
    {
    floatInput->DeepCopy(inArray);
    inPtr = floatInput->GetVoidPointer(0);
    }

  vtkNew<vtkTextureObject> inputTex;
  inputTex->SetContext(this->RenderWindow);
  inputTex->SetInternalFormat(GL_R32F);
  inputTex->Create3DFromRaw(
    dims[0], dims[1], dims[2],
    inArray->GetNumberOfComponents(),
    VTK_FLOAT, inPtr);
  this->ConfigureBorder(inputTex);

  // now create the framebuffer for the output
  int outDims[3];
  outDims[0] = outExt[1] - outExt[0] + 1;
  outDims[1] = outExt[3] - outExt[2] + 1;
  outDims[2] = outExt[5] - outExt[4] + 1;

  vtkNew<vtkTextureObject> outputTex;
  outputTex->SetContext(this->RenderWindow);
  outputTex->Create3D(outDims[0], outDims[1], outDims[2], 1, VTK_FLOAT, false);
  this->ConfigureBorder(outputTex);

  vtkNew<vtkOpenGLFramebufferObject> fbo;
  fbo->SetContext(this->RenderWindow);
  vtkOpenGLState* ostate = this->RenderWindow->GetState();
  ostate->PushFramebufferBindings();
  fbo->Bind();

  ostate->vtkglViewport(0, 0, outDims[0], outDims[1]);
  ostate->vtkglScissor(0, 0, outDims[0], outDims[1]);
  ostate->vtkglDisable(GL_DEPTH_TEST);
  ostate->vtkglDepthMask(false);

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
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // for each zslice in the output
    for (int i = outExt[4]; i <= outExt[5]; i++)
      {
      int slice = i - outExt[4];
      fbo->RemoveColorAttachment(0);

      if ((ii % 2) < 0.001)
        {
        fbo->AddColorAttachment(0, outputTex.GetPointer(), slice);
        }
      else
        {
        fbo->AddColorAttachment(0, inputTex.GetPointer(), slice);
        }

      fbo->ActivateDrawBuffer(0);
      fbo->StartNonOrtho(outDims[0], outDims[1]);

      this->Quad.Program->SetUniformf("zPos", (slice + 0.5) / (outDims[2]));

      fbo->RenderQuad(
        0, outDims[0] - 1,
        0, outDims[1] - 1,
        this->Quad.Program, this->Quad.VAO);
      }
    }

  // Ping-pong: even iterations write outputTex, odd ones inputTex. Download
  // the texture written last (n == 0: the input).
  vtkTextureObject* resultTex = (n % 2 == 1) ? outputTex.GetPointer() : inputTex.GetPointer();
  vtkPixelBufferObject *outPBO = resultTex->Download();

  unsigned int newDims[3];
  newDims[0] = dims[0];
  newDims[1] = dims[1];
  newDims[2] = dims[2];
  vtkIdType increments[3];
  increments[0] = 0;
  increments[1] = 0;
  increments[2] = 0;
  outPBO->Download3D(VTK_FLOAT, outImage->GetScalarPointer(),
                     newDims, 1, increments);
  outPBO->Delete();

  fbo->RemoveColorAttachment(0);
  inputTex->Deactivate();
  outputTex->Deactivate();

  ostate->PopFramebufferBindings();
}
