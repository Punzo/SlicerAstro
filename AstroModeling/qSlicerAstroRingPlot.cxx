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

// Qt includes
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

// STD includes
#include <algorithm>
#include <cmath>

// AstroVolume includes
#include <qSlicerAstroTheme.h>

#include "qSlicerAstroRingPlot.h"

namespace
{
//----------------------------------------------------------------------------
QPointF EventPosition(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->position();
#else
  return event->localPos();
#endif
}

//----------------------------------------------------------------------------
/// A step of 1, 2 or 5 times a power of ten giving about count ticks
double NiceStep(double range, int count)
{
  if (!(range > 0.) || count < 1)
    {
    return 1.;
    }
  const double raw = range / count;
  const double power = std::pow(10., std::floor(std::log10(raw)));
  const double fraction = raw / power;
  const double nice = fraction < 1.5 ? 1. : (fraction < 3.5 ? 2. : (fraction < 7.5 ? 5. : 10.));
  return nice * power;
}

//----------------------------------------------------------------------------
QString TickLabel(double value, double step)
{
  const int decimals = step >= 1. ? 0 : std::min(4, static_cast<int>(std::ceil(-std::log10(step))));
  return QString::number(value, 'f', decimals);
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
qSlicerAstroRingPlot::qSlicerAstroRingPlot(QWidget* parent)
  : QWidget(parent)
{
  this->setObjectName("RingPlot");
  this->setMouseTracking(true);
  this->setToolTip(tr("A parameter of the rings against the radius. Drag a point up or down to change the value "
                      "of its ring (the table of the rings follows; Calculate model makes the new model)."));
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  this->PlaceholderText = tr("No rings");
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(update()));
}

//-----------------------------------------------------------------------------
qSlicerAstroRingPlot::~qSlicerAstroRingPlot() = default;

//-----------------------------------------------------------------------------
QSize qSlicerAstroRingPlot::sizeHint() const
{
  return QSize(280, 170);
}

//-----------------------------------------------------------------------------
QSize qSlicerAstroRingPlot::minimumSizeHint() const
{
  return QSize(160, 170);
}

//-----------------------------------------------------------------------------
void qSlicerAstroRingPlot::setRings(const QVector<double>& radii, const QVector<double>& values, const QString& unit)
{
  this->Radii = radii;
  this->Values = values;
  this->Values.resize(this->Radii.size());
  this->Unit = unit;
  if (this->DragIndex >= 0 && this->DragIndex < this->Radii.size())
    {
    // during a drag the axes stay (the point follows the mouse)
    this->update();
    return;
    }
  this->DragIndex = -1;
  this->HoverIndex = -1;
  // the radii from 0, the values with a margin
  double xMax = 0.;
  double yMin = 0., yMax = 0.;
  bool first = true;
  for (int index = 0; index < this->Radii.size(); index++)
    {
    xMax = std::max(xMax, this->Radii[index]);
    const double value = this->Values[index];
    if (!std::isfinite(value))
      {
      continue;
      }
    yMin = first ? value : std::min(yMin, value);
    yMax = first ? value : std::max(yMax, value);
    first = false;
    }
  this->XMin = 0.;
  this->XMax = xMax > 0. ? xMax * 1.05 : 1.;
  const double margin = yMax > yMin ? 0.12 * (yMax - yMin) : std::max(1., 0.1 * std::fabs(yMax));
  this->YMin = yMin - margin;
  this->YMax = yMax + margin;
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroRingPlot::clearRings()
{
  this->setRings(QVector<double>(), QVector<double>(), QString());
}

//-----------------------------------------------------------------------------
QVector<double> qSlicerAstroRingPlot::radii() const
{
  return this->Radii;
}

//-----------------------------------------------------------------------------
QVector<double> qSlicerAstroRingPlot::values() const
{
  return this->Values;
}

//-----------------------------------------------------------------------------
void qSlicerAstroRingPlot::setPlaceholderText(const QString& text)
{
  this->PlaceholderText = text;
  this->update();
}

//-----------------------------------------------------------------------------
QRectF qSlicerAstroRingPlot::plotRect() const
{
  const QFontMetricsF metrics(qSlicerAstroTheme::instance()->font("monoSmall"));
  const double left = 10. + metrics.horizontalAdvance("-00000");
  return QRectF(left, 12., this->width() - left - 12., this->height() - 12. - metrics.height() - 18.);
}

//-----------------------------------------------------------------------------
QPointF qSlicerAstroRingPlot::toWidget(double radius, double value) const
{
  const QRectF plot = this->plotRect();
  return QPointF(plot.left() + (radius - this->XMin) / (this->XMax - this->XMin) * plot.width(),
                 plot.bottom() - (value - this->YMin) / (this->YMax - this->YMin) * plot.height());
}

//-----------------------------------------------------------------------------
void qSlicerAstroRingPlot::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  QPainterPath frame;
  frame.addRoundedRect(QRectF(this->rect()), 9., 9.);
  painter.fillPath(frame, theme->color("view"));
  painter.setClipPath(frame);
  if (this->Radii.isEmpty())
    {
    painter.setPen(theme->color("view-subtle"));
    painter.setFont(theme->font("label"));
    painter.drawText(this->rect().adjusted(12, 0, -12, 0), Qt::AlignCenter | Qt::TextWordWrap, this->PlaceholderText);
    return;
    }
  const QRectF plot = this->plotRect();
  const QFont tickFont = theme->font("monoSmall");
  const QFontMetricsF metrics(tickFont);
  painter.setFont(tickFont);

  // grid and ticks: values (left) and radius (bottom, arcsec)
  const double yStep = NiceStep(this->YMax - this->YMin, 4);
  for (double value = std::ceil(this->YMin / yStep) * yStep; value <= this->YMax; value += yStep)
    {
    const double y = this->toWidget(this->XMin, value).y();
    painter.setPen(QPen(theme->color("view-line"), 1.));
    painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    painter.setPen(theme->color("view-subtle"));
    painter.drawText(QRectF(0., y - metrics.height() / 2., plot.left() - 6., metrics.height()),
                     Qt::AlignRight | Qt::AlignVCenter, TickLabel(value, yStep));
    }
  const double xStep = NiceStep(this->XMax - this->XMin, 4);
  for (double radius = std::ceil(this->XMin / xStep) * xStep; radius <= this->XMax; radius += xStep)
    {
    const double x = this->toWidget(radius, this->YMin).x();
    painter.setPen(QPen(theme->color("view-line"), 1.));
    painter.drawLine(QPointF(x, plot.bottom()), QPointF(x, plot.bottom() + 3.));
    const QString label = TickLabel(radius, xStep) + QString::fromUtf8("″");
    painter.setPen(theme->color("view-subtle"));
    painter.drawText(QPointF(x - metrics.horizontalAdvance(label) / 2., plot.bottom() + 4. + metrics.ascent()), label);
    }
  painter.setPen(QPen(theme->color("view-line"), 1.));
  painter.drawLine(plot.bottomLeft(), plot.bottomRight());
  if (!this->Unit.isEmpty())
    {
    painter.setPen(theme->color("view-subtle"));
    painter.drawText(QPointF(plot.left() + 4., plot.top() + metrics.ascent()), this->Unit);
    }

  // the rings: line and markers
  QPolygonF line;
  for (int index = 0; index < this->Radii.size(); index++)
    {
    if (std::isfinite(this->Values[index]))
      {
      line << this->toWidget(this->Radii[index], this->Values[index]);
      }
    }
  painter.setPen(QPen(theme->color("accent"), 2., Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.drawPolyline(line);
  painter.setBrush(theme->color("accent"));
  painter.setPen(QPen(theme->color("view"), 1.5));
  for (const QPointF& point : line)
    {
    painter.drawEllipse(point, 3.5, 3.5);
    }

  // the ring under the mouse
  if (this->HoverIndex >= 0 && this->HoverIndex < this->Radii.size() &&
      std::isfinite(this->Values[this->HoverIndex]))
    {
    const QPointF point = this->toWidget(this->Radii[this->HoverIndex], this->Values[this->HoverIndex]);
    painter.setBrush(theme->color("view-text"));
    painter.drawEllipse(point, 5., 5.);
    const QString text = tr("ring %1 · %2″ · %3 %4").arg(this->HoverIndex + 1)
      .arg(this->Radii[this->HoverIndex], 0, 'f', 1).arg(this->Values[this->HoverIndex], 0, 'f', 2).arg(this->Unit);
    const double textWidth = metrics.horizontalAdvance(text) + 12.;
    QRectF box(point.x() - textWidth / 2., plot.top(), textWidth, metrics.height() + 6.);
    box.moveLeft(std::max(plot.left(), std::min(box.left(), plot.right() - textWidth)));
    QColor fill = theme->color("view");
    fill.setAlpha(220);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(box, 4., 4.);
    painter.setPen(theme->color("view-text"));
    painter.drawText(box, Qt::AlignCenter, text);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroRingPlot::setEditable(bool editable)
{
  this->Editable = editable;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroRingPlot::isEditable() const
{
  return this->Editable;
}

//-----------------------------------------------------------------------------
int qSlicerAstroRingPlot::draggedRing() const
{
  return this->DragIndex;
}

//-----------------------------------------------------------------------------
double qSlicerAstroRingPlot::valueAt(double y) const
{
  const QRectF plot = this->plotRect();
  return this->YMin + (plot.bottom() - y) / plot.height() * (this->YMax - this->YMin);
}

//-----------------------------------------------------------------------------
QPointF qSlicerAstroRingPlot::pointPosition(int ring) const
{
  if (ring < 0 || ring >= this->Radii.size())
    {
    return QPointF();
    }
  return this->toWidget(this->Radii[ring], this->Values[ring]);
}

//-----------------------------------------------------------------------------
void qSlicerAstroRingPlot::mousePressEvent(QMouseEvent* event)
{
  const QPointF position = EventPosition(event);
  if (!this->Editable || event->button() != Qt::LeftButton)
    {
    this->QWidget::mousePressEvent(event);
    return;
    }
  // the point under the mouse (within a few pixels)
  int closest = -1;
  double best = 10.;
  for (int index = 0; index < this->Radii.size(); index++)
    {
    if (!std::isfinite(this->Values[index]))
      {
      continue;
      }
    const QPointF point = this->toWidget(this->Radii[index], this->Values[index]);
    const double distance = std::hypot(point.x() - position.x(), point.y() - position.y());
    if (distance < best)
      {
      best = distance;
      closest = index;
      }
    }
  this->DragIndex = closest;
  if (closest >= 0)
    {
    this->setCursor(Qt::SizeVerCursor);
    this->HoverIndex = closest;
    this->update();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroRingPlot::mouseReleaseEvent(QMouseEvent* event)
{
  Q_UNUSED(event);
  if (this->DragIndex < 0)
    {
    return;
    }
  const int ring = this->DragIndex;
  this->DragIndex = -1;
  this->unsetCursor();
  // the axes for the new values
  this->setRings(this->Radii, this->Values, this->Unit);
  emit this->ringDragFinished(ring);
}

//-----------------------------------------------------------------------------
void qSlicerAstroRingPlot::mouseMoveEvent(QMouseEvent* event)
{
  const QPointF position = EventPosition(event);
  if (this->DragIndex >= 0 && this->DragIndex < this->Values.size())
    {
    // only the value (vertical), within the plot
    const QRectF plot = this->plotRect();
    const double y = std::max(plot.top(), std::min(plot.bottom(), position.y()));
    const double value = this->valueAt(y);
    this->Values[this->DragIndex] = value;
    this->HoverIndex = this->DragIndex;
    this->update();
    emit this->ringValueChanged(this->DragIndex, value);
    return;
    }
  int closest = -1;
  double best = 18.;
  for (int index = 0; index < this->Radii.size(); index++)
    {
    const double distance = std::fabs(this->toWidget(this->Radii[index], this->YMin).x() - position.x());
    if (distance < best)
      {
      best = distance;
      closest = index;
      }
    }
  if (closest != this->HoverIndex)
    {
    this->HoverIndex = closest;
    this->update();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroRingPlot::leaveEvent(QEvent* event)
{
  Q_UNUSED(event);
  this->HoverIndex = -1;
  this->update();
}
