/*=auto=========================================================================

Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

See COPYRIGHT.txt
or http://www.slicer.org/copyright/copyright.txt for details.

Program:   3D Slicer
Module:    $RCSfile: vtkMRMLNRRDStorageNode.cxx,v $
Date:      $Date: 2006/03/17 15:10:10 $
Version:   $Revision: 1.6 $

=========================================================================auto=*/

// MRML includes
#include "vtkMRMLFITSStorageNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLVolumeNode.h"
#include "vtkMRMLScalarVolumeNode.h"

//vtkFits includes
#include <vtkFITSReader.h>

// VTK includes
#include <vtkImageChangeInformation.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkStringArray.h>
#include <vtkVersion.h>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLFITSStorageNode);

//----------------------------------------------------------------------------
vtkMRMLFITSStorageNode::vtkMRMLFITSStorageNode()
{
  this->CenterImage = 0;
}

//----------------------------------------------------------------------------
vtkMRMLFITSStorageNode::~vtkMRMLFITSStorageNode()
{
}


//----------------------------------------------------------------------------
void vtkMRMLFITSStorageNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);
  vtkIndent indent(nIndent);

  std::stringstream ss;
  ss << this->CenterImage;
  of << indent << " centerImage=\"" << ss.str() << "\"";

}

//----------------------------------------------------------------------------
void vtkMRMLFITSStorageNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  Superclass::ReadXMLAttributes(atts);

  const char* attName;
  const char* attValue;
  while (*atts != NULL)
    {
    attName = *(atts++);
    attValue = *(atts++);
    if (!strcmp(attName, "centerImage"))
      {
      std::stringstream ss;
      ss << attValue;
      ss >> this->CenterImage;
      }
    }

  this->EndModify(disabledModify);

}

//----------------------------------------------------------------------------
// Copy the node's attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, StorageID
void vtkMRMLFITSStorageNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();

  Superclass::Copy(anode);
  vtkMRMLFITSStorageNode *node = (vtkMRMLFITSStorageNode *) anode;

  this->SetCenterImage(node->CenterImage);

  this->EndModify(disabledModify);

}

//----------------------------------------------------------------------------
void vtkMRMLFITSStorageNode::PrintSelf(ostream& os, vtkIndent indent)
{
  vtkMRMLStorageNode::PrintSelf(os,indent);
  os << indent << "CenterImage:   " << this->CenterImage << "\n";
}

//----------------------------------------------------------------------------
bool vtkMRMLFITSStorageNode::CanReadInReferenceNode(vtkMRMLNode *refNode)
{
  return refNode->IsA("vtkMRMLScalarVolumeNode");
}

