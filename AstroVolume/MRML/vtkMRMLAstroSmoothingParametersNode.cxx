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

// VTK includes
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkObjectFactory.h>

// MRML includes
#include <vtkMRMLVolumeNode.h>

// CropModuleMRML includes
#include <vtkMRMLAstroSmoothingParametersNode.h>

// STD includes
#include <math.h>

#define SigmatoFWHM 2.3548200450309493

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroSmoothingParametersNode);

//----------------------------------------------------------------------------
vtkMRMLAstroSmoothingParametersNode::vtkMRMLAstroSmoothingParametersNode()
{
  this->HideFromEditors = 1;

  this->InputVolumeNodeID = NULL;
  this->OutputVolumeNodeID = NULL;
  this->Mode = NULL;
  this->MasksCommand = NULL;
  this->OutputSerial = 1;
  this->SetMode("Automatic");
  this->SetMasksCommand("Skip");
  this->SetStatus(0);
  this->SetFilter(2);
  this->SetHardware(0);
  this->SetCores(0);
  this->SetLink(false);
  this->SetAutoRun(false);
  this->SetAccuracy(20);
  this->SetTimeStep(0.0325);
  this->SetK(2);
  this->SetParameterX(5);
  this->SetParameterY(5);
  this->SetParameterZ(5);
  this->SetRx(0);
  this->SetRy(0);
  this->SetRz(0);
  this->gaussianKernel3D = vtkSmartPointer<vtkDoubleArray>::New();
  this->gaussianKernel3D->SetNumberOfComponents(1);
  this->gaussianKernel1D = vtkSmartPointer<vtkDoubleArray>::New();
  this->gaussianKernel1D->SetNumberOfComponents(1);
  this->SetKernelLengthX(0);
  this->SetKernelLengthY(0);
  this->SetKernelLengthZ(0);
  this->DegToRad = atan(1.) / 45.;
}

//----------------------------------------------------------------------------
vtkMRMLAstroSmoothingParametersNode::~vtkMRMLAstroSmoothingParametersNode()
{
  if (this->InputVolumeNodeID)
    {
    delete [] this->InputVolumeNodeID;
    this->InputVolumeNodeID = NULL;
    }

  if (this->OutputVolumeNodeID)
    {
    delete [] this->OutputVolumeNodeID;
    this->OutputVolumeNodeID = NULL;
    }

  if (this->Mode)
    {
    delete [] this->Mode;
    this->Mode = NULL;
    }

  if (this->MasksCommand)
    {
    delete [] this->MasksCommand;
    this->MasksCommand = NULL;
    }
}

namespace
{
//----------------------------------------------------------------------------
template <typename T> T StringToNumber(const char* num)
{
  std::stringstream ss;
  ss << num;
  T result;
  return ss >> result ? result : 0;
}

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

//----------------------------------------------------------------------------
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
}

}// end namespace

