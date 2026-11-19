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

==============================================================================*/


#ifndef __qSlicerAstroScrollAreaFitter_h
#define __qSlicerAstroScrollAreaFitter_h

// Qt includes
#include <QObject>
#include <QPointer>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class QScrollArea;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Keeps a scroll area at least as wide as the minimum width of its contents
/// plus the vertical scroll bar, so that the scroll bar never covers the
/// contents (a scroll area clips contents wider than its viewport: when the
/// vertical scroll bar appears, the viewport becomes narrower). Only while
/// the scroll area is visible: hidden pages (e.g. of a stacked widget) do not
/// widen their parent. Created with the scroll area as parent:
/// new qSlicerAstroScrollAreaFitter(scrollArea) (Python:
/// slicer.qSlicerAstroScrollAreaFitter(scrollArea)).
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroScrollAreaFitter : public QObject
{
  Q_OBJECT

public:
  /// Fits the parent if it is a scroll area (see setScrollArea)
  explicit qSlicerAstroScrollAreaFitter(QObject* parent = nullptr);
  ~qSlicerAstroScrollAreaFitter() override;

  /// The fitter of a scroll area (created if needed)
  static qSlicerAstroScrollAreaFitter* install(QScrollArea* scrollArea);

  /// The scroll area kept wide enough
  Q_INVOKABLE void setScrollArea(QScrollArea* scrollArea);

  /// Minimum width of the scroll area for its contents and the scroll bar
  Q_INVOKABLE int requiredWidth() const;

public slots:
  /// Minimum width of the scroll area updated (contents changed)
  void fit();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

  QPointer<QScrollArea> ScrollArea;
  QPointer<QWidget> Contents;
  bool Fitting{false};
};

#endif