//----------------------------------------------------------------------------
int vtkMRMLFITSStorageNode::ReadDataInternal(vtkMRMLNode *refNode)
{

    //cout<<"I am NRRDStorage and flag1 and refNode is  "<<refNode->GetClassName()<<endl;

    vtkMRMLVolumeNode *volNode = NULL;

  if ( refNode->IsA("vtkMRMLScalarVolumeNode") )
    {
    volNode = dynamic_cast <vtkMRMLScalarVolumeNode *> (refNode);
    }
  else if ( refNode->IsA("vtkMRMLVolumeNode") )
    {
    // Generic case used for any VolumeNode. Used when Extensions add
    // node types that are subclasses of VolumeNode.
    volNode = dynamic_cast<vtkMRMLVolumeNode *>(refNode);
    }
  else
    {
    vtkErrorMacro(<< "Do not recognize node type " << refNode->GetClassName());
    return 0;
    }

  vtkNew<vtkFITSReader> reader;

  // Set Reader member variables
  if (this->CenterImage)
    {
    reader->SetUseNativeOriginOff();
    }
  else
    {
    reader->SetUseNativeOriginOn();
    }

  if (volNode->GetImageData())
    {
    volNode->SetAndObserveImageData (NULL);
    }

  std::string fullName = this->GetFullNameFromFileName();

  if (fullName == std::string(""))
    {
    vtkErrorMacro("ReadData: File name not specified");
    return 0;
    }

  reader->SetFileName(fullName.c_str());

  //cout<<"I am NRRDStorage and flag2and refNode is  "<<refNode->GetClassName()<<endl;


  // Check if this is a FITS file that we can read
  if (!reader->CanReadFile(fullName.c_str()))
    {
    vtkErrorMacro("ReadData: This is not a fits file");
    return 0;
    }

  // Read the header to see if the NRRD file corresponds to the
  // MRML Node

  //cout<<"I am NRRDStorage and flag3 and refNode is  "<<refNode->GetClassName()<<endl;
  reader->UpdateInformation();
  //cout<<"I am NRRDStorage and flag4 and refNode is  "<<refNode->GetClassName()<<endl;


  // Check type
  if( refNode->IsA("vtkMRMLScalarVolumeNode") )
    {
      //cout<<"cazzo4 : vtkMRMLScalarVolumeNode"<<endl;
    if (!(reader->GetPointDataType() == vtkDataSetAttributes::SCALARS &&
        (reader->GetNumberOfComponents() == 1 || reader->GetNumberOfComponents()==2 || reader->GetNumberOfComponents()==3) ))
      {
      vtkErrorMacro("ReadData: MRMLVolumeNode does not match file kind");
      return 0;
      }
    }

  //cout<<"I am NRRDStorage and flag5 and refNode is  "<<refNode->GetClassName()<<endl;
  reader->Update();
  //cout<<"I am NRRDStorage and flag6 and refNode is  "<<refNode->GetClassName()<<endl;
  // set volume attributes
  vtkMatrix4x4* mat = reader->GetRasToIjkMatrix();
  volNode->SetRASToIJKMatrix(mat);


  // parse non-specific key-value pairs
  std::vector<std::string> keys = reader->GetHeaderKeysVector();
  for ( std::vector<std::string>::iterator kit = keys.begin();
        kit != keys.end(); ++kit)
    {
    volNode->SetAttribute((*kit).c_str(), reader->GetHeaderValue((*kit).c_str()));
    }

//cout<<"I am NRRDStorage and flag7 and refNode is  "<<refNode->GetClassName()<<endl;
  vtkNew<vtkImageChangeInformation> ici;
#if (VTK_MAJOR_VERSION <= 5)
  ici->SetInput (reader->GetOutput());
#else
  ici->SetInputConnection(reader->GetOutputPort());
#endif
  //check this:
  ici->SetOutputSpacing( 1, 1, 1 );
  ici->SetOutputOrigin( 0, 0, 0 );
  ici->Update();

#if (VTK_MAJOR_VERSION <= 5)
  volNode->SetAndObserveImageData (ici->GetOutput());
#else
  volNode->SetImageDataConnection(ici->GetOutputPort());
#endif
  return 1;
}

