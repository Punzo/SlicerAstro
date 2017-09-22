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
#include "vtkAstroOpenGLImageGradient.h"
#include "vtkAstroOpenGLImageAlgorithmHelper.h"

// VTK includes
#include "vtkCellData.h"
#include "vtkDataArray.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkShaderProgram.h"

#include <algorithm> // for std::nth_element

vtkStandardNewMacro(vtkAstroOpenGLImageGradient);

//-----------------------------------------------------------------------------
// Construct an instance of vtkAstroOpenGLImageGradient fitler.
vtkAstroOpenGLImageGradient::vtkAstroOpenGLImageGradient()
{
  // for GPU we do not want threading
  this->NumberOfThreads = 1;
  this->EnableSMP = false;
  this->Dimensionality = 1;
  this->Cl[0] = 5.;
  this->Cl[1] = 5.;
  this->Cl[2] = 5.;
  this->K = 1.5;
  this->Accuracy = 20;
  this->TimeStep = 0.0325;

  this->Helper = vtkSmartPointer<vtkAstroOpenGLImageAlgorithmHelper>::New();
}

//-----------------------------------------------------------------------------
vtkAstroOpenGLImageGradient::~vtkAstroOpenGLImageGradient()
{
}

void vtkAstroOpenGLImageGradient::SetRenderWindow(vtkRenderWindow *renWin)
{
  this->Helper->SetRenderWindow(renWin);
}

//-----------------------------------------------------------------------------
void vtkAstroOpenGLImageGradient::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Helper: ";
  this->Helper->PrintSelf(os, indent);
}

// this is used as a callback by the helper to set shader parameters
// before running and to update them on each slice
class vtkOpenGLGradientCB : public vtkOpenGLImageAlgorithmCallback
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
    float cl[3];
    cl[0] = this->Cl[0];
    cl[1] = this->Cl[1];
    cl[2] = this->Cl[2];
    program->SetUniform3f("Cl", cl);
    program->SetUniformf("TimeStep", this->TimeStep);
    program->SetUniformf("norm", this->norm);
    }

  // no uniforms change on a per slice basis so empty
  virtual void UpdateShaderUniforms(
    vtkShaderProgram * /* program */, int /* zExtent */) {};

  double *Spacing;
  double *Cl;
  double TimeStep, norm;
};

//-----------------------------------------------------------------------------
// This method contains the first switch statement that calls the correct
// templated function for the input and output region types.
void vtkAstroOpenGLImageGradient::ThreadedRequestData(
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

  vtkOpenGLGradientCB cb;
  cb.Spacing = inData[0][0]->GetSpacing();
  int * extent = inData[0][0]->GetExtent();
  cb.Spacing[0] /= extent[1] - extent[0];
  cb.Spacing[1] /= extent[3] - extent[2];
  cb.Spacing[2] /= extent[5] - extent[4];
  cb.Cl = this->Cl;
  cb.norm = this->K * this->K * this->RMS * this->RMS;
  cb.TimeStep = this->TimeStep;

  std::string fragShader =
  "//VTK::System::Dec\n"
  "varying vec2 tcoordVSOutput;\n"
  "uniform sampler3D inputTex1;\n"
  "uniform float zPos;\n"
  "uniform vec3 spacing;\n"
  "uniform vec3 Cl;\n"
  "uniform float norm;\n"
  "uniform float TimeStep;\n"
  "//VTK::Output::Dec\n"
  "void main(void) {\n"
  "vec3 samplePoint = vec3(tcoordVSOutput, zPos);\n"
  "float sample = texture3D(inputTex1, samplePoint).r;\n"
  "float sample2 = sample * sample;\n"
  "float Norm = 1. + (sample2 / norm);\n"
  "vec3 diff = vec3(0.);"
  "vec3 offsetA1 = spacing * vec3(-1., 0., 0.);\n"
  "vec3 offsetB1 = spacing * vec3(+1., 0., 0.);\n"
  "float sampleA1 = texture3D(inputTex1, samplePoint + offsetA1).r;\n"
  "float sampleB1 = texture3D(inputTex1, samplePoint + offsetB1).r;\n"
  "float diffX = ((sampleA1 - sample) + (sampleB1 - sample)) * Cl.x;\n"
  "vec3 offsetA2 = spacing * vec3(0., -1., 0.);\n"
  "vec3 offsetB2 = spacing * vec3(0., +1., 0.);\n"
  "float sampleA2 = texture3D(inputTex1, samplePoint + offsetA2).r;\n"
  "float sampleB2 = texture3D(inputTex1, samplePoint + offsetB2).r;\n"
  "float diffY = ((sampleA2 - sample) + (sampleB2 - sample)) * Cl.y;\n"
  "vec3 offsetA3 = spacing * vec3(0., 0., -1.);\n"
  "vec3 offsetB3 = spacing * vec3(0., 0., +1.);\n"
  "float sampleA3 = texture3D(inputTex1, samplePoint + offsetA3).r;\n"
  "float sampleB3 = texture3D(inputTex1, samplePoint + offsetB3).r;\n"
  "float diffZ = ((sampleA3 - sample) + (sampleB3 - sample)) * Cl.z;\n"
  "float data = sample + (TimeStep * (diffX + diffY + diffZ) / Norm);\n"
  "gl_FragData[0] = vec4(data, 1., 1., 1.); \n"
  "}\n";

  // call the helper to execute this code
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

    "",
    this->Accuracy);
}
