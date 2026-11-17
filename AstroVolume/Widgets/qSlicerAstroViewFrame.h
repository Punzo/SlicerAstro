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

#ifndef __qSlicerAstroViewFrame_h
#define __qSlicerAstroViewFrame_h

// Qt includes
#include <QPointer>
#include <QWidget>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Frame of a view of the Workbench, drawn over the view (a transparent
/// overlay, mouse events go through to the view):
/// - rounded corners in the colour around the views;
/// - the border of the active view (amber);
/// - the drop target while data are dragged over the view (cyan).
/// It reports a click in the view (activated) and the data dropped on it
/// (MIME type mimeType(): the ID of the MRML node).
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroViewFrame
  : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(bool active READ isActive WRITE setActive)
  Q_PROPERTY(double cornerRadius READ cornerRadius WRITE setCornerRadius)
  Q_PROPERTY(QString cornerToken READ cornerToken WRITE setCornerToken)

public:
  explicit qSlicerAstroViewFrame(QWidget* parent = nullptr);
  ~qSlicerAstroViewFrame() override;

  /// MIME type of the data dragged from the Datasets drawer (node ID)
  static QString mimeType();

  /// Drawn over a container (e.g. a slice widget: its bar, the view and
  /// the slider), with the clicks and drops of its view
  Q_INVOKABLE void attach(QWidget* container, QWidget* view);
  Q_INVOKABLE QWidget* view() const;

  bool isActive() const;
  void setActive(bool active);

  /// Compass of a sky view: directions of north and east on the screen
  /// (unit vectors, y down); hidden when visible is false
  Q_INVOKABLE void setCompass(bool visible, double northX, double northY, double eastX, double eastY);
  /// Angular scale bar: length in pixels of the view and its label (e.g.
  /// "5′"); hidden for a length of 0
  Q_INVOKABLE void setScaleBar(double pixels, const QString& label);
  double cornerRadius() const;
  void setCornerRadius(double radius);
  /// Theme token of the colour around the views (default "bg")
  QString cornerToken() const;
  void setCornerToken(const QString& token);

signals:
  /// Mouse press in the view
  void activated();
  /// Data dropped on the view
  void nodeDropped(const QString& nodeID);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

  QPointer<QWidget> Container;
  QPointer<QWidget> View;
  bool Active{false};
  bool DropTarget{false};
  double CornerRadius{8.};
  bool CompassVisible{false};
  double North[2]{0., -1.};
  double East[2]{-1., 0.};
  double ScaleBarPixels{0.};
  QString ScaleBarLabel;
  QString CornerToken{"bg"};
};

#endif
