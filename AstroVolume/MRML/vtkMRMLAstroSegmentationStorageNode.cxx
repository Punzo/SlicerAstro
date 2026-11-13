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
#include <vtkMRMLAstroSegmentationStorageNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLDisplayNode.h>
#include <vtkMRMLMessageCollection.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLVolumeNode.h>

// Segmentations includes
#include <vtkOrientedImageData.h>
#include <vtkOrientedImageDataResample.h>
#include <vtkSegment.h>
#include <vtkSegmentation.h>
#include <vtkSegmentationConverter.h>

// vtkFits includes
#include <vtkFITSReader.h>
#include <vtkFITSWriter.h>

// VTK includes
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSMPThreadLocal.h>
#include <vtkSMPTools.h>
#include <vtksys/SystemTools.hxx>

// STD includes
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <functional>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroSegmentationStorageNode);

namespace
{
const char* AttributePrefix = "SlicerAstro.";

//----------------------------------------------------------------------------
bool IsAstroAttribute(const std::string& name)
{
  return name.compare(0, strlen(AttributePrefix), AttributePrefix) == 0;
}

//----------------------------------------------------------------------------
/// Distinct colors for consecutive segments (golden ratio hue sequence).
void SegmentColor(int index, double rgb[3])
{
  const double goldenRatio = 0.618033988749895;
  const double hue = std::fmod(0.11 + index * goldenRatio, 1.0);
  vtkMath::HSVToRGB(hue, 0.65, 0.95, &rgb[0], &rgb[1], &rgb[2]);
}

//----------------------------------------------------------------------------
/// Sorted non-zero values of a label image.
std::vector<int> LabelValues(const int* values, vtkIdType numberOfValues)
{
  vtkSMPThreadLocal<std::set<int>> localLabels;
  vtkSMPTools::For(0, numberOfValues, [&](vtkIdType first, vtkIdType last)
    {
    std::set<int>& labels = localLabels.Local();
    int previous = 0;
    for (vtkIdType i = first; i < last; ++i)
      {
      const int value = values[i];
      if (value != 0 && value != previous)
        {
        labels.insert(value);
        previous = value;
        }
      }
    });
  std::set<int> labels;
  for (const std::set<int>& threadLabels : localLabels)
    {
    labels.insert(threadLabels.begin(), threadLabels.end());
    }
  return std::vector<int>(labels.begin(), labels.end());
}

//----------------------------------------------------------------------------
/// Write sourceID in the output where the labelmap has one of the label
/// values of the lookup table (extent: intersection of the two images).
template <typename T>
void PaintLabels(vtkImageData* labelmap, vtkImageData* output, const int extent[6],
                 const std::unordered_map<int, int>& sourceIDs)
{
  vtkSMPTools::For(extent[4], extent[5] + 1, [&](vtkIdType zFirst, vtkIdType zLast)
    {
    for (vtkIdType z = zFirst; z < zLast; ++z)
      {
      for (int y = extent[2]; y <= extent[3]; ++y)
        {
        const T* in = static_cast<const T*>(labelmap->GetScalarPointer(extent[0], y, static_cast<int>(z)));
        int* out = static_cast<int*>(output->GetScalarPointer(extent[0], y, static_cast<int>(z)));
        for (int x = 0; x <= extent[1] - extent[0]; ++x)
          {
          const int value = static_cast<int>(in[x]);
          if (value == 0)
            {
            continue;
            }
          const std::unordered_map<int, int>::const_iterator it = sourceIDs.find(value);
          if (it != sourceIDs.end())
            {
            out[x] = it->second;
            }
          }
        }
      }
    });
}
}

//----------------------------------------------------------------------------
vtkMRMLAstroSegmentationStorageNode::vtkMRMLAstroSegmentationStorageNode()
{
  // The masks are written as they are (the compression of FITS files is
  // done with the .fits.gz extension)
  this->UseCompression = 0;
  this->DefaultWriteFileExtension = "fits";
}

//----------------------------------------------------------------------------
vtkMRMLAstroSegmentationStorageNode::~vtkMRMLAstroSegmentationStorageNode() = default;

