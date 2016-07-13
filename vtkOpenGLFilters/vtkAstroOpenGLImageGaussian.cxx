// vtkOpenGLFilters includes
#include "vtkAstroOpenGLImageGaussian.h"
#include "vtkAstroOpenGLImageAlgorithmHelper.h"

// VTK includes
#include "vtkCellData.h"
#include "vtkDataArray.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkShaderProgram.h"

#include <algorithm> // for std::nth_element

#define SigmatoFWHM 2.3548200450309493

vtkStandardNewMacro(vtkAstroOpenGLImageGaussian);

//-----------------------------------------------------------------------------
// Construct an instance of vtkAstroOpenGLImageGaussian fitler.
vtkAstroOpenGLImageGaussian::vtkAstroOpenGLImageGaussian()
{
  // for GPU we do not want threading
  this->NumberOfThreads = 1;
  this->EnableSMP = false;
  this->Dimensionality = 1;
  this->KernelLength[0] = 10;
  this->KernelLength[1] = 10;
  this->KernelLength[2] = 10;
  this->FWHM[0] = 3;
  this->FWHM[1] = 3;
  this->FWHM[2] = 3;
  this->RotationAngles[0] = 0.;
  this->RotationAngles[1] = 0.;
  this->RotationAngles[2] = 0.;
  this->Helper = vtkSmartPointer<vtkAstroOpenGLImageAlgorithmHelper>::New();
}

//-----------------------------------------------------------------------------
vtkAstroOpenGLImageGaussian::~vtkAstroOpenGLImageGaussian()
{
}

void vtkAstroOpenGLImageGaussian::SetRenderWindow(vtkRenderWindow *renWin)
{
  this->Helper->SetRenderWindow(renWin);
}

//-----------------------------------------------------------------------------
void vtkAstroOpenGLImageGaussian::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Helper: ";
  this->Helper->PrintSelf(os, indent);
}

// this is used as a callback by the helper to set shader parameters
// before running and to update them on each slice
class vtkOpenGLGaussianCB : public vtkOpenGLImageAlgorithmCallback
{
public:
  // initialize the spacing
  virtual void InitializeShaderUniforms(vtkShaderProgram *program)
    {
    float sp[3];
    sp[0] = this->Spacing[0];
    sp[1] = this->Spacing[1];
    sp[2] = this->Spacing[2];
    program->SetUniform3f("spacing", sp);
    int kl[3];
    kl[0] = this->KernelLength[0];
    program->SetUniformi("kernelLengthX", kl[0]);
    kl[1] = this->KernelLength[1];
    program->SetUniformi("kernelLengthY", kl[1]);
    kl[2] = this->KernelLength[2];
    program->SetUniformi("kernelLengthZ", kl[2]);
    float stdev[3];
    stdev[0] = this->StandardDev[0];
    stdev[1] = this->StandardDev[1];
    stdev[2] = this->StandardDev[2];
    program->SetUniform3f("StandardDev", stdev);
    program->SetUniformf("cx", this->cx);
    program->SetUniformf("sx", this->sx);
    program->SetUniformf("cy", this->cy);
    program->SetUniformf("sy", this->sy);
    program->SetUniformf("cz", this->cz);
    program->SetUniformf("sz", this->sz);
    }

  // no uniforms change on a per slice basis so empty
  virtual void UpdateShaderUniforms(
    vtkShaderProgram * /* program */, int /* zExtent */) {};

  double *Spacing;
  double *StandardDev;
  int *KernelLength;
  float cx, sx, cy, sy, cz, sz;
};

