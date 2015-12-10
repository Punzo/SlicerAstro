// VTK includes
#include <vtkCommand.h>
#include <vtkObjectFactory.h>

// MRML includes
#include "vtkMRMLVolumeNode.h"

// CropModuleMRML includes
#include "vtkMRMLSmoothingParametersNode.h"

// STD includes
#include <math.h>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLSmoothingParametersNode);

//----------------------------------------------------------------------------
vtkMRMLSmoothingParametersNode::vtkMRMLSmoothingParametersNode()
{
  this->HideFromEditors = 1;

  this->InputVolumeNodeID = NULL;
  this->OutputVolumeNodeID = NULL;
  this->Mode = NULL;
  this->SetMode("Automatic");
  this->SetStatus(0);
  this->SetFilter(0);
  this->SetAccuracy(2);
  this->SetParameterX(1.);
  this->SetParameterY(1.);
  this->SetParameterZ(1.);
  this->gaussianKernelX = vtkSmartPointer<vtkDoubleArray>::New();
  this->gaussianKernelX->SetNumberOfComponents(1);
  this->gaussianKernelX->SetNumberOfTuples((int) (this->GetParameterX() *
                                                  this->GetAccuracy() * 2 + 1));
  this->gaussianKernelY = vtkSmartPointer<vtkDoubleArray>::New();
  this->gaussianKernelY->SetNumberOfComponents(1);
  this->gaussianKernelY->SetNumberOfTuples((int) (this->GetParameterY() *
                                                  this->GetAccuracy() * 2 + 1));
  this->gaussianKernelZ = vtkSmartPointer<vtkDoubleArray>::New();
  this->gaussianKernelZ->SetNumberOfComponents(1);
  this->gaussianKernelZ->SetNumberOfTuples((int) (this->GetParameterZ() *
                                                  this->GetAccuracy() * 2 + 1));
}

//----------------------------------------------------------------------------
vtkMRMLSmoothingParametersNode::~vtkMRMLSmoothingParametersNode()
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

//----------------------------------------------------------------------------
template <typename T> std::string NumberToString(T V)
{
  std::string stringValue;
  std::stringstream strstream;
  strstream << V;
  strstream >> stringValue;
  return stringValue;
}

//----------------------------------------------------------------------------
std::string IntToString(int Value)
{
  return NumberToString<int>(Value);
}

}// end namespace

//----------------------------------------------------------------------------
void vtkMRMLSmoothingParametersNode::ReadXMLAttributes(const char** atts)
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

    if (!strcmp(attName, "Filter"))
      {
      this->Filter = StringToInt(attValue);
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

    this->gaussianKernelX->SetNumberOfTuples((int) (this->GetParameterX() *
                                                    this->GetAccuracy() * 2 + 1));
    int nItems = this->gaussianKernelX->GetNumberOfTuples();
    for (int i = 0; i < nItems; i++)
      {
      std::string temp = "gaussianKernelX[" + IntToString(i) + "]";
      if (!strcmp(attName, temp.c_str()))
        {
        this->gaussianKernelX->SetComponent(i, 0, StringToDouble(attValue));
        }
      continue;
      }

    this->gaussianKernelY->SetNumberOfTuples((int) (this->GetParameterY() *
                                                    this->GetAccuracy() * 2 + 1));
    nItems = this->gaussianKernelY->GetNumberOfTuples();
    for (int i = 0; i < nItems; i++)
      {
      std::string temp = "gaussianKernelY[" + IntToString(i) + "]";
      if (!strcmp(attName, temp.c_str()))
        {
        this->gaussianKernelY->SetComponent(i, 0, StringToDouble(attValue));
        }
      continue;
      }

    this->gaussianKernelZ->SetNumberOfTuples((int) (this->GetParameterZ() *
                                                    this->GetAccuracy() * 2 + 1));
    nItems = this->gaussianKernelZ->GetNumberOfTuples();
    for (int i = 0; i < nItems; i++)
      {
      std::string temp = "gaussianKernelZ[" + IntToString(i) + "]";
      if (!strcmp(attName, temp.c_str()))
        {
        this->gaussianKernelZ->SetComponent(i, 0, StringToDouble(attValue));
        }
      continue;
      }
    }
}

//----------------------------------------------------------------------------
void vtkMRMLSmoothingParametersNode::WriteXML(ostream& of, int nIndent)
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

  of << indent << " Filter=\"" << this->Filter << "\"";
  of << indent << " Status=\"" << this->Status << "\"";
  of << indent << " Accuracy=\"" << this->Accuracy << "\"";
  of << indent << " ParameterX=\"" << this->ParameterX << "\"";
  of << indent << " ParameterY=\"" << this->ParameterY << "\"";
  of << indent << " ParameterZ=\"" << this->ParameterZ << "\"";

  if (this->gaussianKernelX)
    {
    int nItems = this->gaussianKernelX->GetNumberOfTuples();
    for (int i = 0; i < nItems; i++)
      {
      of << indent << "gaussianKernelX["<< i << "]=\"" <<
            this->gaussianKernelX->GetComponent(i, 0) << "\"";
      }
    }

  if (this->gaussianKernelY)
    {
    int nItems = this->gaussianKernelY->GetNumberOfTuples();
    for (int i = 0; i < nItems; i++)
      {
      of << indent << "gaussianKernelY["<< i << "]=\"" <<
            this->gaussianKernelY->GetComponent(i, 0) << "\"";
      }
    }

  if (this->gaussianKernelZ)
    {
    int nItems = this->gaussianKernelZ->GetNumberOfTuples();
    for (int i = 0; i < nItems; i++)
      {
      of << indent << "gaussianKernelZ["<< i << "]=\"" <<
            this->gaussianKernelZ->GetComponent(i, 0) << "\"";
      }
    }
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, SliceID
void vtkMRMLSmoothingParametersNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();
  
  Superclass::Copy(anode);
  vtkMRMLSmoothingParametersNode *node = vtkMRMLSmoothingParametersNode::SafeDownCast(anode);

  this->SetInputVolumeNodeID(node->GetInputVolumeNodeID());
  this->SetOutputVolumeNodeID(node->GetOutputVolumeNodeID());
  this->SetMode(node->GetMode());
  this->SetFilter(node->GetFilter());
  this->SetStatus(node->GetStatus());
  this->SetAccuracy(node->GetAccuracy());
  this->SetParameterX(node->GetParameterX());
  this->SetParameterY(node->GetParameterY());
  this->SetParameterZ(node->GetParameterZ());
  this->SetGaussianKernels();

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
vtkDoubleArray *vtkMRMLSmoothingParametersNode::GetGaussianKernelX()
{
  return this->gaussianKernelX;
}

