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

// MRML includes
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLVolumeNode.h>

//vtkFits includes
#include <vtkAstroLogMessages.h>
#include <vtkFITSReader.h>
#include <vtkFITSWriter.h>

// VTK includes
#include <vtkDataSetAttributes.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkType.h>

// STD includes
#include <cstring>
#include <map>
#include <string>
#include <vector>


//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroVolumeStorageNode);

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeStorageNode::vtkMRMLAstroVolumeStorageNode()
{
  this->CenterImage = 2;
  this->DefaultWriteFileExtension = "fits";
  this->UseCompression = 0;
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeStorageNode::~vtkMRMLAstroVolumeStorageNode()
{
}

namespace
{
//----------------------------------------------------------------------------
// The reader converts the spectral axis (e.g. FREQ -> VOPT-F2W) and adds
// missing keywords (e.g. RESTFREQ of HI). The keywords of the file and the
// converted ones are kept in node attributes (not starting with SlicerAstro.,
// so that they are not written as FITS keywords): if the spectral axis of the
// volume is unchanged when it is written, the keywords of the file are
// written back.
const char* OriginalKeywordPrefix = "FITSOriginal.";
const char* ConvertedKeywordPrefix = "FITSConverted.";

void StoreSpectralAxisKeywordsImpl(vtkMRMLNode* node, const std::string& fileName)
{
  const std::vector<std::string> keys = vtkFITSReader::GetSpectralAxisKeywords();
  const std::map<std::string, std::string> original = vtkFITSReader::ReadHeaderKeywords(fileName.c_str(), keys);
  for (const std::string& key : keys)
    {
    const std::map<std::string, std::string>::const_iterator originalValue = original.find(key);
    node->SetAttribute((OriginalKeywordPrefix + key).c_str(),
                       originalValue != original.end() ? originalValue->second.c_str() : nullptr);
    node->SetAttribute((ConvertedKeywordPrefix + key).c_str(),
                       node->GetAttribute(("SlicerAstro." + key).c_str()));
    }
}

void RestoreSpectralAxisKeywordsImpl(vtkMRMLNode* node, vtkFITSWriter* writer)
{
  const std::vector<std::string> keys = vtkFITSReader::GetSpectralAxisKeywords();
  bool stored = false;
  for (const std::string& key : keys)
    {
    const char* converted = node->GetAttribute((ConvertedKeywordPrefix + key).c_str());
    const char* current = node->GetAttribute(("SlicerAstro." + key).c_str());
    stored = stored || converted || node->GetAttribute((OriginalKeywordPrefix + key).c_str());
    if ((converted == nullptr) != (current == nullptr) || (converted && strcmp(converted, current) != 0))
      {
      // the spectral axis was modified (e.g. by a module): written as it is
      return;
      }
    }
  if (!stored)
    {
    return;
    }
  for (const std::string& key : keys)
    {
    const char* original = node->GetAttribute((OriginalKeywordPrefix + key).c_str());
    // the writer skips the UNDEFINED keywords (not in the file)
    writer->SetAttribute("SlicerAstro." + key, original ? original : "UNDEFINED");
    }
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
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
}

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
}// end namespace


//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeStorageNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);
  vtkIndent indent(nIndent);

  std::stringstream ss;
  ss << this->CenterImage;
  of << indent << " centerImage=\"" << ss.str() << "\"";
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeStorageNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  Superclass::ReadXMLAttributes(atts);

  const char* attName;
  const char* attValue;
  while (*atts != nullptr)
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
void vtkMRMLAstroVolumeStorageNode::Copy(vtkMRMLNode *anode)
{
  if (!anode)
    {
    return;
    }

  int disabledModify = this->StartModify();

  Superclass::Copy(anode);
  vtkMRMLAstroVolumeStorageNode *node = (vtkMRMLAstroVolumeStorageNode *) anode;

  this->SetCenterImage(node->CenterImage);

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeStorageNode::PrintSelf(ostream& os, vtkIndent indent)
{
  vtkMRMLStorageNode::PrintSelf(os,indent);
  os << indent << "CenterImage:   " << this->CenterImage << "\n";
}

//----------------------------------------------------------------------------
bool vtkMRMLAstroVolumeStorageNode::CanReadInReferenceNode(vtkMRMLNode *refNode)
{
  return refNode->IsA("vtkMRMLAstroVolumeNode") ||
         refNode->IsA("vtkMRMLAstroLabelMapVolumeNode");
}

//----------------------------------------------------------------------------
int vtkMRMLAstroVolumeStorageNode::ReadDataInternal(vtkMRMLNode *refNode)
{
  vtkMRMLAstroVolumeNode *volNode = nullptr;
  vtkMRMLAstroLabelMapVolumeNode *labvolNode = nullptr;
  vtkMRMLAstroVolumeDisplayNode *disNode = nullptr;
  vtkMRMLAstroLabelMapVolumeDisplayNode *labdisNode = nullptr;

  if (refNode->IsA("vtkMRMLAstroVolumeNode"))
    {
    volNode = vtkMRMLAstroVolumeNode::SafeDownCast(refNode);
    disNode = volNode->GetAstroVolumeDisplayNode();
    }
  else if (refNode->IsA("vtkMRMLAstroLabelMapVolumeNode"))
    {
    labvolNode = vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(refNode);
    labdisNode = labvolNode->GetAstroLabelMapVolumeDisplayNode();
    }
  else
    {
    vtkErrorMacro(<< "vtkMRMLAstroVolumeStorageNode::ReadDataInternal : "
                     "do not recognize node type " << refNode->GetClassName());
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

  if (refNode->IsA("vtkMRMLAstroVolumeNode"))
    {
    if (volNode->GetImageData())
      {
      volNode->SetAndObserveImageData (nullptr);
      }
    }
  else if (refNode->IsA("vtkMRMLAstroLabelMapVolumeNode"))
    {
    if (labvolNode->GetImageData())
      {
      labvolNode->SetAndObserveImageData (nullptr);
      }
    }

  std::string fullName = this->GetFullNameFromFileName();

  if (fullName.empty())
    {
    vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::ReadDataInternal : "
                  "file name not specified");
    return 0;
    }

  reader->SetFileName(fullName.c_str());

  // Check if this is a FITS file that we can read
  if (!reader->CanReadFile(fullName.c_str()))
    {
    vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::ReadDataInternal : "
                  "this is not a fits file or corrupted header");
    return 0;
    }

  // Read the header to see if the file corresponds to the MRML Node
  reader->UpdateInformation();

  if(refNode->IsA("vtkMRMLAstroVolumeNode") || refNode->IsA("vtkMRMLAstroLabelMapVolumeNode"))
    {
    if (reader->GetPointDataType() != vtkDataSetAttributes::SCALARS &&
         reader->GetNumberOfComponents() > 1)
      {
      vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::ReadDataInternal : "
                    "MRMLVolumeNode does not match file kind");
      return 0;
      }
    }

  reader->Update();

  if (reader->GetWCSStruct() == nullptr)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::ReadDataInternal : "
                  "WCS not allocated.");
    return 0;
    }

  if (refNode->IsA("vtkMRMLAstroVolumeNode"))
    {
    // set volume attributes
    vtkMatrix4x4* mat = reader->GetRasToIjkMatrix();
    volNode->SetRASToIJKMatrix(mat);

    // parse non-specific key-value pairs
    std::vector<std::string> keys = reader->GetHeaderKeysVector();
    for (std::vector<std::string>::iterator kit = keys.begin(); kit != keys.end(); ++kit)
      {
      volNode->SetAttribute((*kit).c_str(), reader->GetHeaderValue((*kit).c_str()));
      }
    vtkMRMLAstroVolumeStorageNode::StoreSpectralAxisKeywords(volNode, fullName);
    disNode->SetAttribute("SlicerAstro.NAXIS", reader->GetHeaderValue("SlicerAstro.NAXIS"));

    // parse WCS Status
    disNode->SetWCSStatus(reader->GetWCSStatus());

    if (disNode->GetWCSStatus() != 0)
      {
      disNode->SetSpace("IJK");
      }

    // set WCSstruct
    disNode->SetWCSStruct(reader->GetWCSStruct());

    if(!strcmp(disNode->GetVelocityDefinition().c_str() , "FREQ"))
      {
      disNode->SetSpaceQuantity(2,"frequency");
      }

    if (!strcmp(reader->GetHeaderValue("SlicerAstro.BUNIT"), "W.U."))
      {
      volNode->SetAttribute("SlicerAstro.BUNIT", "JY/BEAM");
      // Not vtkWarningMacro: Slicer drops the warnings of storage nodes when the reading succeeds
      vtkAstroLogMessage(this, vtkAstroLogLevel::Warning,
                         std::string("the flux unit of ") + (volNode->GetName() ? volNode->GetName() : "")
                         + " is the Westerbork Unit (W.U.): it is converted to JY/BEAM.");
      }

    // rescaling flux
    if (!strcmp(reader->GetHeaderValue("SlicerAstro.BUNIT"), "W.U."))
      {
      vtkImageData *imageData = reader->GetOutput();
      if (imageData == nullptr)
        {
        vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::ReadDataInternal : "
                      "imageData not allocated.");
        return 0;
        }

      int vtkType = reader->GetDataType();
      int *dims = imageData->GetDimensions();
      const int numComponents = imageData->GetNumberOfScalarComponents();
      const vtkIdType numElements = static_cast<vtkIdType>(dims[0]) * dims[1] * dims[2] * numComponents;
      switch (vtkType)
        {
        case VTK_DOUBLE:
          double *dPixel;
          dPixel = static_cast<double*>(imageData->GetScalarPointer(0,0,0));

          if (!strcmp(reader->GetHeaderValue("SlicerAstro.BUNIT"), "W.U."))
            {
            for (vtkIdType elemCnt = 0; elemCnt < numElements; elemCnt++)
              {
              *(dPixel+elemCnt) *= 0.005;
              }
            }
          break;
        case VTK_FLOAT:
          float *fPixel;
          fPixel = static_cast<float*>(imageData->GetScalarPointer(0,0,0));
          if (!strcmp(reader->GetHeaderValue("SlicerAstro.BUNIT"), "W.U."))
            {
            for (vtkIdType elemCnt = 0; elemCnt < numElements; elemCnt++)
              {
              *(fPixel+elemCnt) *= 0.005;
              }
            }
          break;
        default:
          vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::ReadDataInternal :"
                        "could not get the data pointer. DataType not allowed.");
          return 0;
        }
      }
    }
  else if (refNode->IsA("vtkMRMLAstroLabelMapVolumeNode"))
    {
    // set volume attributes
    vtkMatrix4x4* mat = reader->GetRasToIjkMatrix();
    labvolNode->SetRASToIJKMatrix(mat);

    // parse non-specific key-value pairs
    std::vector<std::string> keys = reader->GetHeaderKeysVector();
    for (std::vector<std::string>::iterator kit = keys.begin(); kit != keys.end(); ++kit)
      {
      labvolNode->SetAttribute((*kit).c_str(), reader->GetHeaderValue((*kit).c_str()));
      }
    vtkMRMLAstroVolumeStorageNode::StoreSpectralAxisKeywords(labvolNode, fullName);
    labdisNode->SetAttribute("SlicerAstro.NAXIS", reader->GetHeaderValue("SlicerAstro.NAXIS"));

    // parse WCS Status
    labdisNode->SetWCSStatus(reader->GetWCSStatus());

    // parse Space (WCS or IJK) to the display node
    if (labdisNode->GetWCSStatus() != 0)
      {
      labdisNode->SetSpace("IJK");
      }

    //set WCSstruct
    labdisNode->SetWCSStruct(reader->GetWCSStruct());

    if(!strcmp(labdisNode->GetVelocityDefinition().c_str() , "FREQ"))
      {
      labdisNode->SetSpaceQuantity(2,"frequency");
      }

    if (!strcmp(reader->GetHeaderValue("SlicerAstro.BUNIT"), ""))
      {
      labvolNode->SetAttribute("SlicerAstro.BUNIT", "");
      }
    }

  // Detach the image from the reader pipeline: the node keeps a shallow copy
  // (the voxel buffer is shared, not duplicated). Keeping the pipeline
  // connected would keep the reader alive and could trigger a full re-read
  // of the file on any upstream modification.
  vtkNew<vtkImageData> imageData;
  imageData->ShallowCopy(reader->GetOutput());
  imageData->SetSpacing(1., 1., 1.);
  imageData->SetOrigin(0., 0., 0.);

  if (refNode->IsA("vtkMRMLAstroVolumeNode"))
    {
    volNode->SetAndObserveImageData(imageData);
    if(!strcmp(reader->GetHeaderValue("SlicerAstro.DATAMAX"), "0.") ||
       !strcmp(reader->GetHeaderValue("SlicerAstro.DATAMIN"), "0."))
      {  
      if (!volNode->UpdateRangeAttributes())
        {
        vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::ReadDataInternal :"
                      "could not calculate range attributes.");
        return 0;
        }
      }
    if (!strcmp(reader->GetHeaderValue("SlicerAstro.DisplayThreshold"), "0."))
      {
      if (!volNode->UpdateDisplayThresholdAttributes())
        {
        vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::ReadDataInternal :"
                      "could not calculate noise attributes.");
        return 0;
        }
      }

    // set range in display
    double min = StringToDouble(volNode->GetAttribute("SlicerAstro.DATAMIN"));
    double max = StringToDouble(volNode->GetAttribute("SlicerAstro.DATAMAX"));
    double window = max - min;
    double level = 0.5 * (max + min);

    int disabledModify = disNode->StartModify();
    disNode->SetWindowLevel(window, level);
    disNode->SetThreshold(min, max);
    disNode->EndModify(disabledModify);
    }
  else if (refNode->IsA("vtkMRMLAstroLabelMapVolumeNode"))
    {
    labvolNode->SetAndObserveImageData(imageData);
    if(!strcmp(reader->GetHeaderValue("SlicerAstro.DATAMAX"), "0.") ||
       !strcmp(reader->GetHeaderValue("SlicerAstro.DATAMIN"), "0."))
      {
      if (!labvolNode->UpdateRangeAttributes())
        {
        vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::ReadDataInternal :"
                      "could not calculate noise attributes.");
        return 0;
        }
      }

    if (!strcmp(reader->GetHeaderValue("SlicerAstro.DisplayThreshold"), "0."))
      {
      labvolNode->SetAttribute("SlicerAstro.DisplayThreshold", DoubleToString(1.).c_str());
      }
    }

  return 1;
}

