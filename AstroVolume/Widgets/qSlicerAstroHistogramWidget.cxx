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
                      "function, drag its edges or move it. White dashed line: noise σ of the 3D colour functions, drag it."));
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
  this->XMin = 0.;
  this->XMax = 1.;
  if (this->Centres.size() > 1)
    {
    const double halfBin = 0.5 * (this->Centres[1] - this->Centres[0]);
    this->XMin = this->Centres.first() - halfBin;
    this->XMax = this->Centres.last() + halfBin;
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
  return plot.left() + (value - this->XMin) / (this->XMax - this->XMin) * plot.width();
}

//-----------------------------------------------------------------------------
double qSlicerAstroHistogramWidget::pixelToValue(double pixel) const
{
  const QRectF plot = this->plotRect();
  const double value = this->XMin + (pixel - plot.left()) / plot.width() * (this->XMax - this->XMin);
  return std::max(this->XMin, std::min(value, this->XMax));
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
  QPainterPath bars;
  bars.moveTo(this->valueToPixel(this->XMin), plot.bottom());
  const double binWidth = this->Centres[1] - this->Centres[0];
  for (int i = 0; i < this->Centres.size(); i++)
    {
    const double y = this->countToPixel(this->Counts[i]);
    bars.lineTo(this->valueToPixel(this->Centres[i] - binWidth / 2.), y);
    bars.lineTo(this->valueToPixel(this->Centres[i] + binWidth / 2.), y);
    }
  bars.lineTo(this->valueToPixel(this->XMax), plot.bottom());
  bars.closeSubpath();
  QColor data = theme->color("view-data");
  QColor dataFill = data;
  dataFill.setAlphaF(0.35);
  painter.fillPath(bars, dataFill);
  painter.setPen(QPen(data, 1.2));
  painter.drawPath(bars);

  // value axis
  painter.setPen(QPen(theme->color("view-line"), 1.));
  painter.drawLine(QPointF(plot.left(), plot.bottom()), QPointF(plot.right(), plot.bottom()));
  double lastRight = -1e9;
  for (double tick : qSlicerAstroTheme::niceTicks(this->XMin, this->XMax, 4))
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

  // noise of the 3D colour functions
  if (std::isfinite(this->Noise) && this->Noise > this->XMin && this->Noise < this->XMax)
    {
    const double x = this->valueToPixel(this->Noise);
    QPen pen(theme->color("view-text"), 1.5, Qt::DashLine);
    pen.setDashPattern(QVector<qreal>() << 4. << 3.);
    painter.setPen(pen);
    painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    painter.setPen(theme->color("view-text"));
    painter.drawText(QPointF(x + 3., plot.top() + metrics.ascent()), QString::fromUtf8("σ"));
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
  const bool noiseShown = std::isfinite(this->Noise) && this->Noise > this->XMin && this->Noise < this->XMax;
  if (this->NoiseEditable && noiseShown && std::abs(x - this->valueToPixel(this->Noise)) <= GrabTolerance)
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
      index = static_cast<int>(std::floor((this->pixelToValue(x) - this->XMin) / binWidth));
      index = std::max(0, std::min(index, static_cast<int>(this->Centres.size()) - 1));
      }
    const bool nearEdge = std::abs(x - this->valueToPixel(this->WindowMin)) <= GrabTolerance ||
      std::abs(x - this->valueToPixel(this->WindowMax)) <= GrabTolerance ||
      (this->NoiseEditable && std::isfinite(this->Noise) &&
       std::abs(x - this->valueToPixel(this->Noise)) <= GrabTolerance);
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