//-----------------------------------------------------------------------------
// This method contains the first switch statement that calls the correct
// templated function for the input and output region types.
void vtkAstroOpenGLImageGaussian::ThreadedRequestData(
  vtkInformation *vtkNotUsed(request),
  vtkInformationVector **inputVector,
  vtkInformationVector *vtkNotUsed(outputVector),
  vtkImageData ***inData,
  vtkImageData **outData,
  int outExt[6], int vtkNotUsed(id))
{

  vtkDataArray *inArray = this->GetInputArrayToProcess(0,inputVector);
  outData[0]->GetPointData()->GetScalars()->SetName(inArray->GetName());

  // The ouptut scalar type must be double to store proper gradients.
  if(outData[0]->GetScalarType() != VTK_DOUBLE)
    {
    vtkErrorMacro("Execute: output ScalarType is "
                  << outData[0]->GetScalarType() << "but must be double.");
    return;
    }

  // Box makes sense only with one input component.
  if(inArray->GetNumberOfComponents() != 1)
    {
    vtkErrorMacro(
      "Execute: input has more than one component. "
      "The input to gradient should be a single component image. "
      "Think about it. If you insist on using a color image then "
      "run it though RGBToHSV then ExtractComponents to get the V "
      "components. That's probably what you want anyhow.");
    return;
    }

  vtkOpenGLGaussianCB cb;
  cb.Spacing = inData[0][0]->GetSpacing();
  int * extent = inData[0][0]->GetExtent();
  cb.Spacing[0] /= extent[1] - extent[0];
  cb.Spacing[1] /= extent[3] - extent[2];
  cb.Spacing[2] /= extent[5] - extent[4];

  this->KernelLength[0] = (int) (this->KernelLength[0] - 1.) / 2.;
  this->KernelLength[1] = (int) (this->KernelLength[1] - 1.) / 2.;
  this->KernelLength[2] = (int) (this->KernelLength[2] - 1.) / 2.;
  cb.KernelLength = this->KernelLength;


  this->FWHM[0] /= SigmatoFWHM;
  this->FWHM[0] = 2. * this->FWHM[0] * this->FWHM[0];
  this->FWHM[1] /= SigmatoFWHM;
  this->FWHM[1] = 2. * this->FWHM[1] * this->FWHM[1];
  this->FWHM[2] /= SigmatoFWHM;
  this->FWHM[2] = 2. * this->FWHM[2] * this->FWHM[2];
  cb.StandardDev = this->FWHM;

  std::string fragShader;

  if (fabs(KernelLength[0] - KernelLength[1]) < 0.001 &&
      fabs(KernelLength[1] - KernelLength[2]) < 0.001)
    {
    std::string fragShaderBegin =
    "//VTK::System::Dec\n"
    "varying vec2 tcoordVSOutput;\n"
    "uniform sampler3D inputTex1;\n"
    "uniform float zPos;\n"
    "uniform vec3 spacing;\n"
    "uniform int kernelLengthX;\n"
    "uniform int kernelLengthY;\n"
    "uniform int kernelLengthZ;\n"
    "uniform vec3 StandardDev;\n"
    "//VTK::Output::Dec\n"
    "void main(void) {\n"
    "float data = 0.; \n"
    "float sum = 0.; \n";

    std::string fragShaderEnd =
    "gl_FragData[0] = vec4(data / sum, 1., 1., 1.); \n"
    "}\n";

    std::string fragShaderX =
    "  for (int offsetX = -kernelLengthX; offsetX <= kernelLengthX; offsetX++){ \n"
    "        vec3 offset = vec3(offsetX, 0., 0.) * spacing; \n"
    "        float expA = offsetX * offsetX / StandardDev.x; \n"
    "        float kernel = exp(-(expA)); \n"
    "        data = data + texture3D(inputTex1, vec3(tcoordVSOutput, zPos) + offset).r * kernel; \n"
    "        sum = sum + kernel; \n"
    "  } \n";

    std::string fragShaderY =
    "  for (int offsetY = -kernelLengthY; offsetY <= kernelLengthY; offsetY++){ \n"
    "        vec3 offset = vec3(0., offsetY, 0.) * spacing; \n"
    "        float expA = offsetY * offsetY / StandardDev.y; \n"
    "        float kernel = exp(-(expA)); \n"
    "        data = data + texture3D(inputTex1, vec3(tcoordVSOutput, zPos) + offset).r * kernel; \n"
    "        sum = sum + kernel; \n"
    "  } \n";


    std::string fragShaderZ =
    "  for (int offsetZ = -kernelLengthZ; offsetZ <= kernelLengthZ; offsetZ++){ \n"
    "        vec3 offset = vec3(0., 0., offsetZ) * spacing; \n"
    "        float expA = offsetZ * offsetZ / StandardDev.z; \n"
    "        float kernel = exp(-(expA)); \n"
    "        data = data + texture3D(inputTex1, vec3(tcoordVSOutput, zPos) + offset).r * kernel; \n"
    "        sum = sum + kernel; \n"
    "  } \n";


    fragShaderX = fragShaderBegin + fragShaderX + fragShaderEnd;
    fragShaderY = fragShaderBegin + fragShaderY + fragShaderEnd;
    fragShaderZ = fragShaderBegin + fragShaderZ + fragShaderEnd;

    // call the helper to execte this code
    this->Helper->Execute(&cb,
      inData[0][0], inArray,
      outData[0], outExt,

      "//VTK::System::Dec\n"
      "attribute vec4 vertexMC;\n"
      "attribute vec2 tcoordMC;\n"
      "varying vec2 tcoordVSOutput;\n"
      "void main() {\n"
      "  tcoordVSOutput = tcoordMC;\n"
      "  gl_Position = vertexMC;\n"
      "}\n",

      fragShaderX.c_str(),
      fragShaderY.c_str(),
      fragShaderZ.c_str(),

      "");
    }
  else
    {
    if (this->RotationAngles[0] < 0.001 &&
       this->RotationAngles[1] < 0.001 &&
       this->RotationAngles[2] < 0.001)
      {
      fragShader =
      "//VTK::System::Dec\n"
      "varying vec2 tcoordVSOutput;\n"
      "uniform sampler3D inputTex1;\n"
      "uniform float zPos;\n"
      "uniform vec3 spacing;\n"
      "uniform int kernelLengthX;\n"
      "uniform int kernelLengthY;\n"
      "uniform int kernelLengthZ;\n"
      "uniform vec3 StandardDev;\n"
      "//VTK::Output::Dec\n"
      "void main(void) {\n"
      "float data = 0.; \n"
      "float sum = 0.; \n"
      "  for (int offsetX = -kernelLengthX; offsetX <= kernelLengthX; offsetX++){ \n"
      "    for (int offsetY = -kernelLengthY; offsetY <= kernelLengthY; offsetY++){ \n"
      "      for (int offsetZ = -kernelLengthZ; offsetZ <= kernelLengthZ; offsetZ++){ \n"
      "        vec3 offset = vec3(offsetX, offsetY, offsetZ) * spacing; \n"
      "        float expA = offsetX * offsetX / StandardDev.x; \n"
      "        float expB = offsetY * offsetY / StandardDev.y; \n"
      "        float expC = offsetZ * offsetZ / StandardDev.z; \n"
      "        float kernel = exp(-(expA + expB + expC)); \n"
      "        data = data + texture3D(inputTex1, vec3(tcoordVSOutput, zPos) + offset).r * kernel; \n"
      "        sum = sum + kernel; \n"
      "      } \n"
      "    } \n"
      "  } \n"
      "gl_FragData[0] = vec4(data / sum, 1., 1., 1.); \n"
      "}\n";
      }
    else
      {
      double rx = this->RotationAngles[0] * atan(1.) / 45.;
      double ry = this->RotationAngles[1] * atan(1.) / 45.;
      double rz = this->RotationAngles[2] * atan(1.) / 45.;

      cb.cx = cos(rx);
      cb.sx = sin(rx);
      cb.cy = cos(ry);
      cb.sy = sin(ry);
      cb.cz = cos(rz);
      cb.sz = sin(rz);

      fragShader =
      "//VTK::System::Dec\n"
      "varying vec2 tcoordVSOutput;\n"
      "uniform sampler3D inputTex1;\n"
      "uniform float zPos;\n"
      "uniform vec3 spacing;\n"
      "uniform int kernelLengthX;\n"
      "uniform int kernelLengthY;\n"
      "uniform int kernelLengthZ;\n"
      "uniform vec3 StandardDev;\n"
      "uniform float cx;\n"
      "uniform float sx;\n"
      "uniform float cy;\n"
      "uniform float sy;\n"
      "uniform float cz;\n"
      "uniform float sz;\n"
      "//VTK::Output::Dec\n"
      "void main(void) {\n"
      "float data = 0.; \n"
      "float sum = 0.; \n"
      "  for (int offsetX = -kernelLengthX; offsetX <= kernelLengthX; offsetX++){ \n"
      "    for (int offsetY = -kernelLengthY; offsetY <= kernelLengthY; offsetY++){ \n"
      "      for (int offsetZ = -kernelLengthZ; offsetZ <= kernelLengthZ; offsetZ++){ \n"
      "        vec3 offset = vec3(offsetX, offsetY, offsetZ) * spacing; \n"
      "        float x = offsetX * cy * cz - (offsetY * cy * sz) + offsetZ * sy; \n"
      "        float y = offsetX * (cz * sx * sy + cx * sz) + offsetY * (cx * cz - (sx * sy * sz)) - (offsetZ * cy * sx); \n"
      "        float z = offsetX * (-(cx * cz * sy) + sx * sz) + offsetY * (cz * sx + cx * sy * sz) + offsetZ * cx * cy; \n"
      "        float expA = x * x / StandardDev.x; \n"
      "        float expB = y * y / StandardDev.y; \n"
      "        float expC = z * z / StandardDev.z; \n"
      "        float kernel = exp(-(expA + expB + expC)); \n"
      "        data = data + texture3D(inputTex1, vec3(tcoordVSOutput, zPos) + offset).r * kernel; \n"
      "        sum = sum + kernel; \n"
      "      } \n"
      "    } \n"
      "  } \n"
      "gl_FragData[0] = vec4(data / sum, 1., 1., 1.); \n"
      "}\n";
      }

    // call the helper to execte this code
    this->Helper->Execute(&cb,
      inData[0][0], inArray,
      outData[0], outExt,

      "//VTK::System::Dec\n"
      "attribute vec4 vertexMC;\n"
      "attribute vec2 tcoordMC;\n"
      "varying vec2 tcoordVSOutput;\n"
      "void main() {\n"
      "  tcoordVSOutput = tcoordMC;\n"
      "  gl_Position = vertexMC;\n"
      "}\n",

      fragShader.c_str(),

      "");
    }
}
