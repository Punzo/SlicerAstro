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

#ifndef __qMRMLAstroVolumeInfoWidget_h
#define __qMRMLAstroVolumeInfoWidget_h


// CTK includes
#include <ctkVTKObject.h>

// qMRML includes
#include "qMRMLWidget.h"

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qMRMLAstroVolumeInfoWidgetPrivate;
class vtkMRMLNode;
class vtkMRMLVolumeNode;

/// \ingroup Slicer_QtModules_AstroVolume_Widgets
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qMRMLAstroVolumeInfoWidget : public qMRMLWidget
{
  Q_OBJECT
  QVTK_OBJECT
public:
  typedef qMRMLWidget Superclass;

  qMRMLAstroVolumeInfoWidget(QWidget *parent=0);
  virtual ~qMRMLAstroVolumeInfoWidget();

  vtkMRMLVolumeNode* volumeNode()const;
  // Depends on the dimension, spacing and origin of the volume
  bool isCentered()const;

  // Disabled by default
  bool isDataTypeEditable()const;

public slots:
  /// Utility function to be connected with generic signals
  void setVolumeNode(vtkMRMLNode *node);
  /// Set the volume node to display
  void setVolumeNode(vtkMRMLVolumeNode *node);

  void setImageSpacing(double*);
  void setImageOrigin(double*);
  void center();
  void setNumberOfScalars(int);

protected slots:
  void updateWidgetFromMRML();

protected:
  QScopedPointer<qMRMLAstroVolumeInfoWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLAstroVolumeInfoWidget);
  Q_DISABLE_COPY(qMRMLAstroVolumeInfoWidget);
};

#endif