//----------------------------------------------------------------------------
int vtkMRMLAstroVolumeStorageNode::WriteDataInternal(vtkMRMLNode *refNode)
{
  vtkMRMLVolumeNode *volNode = nullptr;

  if ( refNode->IsA("vtkMRMLAstroVolumeNode") )
    {
    volNode = vtkMRMLAstroVolumeNode::SafeDownCast(refNode);
    }
  else if ( refNode->IsA("vtkMRMLAstroLabelMapVolumeNode") )
    {
    volNode = vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(refNode);
    }
  else if ( refNode->IsA("vtkMRMLVolumeNode") )
    {
    // Generic case used for any VolumeNode. Used when Extensions add
    // node types that are subclasses of VolumeNode.
    volNode = vtkMRMLVolumeNode::SafeDownCast(refNode);
    }
  else
    {
    vtkErrorMacro(<< "vtkMRMLAstroVolumeStorageNode::WriteDataInternal :"
                     " do not recognize node type " << refNode->GetClassName());
    return 0;
    }

  if (volNode->GetImageData() == nullptr)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::WriteDataInternal :"
                  " cannot write nullptr ImageData");
    }

  std::string fullName = this->GetFullNameFromFileName();
  if (fullName == std::string(""))
    {
    vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::WriteDataInternal :"
                  " file name not specified");
    return 0;
    }

  // Use here the FITS Writer
  vtkNew<vtkFITSWriter> writer;
  writer->SetFileName(fullName.c_str());
  writer->SetInputConnection(volNode->GetImageDataConnection());
  // Compression is chosen with the file name: CFITSIO compresses the files
  // named .fits.gz. The compression of the writer (gzip of the written file,
  // then removal of the file) would leave the storage node pointing to a
  // file that no longer exists.
  writer->SetUseCompression(0);

  // pass down all MRML attributes
  std::vector<std::string> attributeNames = volNode->GetAttributeNames();
  std::vector<std::string>::iterator ait = attributeNames.begin();
  for (; ait != attributeNames.end(); ++ait)
    {
    writer->SetAttribute((*ait), volNode->GetAttribute((*ait).c_str()));
    }
  vtkMRMLAstroVolumeStorageNode::RestoreSpectralAxisKeywords(volNode, writer);

  writer->Write();
  int writeFlag = 1;
  if (writer->GetWriteError())
    {
    vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::WriteDataInternal : "
                  "ERROR writing FITS file " <<
                  (writer->GetFileName() == nullptr ? "nullptr" : writer->GetFileName()));
    writeFlag = 0;
    }

  this->StageWriteData(refNode);

  return writeFlag;
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeStorageNode::InitializeSupportedReadFileTypes()
{
  this->SupportedReadFileTypes->InsertNextValue("FITS (.fits)");
  this->SupportedReadFileTypes->InsertNextValue("FITS (.fits.gz)");
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeStorageNode::InitializeSupportedWriteFileTypes()
{
  this->SupportedWriteFileTypes->InsertNextValue("FITS (.fits)");
  this->SupportedWriteFileTypes->InsertNextValue("FITS (.fits.gz)");
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeStorageNode::ConfigureForDataExchange()
{
  this->UseCompressionOff();
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeStorageNode::StoreSpectralAxisKeywords(vtkMRMLNode* node, const std::string& fileName)
{
  if (node)
    {
    StoreSpectralAxisKeywordsImpl(node, fileName);
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeStorageNode::RestoreSpectralAxisKeywords(vtkMRMLNode* node, vtkFITSWriter* writer)
{
  if (node && writer)
    {
    RestoreSpectralAxisKeywordsImpl(node, writer);
    }
}
