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

#include <algorithm>
#include <cmath>
#include <vector>

#include "qSlicerAstroSpectrumRangeWidget.h"
#include "qSlicerAstroTheme.h"

namespace
{
const double EdgeTolerance = 6.;

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
qSlicerAstroSpectrumRangeWidget::qSlicerAstroSpectrumRangeWidget(QWidget* parent)
  : QWidget(parent)
{
  this->setMouseTracking(true);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  this->setToolTip(tr("Drag the amber edges to change the range, inside it to move it, "
                      "or outside it to draw a new one."));
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(update()));
}

//-----------------------------------------------------------------------------
qSlicerAstroSpectrumRangeWidget::~qSlicerAstroSpectrumRangeWidget() = default;

//-----------------------------------------------------------------------------
QSize qSlicerAstroSpectrumRangeWidget::sizeHint() const
{
  return QSize(280, 64);
}

//-----------------------------------------------------------------------------
QSize qSlicerAstroSpectrumRangeWidget::minimumSizeHint() const
{
  return QSize(120, 64);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumRangeWidget::setSpectrum(const QVector<double>& x, const QVector<double>& y)
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
  bool first = true;
  for (double value : this->Y)
    {
    if (!std::isfinite(value))
      {
      continue;
      }
    if (first)
      {
      yMin = yMax = value;
      first = false;
      }
    yMin = std::min(yMin, value);
    yMax = std::max(yMax, value);
    }
  // the baseline (0) in view when the spectrum is positive
  yMin = std::min(yMin, 0.);
  if (yMax <= yMin)
    {
    yMax = yMin + 1.;
    }
  const double margin = 0.08 * (yMax - yMin);
  this->YMin = yMin - margin;
  this->YMax = yMax + margin;
  this->setRange(this->RangeMin, this->RangeMax);
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumRangeWidget::clearSpectrum()
{
  this->setSpectrum(QVector<double>(), QVector<double>());
}

//-----------------------------------------------------------------------------
QVector<double> qSlicerAstroSpectrumRangeWidget::spectrumX() const
{
  return this->X;
}

//-----------------------------------------------------------------------------
QVector<double> qSlicerAstroSpectrumRangeWidget::spectrumY() const
{
  return this->Y;
}

//-----------------------------------------------------------------------------
double qSlicerAstroSpectrumRangeWidget::minimumValue() const
{
  return this->RangeMin;
}

//-----------------------------------------------------------------------------
double qSlicerAstroSpectrumRangeWidget::maximumValue() const
{
  return this->RangeMax;
}

//-----------------------------------------------------------------------------
double qSlicerAstroSpectrumRangeWidget::minimumX() const
{
  return this->XMin;
}

//-----------------------------------------------------------------------------
double qSlicerAstroSpectrumRangeWidget::maximumX() const
{
  return this->XMax;
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumRangeWidget::setRange(double minimum, double maximum)
{
  if (minimum > maximum)
    {
    std::swap(minimum, maximum);
    }
  if (!this->X.isEmpty())
    {
    minimum = std::max(this->XMin, std::min(minimum, this->XMax));
    maximum = std::max(this->XMin, std::min(maximum, this->XMax));
    }
  this->RangeMin = minimum;
  this->RangeMax = maximum;
  this->update();
}

//-----------------------------------------------------------------------------
QRectF qSlicerAstroSpectrumRangeWidget::plotRect() const
{
  return QRectF(this->rect()).adjusted(6., 6., -6., -6.);
}

//-----------------------------------------------------------------------------
double qSlicerAstroSpectrumRangeWidget::xToPixel(double x) const
{
  const QRectF plot = this->plotRect();
  return plot.left() + (x - this->XMin) / (this->XMax - this->XMin) * plot.width();
}

//-----------------------------------------------------------------------------
double qSlicerAstroSpectrumRangeWidget::pixelToX(double pixel) const
{
  const QRectF plot = this->plotRect();
  const double x = this->XMin + (pixel - plot.left()) / plot.width() * (this->XMax - this->XMin);
  return std::max(this->XMin, std::min(x, this->XMax));
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumRangeWidget::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  QPainterPath frame;
  frame.addRoundedRect(QRectF(this->rect()), 8., 8.);
  painter.fillPath(frame, theme->color("view"));
  painter.setClipPath(frame);
  const QRectF plot = this->plotRect();
  const bool enabled = this->isEnabled();

  // range
  if (!this->X.isEmpty())
    {
    const double left = this->xToPixel(this->RangeMin);
    const double right = this->xToPixel(this->RangeMax);
    QColor fill = theme->color("accent");
    fill.setAlphaF(enabled ? 0.13 : 0.05);
    painter.fillRect(QRectF(left, 0., right - left, this->height()), fill);
    QColor edge = theme->color("accent");
    if (!enabled)
      {
      edge.setAlphaF(0.35);
      }
    painter.setPen(QPen(edge, 2.));
    painter.drawLine(QPointF(left, 0.), QPointF(left, this->height()));
    painter.drawLine(QPointF(right, 0.), QPointF(right, this->height()));
    }

  // zero line
  if (this->YMin < 0. && this->YMax > 0.)
    {
    const double zero = plot.bottom() - (0. - this->YMin) / (this->YMax - this->YMin) * plot.height();
    painter.setPen(QPen(theme->color("view-line"), 1.));
    painter.drawLine(QPointF(plot.left(), zero), QPointF(plot.right(), zero));
    }

  // spectrum, in channel order (gaps at blank channels)
  if (this->X.size() > 1)
    {
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
      const QPointF point(this->xToPixel(this->X[i]),
                          plot.bottom() - (y - this->YMin) / (this->YMax - this->YMin) * plot.height());
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
    QColor data = theme->color("view-data");
    if (!enabled)
      {
      data.setAlphaF(0.4);
      }
    painter.setPen(QPen(data, 2., Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(line);
    }
  else
    {
    painter.setPen(theme->color("view-subtle"));
    painter.setFont(theme->font("label"));
    painter.drawText(this->rect(), Qt::AlignCenter, tr("No spectrum"));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumRangeWidget::mousePressEvent(QMouseEvent* event)
{
  if (event->button() != Qt::LeftButton || this->X.isEmpty())
    {
    QWidget::mousePressEvent(event);
    return;
    }
  const double pixel = EventPosition(event).x();
  const double left = this->xToPixel(this->RangeMin);
  const double right = this->xToPixel(this->RangeMax);
  this->DragAnchor = this->pixelToX(pixel);
  this->DragRangeMin = this->RangeMin;
  this->DragRangeMax = this->RangeMax;
  if (std::abs(pixel - left) <= EdgeTolerance)
    {
    this->Drag = LowerEdge;
    }
  else if (std::abs(pixel - right) <= EdgeTolerance)
    {
    this->Drag = UpperEdge;
    }
  else if (pixel > left && pixel < right)
    {
    this->Drag = Shift;
    }
  else
    {
    this->Drag = Draw;
    this->setRange(this->DragAnchor, this->DragAnchor);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumRangeWidget::mouseMoveEvent(QMouseEvent* event)
{
  const double pixel = EventPosition(event).x();
  if (this->Drag == None)
    {
    const double left = this->xToPixel(this->RangeMin);
    const double right = this->xToPixel(this->RangeMax);
    if (!this->X.isEmpty() &&
        (std::abs(pixel - left) <= EdgeTolerance || std::abs(pixel - right) <= EdgeTolerance))
      {
      this->setCursor(Qt::SizeHorCursor);
      }
    else if (!this->X.isEmpty() && pixel > left && pixel < right)
      {
      this->setCursor(Qt::OpenHandCursor);
      }
    else
      {
      this->setCursor(Qt::CrossCursor);
      }
    return;
    }
  const double x = this->pixelToX(pixel);
  switch (this->Drag)
    {
    case LowerEdge:
      this->setRange(x, this->DragRangeMax);
      break;
    case UpperEdge:
      this->setRange(this->DragRangeMin, x);
      break;
    case Shift:
      {
      double shift = x - this->DragAnchor;
      shift = std::max(shift, this->XMin - this->DragRangeMin);
      shift = std::min(shift, this->XMax - this->DragRangeMax);
      this->setRange(this->DragRangeMin + shift, this->DragRangeMax + shift);
      break;
      }
    case Draw:
      this->setRange(this->DragAnchor, x);
      break;
    default:
      break;
    }
  emit this->rangeChanging(this->RangeMin, this->RangeMax);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSpectrumRangeWidget::mouseReleaseEvent(QMouseEvent* event)
{
  if (this->Drag == None)
    {
    QWidget::mouseReleaseEvent(event);
    return;
    }
  if (this->Drag == Draw && this->RangeMax - this->RangeMin <= 0.)
    {
    // a click outside the range: the previous range is kept
    this->setRange(this->DragRangeMin, this->DragRangeMax);
    }
  this->Drag = None;
  emit this->rangeChanged(this->RangeMin, this->RangeMax);
}

//-----------------------------------------------------------------------------
QString qSlicerAstroSpectrumRangeWidget::setRangeFromLine()
{
  const QVector<double> spectrum = this->spectrumY();
  const QVector<double> velocities = this->spectrumX();
  std::vector<double> values;
  for (double value : spectrum)
    {
    if (std::isfinite(value))
      {
      values.push_back(value);
      }
    }
  if (values.size() < 5 || velocities.size() != spectrum.size())
    {
    return tr("The cube has too few channels to find the line.");
    }
  // baseline and noise of the spectrum: median and median absolute deviation
  std::vector<double> sorted = values;
  std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
  const double baseline = sorted[sorted.size() / 2];
  std::vector<double> deviations;
  for (double value : values)
    {
    deviations.push_back(std::abs(value - baseline));
    }
  std::nth_element(deviations.begin(), deviations.begin() + deviations.size() / 2, deviations.end());
  const double sigma = 1.4826 * deviations[deviations.size() / 2];
  int peak = -1;
  for (int k = 0; k < spectrum.size(); k++)
    {
    if (std::isfinite(spectrum[k]) && (peak < 0 || spectrum[k] > spectrum[peak]))
      {
      peak = k;
      }
    }
  if (peak < 0 || spectrum[peak] - baseline < 5. * sigma)
    {
    return tr("No line above the noise of the integrated spectrum: the range is unchanged.");
    }
  // the channels around the peak above 1.5 times the noise, plus two
  int first = peak, last = peak;
  const double limit = baseline + 1.5 * sigma;
  while (first > 0 && (!std::isfinite(spectrum[first - 1]) || spectrum[first - 1] > limit))
    {
    first--;
    }
  while (last < spectrum.size() - 1 && (!std::isfinite(spectrum[last + 1]) || spectrum[last + 1] > limit))
    {
    last++;
    }
  first = std::max(0, first - 2);
  last = std::min(static_cast<int>(spectrum.size()) - 1, last + 2);
  const double halfChannel = velocities.size() > 1 ? 0.5 * std::abs(velocities[1] - velocities[0]) : 0.;
  const double v1 = std::min(velocities[first], velocities[last]) - halfChannel;
  const double v2 = std::max(velocities[first], velocities[last]) + halfChannel;
  const double minimum = std::max(v1, this->minimumX());
  const double maximum = std::min(v2, this->maximumX());
  this->setRange(minimum, maximum);
  emit this->rangeChanged(minimum, maximum);
  return QString();
}
