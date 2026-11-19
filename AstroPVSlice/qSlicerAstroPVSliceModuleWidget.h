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

#ifndef __qSlicerAstroPVSliceModuleWidget_h
#define __qSlicerAstroPVSliceModuleWidget_h

// CTK includes
#include <ctkVTKObject.h>

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroPVSliceModuleExport.h"

class qSlicerAstroPVSliceModuleWidgetPrivate;
class vtkMRMLNode;
class vtkObject;

/// \ingroup SlicerAstro_QtModules_AstroPVSlice
/// Position-velocity slice through a cube along a straight line on the sky.
/// The line is drawn on the moment-0 map of the cube (the map shared by the
/// modules, <cube>_mom0) in the panel preview and in the red view; it is
/// moved by dragging (ends, centre) or by its centre, position angle and
/// length on the sky. The yellow view shows the PV slice, the 3D view the
/// cube from the sky, along the line or facing the PV plane. The behaviour
/// is in vtkSlicerAstroPVSliceLogic.
class Q_SLICERASTRO_QTMODULES_ASTROPVSLICE_EXPORT qSlicerAstroPVSliceModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroPVSliceModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroPVSliceModuleWidget();

  void enter() override;
  void exit() override;

  /// Centre (RA, Dec in degrees), position angle (degrees, North to East)
  /// and length (arcmin) of the line; empty without line
  Q_INVOKABLE QList<double> lineSky() const;

public slots:
  void setMRMLScene(vtkMRMLScene*) override;
  /// The cube of the PV slice
  void setInputVolume(vtkMRMLNode* node);
  /// The line on the sky (centre in degrees, position angle in degrees from
  /// North to East, length in arcmin)
  void setLineSky(double rightAscension, double declination, double positionAngle, double length);
  /// The line centred on the peak of the moment map
  void centerOnPeak();
  /// The line along the major axis of the emission
  void alignWithMajorAxis();
  /// 3D view: 0 sky, 1 along the line, 2 facing the PV plane
  void setCameraView(int view);

protected slots:
  void setMRMLAstroPVSliceParametersNode(vtkMRMLNode* node);
  void onParametersNodeModified();
  void onLineModified();
  void onSkyFieldEdited();
  void onSelectionNodeModified();
  void onEndCloseEvent();
  void onEndImportEvent();

protected:
  QScopedPointer<qSlicerAstroPVSliceModuleWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroPVSliceModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroPVSliceModuleWidget);
};

#endif