//----------------------------------------------------------------------------
vtkDoubleArray *vtkMRMLSmoothingParametersNode::GetGaussianKernelY()
{
  return this->gaussianKernelY;
}

//----------------------------------------------------------------------------
vtkDoubleArray *vtkMRMLSmoothingParametersNode::GetGaussianKernelZ()
{
  return this->gaussianKernelZ;
}

//----------------------------------------------------------------------------
inline double gauss1D(double x, double sigma)
{
 return exp(-x * x / (2. * sigma * sigma)) / (sigma * sqrt(2. * M_PI));
};

//----------------------------------------------------------------------------
void vtkMRMLSmoothingParametersNode::SetGaussianKernels()
{
  if (this->gaussianKernelX)
    {
    int nItems = this->gaussianKernelX->GetNumberOfTuples();
    double sum = 0.;
    double midpoint = (nItems - 1) / 2.;
    for (int i = 0; i < nItems; i++)
      {
      double x = i - midpoint;
      double gx = gauss1D(x, this->GetParameterX());
      sum += gx;
      this->gaussianKernelX->SetComponent(i, 0, gx);
      }
    for (int i = 0; i < nItems; i++)
      {
      double gx = this->gaussianKernelX->GetComponent(i, 0);
      this->gaussianKernelX->SetComponent(i, 0, gx / sum);
      }
    }

  if (this->gaussianKernelY)
    {
    int nItems = this->gaussianKernelY->GetNumberOfTuples();
    double sum = 0.;
    double midpoint = (nItems - 1) / 2.;
    for (int i = 0; i < nItems; i++)
      {
      double y = i - midpoint;
      double gy = gauss1D(y, this->GetParameterY());
      sum += gy;
      this->gaussianKernelY->SetComponent(i, 0, gy);
      }
    for (int i = 0; i < nItems; i++)
      {
      double gy = this->gaussianKernelY->GetComponent(i, 0);
      this->gaussianKernelY->SetComponent(i, 0, gy / sum);
      }
    }

  if (this->gaussianKernelZ)
    {
    int nItems = this->gaussianKernelZ->GetNumberOfTuples();
    double sum = 0.;
    double midpoint = (nItems - 1) / 2.;
    for (int i = 0; i < nItems; i++)
      {
      double z = i - midpoint;
      double gz = gauss1D(z, this->GetParameterZ());
      sum += gz;
      this->gaussianKernelZ->SetComponent(i, 0, gz);
      }
    for (int i = 0; i < nItems; i++)
      {
      double gz = this->gaussianKernelZ->GetComponent(i, 0);
      this->gaussianKernelZ->SetComponent(i, 0, gz / sum);
      }
    }
}

//----------------------------------------------------------------------------
void vtkMRMLSmoothingParametersNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << "InputVolumeNodeID: " << ( (this->InputVolumeNodeID) ? this->InputVolumeNodeID : "None" ) << "\n";
  os << "OutputVolumeNodeID: " << ( (this->OutputVolumeNodeID) ? this->OutputVolumeNodeID : "None" ) << "\n";
  os << "Mode: " << ( (this->Mode) ? this->Mode : "None" ) << "\n";
  os << "Filter: " << this->Filter << "\n";
  os << "Status: " << this->Status << "\n";
  os << "Accuracy: " << this->Accuracy << "\n";
  os << "ParameterX: " << this->ParameterX << "\n";
  os << "ParameterY: " << this->ParameterY << "\n";
  os << "ParameterZ: " << this->ParameterZ << "\n";

  if (this->gaussianKernelX)
    {
    int nItems = this->gaussianKernelX->GetNumberOfTuples();
    os << indent << " gaussianKernelX: ";
    for (int i = 0; i < nItems; i++)
      {
      os << indent << this->gaussianKernelX->GetComponent(i, 0) << indent;
      }
    os << indent << "\n";
    }

  if (this->gaussianKernelY)
    {
    int nItems = this->gaussianKernelY->GetNumberOfTuples();
    os << indent << " gaussianKernelY: ";
    for (int i = 0; i < nItems; i++)
      {
      os << indent << this->gaussianKernelY->GetComponent(i, 0) << indent;
      }
    os << indent << "\n";
    }

  if (this->gaussianKernelZ)
    {
    int nItems = this->gaussianKernelZ->GetNumberOfTuples();
    os << indent << " gaussianKernelZ: ";
    for (int i = 0; i < nItems; i++)
      {
      os << indent << this->gaussianKernelZ->GetComponent(i, 0) << indent;
      }
    os << indent << "\n";
    }
}
