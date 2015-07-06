
// MRML includes
#include "vtkMRMLAstroVolumeDisplayNode.h"

// VTK includes
#include <vtkAlgorithmOutput.h>
#include <vtkImageAppendComponents.h>
#include <vtkImageData.h>
#include <vtkImageExtractComponents.h>
#include <vtkImageMapToWindowLevelColors.h>
#include <vtkImageThreshold.h>
#include <vtkObjectFactory.h>

// STD includes
#include <sstream>

//------------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroVolumeDisplayNode);

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode::vtkMRMLAstroVolumeDisplayNode()
{

}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode::~vtkMRMLAstroVolumeDisplayNode()
{
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);

  std::stringstream ss;
  //ss << this->;
  //of << indent << " =\"" << ss.str() << "\"";
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  Superclass::ReadXMLAttributes(atts);
  /*
  const char* attName;
  const char* attValue;
  while (*atts != NULL) 
    {
    attName = *(atts++);
    attValue = *(atts++);
    if (!strcmp(attName, ""))
      {
      std::stringstream ss;
      ss << attValue;
      ss >> this->;
      }
    }*/
  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, VolumeID
void vtkMRMLAstroVolumeDisplayNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();

  Superclass::Copy(anode);
  //vtkMRMLAstroVolumeDisplayNode *node = (vtkMRMLAstroVolumeDisplayNode *) anode;

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::PrintSelf(ostream& os, vtkIndent indent)
{
  
  Superclass::PrintSelf(os,indent);

  //os << indent << ":   " << this-> << "\n";

}
