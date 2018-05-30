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

#ifndef __qMRMLSliceAstroControllerWidget_h
#define __qMRMLSliceAstroControllerWidget_h

// qMRMLWidget includes
#include "qMRMLSliceControllerWidget.h"
#include <vtkVersion.h>

class qMRMLSliceAstroControllerWidgetPrivate;

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

/// \ingroup Slicer_QtModules_AstroVolume_Widgets
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qMRMLSliceAstroControllerWidget
  : public qMRMLSliceControllerWidget
{
  Q_OBJECT
public:
  /// Superclass typedef
  typedef qMRMLSliceControllerWidget Superclass;

  /// Constructors
  explicit qMRMLSliceAstroControllerWidget(QWidget* parent = 0);
  virtual ~qMRMLSliceAstroControllerWidget();

public slots:

  /// Set the display of the WCS coordinate on the slice.
  void setWCSDisplay();

protected:
  qMRMLSliceAstroControllerWidget(qMRMLSliceAstroControllerWidgetPrivate* pimpl, QWidget* parent = 0);

private:
  Q_DECLARE_PRIVATE(qMRMLSliceAstroControllerWidget);
  Q_DISABLE_COPY(qMRMLSliceAstroControllerWidget);
};

#endif
