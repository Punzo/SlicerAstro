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
  and was supported through the European Research Consil grant nr. 291531.

==============================================================================*/

#ifndef __qSlicerAstroVolumeModuleWidget_h
#define __qSlicerAstroVolumeModuleWidget_h

// CTK includes
#include <ctkVTKObject.h>

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qSlicerAstroVolumeModuleWidgetPrivate;
class vtkMRMLNode;
class vtkMRMLAstroVolumeNode;
class vtkMRMLAstroLabelMapVolumeNode;
class qSlicerVolumeRenderingModuleWidget;
class vtkMRMLVolumeRenderingDisplayNode;
class qMRMLAstroVolumeInfoWidget;
class qSlicerAstroVolumeDisplayWidget;

/// \ingroup Slicer_QtModules_AstroVolume_Widgets
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroVolumeModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT
  QVTK_OBJECT
public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroVolumeModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroVolumeModuleWidget();

  /// Get volumeRenderingWidget
  Q_INVOKABLE qSlicerVolumeRenderingModuleWidget* volumeRenderingWidget()const;

  /// Get columeRenderingDisplay
  Q_INVOKABLE vtkMRMLVolumeRenderingDisplayNode* volumeRenderingDisplay()const;

  /// Get AstroVolumeInfoWidget
  Q_INVOKABLE qMRMLAstroVolumeInfoWidget* astroVolumeInfoWidget()const;

  /// Get AstroVolumeInfoWidget
  Q_INVOKABLE qSlicerAstroVolumeDisplayWidget* astroVolumeDisplayWidget()const;

public slots:
  void onVisibilityChanged(bool visibility);
  void setComparative3DViews(const char* volumeNodeOneID,
                             const char* volumeNodeTwoID);
  void stopRockView();
  /// Set the MRML node of interest
  void setMRMLVolumeNode(vtkMRMLNode* node);
  void setMRMLVolumeNode(vtkMRMLAstroVolumeNode* volumeNode);
  void setMRMLVolumeNode(vtkMRMLAstroLabelMapVolumeNode* volumeNode);

protected slots:
  void resetOffset(vtkMRMLNode* node);
  void SetPresets(vtkMRMLNode* node);
  void onROICropDisplayCheckBoxToggled(bool toggle);
  void onMRMLVolumeRenderingDisplayNodeModified(vtkObject*);
  void onMRMLDisplayROINodeModified(vtkObject*);
  void onCurrentQualityControlChanged(int);
  void setDisplayROIEnabled(bool);
  void onCropToggled(bool);
  void onInputVolumeChanged(vtkMRMLNode *node);
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void setDisplayConnection(vtkMRMLNode* node);
  void clearPresets();

protected:
  virtual void setup();
  virtual void setMRMLScene(vtkMRMLScene*);
  QScopedPointer<qSlicerAstroVolumeModuleWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroVolumeModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroVolumeModuleWidget);
};

#endif
