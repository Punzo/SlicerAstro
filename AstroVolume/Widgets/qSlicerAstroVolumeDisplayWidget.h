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

#ifndef __qSlicerAstroVolumeDisplayWidget_h
#define __qSlicerAstroVolumeDisplayWidget_h

// Qt includes
#include <QStackedWidget>

// CTK includes
#include <ctkVTKObject.h>

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class vtkMRMLNode;
class vtkMRMLScene;
class qSlicerAstroVolumeDisplayWidgetPrivate;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroVolumeDisplayWidget : public QStackedWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  /// Constructors
  typedef QStackedWidget Superclass;
  explicit qSlicerAstroVolumeDisplayWidget(QWidget* parent=0);
  virtual ~qSlicerAstroVolumeDisplayWidget();

public slots:
  /// Set the MRML node of interest
  void setMRMLVolumeNode(vtkMRMLNode* node);

protected slots:
  /// Internally use in case the current display widget should change when the
  /// volume node changes (typically if the LabelMap attribute is changed)
  void updateFromMRML(vtkObject* volume);
protected:
  QScopedPointer<qSlicerAstroVolumeDisplayWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroVolumeDisplayWidget);
  Q_DISABLE_COPY(qSlicerAstroVolumeDisplayWidget);
};

#endif