//----------------------------------------------------------------------------
int vtkMRMLFITSStorageNode::WriteDataInternal(vtkMRMLNode *refNode)
{
 /* vtkMRMLVolumeNode *volNode = NULL;
  //Store volume nodes attributes.
  vtkNew<vtkMatrix4x4> mf;
  vtkDoubleArray *grads = NULL;
  vtkDoubleArray *bValues = NULL;
  vtkNew<vtkMatrix4x4> ijkToRas;

  if ( refNode->IsA("vtkMRMLDiffusionTensorVolumeNode") )
    {
    volNode = vtkMRMLDiffusionTensorVolumeNode::SafeDownCast(refNode);
    if (volNode)
      {
      ((vtkMRMLDiffusionTensorVolumeNode *) volNode)->GetMeasurementFrameMatrix(mf.GetPointer());
      }
    }
  else if ( refNode->IsA("vtkMRMLDiffusionWeightedVolumeNode") )
    {

    volNode = vtkMRMLDiffusionWeightedVolumeNode::SafeDownCast(refNode);
    if (volNode)
      {
      ((vtkMRMLDiffusionWeightedVolumeNode *) volNode)->GetMeasurementFrameMatrix(mf.GetPointer());
      grads = ((vtkMRMLDiffusionWeightedVolumeNode *) volNode)->GetDiffusionGradients();
      bValues = ((vtkMRMLDiffusionWeightedVolumeNode *) volNode)->GetBValues();
      }
    }
  else if ( refNode->IsA("vtkMRMLVectorVolumeNode") )
    {
    volNode = vtkMRMLVectorVolumeNode::SafeDownCast(refNode);
    if (volNode)
      {
      ((vtkMRMLVectorVolumeNode *) volNode)->GetMeasurementFrameMatrix(mf.GetPointer());
      }
    }
  else if ( refNode->IsA("vtkMRMLScalarVolumeNode") )
    {
    volNode = vtkMRMLScalarVolumeNode::SafeDownCast(refNode);
    }
  else if ( refNode->IsA("vtkMRMLVolumeNode") )
    {
    // Generic case used for any VolumeNode. Used when Extensions add
    // node types that are subclasses of VolumeNode.
    volNode = vtkMRMLVolumeNode::SafeDownCast(refNode);
    }
  else
    {
    vtkErrorMacro(<< "WriteData: Do not recognize node type " << refNode->GetClassName());
    return 0;
    }

  volNode->GetIJKToRASMatrix(ijkToRas.GetPointer());

  if (volNode->GetImageData() == NULL)
    {
    vtkErrorMacro("WriteData: Cannot write NULL ImageData");
    }

  std::string fullName = this->GetFullNameFromFileName();
  if (fullName == std::string(""))
    {
    vtkErrorMacro("WriteData: File name not specified");
    return 0;
    }
  // Use here the NRRD Writer
  vtkNew<vtkNRRDWriter> writer;
  writer->SetFileName(fullName.c_str());
#if (VTK_MAJOR_VERSION <= 5)
  writer->SetInput(volNode->GetImageData() );
#else
  writer->SetInputConnection(volNode->GetImageDataConnection());
#endif
  writer->SetUseCompression(this->GetUseCompression());

  // set volume attributes
  writer->SetIJKToRASMatrix(ijkToRas.GetPointer());
  writer->SetMeasurementFrameMatrix(mf.GetPointer());
  if (grads)
    {
    writer->SetDiffusionGradients(grads);
    }
  if (bValues)
    {
    writer->SetBValues(bValues);
    }

  // pass down all MRML attributes to NRRD
  std::vector<std::string> attributeNames = volNode->GetAttributeNames();
  std::vector<std::string>::iterator ait = attributeNames.begin();
  for (; ait != attributeNames.end(); ++ait)
    {
    writer->SetAttribute((*ait), volNode->GetAttribute((*ait).c_str()));
    }

  writer->Write();
  int writeFlag = 1;
  if (writer->GetWriteError())
    {
    vtkErrorMacro("ERROR writing NRRD file " << (writer->GetFileName() == NULL ? "null" : writer->GetFileName()));
    writeFlag = 0;
    }

  this->StageWriteData(refNode);

  return writeFlag;
  */
}

//----------------------------------------------------------------------------
void vtkMRMLFITSStorageNode::InitializeSupportedReadFileTypes()
{
  this->SupportedReadFileTypes->InsertNextValue("FITS (.fits)");
}

//----------------------------------------------------------------------------
void vtkMRMLFITSStorageNode::InitializeSupportedWriteFileTypes()
{
  this->SupportedWriteFileTypes->InsertNextValue("FITS (.fits)");
}

//----------------------------------------------------------------------------
const char* vtkMRMLFITSStorageNode::GetDefaultWriteFileExtension()
{
  return "fits";
}

//----------------------------------------------------------------------------
void vtkMRMLFITSStorageNode::ConfigureForDataExchange()
{
  this->UseCompressionOff();
}
