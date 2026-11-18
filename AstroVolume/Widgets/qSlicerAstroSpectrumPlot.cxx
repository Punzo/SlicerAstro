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
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <limits>

#include "qSlicerAstroSpectrumPlot.h"
#include "qSlicerAstroTheme.h"

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

} // end of anonymous namespace

//-----------------------------------------------------------------------------
qSlicerAstroSpectrumPlot::qSlicerAstroSpectrumPlot(QWidget* parent)
  : QWidget(parent)
  , Low50(std::numeric_limits<double>::quiet_NaN())
  , High50(std::numeric_limits<double>::quiet_NaN())
  , Low20(std::numeric_limits<double>::quiet_NaN())
  , High20(std::numeric_limits<double>::quiet_NaN())
{
  this->setMouseTracking(true);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  this->PlaceholderText = tr("No spectrum");
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(update()));
}

//-----------------------------------------------------------------------------
qSlicerAstroSpectrumPlot::~qSlicerAstroSpectrumPlot() = default;

//-----------------------------------------------------------------------------
QSize qSlicerAstroSpectrumPlot::sizeHint() const
{
  return QSize(280, 170);
}

//-----------------------------------------------------------------------------
QSize qSlicerAstroSpectrumPlot::minimumSizeHint() const
{
  return QSize(160, 170);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumPlot::setSpectrum(const QVector<double>& x, const QVector<double>& y)
{
  this->X = x;
  this->Y = y;
  if (this->Y.size() != this->X.size())
    {
    this->Y.resize(this->X.size());
    }
  this->XMin = 0.;
  this->XMax = 1.;
  if (!this->X.isEmpty())
    {
    this->XMin = *std::min_element(this->X.constBegin(), this->X.constEnd());
    this->XMax = *std::max_element(this->X.constBegin(), this->X.constEnd());
    if (this->XMax <= this->XMin)
      {
      this->XMax = this->XMin + 1.;
      }
    }
  double yMin = 0., yMax = 0.;
  for (double value : this->Y)
    {
    if (std::isfinite(value))
      {
      yMin = std::min(yMin, value);
      yMax = std::max(yMax, value);
      }
    }
  if (yMax <= yMin)
    {
    yMax = yMin + 1.;
    }
  const double margin = 0.06 * (yMax - yMin);
  this->YMin = yMin < 0. ? yMin - margin : 0.;
  this->YMax = yMax + margin;
  this->HoverIndex = -1;
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumPlot::clearSpectrum()
{
  this->setSpectrum(QVector<double>(), QVector<double>());
  this->clearWidthMarkers();
}

//-----------------------------------------------------------------------------
QVector<double> qSlicerAstroSpectrumPlot::spectrumX() const
{
  return this->X;
}

//-----------------------------------------------------------------------------
QVector<double> qSlicerAstroSpectrumPlot::spectrumY() const
{
  return this->Y;
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumPlot::setUnits(const QString& xUnit, const QString& yUnit)
{
  this->XUnit = xUnit;
  this->YUnit = yUnit;
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumPlot::setWidthMarkers(double low50, double high50, double low20, double high20)
{
  this->Low50 = low50;
  this->High50 = high50;
  this->Low20 = low20;
  this->High20 = high20;
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumPlot::clearWidthMarkers()
{
  const double nan = std::numeric_limits<double>::quiet_NaN();
  this->setWidthMarkers(nan, nan, nan, nan);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumPlot::setPlaceholderText(const QString& text)
{
  this->PlaceholderText = text;
  this->update();
}

//-----------------------------------------------------------------------------
QRectF qSlicerAstroSpectrumPlot::plotRect() const
{
  // room for the labels of the y axis (left) and of the x axis (bottom)
  const QFontMetricsF metrics(qSlicerAstroTheme::instance()->font("monoSmall"));
  double labelWidth = metrics.horizontalAdvance(QStringLiteral("0"));
  for (double tick : qSlicerAstroTheme::niceTicks(this->YMin, this->YMax, 4))
    {
    labelWidth = std::max(labelWidth, metrics.horizontalAdvance(qSlicerAstroTheme::tickLabel(tick)));
    }
  return QRectF(this->rect()).adjusted(labelWidth + 14., 10., -10., -(metrics.height() + 10.));
}

//-----------------------------------------------------------------------------
double qSlicerAstroSpectrumPlot::xToPixel(double x) const
{
  const QRectF plot = this->plotRect();
  return plot.left() + (x - this->XMin) / (this->XMax - this->XMin) * plot.width();
}

//-----------------------------------------------------------------------------
double qSlicerAstroSpectrumPlot::yToPixel(double y) const
{
  const QRectF plot = this->plotRect();
  return plot.bottom() - (y - this->YMin) / (this->YMax - this->YMin) * plot.height();
}

//-----------------------------------------------------------------------------
int qSlicerAstroSpectrumPlot::sampleAt(double pixel) const
{
  int nearest = -1;
  double distance = 0.;
  for (int i = 0; i < this->X.size(); i++)
    {
    const double d = std::abs(this->xToPixel(this->X[i]) - pixel);
    if (nearest < 0 || d < distance)
      {
      nearest = i;
      distance = d;
      }
    }
  return nearest;
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumPlot::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  QPainterPath frame;
  frame.addRoundedRect(QRectF(this->rect()), 9., 9.);
  painter.fillPath(frame, theme->color("view"));
  painter.setClipPath(frame);

  if (this->X.size() < 2)
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

  // W50 band and W20 lines
  if (std::isfinite(this->Low50) && std::isfinite(this->High50))
    {
    const double left = this->xToPixel(std::min(this->Low50, this->High50));
    const double right = this->xToPixel(std::max(this->Low50, this->High50));
    QColor fill = theme->color("accent");
    fill.setAlphaF(0.13);
    painter.fillRect(QRectF(left, plot.top(), right - left, plot.height()), fill);
    painter.setPen(QPen(theme->color("accent"), 1.5));
    painter.drawLine(QPointF(left, plot.top()), QPointF(left, plot.bottom()));
    painter.drawLine(QPointF(right, plot.top()), QPointF(right, plot.bottom()));
    }
  if (std::isfinite(this->Low20) && std::isfinite(this->High20))
    {
    QPen dashed(theme->color("accent"), 1., Qt::DashLine);
    dashed.setDashPattern(QVector<qreal>() << 3. << 3.);
    painter.setPen(dashed);
    for (double edge : {this->Low20, this->High20})
      {
      const double pixel = this->xToPixel(edge);
      painter.drawLine(QPointF(pixel, plot.top()), QPointF(pixel, plot.bottom()));
      }
    }

  // grid and tick labels
  painter.setPen(QPen(theme->color("view-line"), 1.));
  for (double tick : qSlicerAstroTheme::niceTicks(this->YMin, this->YMax, 4))
    {
    const double y = this->yToPixel(tick);
    painter.setPen(QPen(theme->color("view-line"), 1.));
    painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    painter.setPen(theme->color("view-subtle"));
    painter.drawText(QRectF(0., y - metrics.height() / 2., plot.left() - 6., metrics.height()),
                     Qt::AlignRight | Qt::AlignVCenter, qSlicerAstroTheme::tickLabel(tick));
    }
  double lastLabelRight = -1e9;
  for (double tick : qSlicerAstroTheme::niceTicks(this->XMin, this->XMax, 4))
    {
    const double x = this->xToPixel(tick);
    painter.setPen(QPen(theme->color("view-line"), 1.));
    painter.drawLine(QPointF(x, plot.bottom()), QPointF(x, plot.bottom() + 3.));
    const QString label = qSlicerAstroTheme::tickLabel(tick);
    const double width = metrics.horizontalAdvance(label);
    const double left = std::max(0., std::min(x - width / 2., this->width() - width - 2.));
    if (left > lastLabelRight + 6.)
      {
      painter.setPen(theme->color("view-subtle"));
      painter.drawText(QPointF(left, plot.bottom() + 4. + metrics.ascent()), label);
      lastLabelRight = left + width;
      }
    }
  painter.setPen(QPen(theme->color("view-line"), 1.));
  painter.drawLine(QPointF(plot.left(), plot.bottom()), QPointF(plot.right(), plot.bottom()));

  // spectrum, in channel order (gaps at blank channels)
  QPainterPath line;
  bool drawing = false;
  for (int i = 0; i < this->X.size(); i++)
    {
    const double y = this->Y[i];
    if (!std::isfinite(y))
      {
      drawing = false;
      continue;
      }
    const QPointF point(this->xToPixel(this->X[i]), this->yToPixel(y));
    if (drawing)
      {
      line.lineTo(point);
      }
    else
      {
      line.moveTo(point);
      drawing = true;
      }
    }
  painter.setClipRect(plot.adjusted(-2., -2., 2., 2.));
  painter.setPen(QPen(theme->color("view-data"), 2., Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(line);
  painter.setClipPath(frame);

  // readout of the channel under the pointer
  if (this->HoverIndex >= 0 && this->HoverIndex < this->X.size())
    {
    const double x = this->xToPixel(this->X[this->HoverIndex]);
    painter.setPen(QPen(theme->color("view-subtle"), 1.));
    painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    const double value = this->Y[this->HoverIndex];
    if (std::isfinite(value))
      {
      painter.setPen(Qt::NoPen);
      painter.setBrush(theme->color("view-text"));
      painter.drawEllipse(QPointF(x, this->yToPixel(value)), 3., 3.);
      }
    QString text = QString("%1 %2  %3 %4")
      .arg(qSlicerAstroTheme::tickLabel(this->X[this->HoverIndex])).arg(this->XUnit)
      .arg(std::isfinite(value) ? QString::number(value, 'g', 4).replace('-', QChar(0x2212)) : tr("blank"))
      .arg(this->YUnit).trimmed();
    const double width = metrics.horizontalAdvance(text) + 12.;
    const QRectF box(std::min(std::max(plot.left(), x - width / 2.), plot.right() - width), plot.top(),
                     width, metrics.height() + 6.);
    QPainterPath background;
    background.addRoundedRect(box, 5., 5.);
    QColor fill = theme->color("view");
    fill.setAlphaF(0.85);
    painter.fillPath(background, fill);
    painter.setPen(theme->color("view-text"));
    painter.drawText(box, Qt::AlignCenter, text);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumPlot::mouseMoveEvent(QMouseEvent* event)
{
  const double pixel = EventPosition(event).x();
  const QRectF plot = this->plotRect();
  const int index = (pixel >= plot.left() - 4. && pixel <= plot.right() + 4.) ? this->sampleAt(pixel) : -1;
  if (index != this->HoverIndex)
    {
    this->HoverIndex = index;
    this->update();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumPlot::leaveEvent(QEvent* event)
{
  Q_UNUSED(event);
  if (this->HoverIndex >= 0)
    {
    this->HoverIndex = -1;
    this->update();
    }
}