//----------------------------------------------------------------------------
void vtkMRMLAstroSegmentationStorageNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "CenterImage: " << (this->CenterImage ? "true" : "false") << "\n";
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSegmentationStorageNode::ReadXMLAttributes(const char** atts)
{
  MRMLNodeModifyBlocker blocker(this);
  this->Superclass::ReadXMLAttributes(atts);
  vtkMRMLReadXMLBeginMacro(atts);
  vtkMRMLReadXMLBooleanMacro(centerImage, CenterImage);
  vtkMRMLReadXMLEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSegmentationStorageNode::WriteXML(ostream& of, int indent)
{
  this->Superclass::WriteXML(of, indent);
  vtkMRMLWriteXMLBeginMacro(of);
  vtkMRMLWriteXMLBooleanMacro(centerImage, CenterImage);
  vtkMRMLWriteXMLEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSegmentationStorageNode::Copy(vtkMRMLNode* anode)
{
  MRMLNodeModifyBlocker blocker(this);
  this->Superclass::Copy(anode);
  vtkMRMLCopyBeginMacro(anode);
  vtkMRMLCopyBooleanMacro(CenterImage);
  vtkMRMLCopyEndMacro();
}

//----------------------------------------------------------------------------
bool vtkMRMLAstroSegmentationStorageNode::CanReadInReferenceNode(vtkMRMLNode* refNode)
{
  return refNode && refNode->IsA("vtkMRMLSegmentationNode");
}

//----------------------------------------------------------------------------
bool vtkMRMLAstroSegmentationStorageNode::ApplySavedOpacity(vtkMRMLSegmentationNode* segmentationNode)
{
  const char* value = segmentationNode ? segmentationNode->GetAttribute("SlicerAstro.SEGOPAC") : nullptr;
  if (!value || !*value || !segmentationNode->GetDisplayNode())
    {
    return false;
    }
  char* end = nullptr;
  const double opacity = strtod(value, &end);
  if (end == value || opacity < 0. || opacity > 1.)
    {
    return false;
    }
  segmentationNode->GetDisplayNode()->SetOpacity(opacity);
  return true;
}

//----------------------------------------------------------------------------
int vtkMRMLAstroSegmentationStorageNode::ReadData(vtkMRMLNode* refNode, bool temporaryFile)
{
  if ((!this->GetFileName() || !*this->GetFileName()) && this->GetNumberOfFileNames() == 0 &&
      (!this->GetURI() || !*this->GetURI()))
    {
    return 1;
    }
  return this->Superclass::ReadData(refNode, temporaryFile);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSegmentationStorageNode::InitializeSupportedReadFileTypes()
{
  this->SupportedReadFileTypes->InsertNextValue("FITS mask (.fits)");
  this->SupportedReadFileTypes->InsertNextValue("FITS mask (.fits.gz)");
  this->SupportedReadFileTypes->InsertNextValue("FITS mask (.fit)");
  this->SupportedReadFileTypes->InsertNextValue("FITS mask (.fit.gz)");
  this->SupportedReadFileTypes->InsertNextValue("FITS mask (.fts)");
  this->SupportedReadFileTypes->InsertNextValue("FITS mask (.fts.gz)");
  this->SupportedReadFileTypes->InsertNextValue("FITS mask (.fits.fz)");
  this->SupportedReadFileTypes->InsertNextValue("FITS mask (.fz)");
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSegmentationStorageNode::InitializeSupportedWriteFileTypes()
{
  this->SupportedWriteFileTypes->InsertNextValue("FITS mask (.fits)");
}

//----------------------------------------------------------------------------
std::map<int, std::string> vtkMRMLAstroSegmentationStorageNode::ReadSoFiACatalogNames(const std::string& maskFileName)
{
  std::map<int, std::string> names;

  // <name>_mask.fits (also _mask-2d, _mask-raw, .fits.gz) -> <name>_cat.txt
  // (SoFiA 2) or <name>_cat.ascii (SoFiA 1)
  static const std::regex maskSuffix("_mask(-2d|-raw)?\\.fits(\\.gz)?$", std::regex::icase);
  if (!std::regex_search(maskFileName, maskSuffix))
    {
    return names;
    }
  std::ifstream catalog;
  for (const char* suffix : { "_cat.txt", "_cat.ascii" })
    {
    catalog.open(std::regex_replace(maskFileName, maskSuffix, suffix));
    if (catalog)
      {
      break;
      }
    catalog.clear();
    }
  if (!catalog)
    {
    return names;
    }

  // Values of a row; quoted values may contain spaces (SoFiA 2 names)
  std::function<std::vector<std::string>(const std::string&)> split = [](const std::string& text)
    {
    std::vector<std::string> values;
    static const std::regex value("\"([^\"]*)\"|(\\S+)");
    for (std::sregex_iterator it(text.begin(), text.end(), value); it != std::sregex_iterator(); ++it)
      {
      values.push_back((*it)[1].matched ? (*it)[1].str() : (*it)[2].str());
      }
    return values;
    };

  // Header row with the column names ("# ... name ... id ..."), then data rows
  int nameColumn = -1, idColumn = -1;
  std::string line;
  while (std::getline(catalog, line))
    {
    if (line.find_first_not_of(" \t\r") == std::string::npos)
      {
      continue;
      }
    if (line[line.find_first_not_of(" \t")] == '#')
      {
      const std::vector<std::string> columns = split(line.substr(line.find('#') + 1));
      const std::vector<std::string>::const_iterator name = std::find(columns.begin(), columns.end(), "name");
      const std::vector<std::string>::const_iterator id = std::find(columns.begin(), columns.end(), "id");
      if (name != columns.end() && id != columns.end())
        {
        nameColumn = static_cast<int>(name - columns.begin());
        idColumn = static_cast<int>(id - columns.begin());
        }
      continue;
      }
    const std::vector<std::string> values = split(line);
    if (nameColumn < 0 || static_cast<int>(values.size()) <= std::max(nameColumn, idColumn))
      {
      continue;
      }
    try
      {
      names[static_cast<int>(std::stod(values[idColumn]))] = values[nameColumn];
      }
    catch (const std::exception&)
      {
      }
    }
  return names;
}

//----------------------------------------------------------------------------
int vtkMRMLAstroSegmentationStorageNode::ReadDataInternal(vtkMRMLNode* refNode)
{
  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(refNode);
  if (!segmentationNode || !segmentationNode->GetSegmentation())
    {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::ReadDataInternal",
                                     "the node is not a segmentation node.");
    return 0;
    }
  const std::string fullName = this->GetFullNameFromFileName();
  if (fullName.empty())
    {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::ReadDataInternal",
                                     "file name not specified.");
    return 0;
    }

  vtkNew<vtkFITSReader> reader;
  if (this->CenterImage)
    {
    reader->SetUseNativeOriginOff();
    }
  else
    {
    reader->SetUseNativeOriginOn();
    }
  // Source IDs of large surveys do not fit in 16 bits
  reader->SetMaskScalarType(VTK_INT);
  reader->SetFileName(fullName.c_str());
  if (!reader->CanReadFile(fullName.c_str()))
    {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::ReadDataInternal",
                                     "cannot read " << fullName << ": not a FITS file or corrupted header.");
    return 0;
    }
  reader->UpdateInformation();
  const char* dataModel = reader->GetHeaderValue("SlicerAstro.DATAMODEL");
  if (!dataModel || strcmp(dataModel, "MASK") != 0)
    {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::ReadDataInternal",
                                     fullName << " is not a mask (DATAMODEL = " << (dataModel ? dataModel : "") << ").");
    return 0;
    }
  reader->Update();
  vtkImageData* image = reader->GetOutput();
  if (!image || !image->GetPointData()->GetScalars() || image->GetScalarType() != VTK_INT)
    {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::ReadDataInternal",
                                     "the voxels of " << fullName << " could not be read.");
    return 0;
    }

  // Labelmap sharing the voxels of the reader, with the geometry of the
  // astronomical volumes
  vtkNew<vtkMatrix4x4> ijkToRas;
  vtkMatrix4x4::Invert(reader->GetRasToIjkMatrix(), ijkToRas);
  vtkNew<vtkOrientedImageData> labelmap;
  labelmap->vtkImageData::ShallowCopy(image);
  labelmap->SetOrigin(0., 0., 0.);
  labelmap->SetSpacing(1., 1., 1.);
  labelmap->SetGeometryFromImageToWorldMatrix(ijkToRas);

  const std::vector<int> labels = LabelValues(static_cast<int*>(image->GetScalarPointer()), image->GetNumberOfPoints());
  const std::map<int, std::string> catalogNames = ReadSoFiACatalogNames(fullName);

  MRMLNodeModifyBlocker blocker(segmentationNode);
  vtkSegmentation* segmentation = segmentationNode->GetSegmentation();
  segmentation->RemoveAllSegments();
  segmentation->SetSourceRepresentationName(vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName());
  segmentation->SetConversionParameter(vtkSegmentationConverter::GetReferenceImageGeometryParameterName(),
                                       vtkSegmentationConverter::SerializeImageGeometry(labelmap));

  int index = 0;
  for (int label : labels)
    {
    vtkNew<vtkSegment> segment;
    // name: saved by SlicerAstro in the header, else from the SoFiA catalogue
    const char* headerName = reader->GetHeaderValue(("SlicerAstro.SEGNAM" + std::to_string(label)).c_str());
    const std::map<int, std::string>::const_iterator catalogName = catalogNames.find(label);
    const std::string name = headerName && *headerName ? std::string(headerName)
                             : catalogName != catalogNames.end() ? catalogName->second
                                                                 : "Source " + std::to_string(label);
    segment->SetName(name.c_str());
    double color[3] = { 0., 0., 0. };
    SegmentColor(index++, color);
    const char* headerColor = reader->GetHeaderValue(("SlicerAstro.SEGCOL" + std::to_string(label)).c_str());
    unsigned int rgb[3] = { 0, 0, 0 };
    if (headerColor && sscanf(headerColor, "#%02x%02x%02x", &rgb[0], &rgb[1], &rgb[2]) == 3)
      {
      for (int c = 0; c < 3; ++c)
        {
        color[c] = rgb[c] / 255.;
        }
      }
    segment->SetColor(color);
    segment->SetLabelValue(label);
    segment->SetTag(vtkMRMLAstroSegmentationStorageNode::GetSourceIDTagName(), std::to_string(label));
    segment->AddRepresentation(vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName(), labelmap);
    if (!segmentation->AddSegment(segment, "Source_" + std::to_string(label)))
      {
      vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::ReadDataInternal",
                                       "the segment of the label " << label << " could not be added.");
      return 0;
      }
    }
  if (labels.empty())
    {
    vtkWarningToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::ReadDataInternal",
                                       "the mask " << fullName << " is empty.");
    }

  // FITS header, to write the mask back
  std::vector<std::string> attributeNames = segmentationNode->GetAttributeNames();
  for (const std::string& attributeName : attributeNames)
    {
    if (IsAstroAttribute(attributeName))
      {
      segmentationNode->RemoveAttribute(attributeName.c_str());
      }
    }
  for (const std::string& key : reader->GetHeaderKeysVector())
    {
    segmentationNode->SetAttribute(key.c_str(), reader->GetHeaderValue(key.c_str()));
    }
  // Spectral axis as in the file (the reader converts it, e.g. FREQ -> VOPT-F2W,
  // and adds missing keywords): the mask is written back with its header
  const std::vector<std::string> spectralKeys = vtkFITSReader::GetSpectralAxisKeywords();
  const std::map<std::string, std::string> originalKeys = vtkFITSReader::ReadHeaderKeywords(fullName.c_str(), spectralKeys);
  for (const std::string& key : spectralKeys)
    {
    const std::string attribute = std::string(AttributePrefix) + key;
    const std::map<std::string, std::string>::const_iterator original = originalKeys.find(key);
    if (original != originalKeys.end())
      {
      segmentationNode->SetAttribute(attribute.c_str(), original->second.c_str());
      }
    else
      {
      segmentationNode->RemoveAttribute(attribute.c_str());
      }
    }

  return 1;
}

