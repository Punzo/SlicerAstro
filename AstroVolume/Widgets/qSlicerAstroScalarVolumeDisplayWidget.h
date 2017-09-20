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
class qSlicerAstroScalarVolumeDisplayWidgetPrivate;
class vtkImageData;

/// \ingroup Slicer_QtModules_AstroVolume_Widgets
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroScalarVolumeDisplayWidget : public qSlicerWidget
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

  vtkMRMLAstroVolumeNode* volumeNode()const;
  vtkMRMLAstroVolumeDisplayNode* volumeDisplayNode()const;
  vtkImageData* volumeImageData()const;

  bool isColorTableComboBoxEnabled()const;
  void setColorTableComboBoxEnabled(bool);

  bool isMRMLWindowLevelWidgetEnabled()const;
  void setMRMLWindowLevelWidgetEnabled(bool);

public slots:

  ///
  /// Set the MRML node of interest
  void setMRMLVolumeNode(vtkMRMLAstroVolumeNode* volumeNode);
  void setMRMLVolumeNode(vtkMRMLNode* node);

  void setInterpolate(bool interpolate);
  void setColorNode(vtkMRMLNode* colorNode);

protected slots:
  void updateWidgetFromMRML();
  void updateTransferFunction();

protected:
  void showEvent(QShowEvent * event);
protected:
  QScopedPointer<qSlicerAstroScalarVolumeDisplayWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroScalarVolumeDisplayWidget);
  Q_DISABLE_COPY(qSlicerAstroScalarVolumeDisplayWidget);
};

#endif
