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

#ifndef __qSlicerAstroMapPreview_h
#define __qSlicerAstroMapPreview_h

// Qt includes
#include <QPointF>
#include <QScopedPointer>
#include <QWidget>

// CTK includes
#include <ctkVTKObject.h>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qSlicerAstroMapPreviewPrivate;
class vtkMRMLNode;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// A map (e.g. a moment-0 map, north up) with a markups line or curve drawn
/// over it: drag an end or a point to move it, drag the centre of a line to
/// move the whole line. With pointAdding (default), a click on the map adds
/// a point at the end of a curve and a right click on a point of a curve
/// removes it. The positions of the markups are on the plane of the map
/// (used by the PV slice and PV diagram panels).
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroMapPreview : public QWidget
{
  Q_OBJECT
  QVTK_OBJECT
  Q_PROPERTY(bool pointAdding READ pointAdding WRITE setPointAdding)

public:
  explicit qSlicerAstroMapPreview(QWidget* parent = nullptr);
  ~qSlicerAstroMapPreview() override;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;
  bool hasHeightForWidth() const override;
  int heightForWidth(int width) const override;

  /// Widget position of a point of the map (pixel coordinates i, j);
  /// (-1, -1) without map
  Q_INVOKABLE QPointF mapToWidget(double i, double j) const;
  /// Pixel coordinates (i, j) of the map at a widget position
  Q_INVOKABLE QPointF widgetToMap(const QPointF& position) const;

  /// Points added (click) and removed (right click) on a curve
  bool pointAdding() const;
  void setPointAdding(bool adding);

public slots:
  /// The map (2D astronomical volume)
  void setVolumeNode(vtkMRMLNode* node);
  /// The markups line or curve drawn over the map
  void setMarkupsNode(vtkMRMLNode* node);

protected slots:
  void onDataModified();

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

  QScopedPointer<qSlicerAstroMapPreviewPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroMapPreview);
  Q_DISABLE_COPY(qSlicerAstroMapPreview);
};

#endif