//----------------------------------------------------------------------------
int vtkMRMLAstroSegmentationStorageNode::WriteDataInternal(vtkMRMLNode* refNode)
{
  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(refNode);
  if (!segmentationNode || !segmentationNode->GetSegmentation())
    {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::WriteDataInternal",
                                     "the node is not a segmentation node.");
    return 0;
    }
  const std::string fullName = this->GetFullNameFromFileName();
  if (fullName.empty())
    {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::WriteDataInternal",
                                     "file name not specified.");
    return 0;
    }
  vtkSegmentation* segmentation = segmentationNode->GetSegmentation();

  // FITS header: the one of the mask that was read, or the one of the
  // astronomical volume used as reference geometry
  std::map<std::string, std::string> header;
  for (const std::string& name : segmentationNode->GetAttributeNames())
    {
    if (IsAstroAttribute(name))
      {
      header[name] = segmentationNode->GetAttribute(name.c_str());
      }
    }
  bool headerFromVolume = false;
  vtkMRMLVolumeNode* referenceVolume = nullptr;
  if (header.empty())
    {
    referenceVolume = vtkMRMLVolumeNode::SafeDownCast(
      segmentationNode->GetNodeReference(vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str()));
    if (referenceVolume)
      {
      for (const std::string& name : referenceVolume->GetAttributeNames())
        {
        if (IsAstroAttribute(name))
          {
          header[name] = referenceVolume->GetAttribute(name.c_str());
          }
        }
      headerFromVolume = !header.empty();
      }
    }
  if (header.find("SlicerAstro.NAXIS1") == header.end())
    {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::WriteDataInternal",
                                     "the segmentation has no FITS header: it was not read from a FITS mask and "
                                     "its reference geometry is not an astronomical volume.");
    return 0;
    }

  // Geometry of the FITS mask: the one of the segmentation, or for an empty
  // segmentation (e.g. a new mask, without segments) the one of its
  // reference volume
  std::string geometryString =
    segmentation->GetConversionParameter(vtkSegmentationConverter::GetReferenceImageGeometryParameterName());
  if (geometryString.empty())
    {
    vtkMRMLVolumeNode* geometryVolume = vtkMRMLVolumeNode::SafeDownCast(
      segmentationNode->GetNodeReference(vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str()));
    if (geometryVolume && geometryVolume->GetImageData())
      {
      vtkNew<vtkMatrix4x4> ijkToRAS;
      geometryVolume->GetIJKToRASMatrix(ijkToRAS);
      geometryString = vtkSegmentationConverter::SerializeImageGeometry(ijkToRAS, geometryVolume->GetImageData());
      }
    }
  vtkNew<vtkOrientedImageData> reference;
  if (geometryString.empty() ||
      !vtkSegmentationConverter::DeserializeImageGeometry(geometryString, reference, false))
    {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::WriteDataInternal",
                                     "the segmentation has no reference geometry.");
    return 0;
    }
  int extent[6] = { 0, -1, 0, -1, 0, -1 };
  reference->GetExtent(extent);
  int dimensions[3] = { 0, 0, 0 };
  reference->GetDimensions(dimensions);
  for (int axis = 0; axis < 3; ++axis)
    {
    const std::map<std::string, std::string>::const_iterator naxis = header.find("SlicerAstro.NAXIS" + std::to_string(axis + 1));
    const int expected = naxis != header.end() ? atoi(naxis->second.c_str()) : 1;
    if (dimensions[axis] != expected)
      {
      vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::WriteDataInternal",
                                       "the reference geometry of the segmentation (" << dimensions[0] << " x "
                                       << dimensions[1] << " x " << dimensions[2]
                                       << " voxels) does not match the FITS header (NAXIS" << axis + 1 << " = "
                                       << expected << ").");
      return 0;
      }
    }

  // Source IDs: SlicerAstro.SourceID tags (unique), new IDs for the others
  std::vector<std::string> segmentIDs;
  segmentation->GetSegmentIDs(segmentIDs);
  std::vector<int> sourceIDs(segmentIDs.size(), 0);
  std::set<int> usedIDs;
  for (size_t i = 0; i < segmentIDs.size(); ++i)
    {
    std::string tag;
    vtkSegment* segment = segmentation->GetSegment(segmentIDs[i]);
    if (segment && segment->GetTag(vtkMRMLAstroSegmentationStorageNode::GetSourceIDTagName(), tag))
      {
      const int id = atoi(tag.c_str());
      if (id != 0 && usedIDs.insert(id).second)
        {
        sourceIDs[i] = id;
        }
      }
    }
  int nextID = usedIDs.empty() ? 1 : std::max(1, *usedIDs.rbegin() + 1);
  for (int& id : sourceIDs)
    {
    if (id == 0)
      {
      id = nextID++;
      }
    }

  // Segment names and colors in the header (read back with the mask)
  for (std::map<std::string, std::string>::iterator it = header.begin(); it != header.end();)
    {
    const bool segmentKeyword = it->first.compare(0, 18, "SlicerAstro.SEGNAM") == 0 ||
                                it->first.compare(0, 18, "SlicerAstro.SEGCOL") == 0 ||
                                it->first == "SlicerAstro.SEGOPAC";
    it = segmentKeyword ? header.erase(it) : std::next(it);
    }
  for (size_t i = 0; i < segmentIDs.size(); ++i)
    {
    vtkSegment* segment = segmentation->GetSegment(segmentIDs[i]);
    if (!segment)
      {
      continue;
      }
    std::string name = segment->GetName() ? segment->GetName() : "";
    // FITS string values: at most 68 characters, no quotes
    std::replace(name.begin(), name.end(), '\'', ' ');
    header["SlicerAstro.SEGNAM" + std::to_string(sourceIDs[i])] = name.substr(0, 60);
    double color[3] = { 0., 0., 0. };
    segment->GetColor(color);
    char hexColor[8];
    snprintf(hexColor, sizeof(hexColor), "#%02x%02x%02x",
             static_cast<unsigned int>(std::lround(std::clamp(color[0], 0., 1.) * 255.)),
             static_cast<unsigned int>(std::lround(std::clamp(color[1], 0., 1.) * 255.)),
             static_cast<unsigned int>(std::lround(std::clamp(color[2], 0., 1.) * 255.)));
    header["SlicerAstro.SEGCOL" + std::to_string(sourceIDs[i])] = hexColor;
    }
  // opacity of the display of the mask (read back with the mask)
  if (segmentationNode->GetDisplayNode())
    {
    std::ostringstream opacity;
    opacity << segmentationNode->GetDisplayNode()->GetOpacity();
    header["SlicerAstro.SEGOPAC"] = opacity.str();
    }

  // Merge the segments: one pass per binary labelmap (segments created by
  // reading a mask share a single labelmap)
  vtkNew<vtkImageData> mask;
  mask->SetExtent(extent);
  mask->AllocateScalars(VTK_INT, 1);
  std::fill_n(static_cast<int*>(mask->GetScalarPointer()), mask->GetNumberOfPoints(), 0);

  // Segments without binary labelmap (e.g. only closed surfaces): converted
  // on a copy (the segmentation is not modified by writing it)
  const std::string labelmapName = vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName();
  const std::string surfaceName = vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName();
  vtkNew<vtkSegmentation> converted;
  converted->CopyConversionParameters(segmentation);
  converted->SetSourceRepresentationName(surfaceName);
  for (const std::string& segmentID : segmentIDs)
    {
    vtkSegment* segment = segmentation->GetSegment(segmentID);
    if (segment && !segment->GetRepresentation(labelmapName) && segment->GetRepresentation(surfaceName))
      {
      vtkNew<vtkSegment> copy;
      copy->DeepCopy(segment);
      converted->AddSegment(copy, segmentID);
      }
    }
  if (converted->GetNumberOfSegments() > 0)
    {
    converted->SetConversionParameter(vtkSegmentationConverter::GetReferenceImageGeometryParameterName(), geometryString);
    converted->CreateRepresentation(labelmapName);
    }

  std::vector<vtkOrientedImageData*> labelmaps;
  std::map<vtkOrientedImageData*, std::unordered_map<int, int>> sourceIDsByLabelmap;
  for (size_t i = 0; i < segmentIDs.size(); ++i)
    {
    vtkSegment* segment = segmentation->GetSegment(segmentIDs[i]);
    if (segment && !segment->GetRepresentation(labelmapName) && converted->GetSegment(segmentIDs[i]))
      {
      segment = converted->GetSegment(segmentIDs[i]);
      }
    vtkOrientedImageData* labelmap = segment ? vtkOrientedImageData::SafeDownCast(segment->GetRepresentation(
      labelmapName)) : nullptr;
    if (!labelmap || !labelmap->GetPointData()->GetScalars())
      {
      vtkWarningToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::WriteDataInternal",
                                         "the segment " << (segment && segment->GetName() ? segment->GetName() : segmentIDs[i])
                                         << " has no binary labelmap: it is not written.");
      continue;
      }
    if (sourceIDsByLabelmap.find(labelmap) == sourceIDsByLabelmap.end())
      {
      labelmaps.push_back(labelmap);
      }
    sourceIDsByLabelmap[labelmap][segment->GetLabelValue()] = sourceIDs[i];
    }

  for (vtkOrientedImageData* labelmap : labelmaps)
    {
    vtkSmartPointer<vtkOrientedImageData> source = labelmap;
    if (!vtkOrientedImageDataResample::DoGeometriesMatch(reference, labelmap))
      {
      source = vtkSmartPointer<vtkOrientedImageData>::New();
      if (!vtkOrientedImageDataResample::ResampleOrientedImageToReferenceOrientedImage(labelmap, reference, source, false))
        {
        vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::WriteDataInternal",
                                         "a labelmap could not be resampled to the geometry of the mask.");
        return 0;
        }
      }
    int sourceExtent[6] = { 0, -1, 0, -1, 0, -1 };
    source->GetExtent(sourceExtent);
    int paintExtent[6];
    bool empty = false;
    for (int axis = 0; axis < 3; ++axis)
      {
      paintExtent[2 * axis] = std::max(extent[2 * axis], sourceExtent[2 * axis]);
      paintExtent[2 * axis + 1] = std::min(extent[2 * axis + 1], sourceExtent[2 * axis + 1]);
      empty = empty || paintExtent[2 * axis + 1] < paintExtent[2 * axis];
      }
    if (empty)
      {
      continue;
      }
    const std::unordered_map<int, int>& lookup = sourceIDsByLabelmap[labelmap];
    switch (source->GetScalarType())
      {
      vtkTemplateMacro(PaintLabels<VTK_TT>(source, mask, paintExtent, lookup));
      }
    }

  // Header of the mask
  vtkNew<vtkFITSWriter> writer;
  writer->SetFileName(fullName.c_str());
  writer->SetInputDataObject(mask);
  writer->SetUseCompression(0);
  for (const std::pair<const std::string, std::string>& keyValue : header)
    {
    writer->SetAttribute(keyValue.first, keyValue.second);
    }
  writer->SetAttribute("SlicerAstro.DATAMODEL", "MASK");
  if (headerFromVolume)
    {
    writer->SetAttribute("SlicerAstro.BUNIT", " ");
    // spectral axis as in the file of the volume
    vtkMRMLAstroVolumeStorageNode::RestoreSpectralAxisKeywords(referenceVolume, writer);
    }
  const int* values = static_cast<int*>(mask->GetScalarPointer());
  const std::pair<const int*, const int*> range = std::minmax_element(values, values + mask->GetNumberOfPoints());
  writer->SetAttribute("SlicerAstro.DATAMIN", std::to_string(*range.first));
  writer->SetAttribute("SlicerAstro.DATAMAX", std::to_string(*range.second));

  writer->Write();
  if (writer->GetWriteError())
    {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLAstroSegmentationStorageNode::WriteDataInternal",
                                     "the FITS mask " << fullName << " could not be written.");
    return 0;
    }
  return 1;
}
