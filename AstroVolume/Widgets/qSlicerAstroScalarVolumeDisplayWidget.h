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

#ifndef __qSlicerAstroScalarVolumeDisplayWidget_h
#define __qSlicerAstroScalarVolumeDisplayWidget_h

// Qt includes
#include <QWidget>

// CTK includes
#include <ctkVTKObject.h>

// SlicerQt includes
#include <qSlicerWidget.h>

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class vtkMRMLNode;
class vtkMRMLAstroVolumeDisplayNode;
class vtkMRMLAstroVolumeNode;
class vtkMRMLColorNode;
class vtkMRMLSegmentationNode;
class qSlicerAstroScalarVolumeDisplayWidgetPrivate;
class vtkImageData;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroScalarVolumeDisplayWidget : public qSlicerWidget
{
  Q_OBJECT
  QVTK_OBJECT
  Q_PROPERTY(bool enableColorTableComboBox READ isColorTableComboBoxEnabled WRITE setColorTableComboBoxEnabled )
  Q_PROPERTY(bool enableMRMLWindowLevelWidget READ isMRMLWindowLevelWidgetEnabled WRITE setMRMLWindowLevelWidgetEnabled )
public:
  /// Constructors
  typedef qSlicerWidget Superclass;
  explicit qSlicerAstroScalarVolumeDisplayWidget(QWidget* parent);
  virtual ~qSlicerAstroScalarVolumeDisplayWidget();

  /// Return the current AstroVolume node
  vtkMRMLAstroVolumeNode* volumeNode()const;

  /// Return the current AstroVolume display node
  vtkMRMLAstroVolumeDisplayNode* volumeDisplayNode()const;

  /// Return the current AstroVolume imagedata
  vtkImageData* volumeImageData()const;

  bool isColorTableComboBoxEnabled()const;
  void setColorTableComboBoxEnabled(bool);

  bool isMRMLWindowLevelWidgetEnabled()const;
  void setMRMLWindowLevelWidgetEnabled(bool);

public slots:

  /// Utility function to be connected with generic signals
  void setMRMLVolumeNode(vtkMRMLNode* node);

  /// Set the volume node to display
  void setMRMLVolumeNode(vtkMRMLAstroVolumeNode* volumeNode);

  /// Set the volume node to contour
  void onContoursVolumeChanged(vtkMRMLNode *node);

  /// Set the color node for 2D color function
  void setColorNode(vtkMRMLNode* colorNode);

  /// Set the color for contours
  void onColorChanged(QColor color);
  void onContours2DOriginChanged(double value);

  /// Generate closed contours (see tooltip for the options)
  void onCreateContours();

  /// Method to change the display visualization of the 2D slices.
  void ExtendAllSlices();
  void onFitSlicesToViewsChanged(bool toggled);
  void setInterpolate(bool interpolate);

  /// 2D color function proprieties
  void setInverse(bool toggled);
  void setLog(bool toggled);
  void setReverse(bool toggled);

  /// customize popup behaviour of double slider for 2D color function
  void onWindowLevelPopupShow(bool show);
  void onWindowLevelPopupShow(int);
  void setThreshold(bool threshold);

protected slots:

  /// Update widget GUI from MRML AstroVolume node
  void updateWidgetFromActiveVolumeMRML();

  /// Update widget GUI from MRML AstroVolume node to contour
  void updateWidgetFromContoursMRML();

  /// Update widget GUI from MRML AstroVolume display node
  void updateWidgetFromDisplayMRML();

  /// Update widget GUI from MRML AstroVolume display node
  void onFitSlicesModified();

  /// Update 2D color function
  void updateTransferFunction();

protected:
  void InvertColorFunction(vtkMRMLColorNode *colorNode);
  void ReverseColorFunction(vtkMRMLColorNode *colorNode);
  void showEvent(QShowEvent * event);
  QScopedPointer<qSlicerAstroScalarVolumeDisplayWidgetPrivate> d_ptr;
  vtkMRMLSegmentationNode* contoursSegNode;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroScalarVolumeDisplayWidget);
  Q_DISABLE_COPY(qSlicerAstroScalarVolumeDisplayWidget);
};

#endif
