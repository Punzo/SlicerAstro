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

#ifndef __vtkMRMLAstroVolumeDisplayNode_h
#define __vtkMRMLAstroVolumeDisplayNode_h

// MRML includes
#include "vtkMRMLScalarVolumeDisplayNode.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

// VTK includes
#include <vtkStringArray.h>
#include <vtkDoubleArray.h>

// WCS includes
#include "wcslib.h"

class vtkAlgorithmOutput;
class vtkImageData;
class vtkMRMLAstroLabelMapVolumeDisplayNode;
class vtkMRMLUnitNode;

class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroVolumeDisplayNode : public vtkMRMLScalarVolumeDisplayNode
{
  public:
  static vtkMRMLAstroVolumeDisplayNode *New();
  vtkTypeMacro(vtkMRMLAstroVolumeDisplayNode,vtkMRMLScalarVolumeDisplayNode);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  virtual vtkMRMLNode* CreateNodeInstance() VTK_OVERRIDE;

  /// 
  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) VTK_OVERRIDE;

  /// 
  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) VTK_OVERRIDE;

  /// 
  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) VTK_OVERRIDE;

  ///
  /// Copy WCS to this object
  virtual void CopyWCS(vtkMRMLNode *node);

  ///
  /// Copy only the spatial WCS information to this object
  virtual void CopySpatialWCS(vtkMRMLNode *node);

  /// 
  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroVolumeDisplay";};

  ///
  /// Set/Get the CoordinateSystem.
  /// Default is "WCS".
  /// \sa SetSpace(), GetSpace()
  vtkGetStringMacro(Space);
  vtkSetStringMacro(Space);

  ///
  /// Set/Get the SpaceQuantities.
  /// The default is 3 and the values are "time; length; velocity".
  /// \sa SetSpaceQuantities(), GetSpaceQuantities()
  vtkGetObjectMacro(SpaceQuantities, vtkStringArray);
  vtkSetObjectMacro(SpaceQuantities, vtkStringArray);

  ///
  /// Set the i-th SpaceQunatity name
  bool SetSpaceQuantity(int ind, const char *name);

  ///
  /// Set/Get the ContoursColor
  /// \sa SetContoursColor(), GetContoursColor()
  vtkGetObjectMacro(ContoursColor, vtkDoubleArray);
  vtkSetObjectMacro(ContoursColor, vtkDoubleArray);

  ///
  /// Set the i-th SpaceQunatity name
  void SetContoursColor(int ind, double color);

  ///
  /// FitSlices
  virtual void SetFitSlices(bool fit);
  vtkGetMacro(FitSlices,bool);

  enum
     {
     FitSlicesModifiedEvent = 123000,
     };

  ///
  /// Set WCSStruct
  virtual void SetWCSStruct(struct wcsprm*);

  ///
  ///Get WCSStruct
  virtual struct wcsprm* GetWCSStruct();

  ///
  /// WcsStatus
  vtkSetMacro(WCSStatus,int);
  vtkGetMacro(WCSStatus,int);

  ///
  /// OpacityShift
  vtkSetMacro(OpacityShift,double);
  vtkGetMacro(OpacityShift,double);

  ///
  /// OldOpacityShift
  vtkSetMacro(OldOpacityShift,double);
  vtkGetMacro(OldOpacityShift,double);

  ///
  /// PresetOffset
  vtkSetMacro(PresetOffset,double);
  vtkGetMacro(PresetOffset,double);

  ///
  /// OldPresetOffset
  vtkSetMacro(OldPresetOffset,double);
  vtkGetMacro(OldPresetOffset,double);

  ///
  /// PresetStretch
  vtkSetMacro(PresetStretch,double);
  vtkGetMacro(PresetStretch,double);

  ///
  /// OldPresetStretch
  vtkSetMacro(OldPresetStretch,double);
  vtkGetMacro(OldPresetStretch,double);

  ///
  /// Set radio as velocity definition
  virtual bool SetRadioVelocityDefinition(bool update = true);

  ///
  /// Set optical as velocity definition
  virtual bool SetOpticalVelocityDefinition(bool update = true);

  ///
  /// Get the velocity definition currently in use
  virtual std::string GetVelocityDefinition();

  ///
  /// Get WCS Coordinates from IJK
  virtual bool GetReferenceSpace(const double ijk[3],
                                 double SpaceCoordinates[3]);

  ///
  /// Get IJK Coordinates from WCS
  virtual bool GetIJKSpace(const double SpaceCoordinates[3],
                           double ijk[3]);

  ///
  /// Get IJK Coordinates from WCS
  virtual bool GetIJKSpace(std::vector<double> SpaceCoordinates,
                           double ijk[3]);

  ///
  /// Get the first tick for the display of axes at given unitNode in WCS units
  virtual double GetFirstWcsTickAxis(const double worldA, const double worldB,
                                     const double wcsStep, vtkMRMLUnitNode *node);

  ///
  /// Get the first tick for the display of the axis X in WCS units
  virtual double GetFirstWcsTickAxisX(const double worldA, const double worldB, const double wcsStep);

  ///
  /// Get the first tick for the display of the axis Y in WCS units
  virtual double GetFirstWcsTickAxisY(const double worldA, const double worldB, const double wcsStep);

  ///
  /// Get the first tick for the display of the axis Z in WCS units
  virtual double GetFirstWcsTickAxisZ(const double worldA, const double worldB, const double wcsStep);

  ///
  /// Get the ticks step for the display of axes at given unitNode in WCS units
  virtual double GetWcsTickStepAxis(const double wcsLength,
                                    int *numberOfPoints,
                                    vtkMRMLUnitNode *node);

  ///
  /// Get the ticks step for the display of the axis X in WCS units
  virtual double GetWcsTickStepAxisX(const double wcsLength,
                                     int* numberOfPoints);

  ///
  /// Get the ticks step for the display of the axis Y in WCS units
  virtual double GetWcsTickStepAxisY(const double wcsLength,
                                     int* numberOfPoints);

  ///
  /// Get the ticks step for the display of the axis Z in WCS units
  virtual double GetWcsTickStepAxisZ(const double wcsLength,
                                     int* numberOfPoints);

  ///
  /// Given a volume node, create a human
  /// readable string describing the contents
  virtual std::string GetPixelString(double *ijk);

  ///
  /// Given a coordinate of the volume and unit node,
  /// create a string if special formatting is required
  virtual std::string GetDisplayStringFromValue(const double world,
                                                vtkMRMLUnitNode *node,
                                                int precision,
                                                const char *language,
                                                const double oldOutputValues[3],
                                                double outputValues[3],
                                                bool additionalSpace = false);

  ///
  /// \brief GetDisplayStringFromValueAxes
  /// \param world
  /// \return Given a coordinate of the volume,
  /// create a string if special formatting is required

  virtual std::string GetDisplayStringFromValueX(const double world,
                                                 const double oldOutputValues[3],
                                                 double outputValues[3],
                                                 int precision = 0,
                                                 bool additionalSpace = false);
  virtual std::string GetDisplayStringFromValueY(const double world,
                                                 const double oldOutputValues[3],
                                                 double outputValues[3],
                                                 int precision = 0,
                                                 bool additionalSpace = false);
  virtual std::string GetDisplayStringFromValueZ(const double world,
                                                 const double oldOutputValues[3],
                                                 double outputValues[3],
                                                 int precision = 0,
                                                 bool additionalSpace = false);

  virtual std::string GetPythonDisplayStringFromValueX(const double world,
                                                       int precision = 0);
  virtual std::string GetPythonDisplayStringFromValueY(const double world,
                                                       int precision = 0);
  virtual std::string GetPythonDisplayStringFromValueZ(const double world,
                                                       int precision = 0);

  virtual std::string AddVelocityInfoToDisplayStringZ(std::string value);

protected:
  char* Space;
  vtkStringArray* SpaceQuantities;
  struct wcsprm* WCS;
  int WCSStatus;
  vtkDoubleArray* ContoursColor;
  static const double CONTOURSCOLOR_INVALID[3];

  bool FitSlices;

  double OpacityShift;
  double OldOpacityShift;

  double PresetOffset;
  double OldPresetOffset;

  double PresetStretch;
  double OldPresetStretch;

  vtkMRMLAstroVolumeDisplayNode();
  ~vtkMRMLAstroVolumeDisplayNode();
  vtkMRMLAstroVolumeDisplayNode(const vtkMRMLAstroVolumeDisplayNode&);
  void operator=(const vtkMRMLAstroVolumeDisplayNode&);
};

#endif

