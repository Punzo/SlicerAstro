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

#ifndef __vtkMRMLAstroVolumeStorageNode_h
#define __vtkMRMLAstroVolumeStorageNode_h

#include "vtkMRMLStorageNode.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

// STD includes
#include <string>

class vtkFITSWriter;

/// \brief MRML node for representing a volume storage.
///
/// vtkMRMLAstroVolumeStorageNode nodes describe the archetybe based volume storage
/// node that allows to read/write volume data from/to AstroVolume files
/// \ingroup SlicerAstro_QtModules_AstroVolumeNode
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroVolumeStorageNode : public vtkMRMLStorageNode
{
  public:
  static vtkMRMLAstroVolumeStorageNode *New();
  vtkTypeMacro(vtkMRMLAstroVolumeStorageNode,vtkMRMLStorageNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  virtual vtkMRMLNode* CreateNodeInstance() override;

  /// Read node attributes from XML file
  virtual void ReadXMLAttributes( const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) override;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) override;

  /// Get node XML tag name (like Storage, Model)
  virtual const char* GetNodeTagName() override {return "AstroStorage";};

  /// Set/Get the CenterImage.
  /// Default is 2.
  /// \sa CenterImage(), CenterImage()
  vtkGetMacro(CenterImage, int);
  vtkSetMacro(CenterImage, int);

  /// HDU (extension) of the file with the data: cfitsio absolute HDU number;
  /// 0 (default): the first image HDU with data. Saved with the scene; reset
  /// to 0 when the data are written (the written file has one HDU).
  vtkGetMacro(HDUNumber, int);
  vtkSetMacro(HDUNumber, int);
  /// Sub-cube of the file with the data (UseSubExtent on): voxel extent
  /// (i0, i1, j0, j1, k0, k1; 0-based, inclusive). Saved with the scene;
  /// turned off when the data are written.
  vtkGetVector6Macro(SubExtent, int);
  vtkSetVector6Macro(SubExtent, int);
  vtkGetMacro(UseSubExtent, bool);
  vtkSetMacro(UseSubExtent, bool);

  /// Options for the next storage node created by the loading of astronomical
  /// volumes (vtkSlicerAstroVolumeLogic node set factory): set by the volume
  /// reader before a load and cleared after it.
  static void SetNextReadOptions(int hduNumber, const int subExtent[6], bool useSubExtent);
  static void ClearNextReadOptions();
  /// Applies the next read options to a storage node (if any are set)
  static void ApplyNextReadOptions(vtkMRMLAstroVolumeStorageNode* node);

  /// Spectral axis keywords of the file: the reader converts the spectral
  /// axis (e.g. FREQ -> VOPT-F2W) and adds missing keywords (e.g. RESTFREQ).
  /// StoreSpectralAxisKeywords keeps the keywords of the file and the
  /// converted ones in attributes of the node (FITSOriginal.*, FITSConverted.*);
  /// RestoreSpectralAxisKeywords sets the keywords of the file in the writer if
  /// the spectral axis of the node is unchanged (not modified by a module).
  static void StoreSpectralAxisKeywords(vtkMRMLNode* node, const std::string& fileName, int hduNumber = 0);
  static void RestoreSpectralAxisKeywords(vtkMRMLNode* node, vtkFITSWriter* writer);
  /// A rest frequency (Hz) set after loading (e.g. the line of optical data
  /// in wavelengths): saved with the spectral axis of the file (RESTFRQ)
  static void RecordRestFrequency(vtkMRMLNode* node, double restFrequency);

  /// Return true if the node can be read in.
  virtual bool CanReadInReferenceNode(vtkMRMLNode *refNode) override;

  /// Configure the storage node for data exchange. This is an
  /// opportunity to optimize the storage node's settings, for
  /// instance to turn off compression.
  virtual void ConfigureForDataExchange() override;

protected:
  vtkMRMLAstroVolumeStorageNode();
  ~vtkMRMLAstroVolumeStorageNode() override;
  vtkMRMLAstroVolumeStorageNode(const vtkMRMLAstroVolumeStorageNode&);
  void operator=(const vtkMRMLAstroVolumeStorageNode&);

  /// Initialize all the supported write file types
  virtual void InitializeSupportedReadFileTypes() override;

  /// Initialize all the supported write file types
  virtual void InitializeSupportedWriteFileTypes() override;

  /// Read data and set it in the referenced node
  virtual int ReadDataInternal(vtkMRMLNode *refNode) override;

  /// Write data from a  referenced node
  virtual int WriteDataInternal(vtkMRMLNode *refNode) override;

  int CenterImage;
  int HDUNumber{0};
  int SubExtent[6]{0, -1, 0, -1, 0, -1};
  bool UseSubExtent{false};
};

#endif
