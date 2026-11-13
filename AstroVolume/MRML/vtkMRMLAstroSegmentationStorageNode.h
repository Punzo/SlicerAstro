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

#ifndef __vtkMRMLAstroSegmentationStorageNode_h
#define __vtkMRMLAstroSegmentationStorageNode_h

#include "vtkMRMLStorageNode.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

// STD includes
#include <map>
#include <string>

class vtkMRMLSegmentationNode;

/// \brief Storage of segmentations as FITS masks.
///
/// A FITS mask (DATAMODEL = 'MASK', or a mask created by SoFiA, see
/// vtkFITSReader::IsMaskFile) is read into a segmentation node: one segment
/// per label value (e.g. the source IDs of SoFiA), all sharing a single
/// binary labelmap (the voxels of the file, not copied). The segments are
/// named after the SoFiA catalogue next to the mask (<name>_cat.txt) if
/// present, "Source <id>" otherwise; the label value is kept in the segment
/// tag SlicerAstro.SourceID. The FITS header is kept in the attributes of the
/// segmentation node.
///
/// Writing creates the FITS mask from the segments (32-bit integers, label =
/// SlicerAstro.SourceID tag, new segments get new IDs) in the geometry of the
/// original mask, with its FITS header. A segmentation created in Slicer on
/// an astronomical volume is written with the header of that volume.
/// \ingroup SlicerAstro_QtModules_AstroVolumeNode
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroSegmentationStorageNode : public vtkMRMLStorageNode
{
public:
  static vtkMRMLAstroSegmentationStorageNode* New();
  vtkTypeMacro(vtkMRMLAstroSegmentationStorageNode, vtkMRMLStorageNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  vtkMRMLNode* CreateNodeInstance() override;

  void ReadXMLAttributes(const char** atts) override;
  void WriteXML(ostream& of, int indent) override;
  void Copy(vtkMRMLNode* node) override;

  const char* GetNodeTagName() override { return "AstroSegmentationStorage"; }

  /// Center the mask as the astronomical volumes loaded with the center
  /// option (default), so that the mask overlays its data cube.
  vtkGetMacro(CenterImage, bool);
  vtkSetMacro(CenterImage, bool);
  vtkBooleanMacro(CenterImage, bool);

  bool CanReadInReferenceNode(vtkMRMLNode* refNode) override;

  /// No file to read (success): the default storage node of a segmentation
  /// that has not been saved yet, e.g. the empty mask of a data cube in a
  /// scene saved without its data.
  int ReadData(vtkMRMLNode* refNode, bool temporaryFile = false) override;

  /// Opacity saved with a FITS mask (keyword SEGOPAC, the opacity of the
  /// display of the segmentation when it was saved) set to the display node.
  /// Returns false if the mask has no saved opacity.
  static bool ApplySavedOpacity(vtkMRMLSegmentationNode* segmentationNode);

  const char* GetDefaultWriteFileExtension() override { return "fits"; }

  /// Segment tag holding the label value of the segment in the FITS mask.
  static const char* GetSourceIDTagName() { return "SlicerAstro.SourceID"; }

  /// Source names (by source ID) of the plain-text SoFiA catalogue written
  /// next to a SoFiA mask (<name>_mask.fits -> <name>_cat.txt). Empty if
  /// there is no catalogue.
  static std::map<int, std::string> ReadSoFiACatalogNames(const std::string& maskFileName);

protected:
  vtkMRMLAstroSegmentationStorageNode();
  ~vtkMRMLAstroSegmentationStorageNode() override;
  vtkMRMLAstroSegmentationStorageNode(const vtkMRMLAstroSegmentationStorageNode&);
  void operator=(const vtkMRMLAstroSegmentationStorageNode&);

  void InitializeSupportedReadFileTypes() override;
  void InitializeSupportedWriteFileTypes() override;

  int ReadDataInternal(vtkMRMLNode* refNode) override;
  int WriteDataInternal(vtkMRMLNode* refNode) override;

  bool CenterImage{ true };
};

#endif
