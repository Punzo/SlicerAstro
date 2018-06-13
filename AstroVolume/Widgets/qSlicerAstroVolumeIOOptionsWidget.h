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

#ifndef __qSlicerAstroVolumeIOOptionsWidget_h
#define __qSlicerAstroVolumeIOOptionsWidget_h

// CTK includes
#include <ctkPimpl.h>

// SlicerQt includes
#include "qSlicerIOOptionsWidget.h"

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qSlicerAstroVolumeIOOptionsWidgetPrivate;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroVolumeIOOptionsWidget :
  public qSlicerIOOptionsWidget
{
  Q_OBJECT
public:
  qSlicerAstroVolumeIOOptionsWidget(QWidget *parent=0);
  virtual ~qSlicerAstroVolumeIOOptionsWidget();

public slots:
  virtual void setFileName(const QString& fileName);
  virtual void setFileNames(const QStringList& fileNames);

protected slots:
  /// Update the name, labelmap, center, singleFile, discardOrientation,
  /// colorNodeID properties
  void updateProperties();
  /// Update the color node selection to the default label map
  /// or volume color node depending on the label map checkbox state.
  void updateColorSelector();

private:
  Q_DECLARE_PRIVATE_D(qGetPtrHelper(qSlicerIOOptions::d_ptr), qSlicerAstroVolumeIOOptionsWidget);
  Q_DISABLE_COPY(qSlicerAstroVolumeIOOptionsWidget);
};

#endif
