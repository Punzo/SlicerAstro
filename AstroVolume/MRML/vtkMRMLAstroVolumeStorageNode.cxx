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
// STD includes
#include <iomanip>
#include <limits>
#include <type_traits>

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
#include <vtkMatrix4x4.h>
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

void StoreSpectralAxisKeywordsImpl(vtkMRMLNode* node, const std::string& fileName, int hduNumber)
{
  const std::vector<std::string> keys = vtkFITSReader::GetSpectralAxisKeywords();
  const std::map<std::string, std::string> original =
    vtkFITSReader::ReadHeaderKeywords(fileName.c_str(), keys, hduNumber);
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
  // floating point: all the significant digits of the WCS values
  if (std::is_floating_point<T>::value)
    {
    strstream << std::setprecision(std::numeric_limits<double>::digits10);
    }
  strstream << V;
  strstream >> stringValue;
  return stringValue;
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
  if (this->HDUNumber > 0)
    {
    of << " hduNumber=\"" << this->HDUNumber << "\"";
    }
  if (this->UseSubExtent)
    {
    of << " subExtent=\"" << this->SubExtent[0] << " " << this->SubExtent[1] << " " << this->SubExtent[2] << " "
       << this->SubExtent[3] << " " << this->SubExtent[4] << " " << this->SubExtent[5] << "\"";
    }
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
    else if (!strcmp(attName, "hduNumber"))
      {
      std::stringstream ss;
      ss << attValue;
      ss >> this->HDUNumber;
      }
    else if (!strcmp(attName, "subExtent"))
      {
      std::stringstream ss;
      ss << attValue;
      for (int index = 0; index < 6; index++)
        {
        ss >> this->SubExtent[index];
        }
      this->UseSubExtent = !ss.fail();
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
  this->SetHDUNumber(node->HDUNumber);
  this->SetSubExtent(node->SubExtent);
  this->SetUseSubExtent(node->UseSubExtent);

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
  return refNode->IsA("vtkMRMLAstroVolumeNode");
}

//----------------------------------------------------------------------------
int vtkMRMLAstroVolumeStorageNode::ReadDataInternal(vtkMRMLNode *refNode)
{
  vtkMRMLAstroVolumeNode *volNode = vtkMRMLAstroVolumeNode::SafeDownCast(refNode);
  vtkMRMLAstroVolumeDisplayNode *disNode = volNode ? volNode->GetAstroVolumeDisplayNode() : nullptr;
  if (!volNode)
    {
    vtkErrorMacro(<< "vtkMRMLAstroVolumeStorageNode::ReadDataInternal : "
                     "do not recognize node type " << refNode->GetClassName());
    return 0;
    }

  // A scene being loaded has the geometry of the volume (IJK to RAS, in the
  // scene file): it is kept, so that a volume placed in the scene (e.g. the
  // moment map of the PV Diagram module, below the data) stays there. A FITS
  // header has no RAS geometry; files opened outside a scene take the
  // geometry of the reader.
  vtkNew<vtkMatrix4x4> sceneRASToIJK;
  vtkMRMLVolumeNode* volumeNode = vtkMRMLVolumeNode::SafeDownCast(refNode);
  bool keepSceneGeometry = false;
  if (volumeNode && volumeNode->GetScene() && volumeNode->GetScene()->IsImporting())
    {
    volumeNode->GetRASToIJKMatrix(sceneRASToIJK);
    keepSceneGeometry = !sceneRASToIJK->IsIdentity();
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
    volNode->SetAndObserveImageData (nullptr);
    }

  std::string fullName = this->GetFullNameFromFileName();

  if (fullName.empty())
    {
    vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::ReadDataInternal : "
                  "file name not specified");
    return 0;
    }

  reader->SetFileName(fullName.c_str());
  // the HDU and the part of the file with the data
  reader->SetHDUNumber(this->HDUNumber);
  reader->SetSubExtent(this->SubExtent);
  reader->SetUseSubExtent(this->UseSubExtent);

  // Check if this is a FITS file that we can read
  if (!reader->CanReadFile(fullName.c_str()))
    {
    vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::ReadDataInternal : "
                  "this is not a fits file or corrupted header");
    return 0;
    }

  // Read the header to see if the file corresponds to the MRML Node
  reader->UpdateInformation();

  if (reader->GetPointDataType() != vtkDataSetAttributes::SCALARS &&
       reader->GetNumberOfComponents() > 1)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeStorageNode::ReadDataInternal : "
                  "MRMLVolumeNode does not match file kind");
    return 0;
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
    vtkMatrix4x4* mat = keepSceneGeometry ? sceneRASToIJK.GetPointer() : reader->GetRasToIjkMatrix();
    volNode->SetRASToIJKMatrix(mat);

    // parse non-specific key-value pairs
    std::vector<std::string> keys = reader->GetHeaderKeysVector();
    for (std::vector<std::string>::iterator kit = keys.begin(); kit != keys.end(); ++kit)
      {
      volNode->SetAttribute((*kit).c_str(), reader->GetHeaderValue((*kit).c_str()));
      }
    vtkMRMLAstroVolumeStorageNode::StoreSpectralAxisKeywords(volNode, fullName, this->HDUNumber);
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
    else if (disNode->IsWavelengthAxis())
      {
      // optical data without a rest wavelength: wavelengths (Angstrom)
      disNode->SetSpaceQuantity(2, "wavelength");
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

    // set range in display: the data range, without isolated outliers
    // (e.g. stars in optical data) for the 2D display
    double min = StringToDouble(volNode->GetAttribute("SlicerAstro.DATAMIN"));
    double max = StringToDouble(volNode->GetAttribute("SlicerAstro.DATAMAX"));
    double displayRange[2] = { min, max };
    volNode->GetDisplayRange(displayRange);
    double window = displayRange[1] - displayRange[0];
    double level = 0.5 * (displayRange[1] + displayRange[0]);

    // (the display of a volume of a scene is the saved one)
    if (!this->GetScene() || !this->GetScene()->IsImporting())
      {
      int disabledModify = disNode->StartModify();
      disNode->SetWindowLevel(window, level);
      disNode->SetThreshold(min, max);
      // the colours of SlicerAstro (singletons of the AstroVolume logic):
      // Observatory, Doppler for velocity fields, Ember for dispersions;
      // set while reading, so that every way of loading has them (Slicer's
      // volume loader sets its grey only to a display without colours)
      const char* colorID = disNode->GetColorNodeID();
      if (this->GetScene() && (!colorID || !strcmp(colorID, "vtkMRMLColorTableNodeGrey")))
        {
        const char* datamodel = volNode->GetAttribute("SlicerAstro.DATAMODEL");
        const std::string table = datamodel && !strcmp(datamodel, "FIRSTMOMENTMAP") ? "Doppler" :
          (datamodel && !strcmp(datamodel, "SECONDMOMENTMAP") ? "Ember" : "Observatory");
        // (Slicer reads into a scene of its own and moves the nodes to the
        // main one, which has the tables: the reference is resolved there)
        const std::string tableID = "vtkMRMLColorTableNodeSlicerAstro." + table;
        disNode->SetAndObserveColorNodeID(tableID.c_str());
        }
      disNode->EndModify(disabledModify);
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
  if (writeFlag)
    {
    // the written file has one HDU with all the data
    this->HDUNumber = 0;
    this->UseSubExtent = false;
    }

  return writeFlag;
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeStorageNode::InitializeSupportedReadFileTypes()
{
  this->SupportedReadFileTypes->InsertNextValue("FITS (.fits)");
  this->SupportedReadFileTypes->InsertNextValue("FITS (.fits.gz)");
  this->SupportedReadFileTypes->InsertNextValue("FITS (.fit)");
  this->SupportedReadFileTypes->InsertNextValue("FITS (.fit.gz)");
  this->SupportedReadFileTypes->InsertNextValue("FITS (.fts)");
  this->SupportedReadFileTypes->InsertNextValue("FITS (.fts.gz)");
  this->SupportedReadFileTypes->InsertNextValue("FITS (.fits.fz)");
  this->SupportedReadFileTypes->InsertNextValue("FITS (.fz)");
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
void vtkMRMLAstroVolumeStorageNode::StoreSpectralAxisKeywords(vtkMRMLNode* node, const std::string& fileName,
                                                               int hduNumber)
{
  if (node)
    {
    StoreSpectralAxisKeywordsImpl(node, fileName, hduNumber);
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeStorageNode::RecordRestFrequency(vtkMRMLNode* node, double restFrequency)
{
  if (!node)
    {
    return;
    }
  const std::vector<std::string> keys = vtkFITSReader::GetSpectralAxisKeywords();
  bool stored = false;
  for (const std::string& key : keys)
    {
    stored = stored || node->GetAttribute((ConvertedKeywordPrefix + key).c_str()) ||
             node->GetAttribute((OriginalKeywordPrefix + key).c_str());
    }
  if (!stored)
    {
    return;
    }
  // the spectral axis of the file with the rest frequency (saved as it was
  // read, plus RESTFRQ): the axis converted from it is the current one
  node->SetAttribute((std::string(OriginalKeywordPrefix) + "RESTFRQ").c_str(), NumberToString(restFrequency).c_str());
  node->SetAttribute((std::string(OriginalKeywordPrefix) + "RESTFREQ").c_str(), nullptr);
  for (const std::string& key : keys)
    {
    node->SetAttribute((ConvertedKeywordPrefix + key).c_str(), node->GetAttribute(("SlicerAstro." + key).c_str()));
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

//----------------------------------------------------------------------------
namespace
{
struct NextReadOptions
{
  bool Set{false};
  int HDUNumber{0};
  int SubExtent[6]{0, -1, 0, -1, 0, -1};
  bool UseSubExtent{false};
};
NextReadOptions& PendingReadOptions()
{
  static NextReadOptions options;
  return options;
}
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeStorageNode::SetNextReadOptions(int hduNumber, const int subExtent[6], bool useSubExtent)
{
  NextReadOptions& options = PendingReadOptions();
  options.Set = true;
  options.HDUNumber = hduNumber;
  for (int index = 0; index < 6; index++)
    {
    options.SubExtent[index] = subExtent ? subExtent[index] : (index % 2 ? -1 : 0);
    }
  options.UseSubExtent = useSubExtent && subExtent;
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeStorageNode::ClearNextReadOptions()
{
  PendingReadOptions() = NextReadOptions();
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeStorageNode::ApplyNextReadOptions(vtkMRMLAstroVolumeStorageNode* node)
{
  const NextReadOptions& options = PendingReadOptions();
  if (!node || !options.Set)
    {
    return;
    }
  node->SetHDUNumber(options.HDUNumber);
  node->SetSubExtent(const_cast<int*>(options.SubExtent));
  node->SetUseSubExtent(options.UseSubExtent);
}
