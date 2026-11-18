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
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

#include "qSlicerAstroHistogramWidget.h"
#include "qSlicerAstroTheme.h"

namespace
{
const double GrabTolerance = 6.;

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
/// 10^n as text, e.g. 10³
QString PowerOfTen(int exponent)
{
  static const QString superscripts = QString::fromUtf8("⁰¹²³⁴⁵⁶⁷⁸⁹");
  QString text;
  const QString digits = QString::number(std::abs(exponent));
  for (const QChar& digit : digits)
    {
    text += superscripts.at(digit.digitValue());
    }
  return QString("10") + (exponent < 0 ? QString::fromUtf8("⁻") : QString()) + text;
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
qSlicerAstroHistogramWidget::qSlicerAstroHistogramWidget(QWidget* parent)
  : QWidget(parent)
  , Noise(std::numeric_limits<double>::quiet_NaN())
{
  this->setMouseTracking(true);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  this->setToolTip(tr("Histogram of the data (counts on a logarithmic axis). Amber: window of the 2D colour "
                      "function, drag its edges or move it. White line with the σ handle: noise σ of the 3D colour functions, "
                      "drag it. Mouse wheel: zoom the values (e.g. on the noise); double click: all the values."));
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(update()));
}

//-----------------------------------------------------------------------------
qSlicerAstroHistogramWidget::~qSlicerAstroHistogramWidget() = default;

//-----------------------------------------------------------------------------
QSize qSlicerAstroHistogramWidget::sizeHint() const
{
  return QSize(280, 150);
}

//-----------------------------------------------------------------------------
QSize qSlicerAstroHistogramWidget::minimumSizeHint() const
{
  return QSize(160, 150);
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::setHistogram(const QVector<double>& centres, const QVector<double>& counts)
{
  this->Centres = centres;
  this->Counts = counts;
  if (this->Counts.size() != this->Centres.size())
    {
    this->Counts.resize(this->Centres.size());
    }
  if (!this->Zoomed)
    {
    // the values of the data: the whole axis
    this->XMin = 0.;
    this->XMax = 1.;
    if (this->Centres.size() > 1)
      {
      const double halfBin = 0.5 * (this->Centres[1] - this->Centres[0]);
      this->XMin = this->Centres.first() - halfBin;
      this->XMax = this->Centres.last() + halfBin;
      }
    this->ViewMin = this->XMin;
    this->ViewMax = this->XMax;
    }
  double maximum = 1.;
  for (double count : this->Counts)
    {
    maximum = std::max(maximum, count);
    }
  this->LogMax = std::ceil(std::log10(maximum) + 0.05);
  this->HoverIndex = -1;
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::clearHistogram()
{
  // a new histogram (e.g. of other data) starts with all the values
  this->Zoomed = false;
  this->setHistogram(QVector<double>(), QVector<double>());
}

//-----------------------------------------------------------------------------
QVector<double> qSlicerAstroHistogramWidget::binCentres() const
{
  return this->Centres;
}

//-----------------------------------------------------------------------------
QVector<double> qSlicerAstroHistogramWidget::binCounts() const
{
  return this->Counts;
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::setUnit(const QString& unit)
{
  this->Unit = qSlicerAstroTheme::displayUnit(unit);
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::setWindow(double minimum, double maximum)
{
  this->WindowMin = std::min(minimum, maximum);
  this->WindowMax = std::max(minimum, maximum);
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::setNoise(double noise)
{
  this->Noise = noise;
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::setNoiseEditable(bool editable)
{
  this->NoiseEditable = editable;
}

//-----------------------------------------------------------------------------
double qSlicerAstroHistogramWidget::windowMinimum() const
{
  return this->WindowMin;
}

//-----------------------------------------------------------------------------
double qSlicerAstroHistogramWidget::windowMaximum() const
{
  return this->WindowMax;
}

//-----------------------------------------------------------------------------
double qSlicerAstroHistogramWidget::noise() const
{
  return this->Noise;
}

//-----------------------------------------------------------------------------
double qSlicerAstroHistogramWidget::minimumValue() const
{
  return this->XMin;
}

//-----------------------------------------------------------------------------
double qSlicerAstroHistogramWidget::maximumValue() const
{
  return this->XMax;
}

//-----------------------------------------------------------------------------
QRectF qSlicerAstroHistogramWidget::plotRect() const
{
  const QFontMetricsF metrics(qSlicerAstroTheme::instance()->font("monoSmall"));
  const double labelWidth = metrics.horizontalAdvance(PowerOfTen(static_cast<int>(this->LogMax)));
  return QRectF(this->rect()).adjusted(labelWidth + 12., 10., -10., -(metrics.height() + 10.));
}

//-----------------------------------------------------------------------------
double qSlicerAstroHistogramWidget::valueToPixel(double value) const
{
  const QRectF plot = this->plotRect();
  return plot.left() + (value - this->ViewMin) / (this->ViewMax - this->ViewMin) * plot.width();
}

//-----------------------------------------------------------------------------
double qSlicerAstroHistogramWidget::pixelToValue(double pixel) const
{
  const QRectF plot = this->plotRect();
  const double value = this->ViewMin + (pixel - plot.left()) / plot.width() * (this->ViewMax - this->ViewMin);
  return std::max(this->ViewMin, std::min(value, this->ViewMax));
}

//-----------------------------------------------------------------------------
double qSlicerAstroHistogramWidget::countToPixel(double count) const
{
  const QRectF plot = this->plotRect();
  const double logCount = count > 0. ? std::log10(count) + 0.3 : 0.;
  return plot.bottom() - std::min(1., logCount / (this->LogMax + 0.3)) * plot.height();
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  QPainterPath frame;
  frame.addRoundedRect(QRectF(this->rect()), 9., 9.);
  painter.fillPath(frame, theme->color("view"));
  painter.setClipPath(frame);
  if (this->Centres.size() < 2)
    {
    painter.setPen(theme->color("view-subtle"));
    painter.setFont(theme->font("label"));
    painter.drawText(this->rect(), Qt::AlignCenter, tr("No histogram"));
    return;
    }

  const QRectF plot = this->plotRect();
  const QFont tickFont = theme->font("monoSmall");
  const QFontMetricsF metrics(tickFont);
  painter.setFont(tickFont);

  // counts: powers of ten
  for (int exponent = 0; exponent <= static_cast<int>(this->LogMax); exponent++)
    {
    const double y = this->countToPixel(std::pow(10., exponent));
    painter.setPen(QPen(theme->color("view-line"), 1.));
    painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    painter.setPen(theme->color("view-subtle"));
    painter.drawText(QRectF(0., y - metrics.height() / 2., plot.left() - 5., metrics.height()),
                     Qt::AlignRight | Qt::AlignVCenter, PowerOfTen(exponent));
    }

  // window of the 2D colour function
  const double left = this->valueToPixel(this->WindowMin);
  const double right = this->valueToPixel(this->WindowMax);
  QColor fill = theme->color("accent");
  fill.setAlphaF(0.12);
  painter.fillRect(QRectF(left, plot.top(), right - left, plot.height()), fill);

  // bars
  // (bins of the whole axis, or of the zoomed values)
  QPainterPath bars;
  const double binWidth = this->Centres[1] - this->Centres[0];
  bars.moveTo(this->valueToPixel(this->Centres.first() - binWidth / 2.), plot.bottom());
  for (int i = 0; i < this->Centres.size(); i++)
    {
    const double y = this->countToPixel(this->Counts[i]);
    bars.lineTo(this->valueToPixel(this->Centres[i] - binWidth / 2.), y);
    bars.lineTo(this->valueToPixel(this->Centres[i] + binWidth / 2.), y);
    }
  bars.lineTo(this->valueToPixel(this->Centres.last() + binWidth / 2.), plot.bottom());
  bars.closeSubpath();
  painter.save();
  painter.setClipRect(plot.adjusted(0., -1., 0., 1.), Qt::IntersectClip);
  QColor data = theme->color("view-data");
  QColor dataFill = data;
  dataFill.setAlphaF(0.35);
  painter.fillPath(bars, dataFill);
  painter.setPen(QPen(data, 1.2));
  painter.drawPath(bars);
  painter.restore();

  // value axis
  painter.setPen(QPen(theme->color("view-line"), 1.));
  painter.drawLine(QPointF(plot.left(), plot.bottom()), QPointF(plot.right(), plot.bottom()));
  double lastRight = -1e9;
  for (double tick : qSlicerAstroTheme::niceTicks(this->ViewMin, this->ViewMax, 4))
    {
    const double x = this->valueToPixel(tick);
    painter.setPen(QPen(theme->color("view-line"), 1.));
    painter.drawLine(QPointF(x, plot.bottom()), QPointF(x, plot.bottom() + 3.));
    const QString label = qSlicerAstroTheme::tickLabel(tick);
    const double width = metrics.horizontalAdvance(label);
    const double labelLeft = std::max(0., std::min(x - width / 2., this->width() - width - 2.));
    if (labelLeft > lastRight + 6.)
      {
      painter.setPen(theme->color("view-subtle"));
      painter.drawText(QPointF(labelLeft, plot.bottom() + 4. + metrics.ascent()), label);
      lastRight = labelLeft + width;
      }
    }
  if (!this->Unit.isEmpty())
    {
    painter.setPen(theme->color("view-subtle"));
    painter.drawText(QRectF(plot.right() - 120., plot.top(), 118., metrics.height()),
                     Qt::AlignRight | Qt::AlignVCenter, this->Unit);
    }

  // window edges
  painter.setPen(QPen(theme->color("accent"), 2.));
  painter.drawLine(QPointF(left, plot.top()), QPointF(left, plot.bottom()));
  painter.drawLine(QPointF(right, plot.top()), QPointF(right, plot.bottom()));

  // noise σ of the 3D colour functions: a line with a handle on top
  if (this->noiseVisible())
    {
    const double x = this->valueToPixel(this->Noise);
    const QColor noiseColor = theme->color("view-text");
    painter.setPen(QPen(noiseColor, 2.));
    painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    const QRectF handle = this->noiseHandleRect();
    QPainterPath tab;
    tab.addRoundedRect(handle, 4., 4.);
    painter.fillPath(tab, noiseColor);
    painter.setPen(theme->color("view"));
    painter.drawText(handle, Qt::AlignCenter, QString::fromUtf8("σ"));
    }
  else if (std::isfinite(this->Noise) && this->Noise > 0. && this->Noise > this->XMin && this->Noise < this->XMax)
    {
    // outside the zoomed values: a mark on the side of the axis
    const bool lower = this->Noise < this->ViewMin;
    painter.setPen(theme->color("view-text"));
    painter.drawText(QRectF(lower ? plot.left() + 2. : plot.right() - 40., plot.top(), 38., metrics.height()),
                     (lower ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter,
                     lower ? QString::fromUtf8("◂ σ") : QString::fromUtf8("σ ▸"));
    }

  // readout of the bin under the pointer
  if (this->HoverIndex >= 0 && this->HoverIndex < this->Centres.size())
    {
    const double value = this->Centres[this->HoverIndex];
    QString text = QString("%1 %2").arg(qSlicerAstroTheme::tickLabel(value), this->Unit).trimmed();
    if (std::isfinite(this->Noise) && this->Noise > 0.)
      {
      text += QString::fromUtf8(" · %1σ").arg(QString::number(value / this->Noise, 'f', 1).replace('-', QChar(0x2212)));
      }
    text += QString::fromUtf8(" · %1").arg(QString::number(this->Counts[this->HoverIndex], 'g', 4));
    const double width = metrics.horizontalAdvance(text) + 12.;
    const double x = this->valueToPixel(value);
    const QRectF box(std::min(std::max(plot.left(), x - width / 2.), plot.right() - width),
                     plot.top() + metrics.height() + 2., width, metrics.height() + 6.);
    QPainterPath background;
    background.addRoundedRect(box, 5., 5.);
    QColor boxFill = theme->color("view");
    boxFill.setAlphaF(0.88);
    painter.fillPath(background, boxFill);
    painter.setPen(theme->color("view-text"));
    painter.drawText(box, Qt::AlignCenter, text);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::mousePressEvent(QMouseEvent* event)
{
  if (event->button() != Qt::LeftButton || this->Centres.size() < 2)
    {
    return;
    }
  const double x = EventPosition(event).x();
  const double left = this->valueToPixel(this->WindowMin);
  const double right = this->valueToPixel(this->WindowMax);
  // the σ handle first; the σ line when it is the nearest line
  const double noiseDistance = this->noiseVisible() ? std::abs(x - this->valueToPixel(this->Noise)) :
    std::numeric_limits<double>::infinity();
  if (this->NoiseEditable && this->noiseVisible() &&
      (this->noiseHandleRect().adjusted(-3., -3., 3., 3.).contains(EventPosition(event)) ||
       (noiseDistance <= GrabTolerance && noiseDistance <= std::min(std::abs(x - left), std::abs(x - right)))))
    {
    this->Drag = NoiseLine;
    }
  else if (std::abs(x - left) <= GrabTolerance)
    {
    this->Drag = LowerEdge;
    }
  else if (std::abs(x - right) <= GrabTolerance)
    {
    this->Drag = UpperEdge;
    }
  else if (x > left && x < right)
    {
    this->Drag = Shift;
    }
  else
    {
    // a click outside the window moves the nearest edge there
    this->Drag = std::abs(x - left) < std::abs(x - right) ? LowerEdge : UpperEdge;
    this->mouseMoveEvent(event);
    }
  this->DragAnchor = this->pixelToValue(x);
  this->DragWindowMin = this->WindowMin;
  this->DragWindowMax = this->WindowMax;
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::mouseMoveEvent(QMouseEvent* event)
{
  const double x = EventPosition(event).x();
  if (this->Drag == None)
    {
    // hover: readout and cursor
    int index = -1;
    if (this->Centres.size() > 1)
      {
      const double binWidth = this->Centres[1] - this->Centres[0];
      index = static_cast<int>(std::floor((this->pixelToValue(x) - (this->Centres.first() - binWidth / 2.)) / binWidth));
      index = std::max(0, std::min(index, static_cast<int>(this->Centres.size()) - 1));
      }
    const bool nearEdge = std::abs(x - this->valueToPixel(this->WindowMin)) <= GrabTolerance ||
      std::abs(x - this->valueToPixel(this->WindowMax)) <= GrabTolerance ||
      (this->NoiseEditable && this->noiseVisible() &&
       (std::abs(x - this->valueToPixel(this->Noise)) <= GrabTolerance ||
        this->noiseHandleRect().contains(EventPosition(event))));
    this->setCursor(nearEdge ? Qt::SizeHorCursor : Qt::ArrowCursor);
    if (index != this->HoverIndex)
      {
      this->HoverIndex = index;
      this->update();
      }
    return;
    }
  const double value = this->pixelToValue(x);
  switch (this->Drag)
    {
    case LowerEdge:
      this->WindowMin = std::min(value, this->WindowMax);
      break;
    case UpperEdge:
      this->WindowMax = std::max(value, this->WindowMin);
      break;
    case Shift:
      {
      double shift = value - this->DragAnchor;
      shift = std::max(shift, this->XMin - this->DragWindowMin);
      shift = std::min(shift, this->XMax - this->DragWindowMax);
      this->WindowMin = this->DragWindowMin + shift;
      this->WindowMax = this->DragWindowMax + shift;
      break;
      }
    case NoiseLine:
      if (value > 0.)
        {
        this->Noise = value;
        emit this->noiseChanging(this->Noise);
        }
      this->update();
      return;
    default:
      break;
    }
  this->update();
  emit this->windowChanging(this->WindowMin, this->WindowMax);
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::mouseReleaseEvent(QMouseEvent* event)
{
  Q_UNUSED(event);
  const DragMode drag = this->Drag;
  this->Drag = None;
  if (drag == NoiseLine)
    {
    emit this->noiseChanged(this->Noise);
    }
  else if (drag != None)
    {
    emit this->windowChanged(this->WindowMin, this->WindowMax);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::leaveEvent(QEvent* event)
{
  Q_UNUSED(event);
  if (this->HoverIndex >= 0)
    {
    this->HoverIndex = -1;
    this->update();
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroHistogramWidget::noiseVisible() const
{
  return std::isfinite(this->Noise) && this->Noise > 0. && this->Noise >= this->ViewMin &&
    this->Noise <= this->ViewMax && this->Centres.size() > 1;
}

//-----------------------------------------------------------------------------
QRectF qSlicerAstroHistogramWidget::noiseHandleRect() const
{
  const QRectF plot = this->plotRect();
  const double x = this->valueToPixel(this->Noise);
  // inside the plot, also when the line is on its edge
  const double width = 16.;
  const double left = std::max(plot.left(), std::min(x - width / 2., plot.right() - width));
  return QRectF(left, plot.top(), width, 15.);
}

//-----------------------------------------------------------------------------
QPointF qSlicerAstroHistogramWidget::noiseHandlePosition() const
{
  return this->noiseVisible() ? this->noiseHandleRect().center() : QPointF(-1., -1.);
}

//-----------------------------------------------------------------------------
double qSlicerAstroHistogramWidget::viewMinimum() const
{
  return this->ViewMin;
}

//-----------------------------------------------------------------------------
double qSlicerAstroHistogramWidget::viewMaximum() const
{
  return this->ViewMax;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroHistogramWidget::isZoomed() const
{
  return this->Zoomed;
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::setViewRange(double minimum, double maximum)
{
  if (this->Centres.size() < 2 || !(maximum > minimum))
    {
    return;
    }
  // within the values of the data, at least 1/1000 of them
  const double fullWidth = this->XMax - this->XMin;
  double width = std::max(std::min(maximum - minimum, fullWidth), fullWidth / 1000.);
  double low = std::max(this->XMin, std::min(minimum, this->XMax - width));
  const bool zoomed = width < fullWidth * (1. - 1e-9);
  if (!zoomed)
    {
    low = this->XMin;
    width = fullWidth;
    }
  this->ViewMin = low;
  this->ViewMax = low + width;
  this->Zoomed = zoomed;
  this->HoverIndex = -1;
  this->update();
  emit this->viewRangeChanged(this->ViewMin, this->ViewMax);
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::resetZoom()
{
  this->setViewRange(this->XMin, this->XMax);
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::wheelEvent(QWheelEvent* event)
{
  if (this->Centres.size() < 2)
    {
    return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const double x = event->position().x();
#else
  const double x = event->posF().x();
#endif
  const int delta = event->angleDelta().y();
  if (delta == 0)
    {
    return;
    }
  // zoom around the value under the pointer
  const double anchor = this->pixelToValue(x);
  const double factor = std::pow(0.8, delta / 120.);
  const double low = anchor - (anchor - this->ViewMin) * factor;
  const double high = anchor + (this->ViewMax - anchor) * factor;
  this->setViewRange(low, high);
  event->accept();
}

//-----------------------------------------------------------------------------
void qSlicerAstroHistogramWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
  Q_UNUSED(event);
  this->Drag = None;
  this->resetZoom();
}
