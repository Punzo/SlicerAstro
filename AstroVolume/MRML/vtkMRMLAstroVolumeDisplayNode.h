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

/// \brief MRML node for representing a volume display attributes.
///
/// vtkMRMLAstroVolumeDisplayNode nodes describe how volume is displayed.
/// \ingroup SlicerAstro_QtModules_AstroVolumeNode
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroVolumeDisplayNode : public vtkMRMLScalarVolumeDisplayNode
{
  public:
  static vtkMRMLAstroVolumeDisplayNode *New();
  vtkTypeMacro(vtkMRMLAstroVolumeDisplayNode,vtkMRMLScalarVolumeDisplayNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  virtual vtkMRMLNode* CreateNodeInstance() override;

  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) override;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) override;

  /// Copy WCS to this object
  virtual void CopyWCS(vtkMRMLNode *node);

  /// Copy only the spatial WCS information to this object
  virtual void CopySpatialWCS(vtkMRMLNode *node);

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() override {return "AstroVolumeDisplay";};

  /// Set/Get the CoordinateSystem.
  /// Default is "WCS"
  /// \sa SetSpace(), GetSpace()
  vtkGetStringMacro(Space);
  vtkSetStringMacro(Space);

  /// Set/Get the SpaceQuantities.
  /// The default is 3 and the values are "time; length; velocity"
  /// \sa SetSpaceQuantities(), GetSpaceQuantities()
  vtkGetObjectMacro(SpaceQuantities, vtkStringArray);
  vtkSetObjectMacro(SpaceQuantities, vtkStringArray);

  /// Set the i-th SpaceQunatity name
  bool SetSpaceQuantity(int ind, const char *name);

  /// Set/Get the ContoursColor
  /// \sa SetContoursColor(), GetContoursColor()
  vtkGetObjectMacro(ContoursColor, vtkDoubleArray);
  vtkSetObjectMacro(ContoursColor, vtkDoubleArray);

  /// Set the i-th SpaceQunatity name
  void SetContoursColor(int ind, double color);

  /// Set FitSlices
  virtual void SetFitSlices(bool fit);

  /// Get FitSlices
  vtkGetMacro(FitSlices,bool);

  enum
     {
     FitSlicesModifiedEvent = 123000,
     };

  /// Set WCSStruct
  virtual void SetWCSStruct(struct wcsprm*);

  ///Get WCSStruct
  virtual struct wcsprm* GetWCSStruct();

  /// Set/Get the WCSStatus.
  /// \sa SetWCSStatus(), GetWCSStatus()
  vtkSetMacro(WCSStatus,int);
  vtkGetMacro(WCSStatus,int);

  /// Set/Get the OpacityShift.
  /// \sa SetOpacityShift(), GetOpacityShift()
  vtkSetMacro(OpacityShift,double);
  vtkGetMacro(OpacityShift,double);

  /// Set/Get the OldOpacityShift.
  /// \sa SetOldOpacityShift(), GetOldOpacityShift()
  vtkSetMacro(OldOpacityShift,double);
  vtkGetMacro(OldOpacityShift,double);

  /// Set/Get the PresetOffset.
  /// \sa SetPresetOffset(), GetPresetOffset()
  vtkSetMacro(PresetOffset,double);
  vtkGetMacro(PresetOffset,double);

  /// Set/Get the OldPresetOffset.
  /// \sa SetOldPresetOffset(), GetOldPresetOffset()
  vtkSetMacro(OldPresetOffset,double);
  vtkGetMacro(OldPresetOffset,double);

  /// Set/Get the PresetStretch.
  /// \sa SetPresetStretch(), GetPresetStretch()
  vtkSetMacro(PresetStretch,double);
  vtkGetMacro(PresetStretch,double);

  /// Set/Get the OldPresetStretch.
  /// \sa SetOldPresetStretch(), GetOldPresetStretch()
  vtkSetMacro(OldPresetStretch,double);
  vtkGetMacro(OldPresetStretch,double);

  /// Set radio as velocity definition
  virtual bool SetRadioVelocityDefinition(bool update = true);

  /// Set optical as velocity definition
  virtual bool SetOpticalVelocityDefinition(bool update = true);

  /// Get the velocity definition currently in use
  virtual std::string GetVelocityDefinition();

  /// Get WCS Coordinates from IJK
  virtual bool GetReferenceSpace(const double ijk[3],
                                 double SpaceCoordinates[3]);

  /// Get IJK Coordinates from WCS
  virtual bool GetIJKSpace(const double SpaceCoordinates[3],
                           double ijk[3]);

  /// Get IJK Coordinates from WCS
  virtual bool GetIJKSpace(std::vector<double> SpaceCoordinates,
                           double ijk[3]);

  /// Get the first tick for the display of axes at given unitNode in WCS units
  virtual double GetFirstWcsTickAxis(const double worldA, const double worldB,
                                     const double wcsStep, vtkMRMLUnitNode *node);

  /// Get the first tick for the display of the axis X in WCS units
  virtual double GetFirstWcsTickAxisX(const double worldA, const double worldB, const double wcsStep);

  /// Get the first tick for the display of the axis Y in WCS units
  virtual double GetFirstWcsTickAxisY(const double worldA, const double worldB, const double wcsStep);

  /// Get the first tick for the display of the axis Z in WCS units
  virtual double GetFirstWcsTickAxisZ(const double worldA, const double worldB, const double wcsStep);

  /// Get the ticks step for the display of axes at given unitNode in WCS units
  virtual double GetWcsTickStepAxis(const double wcsLength,
                                    int *numberOfPoints,
                                    vtkMRMLUnitNode *node);

  /// Get the ticks step for the display of the axis X in WCS units
  virtual double GetWcsTickStepAxisX(const double wcsLength,
                                     int* numberOfPoints);

  /// Get the ticks step for the display of the axis Y in WCS units
  virtual double GetWcsTickStepAxisY(const double wcsLength,
                                     int* numberOfPoints);

  /// Get the ticks step for the display of the axis Z in WCS units
  virtual double GetWcsTickStepAxisZ(const double wcsLength,
                                     int* numberOfPoints);

  /// Given a volume node, create a human
  /// readable string describing the contents
  virtual std::string GetPixelString(double *ijk);

  /// \brief Get display string from a value
  /// \param world wcs coordinate
  /// \param unit node
  /// \param decimal precision
  /// \param language (C++ or Python)
  /// create a string if special formatting is required
  virtual std::string GetDisplayStringFromValue(const double world,
                                                vtkMRMLUnitNode *node,
                                                int precision,
                                                const double oldOutputValues[3],
                                                double outputValues[3],
                                                bool additionalSpace = false);

  /// \brief Get display string from a value for the axes X
  /// \param world wcs coordinate
  /// \return Given a coordinate of the volume
  /// create a string if special formatting is required (C++ method)
  virtual std::string GetDisplayStringFromValueX(const double world,
                                                 const double oldOutputValues[3],
                                                 double outputValues[3],
                                                 int precision = 0,
                                                 bool additionalSpace = false);

  /// \brief Get display string from a value for the axes Y
  /// \param world wcs coordinate
  /// \return Given a coordinate of the volume
  /// create a string if special formatting is required (C++ method)
  virtual std::string GetDisplayStringFromValueY(const double world,
                                                 const double oldOutputValues[3],
                                                 double outputValues[3],
                                                 int precision = 0,
                                                 bool additionalSpace = false);

  /// \brief Get display string from a value for the axes Z
  /// \param world wcs coordinate
  /// \return Given a coordinate of the volume
  /// create a string if special formatting is required (C++ method)
  virtual std::string GetDisplayStringFromValueZ(const double world,
                                                 const double oldOutputValues[3],
                                                 double outputValues[3],
                                                 int precision = 0,
                                                 bool additionalSpace = false);

  /// \brief add velocity type to a display string from a value for the axes Z
  /// \param string value (alrady fromatted by GetDisplayStringFromValue)
  /// \return string value formatted with velocity information
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
  ~vtkMRMLAstroVolumeDisplayNode() override;
  vtkMRMLAstroVolumeDisplayNode(const vtkMRMLAstroVolumeDisplayNode&);
  void operator=(const vtkMRMLAstroVolumeDisplayNode&);
};

#endif

