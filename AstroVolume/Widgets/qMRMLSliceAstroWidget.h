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

#ifndef __qMRMLSliceAstroWidget_h
#define __qMRMLSliceAstroWidget_h


// qMRMLWidget includes
#include "qMRMLSliceWidget.h"

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qMRMLSliceAstroWidgetPrivate;
class qMRMLSliceAstroControllerWidget;

/// \ingroup Slicer_QtModules_AstroVolume_Widgets
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qMRMLSliceAstroWidget
    : public qMRMLSliceWidget
{
  Q_OBJECT

public:
  /// Superclass typedef
  typedef qMRMLSliceWidget Superclass;

  /// Constructors
  explicit qMRMLSliceAstroWidget(QWidget* parent = 0);
  virtual ~qMRMLSliceAstroWidget();

signals:
  void windowsResized();

protected:
  qMRMLSliceAstroWidget(qMRMLSliceAstroWidgetPrivate* pimpl, QWidget* parent = 0);

   void resizeEvent(QResizeEvent *event) override;

private:
  Q_DECLARE_PRIVATE(qMRMLSliceAstroWidget);
  Q_DISABLE_COPY(qMRMLSliceAstroWidget);
};

#endif
