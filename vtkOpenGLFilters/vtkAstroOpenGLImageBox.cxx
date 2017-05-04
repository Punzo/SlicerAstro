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

// vtkOpenGLFilters includes
#include "vtkAstroOpenGLImageBox.h"
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

vtkStandardNewMacro(vtkAstroOpenGLImageBox);

//-----------------------------------------------------------------------------
// Construct an instance of vtkAstroOpenGLImageBox fitler.
vtkAstroOpenGLImageBox::vtkAstroOpenGLImageBox()
{
  // for GPU we do not want threading
  this->NumberOfThreads = 1;
  this->EnableSMP = false;
  this->Dimensionality = 1;
  this->KernelLength[0] = 5;
  this->KernelLength[1] = 5;
  this->KernelLength[2] = 5;
  this->Iterative = true;
  this->Helper = vtkSmartPointer<vtkAstroOpenGLImageAlgorithmHelper>::New();
}

//-----------------------------------------------------------------------------
vtkAstroOpenGLImageBox::~vtkAstroOpenGLImageBox()
{
}

void vtkAstroOpenGLImageBox::SetRenderWindow(vtkRenderWindow *renWin)
{
  this->Helper->SetRenderWindow(renWin);
}

//-----------------------------------------------------------------------------
void vtkAstroOpenGLImageBox::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Helper: ";
  this->Helper->PrintSelf(os, indent);
}

// this is used as a callback by the helper to set shader parameters
// before running and to update them on each slice
class vtkOpenGLBoxCB : public vtkOpenGLImageAlgorithmCallback
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
    program->SetUniformi("cont", cont);
    }

  // no uniforms change on a per slice basis so empty
  virtual void UpdateShaderUniforms(
    vtkShaderProgram * /* program */, int /* zExtent */) {};

  double *Spacing;
  int *KernelLength;
  int cont;
};

//-----------------------------------------------------------------------------
// This method contains the first switch statement that calls the correct
// templated function for the input and output region types.
void vtkAstroOpenGLImageBox::ThreadedRequestData(
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

  vtkOpenGLBoxCB cb;
  cb.Spacing = inData[0][0]->GetSpacing();
  int * extent = inData[0][0]->GetExtent();
  cb.Spacing[0] /= extent[1] - extent[0];
  cb.Spacing[1] /= extent[3] - extent[2];
  cb.Spacing[2] /= extent[5] - extent[4];

  cb.cont = 1;
  if (this->KernelLength[0] < 0.999)
    {
    this->KernelLength[0]++;
    }
  if (this->KernelLength[0] % 2 < 0.001)
    {
    this->KernelLength[0]++;
    }
  cb.cont *= this->KernelLength[0];
  this->KernelLength[0] = (int) (this->KernelLength[0] - 1.) / 2.;

  if (this->KernelLength[1] < 0.999)
    {
    this->KernelLength[1]++;
    }
  if (this->KernelLength[1] % 2 < 0.001)
    {
    this->KernelLength[1]++;
    }
  cb.cont *= this->KernelLength[1];
  int norm1 = this->KernelLength[1];
  this->KernelLength[1] = (int) (this->KernelLength[1] - 1.) / 2.;

  if (this->KernelLength[2] < 0.999)
    {
    this->KernelLength[2]++;
    }
  if (this->KernelLength[2] % 2 < 0.001)
    {
    this->KernelLength[2]++;
    }
  cb.cont *= this->KernelLength[2];
  int norm2 = this->KernelLength[2];
  this->KernelLength[2] = (int) (this->KernelLength[2] - 1.) / 2.;

  cb.KernelLength = this->KernelLength;

  if (fabs(this->KernelLength[0] - this->KernelLength[1]) < 0.001 &&
      fabs(this->KernelLength[1] - this->KernelLength[2]) < 0.001 &&
      this->Iterative)
    {
    cb.cont /= norm1 * norm2;
    std::string fragShaderBegin =
    "//VTK::System::Dec\n"
    "varying vec2 tcoordVSOutput;\n"
    "uniform sampler3D inputTex1;\n"
    "uniform float zPos;\n"
    "uniform vec3 spacing;\n"
    "uniform int kernelLengthX;\n"
    "uniform int kernelLengthY;\n"
    "uniform int kernelLengthZ;\n"
    "uniform int cont;\n"
    "//VTK::Output::Dec\n"
    "void main(void) {\n"
    "float data = 0.; \n";

    std::string fragShaderEnd =
    "gl_FragData[0] = vec4(data / cont, 1., 1., 1.); \n"
    "}\n";

    std::string fragShaderX =
    "  for (int offsetX = -kernelLengthX; offsetX <= kernelLengthX; offsetX++){ \n"
    "        vec3 offset = vec3(offsetX, 0., 0.) * spacing; \n"
    "        data = data + texture3D(inputTex1, vec3(tcoordVSOutput, zPos) + offset).r; \n"
    "  } \n";

    std::string fragShaderY =
    "  for (int offsetY = -kernelLengthY; offsetY <= kernelLengthY; offsetY++){ \n"
    "        vec3 offset = vec3(0., offsetY, 0.) * spacing; \n"
    "        data = data + texture3D(inputTex1, vec3(tcoordVSOutput, zPos) + offset).r; \n"
    "  } \n";

    std::string fragShaderZ =
    "  for (int offsetZ = -kernelLengthZ; offsetZ <= kernelLengthZ; offsetZ++){ \n"
    "        vec3 offset = vec3(0., 0., offsetZ) * spacing; \n"
    "        data = data + texture3D(inputTex1, vec3(tcoordVSOutput, zPos) + offset).r; \n"
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
    std::string fragShader =
    "//VTK::System::Dec\n"
    "varying vec2 tcoordVSOutput;\n"
    "uniform sampler3D inputTex1;\n"
    "uniform float zPos;\n"
    "uniform vec3 spacing;\n"
    "uniform int kernelLengthX;\n"
    "uniform int kernelLengthY;\n"
    "uniform int kernelLengthZ;\n"
    "uniform int cont;\n"
    "//VTK::Output::Dec\n"
    "void main(void) {\n"
    "float data = 0.; \n"
    "  for (int offsetX = -kernelLengthX; offsetX <= kernelLengthX; offsetX++){ \n"
    "    for (int offsetY = -kernelLengthY; offsetY <= kernelLengthY; offsetY++){ \n"
    "      for (int offsetZ = -kernelLengthZ; offsetZ <= kernelLengthZ; offsetZ++){ \n"
    "        vec3 offset = vec3(offsetX, offsetY, offsetZ) * spacing; \n"
    "        data = data + texture3D(inputTex1, vec3(tcoordVSOutput, zPos) + offset).r; \n"
    "      } \n"
    "    } \n"
    "  } \n"
    "gl_FragData[0] = vec4(data / cont, 1., 1., 1.); \n"
    "}\n";

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
