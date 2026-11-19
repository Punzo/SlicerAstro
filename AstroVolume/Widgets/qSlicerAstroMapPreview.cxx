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

// Qt includes
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

// VTK includes
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkWeakPointer.h>

// MRML includes
#include <vtkMRMLMarkupsCurveNode.h>
#include <vtkMRMLMarkupsLineNode.h>
#include <vtkMRMLMarkupsNode.h>
#include <vtkMRMLVolumeNode.h>

#include <cmath>
#include <vector>

#include "qSlicerAstroMapPreview.h"
#include "qSlicerAstroTheme.h"
#include "qSlicerAstroThumbnails.h"

namespace
{
const double GrabTolerance = 8.;

//----------------------------------------------------------------------------
QPointF EventPosition(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->position();
#else
  return event->localPos();
#endif
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroMapPreviewPrivate
{
public:
  /// Rectangle of the map in the widget (aspect ratio of the map kept)
  QRectF mapRect(const QWidget* widget) const;
  int dimension(int axis) const;
  /// Pixel coordinates (i, j) of the control points of the markups
  std::vector<QPointF> points() const;
  void setPoint(int index, const QPointF& ij);

  vtkWeakPointer<vtkMRMLVolumeNode> Map;
  vtkWeakPointer<vtkMRMLMarkupsNode> Markups;
  QImage Image;
  /// Dragged point (-1: none, -2: the whole line)
  int Dragged{-1};
  QPointF DragStart;
  std::vector<QPointF> DragPoints;
};

//-----------------------------------------------------------------------------
int qSlicerAstroMapPreviewPrivate::dimension(int axis) const
{
  vtkImageData* image = this->Map ? this->Map->GetImageData() : nullptr;
  return image ? image->GetDimensions()[axis] : 0;
}

//-----------------------------------------------------------------------------
QRectF qSlicerAstroMapPreviewPrivate::mapRect(const QWidget* widget) const
{
  const int nx = this->dimension(0);
  const int ny = this->dimension(1);
  if (nx < 1 || ny < 1)
    {
    return QRectF();
    }
  const QRectF area = QRectF(widget->rect()).adjusted(1., 1., -1., -1.);
  const double scale = std::min(area.width() / nx, area.height() / ny);
  const QSizeF size(nx * scale, ny * scale);
  return QRectF(area.center() - QPointF(size.width() / 2., size.height() / 2.), size);
}

//-----------------------------------------------------------------------------
std::vector<QPointF> qSlicerAstroMapPreviewPrivate::points() const
{
  std::vector<QPointF> result;
  if (!this->Map || !this->Markups)
    {
    return result;
    }
  vtkNew<vtkMatrix4x4> rasToIJK;
  this->Map->GetRASToIJKMatrix(rasToIJK);
  for (int index = 0; index < this->Markups->GetNumberOfControlPoints(); index++)
    {
    if (!this->Markups->GetNthControlPointPositionStatus(index))
      {
      continue;
      }
    double ras[4] = {0., 0., 0., 1.};
    this->Markups->GetNthControlPointPosition(index, ras);
    double ijk[4] = {0., 0., 0., 1.};
    rasToIJK->MultiplyPoint(ras, ijk);
    result.push_back(QPointF(ijk[0], ijk[1]));
    }
  return result;
}

//-----------------------------------------------------------------------------
void qSlicerAstroMapPreviewPrivate::setPoint(int index, const QPointF& ij)
{
  vtkNew<vtkMatrix4x4> ijkToRAS;
  this->Map->GetIJKToRASMatrix(ijkToRAS);
  const double ijk[4] = {ij.x(), ij.y(), 0., 1.};
  double ras[4] = {0., 0., 0., 1.};
  ijkToRAS->MultiplyPoint(ijk, ras);
  this->Markups->SetNthControlPointPosition(index, ras[0], ras[1], ras[2]);
}

//-----------------------------------------------------------------------------
qSlicerAstroMapPreview::qSlicerAstroMapPreview(QWidget* parent)
  : QWidget(parent)
  , d_ptr(new qSlicerAstroMapPreviewPrivate)
{
  this->setObjectName("MapPreview");
  this->setMouseTracking(true);
  QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  policy.setHeightForWidth(true);
  this->setSizePolicy(policy);
  this->setToolTip(tr("The map with the line: drag its ends or its centre."));
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(update()));
}

//-----------------------------------------------------------------------------
qSlicerAstroMapPreview::~qSlicerAstroMapPreview() = default;

//-----------------------------------------------------------------------------
QSize qSlicerAstroMapPreview::sizeHint() const
{
  return QSize(280, this->heightForWidth(280));
}

//-----------------------------------------------------------------------------
QSize qSlicerAstroMapPreview::minimumSizeHint() const
{
  return QSize(120, 90);
}

//-----------------------------------------------------------------------------
bool qSlicerAstroMapPreview::hasHeightForWidth() const
{
  return true;
}

//-----------------------------------------------------------------------------
int qSlicerAstroMapPreview::heightForWidth(int width) const
{
  Q_D(const qSlicerAstroMapPreview);
  const int nx = d->dimension(0);
  const int ny = d->dimension(1);
  const int height = nx > 0 ? static_cast<int>(width * static_cast<double>(ny) / nx) : width / 2;
  return std::max(90, std::min(height, 320));
}

//-----------------------------------------------------------------------------
QPointF qSlicerAstroMapPreview::mapToWidget(double i, double j) const
{
  Q_D(const qSlicerAstroMapPreview);
  const QRectF rect = d->mapRect(this);
  const int nx = d->dimension(0);
  const int ny = d->dimension(1);
  if (rect.isEmpty())
    {
    return QPointF(-1., -1.);
    }
  // north up: the first row of the map at the bottom
  return QPointF(rect.left() + (i + 0.5) / nx * rect.width(), rect.bottom() - (j + 0.5) / ny * rect.height());
}

//-----------------------------------------------------------------------------
QPointF qSlicerAstroMapPreview::widgetToMap(const QPointF& position) const
{
  Q_D(const qSlicerAstroMapPreview);
  const QRectF rect = d->mapRect(this);
  const int nx = d->dimension(0);
  const int ny = d->dimension(1);
  if (rect.isEmpty())
    {
    return QPointF();
    }
  return QPointF((position.x() - rect.left()) / rect.width() * nx - 0.5,
                 (rect.bottom() - position.y()) / rect.height() * ny - 0.5);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMapPreview::setVolumeNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroMapPreview);
  vtkMRMLVolumeNode* map = vtkMRMLVolumeNode::SafeDownCast(node);
  this->qvtkReconnect(d->Map, map, vtkCommand::ModifiedEvent, this, SLOT(onDataModified()));
  d->Map = map;
  d->Image = QImage();
  this->updateGeometry();
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMapPreview::setMarkupsNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroMapPreview);
  vtkMRMLMarkupsNode* markups = vtkMRMLMarkupsNode::SafeDownCast(node);
  this->qvtkReconnect(d->Markups, markups, vtkMRMLMarkupsNode::PointModifiedEvent, this, SLOT(update()));
  this->qvtkReconnect(d->Markups, markups, vtkMRMLMarkupsNode::PointAddedEvent, this, SLOT(update()));
  this->qvtkReconnect(d->Markups, markups, vtkMRMLMarkupsNode::PointRemovedEvent, this, SLOT(update()));
  this->qvtkReconnect(d->Markups, markups, vtkCommand::ModifiedEvent, this, SLOT(update()));
  d->Markups = markups;
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMapPreview::onDataModified()
{
  Q_D(qSlicerAstroMapPreview);
  d->Image = QImage();
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMapPreview::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);
  Q_D(qSlicerAstroMapPreview);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  QPainterPath frame;
  frame.addRoundedRect(QRectF(this->rect()), 9., 9.);
  painter.fillPath(frame, theme->color("view"));
  painter.setClipPath(frame);
  const QRectF rect = d->mapRect(this);
  if (rect.isEmpty())
    {
    painter.setPen(theme->color("view-subtle"));
    painter.setFont(theme->font("label"));
    painter.drawText(this->rect(), Qt::AlignCenter, tr("No map"));
    return;
    }
  // the map at its aspect ratio (no crop)
  const int width = std::max(1, static_cast<int>(rect.width() * this->devicePixelRatioF()));
  const int height = std::max(1, static_cast<int>(rect.height() * this->devicePixelRatioF()));
  if (d->Image.width() != width || d->Image.height() != height)
    {
    d->Image = qSlicerAstroThumbnails::instance()->image(d->Map, width, height);
    }
  painter.drawImage(rect, d->Image);

  const std::vector<QPointF> points = d->points();
  if (points.empty())
    {
    return;
    }
  QPolygonF path;
  for (const QPointF& point : points)
    {
    path << this->mapToWidget(point.x(), point.y());
    }
  QPen shadow(QColor(0, 0, 0, 140), 4.);
  painter.setPen(shadow);
  painter.drawPolyline(path);
  painter.setPen(QPen(theme->color("accent"), 2.));
  painter.drawPolyline(path);
  painter.setBrush(theme->color("accent"));
  painter.setPen(QPen(QColor(0, 0, 0, 160), 1.));
  for (const QPointF& point : path)
    {
    painter.drawEllipse(point, 4.5, 4.5);
    }
  // the centre of a line
  if (vtkMRMLMarkupsLineNode::SafeDownCast(d->Markups) && path.size() >= 2)
    {
    const QPointF center = 0.5 * (path[0] + path[1]);
    painter.setBrush(theme->color("view"));
    painter.setPen(QPen(theme->color("accent"), 2.));
    painter.drawEllipse(center, 4.5, 4.5);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMapPreview::mousePressEvent(QMouseEvent* event)
{
  Q_D(qSlicerAstroMapPreview);
  if (event->button() != Qt::LeftButton || !d->Markups)
    {
    return;
    }
  const QPointF position = EventPosition(event);
  const std::vector<QPointF> points = d->points();
  d->Dragged = -1;
  double best = GrabTolerance;
  for (size_t index = 0; index < points.size(); index++)
    {
    const QPointF point = this->mapToWidget(points[index].x(), points[index].y());
    const double distance = std::hypot(point.x() - position.x(), point.y() - position.y());
    if (distance <= best)
      {
      best = distance;
      d->Dragged = static_cast<int>(index);
      }
    }
  if (d->Dragged < 0 && vtkMRMLMarkupsLineNode::SafeDownCast(d->Markups) && points.size() >= 2)
    {
    const QPointF center = 0.5 * (this->mapToWidget(points[0].x(), points[0].y()) +
                                  this->mapToWidget(points[1].x(), points[1].y()));
    if (std::hypot(center.x() - position.x(), center.y() - position.y()) <= GrabTolerance)
      {
      d->Dragged = -2;
      }
    }
  d->DragStart = this->widgetToMap(position);
  d->DragPoints = points;
}

//-----------------------------------------------------------------------------
void qSlicerAstroMapPreview::mouseMoveEvent(QMouseEvent* event)
{
  Q_D(qSlicerAstroMapPreview);
  const QPointF position = EventPosition(event);
  if (d->Dragged == -1)
    {
    // the cursor over a point or the centre of the line
    bool over = false;
    const std::vector<QPointF> points = d->points();
    for (const QPointF& point : points)
      {
      const QPointF widgetPoint = this->mapToWidget(point.x(), point.y());
      over = over || std::hypot(widgetPoint.x() - position.x(), widgetPoint.y() - position.y()) <= GrabTolerance;
      }
    if (vtkMRMLMarkupsLineNode::SafeDownCast(d->Markups) && points.size() >= 2)
      {
      const QPointF center = 0.5 * (this->mapToWidget(points[0].x(), points[0].y()) +
                                    this->mapToWidget(points[1].x(), points[1].y()));
      over = over || std::hypot(center.x() - position.x(), center.y() - position.y()) <= GrabTolerance;
      }
    this->setCursor(over ? Qt::OpenHandCursor : Qt::ArrowCursor);
    return;
    }
  if (!d->Markups || !d->Map)
    {
    return;
    }
  const QPointF ij = this->widgetToMap(position);
  const int wasModifying = d->Markups->StartModify();
  if (d->Dragged == -2)
    {
    const QPointF shift = ij - d->DragStart;
    for (size_t index = 0; index < d->DragPoints.size(); index++)
      {
      d->setPoint(static_cast<int>(index), d->DragPoints[index] + shift);
      }
    }
  else
    {
    d->setPoint(d->Dragged, ij);
    }
  d->Markups->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMapPreview::mouseReleaseEvent(QMouseEvent* event)
{
  Q_UNUSED(event);
  Q_D(qSlicerAstroMapPreview);
  d->Dragged = -1;
}