//----------------------------------------------------------------------------
void vtkMRMLAstroSmoothingParametersNode::ReadXMLAttributes(const char** atts)
{
  Superclass::ReadXMLAttributes(atts);

  const char* attName;
  const char* attValue;
  while (*atts != NULL)
    {
    attName = *(atts++);
    attValue = *(atts++);
    if (!strcmp(attName, "inputVolumeNodeID"))
      {
      this->SetInputVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "outputVolumeNodeID"))
      {
      this->SetOutputVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "Mode"))
      {
      this->SetMode(attValue);
      continue;
      }

    if (!strcmp(attName, "MasksCommand"))
      {
      this->SetMasksCommand(attValue);
      continue;
      }

    if (!strcmp(attName, "OutputSerial"))
      {
      this->OutputSerial = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Filter"))
      {
      this->Filter = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Hardware"))
      {
      this->Hardware = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Cores"))
      {
      this->Cores = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Link"))
      {
      this->Link = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "AutoRun"))
      {
      this->AutoRun = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Rx"))
      {
      this->Rx = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Ry"))
      {
      this->Ry = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Rz"))
      {
      this->Rz = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Status"))
      {
      this->Status = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Accuracy"))
      {
      this->Accuracy = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "TimeStep"))
      {
      this->TimeStep = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "K"))
      {
      this->K = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "ParameterX"))
      {
      this->ParameterX = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "ParameterY"))
      {
      this->ParameterY = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "ParameterZ"))
      {
      this->ParameterZ = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "KernelLengthX"))
      {
      this->KernelLengthX = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "KernelLengthY"))
      {
      this->KernelLengthY = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "KernelLengthZ"))
      {
      this->KernelLengthZ = StringToInt(attValue);
      continue;
      }
    }
  this->SetGaussianKernels();
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSmoothingParametersNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);

  if (this->InputVolumeNodeID != NULL)
    {
    of << indent << " inputVolumeNodeID=\"" << this->InputVolumeNodeID << "\"";
    }

  if (this->OutputVolumeNodeID != NULL)
    {
    of << indent << " outputVolumeNodeID=\"" << this->OutputVolumeNodeID << "\"";
    }

  if (this->Mode != NULL)
    {
    of << indent << " Mode=\"" << this->Mode << "\"";
    }

  if (this->MasksCommand != NULL)
    {
    of << indent << " MasksCommand=\"" << this->MasksCommand << "\"";
    }

  of << indent << " OutputSerial=\"" << this->OutputSerial << "\"";
  of << indent << " Filter=\"" << this->Filter << "\"";
  of << indent << " Hardware=\"" << this->Hardware << "\"";
  of << indent << " Cores=\"" << this->Cores << "\"";
  of << indent << " Link=\"" << this->Link << "\"";
  of << indent << " AutoRun=\"" << this->AutoRun << "\"";
  of << indent << " Rx=\"" << this->Rx << "\"";
  of << indent << " Ry=\"" << this->Ry << "\"";
  of << indent << " Rz=\"" << this->Rz << "\"";
  of << indent << " Status=\"" << this->Status << "\"";
  of << indent << " Accuracy=\"" << this->Accuracy << "\"";
  of << indent << " TimeStep=\"" << this->TimeStep << "\"";
  of << indent << " K=\"" << this->K << "\"";
  of << indent << " ParameterX=\"" << this->ParameterX << "\"";
  of << indent << " ParameterY=\"" << this->ParameterY << "\"";
  of << indent << " ParameterZ=\"" << this->ParameterZ << "\"";
  of << indent << " KernelLengthX=\"" << this->KernelLengthX << "\"";
  of << indent << " KernelLengthY=\"" << this->KernelLengthY << "\"";
  of << indent << " KernelLengthZ=\"" << this->KernelLengthZ << "\"";
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, SliceID
void vtkMRMLAstroSmoothingParametersNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();
  
  Superclass::Copy(anode);
  vtkMRMLAstroSmoothingParametersNode *node = vtkMRMLAstroSmoothingParametersNode::SafeDownCast(anode);

  this->SetInputVolumeNodeID(node->GetInputVolumeNodeID());
  this->SetOutputVolumeNodeID(node->GetOutputVolumeNodeID());
  this->SetMode(node->GetMode());
  this->SetMasksCommand(node->GetMasksCommand());
  this->SetOutputSerial(node->GetOutputSerial());
  this->SetFilter(node->GetFilter());
  this->SetHardware(node->GetHardware());
  this->SetCores(node->GetCores());
  this->SetLink(node->GetLink());
  this->SetAutoRun(node->GetAutoRun());
  this->SetRx(node->GetRx());
  this->SetRy(node->GetRy());
  this->SetRz(node->GetRz());
  this->SetStatus(node->GetStatus());
  this->SetAccuracy(node->GetAccuracy());
  this->SetK(node->GetK());
  this->SetTimeStep(node->GetTimeStep());
  this->SetParameterX(node->GetParameterX());
  this->SetParameterY(node->GetParameterY());
  this->SetParameterZ(node->GetParameterZ());
  this->SetKernelLengthX(node->GetKernelLengthX());
  this->SetKernelLengthY(node->GetKernelLengthY());
  this->SetKernelLengthZ(node->GetKernelLengthZ());
  this->SetGaussianKernels();

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
vtkDoubleArray *vtkMRMLAstroSmoothingParametersNode::GetGaussianKernel1D()
{
  return this->gaussianKernel1D;
}

//----------------------------------------------------------------------------
vtkDoubleArray *vtkMRMLAstroSmoothingParametersNode::GetGaussianKernel3D()
{
  return this->gaussianKernel3D;
}

//----------------------------------------------------------------------------
inline double gauss1D(double x, double sigma)
{
 return exp(-x * x / (2. * sigma * sigma));
};

//----------------------------------------------------------------------------
inline double gauss3D(double x, double sigmax,
                      double y, double sigmay,
                      double z, double sigmaz)
{
 return exp(-((x * x / (2. * sigmax * sigmax)) +
           (y * y / (2. * sigmay * sigmay)) +
           (z * z / (2. * sigmaz * sigmaz))));
};


