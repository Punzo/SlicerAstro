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

#ifndef __qSlicerAstroLabelMapVolumeDisplayWidget_h
#define __qSlicerAstroLabelMapVolumeDisplayWidget_h

// Qt includes
#include <QWidget>

// CTK includes
#include <ctkVTKObject.h>

// SlicerQt includes
#include <qSlicerWidget.h>

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class vtkMRMLNode;
class vtkMRMLAstroLabelMapVolumeNode;
class vtkMRMLAstroLabelMapVolumeDisplayNode;
class qSlicerAstroLabelMapVolumeDisplayWidgetPrivate;

/// \ingroup Slicer_QtModules_AstroVolume_Widgets
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroLabelMapVolumeDisplayWidget : public qSlicerWidget
{
  Q_OBJECT
  QVTK_OBJECT
public:
  /// Constructors
  typedef qSlicerWidget Superclass;
  explicit qSlicerAstroLabelMapVolumeDisplayWidget(QWidget* parent);
  virtual ~qSlicerAstroLabelMapVolumeDisplayWidget();

  vtkMRMLAstroLabelMapVolumeNode* volumeNode()const;
  vtkMRMLAstroLabelMapVolumeDisplayNode* volumeDisplayNode()const;

  int sliceIntersectionThickness()const;

public slots:

  /// Set the MRML node of interest
  void setMRMLVolumeNode(vtkMRMLAstroLabelMapVolumeNode* volumeNode);
  void setMRMLVolumeNode(vtkMRMLNode* node);

  void setColorNode(vtkMRMLNode* colorNode);

  void setSliceIntersectionThickness(int);

protected slots:
  void updateWidgetFromMRML();

protected:
  QScopedPointer<qSlicerAstroLabelMapVolumeDisplayWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroLabelMapVolumeDisplayWidget);
  Q_DISABLE_COPY(qSlicerAstroLabelMapVolumeDisplayWidget);
};

#endif
