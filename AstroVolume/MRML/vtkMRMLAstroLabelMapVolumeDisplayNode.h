#ifndef __vtkMRMLAstroLabelMapVolumeDisplayNode_h
#define __vtkMRMLAstroLabelMapVolumeDisplayNode_h

// MRML includes
#include "vtkMRMLLabelMapVolumeDisplayNode.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

// VTK includes
#include "vtkStringArray.h"

// WCS includes
#include "wcslib.h"

class vtkImageAlgorithm;
class vtkImageMapToColors;
class vtkMRMLUnitNode;

/// \brief MRML node for representing a volume display attributes.
///
/// vtkMRMLAstroLabelMapVolumeDisplayNode nodes describe how volume is displayed.
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroLabelMapVolumeDisplayNode : public vtkMRMLLabelMapVolumeDisplayNode
{
  public:
  static vtkMRMLAstroLabelMapVolumeDisplayNode *New();
  vtkTypeMacro(vtkMRMLAstroLabelMapVolumeDisplayNode,vtkMRMLLabelMapVolumeDisplayNode);
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual vtkMRMLNode* CreateNodeInstance();

  ///
  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts);

  ///
  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent);

  ///
  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node);

  ///
  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() {return "AstroLabelMapVolumeDisplay";};

  ///
  /// Set/Get the CoordinateSystem.
  /// Default is "WCS".
  /// \sa SetSpace(), GetSpace()
  vtkGetStringMacro(Space);
  vtkSetStringMacro(Space);

  ///
  /// Set/Get the SpaceQuantities.
  /// The default is 3 and the values are "length; length; velocity".
  /// \sa SetSpaceQuantities(), GetSpaceQuantities()
  vtkGetObjectMacro(SpaceQuantities, vtkStringArray);
  vtkSetObjectMacro(SpaceQuantities, vtkStringArray);

  ///
  /// Set the i-th SpaceQunatity name
  int SetSpaceQuantity(int ind, const char *name);

  ///
  ///Set WCSStruct
  virtual void SetWCSStruct(struct wcsprm*);

  ///
  ///WcsStatus
  vtkSetMacro(WCSStatus,int);
  vtkGetMacro(WCSStatus,int);

  ///
  ///Get WCSCoordinates
  virtual void GetReferenceSpace(const double ijk[3],
                                 double SpaceCoordinates[3]);

  ///
  /// Get IJK Coordinates from WCS
  virtual void GetIJKSpace(const double SpaceCoordinates[3],
                                 double ijk[3]);

  ///
  /// Get the first tick for the display of axes at given unitNode in WCS units
  double GetFirstWcsTickAxis(const double worldA, const double worldB,
                             const double wcsStep, vtkMRMLUnitNode *node);

  ///
  /// Get the first tick for the display of the axis X in WCS units
  double GetFirstWcsTickAxisX(const double worldA, const double worldB, const double wcsStep);

  ///
  /// Get the first tick for the display of the axis Y in WCS units
  double GetFirstWcsTickAxisY(const double worldA, const double worldB, const double wcsStep);

  ///
  /// Get the first tick for the display of the axis Z in WCS units
  double GetFirstWcsTickAxisZ(const double worldA, const double worldB, const double wcsStep);

  ///
  /// Get the ticks step for the display of axes at given unitNode in WCS units
  double GetWcsTickStepAxis(const double wcsLength,
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
  /// create a string (for DAtaProve display)
  /// if special formatting is required
  virtual const char* GetDisplayStringFromValue(const double world,
                                                vtkMRMLUnitNode *node);

  ///
  /// \brief GetDisplayStringFromValueAxes
  /// \param world
  /// \return Given a coordinate of the volume, create a string if special formatting is required
  ///
  virtual const char *GetDisplayStringFromValueX(const double world);
  virtual const char *GetDisplayStringFromValueY(const double world);
  virtual const char *GetDisplayStringFromValueZ(const double world);

  ///
  /// \brief GetAxisDisplayStringFromValue, same as GetDisplayStringFromValue but for display on axis
  /// \param world
  /// \param node
  /// \return string to display
  ///
  virtual const char *GetAxisDisplayStringFromValue(const double world, vtkMRMLUnitNode *node);

  virtual const char *GetAxisDisplayStringFromValueX(const double world);
  virtual const char *GetAxisDisplayStringFromValueY(const double world);
  virtual const char *GetAxisDisplayStringFromValueZ(const double world);

protected:
  char* Space;
  vtkStringArray* SpaceQuantities;
  struct wcsprm* WCS;
  int WCSStatus;

  vtkMRMLAstroLabelMapVolumeDisplayNode();
  virtual ~vtkMRMLAstroLabelMapVolumeDisplayNode();
  vtkMRMLAstroLabelMapVolumeDisplayNode(const vtkMRMLAstroLabelMapVolumeDisplayNode&);
  void operator=(const vtkMRMLAstroLabelMapVolumeDisplayNode&);
};

#endif
