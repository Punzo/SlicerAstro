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

#ifndef __vtkMRMLAstroStatisticsParametersNode_h
#define __vtkMRMLAstroStatisticsParametersNode_h

// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLNode.h>

// Export includes
#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkMRMLAnnotationROINode;
class vtkMRMLTableNode;
class vtkMRMLTransformNode;

/// \ingroup SlicerAstro_QtModules_AstroStatistics
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroStatisticsParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroStatisticsParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroStatisticsParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  virtual vtkMRMLNode* CreateNodeInstance() VTK_OVERRIDE;

  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) VTK_OVERRIDE;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) VTK_OVERRIDE;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) VTK_OVERRIDE;

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroStatisticsParameters";};

  /// Set/Get the InputVolumeNodeID.
  /// \sa SetInputVolumeNodeID(), GetInputVolumeNodeID()
  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  /// Set/Get the MaskVolumeNodeID.
  /// \sa SetMaskVolumeNodeID(), GetMaskVolumeNodeID()
  vtkSetStringMacro(MaskVolumeNodeID);
  vtkGetStringMacro(MaskVolumeNodeID);

  /// Get MRML ROI node
  vtkMRMLAnnotationROINode* GetROINode();

  /// Set MRML ROI node
  void SetROINode(vtkMRMLAnnotationROINode* node);

  /// Get MRML table node
  vtkMRMLTableNode* GetTableNode();

  /// Set MRML table node
  void SetTableNode(vtkMRMLTableNode* node);

  /// Set/Get the Mode.
  /// Default is "ROI"
  /// \sa SetMode(), GetMode()
  vtkSetStringMacro(Mode);
  vtkGetStringMacro(Mode);

  /// Set/Get calculate Max (true/false).
  /// \sa SetMax(), GetMax()
  vtkSetMacro(Max,bool);
  vtkGetMacro(Max,bool);
  vtkBooleanMacro(Max,bool);

  /// Set/Get calculate Mean (true/false).
  /// \sa SetMean(), GetMean()
  vtkSetMacro(Mean,bool);
  vtkGetMacro(Mean,bool);
  vtkBooleanMacro(Mean,bool);

  /// Set/Get calculate Median (true/false).
  /// \sa SetMedian(), GetMedian()
  vtkSetMacro(Median,bool);
  vtkGetMacro(Median,bool);
  vtkBooleanMacro(Median,bool);

  /// Set/Get calculate Min (true/false).
  /// \sa SetMin(), GetMin()
  vtkSetMacro(Min,bool);
  vtkGetMacro(Min,bool);
  vtkBooleanMacro(Min,bool);

  /// Set/Get calculate Npixels (true/false).
  /// \sa SetNpixels(), GetNpixels()
  vtkSetMacro(Npixels,bool);
  vtkGetMacro(Npixels,bool);
  vtkBooleanMacro(Npixels,bool);

  /// Set/Get calculate Std (true/false).
  /// \sa SetStd(), GetStd()
  vtkSetMacro(Std,bool);
  vtkGetMacro(Std,bool);
  vtkBooleanMacro(Std,bool);

  /// Set/Get calculate Sum (true/false).
  /// \sa SetSum(), GetSum()
  vtkSetMacro(Sum,bool);
  vtkGetMacro(Sum,bool);
  vtkBooleanMacro(Sum,bool);

  /// Set/Get calculate TotalFlux (true/false).
  /// \sa SetTotalFlux(), GetTotalFlux()
  vtkSetMacro(TotalFlux,bool);
  vtkGetMacro(TotalFlux,bool);
  vtkBooleanMacro(TotalFlux,bool);

  /// Set/Get the Cores.
  /// Default is 0 (all the free cores will be used)
  /// \sa SetCores(), GetCores()
  vtkSetMacro(Cores,int);
  vtkGetMacro(Cores,int);

  /// Set/Get the OutputSerial.
  /// \sa SetOutputSerial(), GetOutputSerial()
  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

  /// Set/Get the Status.
  /// \sa SetStatus(), GetStatus()
  vtkSetMacro(Status,int);
  vtkGetMacro(Status,int);

protected:
  vtkMRMLAstroStatisticsParametersNode();
  ~vtkMRMLAstroStatisticsParametersNode();

  vtkMRMLAstroStatisticsParametersNode(const vtkMRMLAstroStatisticsParametersNode&);
  void operator=(const vtkMRMLAstroStatisticsParametersNode&);

  static const char* TABLE_REFERENCE_ROLE;
  const char *GetTableNodeReferenceRole();

  static const char* ROI_REFERENCE_ROLE;
  const char *GetROINodeReferenceRole();

  static const char* ROI_ALIGNMENTTRANSFORM_REFERENCE_ROLE;
  const char *GetROIAlignmentTransformNodeReferenceRole();

  char *InputVolumeNodeID;
  char *MaskVolumeNodeID;
  char *Mode;

  int Cores;

  bool Max;
  bool Mean;
  bool Median;
  bool Min;
  bool Npixels;
  bool Std;
  bool Sum;
  bool TotalFlux;

  int OutputSerial;

  int Status;
};

#endif