//----------------------------------------------------------------------------
void vtkMRMLAstroSmoothingParametersNode::SetGaussianKernel1D()
{
  if(this->GetFilter() != 1)
    {
    this->gaussianKernel1D = NULL;
    return;
    }

  if (this->gaussianKernel1D)
    {
    int nItems = (int) ((this->GetParameterX() / SigmatoFWHM) * this->GetAccuracy());
    if (nItems % 2 < 0.001 && nItems > 0.001)
      {
      nItems++;
      }
    this->SetKernelLengthX(nItems);
    this->SetKernelLengthY(nItems);
    this->SetKernelLengthZ(nItems);
    this->gaussianKernel1D->SetNumberOfTuples(nItems);

    if(nItems == 1)
      {
      this->gaussianKernel1D->SetComponent(0, 0, 1.);
      }
    else
      {
      double sigmax = this->GetParameterX() / SigmatoFWHM;
      if(sigmax < 0.001)
        {
        sigmax = 0.001;
        }

      double midpoint = (nItems - 1) / 2.;
      double sumTotal = 0;
      for (int i = 0; i < nItems; i++)
        {
        double x = i - midpoint;
        double gx = gauss1D(x, sigmax);
        sumTotal += gx;
        this->gaussianKernel1D->SetComponent(i, 0, gx);
        }

      for (int i = 0; i < nItems; i++)
        {
        this->gaussianKernel1D->SetComponent(i, 0,
          this->gaussianKernel1D->GetComponent(i, 0) / sumTotal);
        }
      }
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSmoothingParametersNode::SetGaussianKernel3D()
{
  if(this->GetFilter() != 1)
    {
    this->gaussianKernel3D = NULL;
    return;
    }

  if (this->gaussianKernel3D)
    {
    int nItemsX = (int) ((this->GetParameterX() / SigmatoFWHM) * this->GetAccuracy());
    if (nItemsX % 2 < 0.001)
      {
      nItemsX++;
      }
    this->SetKernelLengthX(nItemsX);
    int nItemsY = (int) ((this->GetParameterY() / SigmatoFWHM) * this->GetAccuracy());
    if (nItemsY % 2 < 0.001)
      {
      nItemsY++;
      }
    this->SetKernelLengthY(nItemsY);
    int nItemsZ = (int) ((this->GetParameterZ() / SigmatoFWHM) * this->GetAccuracy());
    if (nItemsZ % 2 < 0.001)
      {
      nItemsZ++;
      }
    this->SetKernelLengthZ(nItemsZ);

    int nItems = nItemsX * nItemsY * nItemsZ;

    double rx = this->Rx * this->DegToRad;
    double ry = this->Ry * this->DegToRad;
    double rz = this->Rz * this->DegToRad;

    double cx = cos(rx);
    double sx = sin(rx);
    double cy = cos(ry);
    double sy = sin(ry);
    double cz = cos(rz);
    double sz = sin(rz);

    this->gaussianKernel3D->SetNumberOfTuples(nItems);

    int Xmax = (int) (nItemsX - 1) / 2.;
    int Ymax = (int) (nItemsY - 1) / 2.;
    int Zmax = (int) (nItemsZ - 1) / 2.;

    double sigmax = this->GetParameterX() / SigmatoFWHM;
    if(sigmax < 0.001)
      {
      sigmax = 0.001;
      }
    double sigmay = this->GetParameterY() / SigmatoFWHM;
    if(sigmay < 0.001)
      {
      sigmay = 0.001;
      }
    double sigmaz = this->GetParameterZ() / SigmatoFWHM;
    if(sigmaz < 0.001)
      {
      sigmaz = 0.001;
      }

    double sumTotal = 0;
    for (int k = -Zmax; k <= Zmax; k++)
      {
      for (int j = -Ymax; j <= Ymax; j++)
        {
        for (int i = -Xmax; i <= Xmax; i++)
          {
          int pos = (k + Zmax) * nItemsX * nItemsY + (j + Ymax) * nItemsX + (i + Xmax);
          double x = i * cy * cz - j * cy * sz + k * sy;
          double y = i * (cz * sx * sy + cx * sz) + j * (cx * cz - sx * sy * sz) - k * cy * sx;
          double z = i * (-cx * cz * sy + sx * sz) + j * (cz * sx + cx * sy * sz) + k * cx * cy;
          double g = gauss3D(x, sigmax,
                             y, sigmay,
                             z, sigmaz);
          sumTotal += g;
          this->gaussianKernel3D->SetComponent(pos, 0, g);
          }
        }
      }

    for (int k = -Zmax; k <= Zmax; k++)
      {
      for (int j = -Ymax; j <= Ymax; j++)
        {
        for (int i = -Xmax; i <= Xmax; i++)
          {
          int pos = (k + Zmax) * nItemsX * nItemsY + (j + Ymax) * nItemsX + (i + Xmax);
          this->gaussianKernel3D->SetComponent(pos, 0,
            this->gaussianKernel3D->GetComponent(pos, 0) / sumTotal);
          }
        }
      }
    }
}


//----------------------------------------------------------------------------
void vtkMRMLAstroSmoothingParametersNode::SetGaussianKernels()
{
  if (this->gaussianKernel3D)
    {
    this->gaussianKernel3D->Initialize();
    }

  if (this->gaussianKernel1D)
    {
    this->gaussianKernel1D->Initialize();
    }

  if(fabs(this->ParameterX - this->ParameterY) < 0.001 &&
     fabs(this->ParameterY - this->ParameterZ) < 0.001)
    {
    this->SetGaussianKernel1D();
    }
  else
    {
    this->SetGaussianKernel3D();
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSmoothingParametersNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << "InputVolumeNodeID: " << ( (this->InputVolumeNodeID) ? this->InputVolumeNodeID : "None" ) << "\n";
  os << "OutputVolumeNodeID: " << ( (this->OutputVolumeNodeID) ? this->OutputVolumeNodeID : "None" ) << "\n";
  os << "Mode: " << ( (this->Mode) ? this->Mode : "None" ) << "\n";
  os << "MasksCommand: " << ( (this->MasksCommand) ? this->MasksCommand : "None" ) << "\n";
  os << "OutputSerial: " << this->OutputSerial << "\n";
  os << "Status: " << this->Status << "\n";

  switch (this->Filter)
    {
    case 0:
      {
      os << "Filter: Box\n";
      break;
      }
    case 1:
      {
      os << "Filter: Gaussian\n";
      break;
      }
    case 2:
      {
      os << "Filter: Intensity Driven Gradient\n";
      break;
      }
    }

  switch (this->Hardware)
    {
    case 0:
      {
      os << "Hardware: CPU\n";
      if (this->Cores != 0)
        {
        os << "Number of cores: "<< this->Cores<< "\n";
        }
      break;
      }
    case 1:
      {
      os << "Hardware: GPU\n";
      break;
      }
    }

  if(this->AutoRun)
    {
    os << "AutoRun: Active\n";
    }
  else
    {
    os << "AutoRun: Inactive\n";
    }

  if(this->Link)
    {
    os << "Link: Active\n";
    }
  else
    {
    os << "Link: Inactive\n";
    }

  os << "ParameterX: " << this->ParameterX << "\n";
  os << "ParameterY: " << this->ParameterY << "\n";
  os << "ParameterZ: " << this->ParameterZ << "\n";

  if (this->Filter < 2)
    {
    os << "KernelLengthX: " << this->KernelLengthX << "\n";
    os << "KernelLengthY: " << this->KernelLengthY << "\n";
    os << "KernelLengthZ: " << this->KernelLengthZ << "\n";
    }

  if (this->Filter == 1)
    {
    os << "Kernel rotation with respect to X: " << this->Rx << "\n";
    os << "Kernel rotation with respect to Y: " << this->Ry << "\n";
    os << "Kernel rotation with respect to Z: " << this->Rz << "\n";
    }

  if (this->Filter != 0)
    {
    os << "Accuracy: " << this->Accuracy << "\n";
    }

  if (this->Filter == 2)
    {
    os << "TimeStep: " << this->TimeStep << "\n";
    os << "K: " << this->K << "\n";
    }

  if (this->gaussianKernel1D)
    {
    int nItems = this->gaussianKernel1D->GetNumberOfTuples();
    if(nItems > 0)
      {
      os << indent << "GaussianKernel1D: ";
      for (int i = 0; i < nItems; i++)
        {
        os << indent << this->gaussianKernel1D->GetComponent(i, 0) << " " << indent;
        }
      os << indent << "\n";
      }
    }

  if (this->gaussianKernel3D)
    {
    if (this->gaussianKernel3D->GetNumberOfTuples() > 0)
      {
      int Xmax = (int) (this->KernelLengthX - 1) / 2.;
      int Ymax = (int) (this->KernelLengthY - 1) / 2.;
      int Zmax = (int) (this->KernelLengthZ - 1) / 2.;

      os << indent <<"kernel 3d :"<<"\n";
      for (int k = -Zmax; k <= Zmax; k++)
        {
        os << indent <<"slice : "<<k<<"\n";
        for (int j = -Ymax; j <= Ymax; j++)
          {
          for (int i = -Xmax; i <= Xmax; i++)
            {
            int pos = (k + Zmax) * this->KernelLengthX * this->KernelLengthY
                    + (j + Ymax) * this->KernelLengthX + (i + Xmax);
            os << indent <<this->gaussianKernel3D->GetComponent(pos, 0)<<"  ";
            }
          os << indent <<"\n";
          }
        os << indent <<"\n";
        }
      }
    }
}
