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
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "qSlicerAstroAxisScrubber.h"
#include "qSlicerAstroTheme.h"

//-----------------------------------------------------------------------------
/// The profile along the axis with the playhead; click or drag to scrub
class qSlicerAstroAxisScrubberProfile : public QWidget
{
public:
  explicit qSlicerAstroAxisScrubberProfile(qSlicerAstroAxisScrubber* scrubber)
    : QWidget(scrubber), Scrubber(scrubber)
    {
    this->setMinimumHeight(44);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    this->setCursor(Qt::SizeHorCursor);
    }

  QSize sizeHint() const override
    {
    return QSize(400, 44);
    }

  QVector<double> Values;
  int Count{0};
  int Index{0};

protected:
  int indexAt(double x) const
    {
    if (this->Count < 2)
      {
      return 0;
      }
    const double t = std::max(0., std::min(1., x / std::max(1, this->width() - 1)));
    return static_cast<int>(std::lround(t * (this->Count - 1)));
    }

  void mousePressEvent(QMouseEvent* event) override
    {
    this->Scrubber->setPlaying(false);
    this->Scrubber->setCurrentIndex(this->indexAt(event->pos().x()));
    }

  void mouseMoveEvent(QMouseEvent* event) override
    {
    if (event->buttons() & Qt::LeftButton)
      {
      this->Scrubber->setCurrentIndex(this->indexAt(event->pos().x()));
      }
    }

  void paintEvent(QPaintEvent* event) override
    {
    Q_UNUSED(event);
    qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const double width = this->width();
    const double height = this->height();
    if (this->Values.size() > 1)
      {
      double minimum = 0., maximum = 0.;
      bool first = true;
      for (double value : this->Values)
        {
        if (std::isfinite(value))
          {
          minimum = first ? value : std::min(minimum, value);
          maximum = first ? value : std::max(maximum, value);
          first = false;
          }
        }
      if (maximum > minimum)
        {
        QPainterPath line;
        bool drawing = false;
        for (int k = 0; k < this->Values.size(); k++)
          {
          if (!std::isfinite(this->Values[k]))
            {
            drawing = false;
            continue;
            }
          const QPointF point(width * k / double(this->Values.size() - 1),
                              3. + (height - 6.) * (1. - (this->Values[k] - minimum) / (maximum - minimum)));
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
        painter.setPen(QPen(theme->color("data"), 2., Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(line);
        }
      }
    if (this->Count > 0)
      {
      const double x = this->Count > 1 ? (width - 2.) * this->Index / double(this->Count - 1) : 0.;
      painter.fillRect(QRectF(x, 0., 2., height), theme->color("accent"));
      }
    }

private:
  qSlicerAstroAxisScrubber* Scrubber;
};

//-----------------------------------------------------------------------------
qSlicerAstroAxisScrubber::qSlicerAstroAxisScrubber(QWidget* parent)
  : QWidget(parent)
  , PlayButton(new QPushButton(this))
  , Profile(new qSlicerAstroAxisScrubberProfile(this))
  , Slider(new QSlider(Qt::Horizontal, this))
  , ValueLabel(new QLabel(this))
  , IndexLabel(new QLabel(this))
  , Timer(new QTimer(this))
  , IndexName(tr("channel"))
{
  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setContentsMargins(16, 8, 16, 8);
  layout->setSpacing(14);
  this->PlayButton->setObjectName("PlayButton");
  this->PlayButton->setProperty("astroRole", "primary");
  this->PlayButton->setFixedSize(36, 36);
  this->PlayButton->setIconSize(QSize(16, 16));
  this->PlayButton->setStyleSheet("QPushButton { border-radius: 18px; padding: 0px; min-height: 0px; }");
  this->PlayButton->setToolTip(tr("Play through the axis"));
  layout->addWidget(this->PlayButton);
  QWidget* center = new QWidget(this);
  QVBoxLayout* centerLayout = new QVBoxLayout(center);
  centerLayout->setContentsMargins(0, 0, 0, 0);
  centerLayout->setSpacing(4);
  centerLayout->addWidget(this->Profile);
  this->Slider->setObjectName("AxisSlider");
  centerLayout->addWidget(this->Slider);
  layout->addWidget(center, 1);
  QWidget* readout = new QWidget(this);
  QVBoxLayout* readoutLayout = new QVBoxLayout(readout);
  readoutLayout->setContentsMargins(0, 0, 0, 0);
  readoutLayout->setSpacing(0);
  this->ValueLabel->setProperty("astroRole", "mono");
  this->ValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  this->ValueLabel->setStyleSheet("font-size: 15px; font-weight: 500;");
  this->IndexLabel->setProperty("astroRole", "monoLabel");
  this->IndexLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  readoutLayout->addWidget(this->ValueLabel);
  readoutLayout->addWidget(this->IndexLabel);
  readout->setMinimumWidth(150);
  layout->addWidget(readout);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  this->setFramesPerSecond(8.);

  QObject::connect(this->PlayButton, SIGNAL(clicked()), this, SLOT(togglePlaying()));
  QObject::connect(this->Slider, SIGNAL(valueChanged(int)), this, SLOT(setCurrentIndex(int)));
  QObject::connect(this->Timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(updatePlayIcon()));
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this->Profile, SLOT(update()));
  this->updatePlayIcon();
  this->clear();
}

//-----------------------------------------------------------------------------
qSlicerAstroAxisScrubber::~qSlicerAstroAxisScrubber() = default;

//-----------------------------------------------------------------------------
void qSlicerAstroAxisScrubber::setAxis(const QString& indexName, const QString& unit, const QString& axisLabel,
                                       const QVector<double>& values)
{
  this->setPlaying(false);
  this->IndexName = indexName;
  this->Unit = unit;
  this->AxisLabel = axisLabel;
  this->Values = values;
  this->Profile->Count = values.size();
  const bool wasBlocked = this->Slider->blockSignals(true);
  this->Slider->setRange(0, std::max(0, static_cast<int>(values.size()) - 1));
  this->Slider->setValue(std::min(this->Slider->value(), this->Slider->maximum()));
  this->Slider->blockSignals(wasBlocked);
  this->Profile->Index = this->Slider->value();
  this->setEnabled(!values.isEmpty());
  this->Profile->update();
  this->updateReadout();
}

//-----------------------------------------------------------------------------
void qSlicerAstroAxisScrubber::setProfile(const QVector<double>& profile)
{
  this->Profile->Values = profile;
  this->Profile->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroAxisScrubber::clear()
{
  this->setAxis(this->IndexName, QString(), QString(), QVector<double>());
  this->setProfile(QVector<double>());
}

//-----------------------------------------------------------------------------
int qSlicerAstroAxisScrubber::count() const
{
  return this->Values.size();
}

//-----------------------------------------------------------------------------
double qSlicerAstroAxisScrubber::currentValue() const
{
  const int index = this->currentIndex();
  return (index >= 0 && index < this->Values.size()) ? this->Values[index] : 0.;
}

//-----------------------------------------------------------------------------
int qSlicerAstroAxisScrubber::currentIndex() const
{
  return this->Slider->value();
}

//-----------------------------------------------------------------------------
void qSlicerAstroAxisScrubber::setCurrentIndex(int index)
{
  if (this->Values.isEmpty())
    {
    return;
    }
  index = std::max(0, std::min(index, static_cast<int>(this->Values.size()) - 1));
  const bool changed = index != this->Profile->Index;
  const bool wasBlocked = this->Slider->blockSignals(true);
  this->Slider->setValue(index);
  this->Slider->blockSignals(wasBlocked);
  this->Profile->Index = index;
  this->Profile->update();
  this->updateReadout();
  if (changed)
    {
    emit this->currentIndexChanged(index);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroAxisScrubber::updateReadout()
{
  if (this->Values.isEmpty())
    {
    this->ValueLabel->setText(QString::fromUtf8("—"));
    this->IndexLabel->setText(tr("no axis"));
    return;
    }
  const int index = this->currentIndex();
  this->ValueLabel->setText(QString("%1 %2").arg(this->Values[index], 0, 'f', 2).arg(this->Unit));
  QString text = QString("%1 %2 / %3").arg(this->IndexName).arg(index).arg(this->Values.size() - 1);
  if (!this->AxisLabel.isEmpty())
    {
    text += QString::fromUtf8(" · %1").arg(this->AxisLabel);
    }
  this->IndexLabel->setText(text);
}

//-----------------------------------------------------------------------------
bool qSlicerAstroAxisScrubber::isPlaying() const
{
  return this->Timer->isActive();
}

//-----------------------------------------------------------------------------
void qSlicerAstroAxisScrubber::setPlaying(bool playing)
{
  if (playing == this->isPlaying() || (playing && this->Values.size() < 2))
    {
    return;
    }
  if (playing)
    {
    this->Timer->start();
    }
  else
    {
    this->Timer->stop();
    }
  this->updatePlayIcon();
  emit this->playingChanged(playing);
}

//-----------------------------------------------------------------------------
void qSlicerAstroAxisScrubber::togglePlaying()
{
  this->setPlaying(!this->isPlaying());
}

//-----------------------------------------------------------------------------
double qSlicerAstroAxisScrubber::framesPerSecond() const
{
  return 1000. / std::max(1, this->Timer->interval());
}

//-----------------------------------------------------------------------------
void qSlicerAstroAxisScrubber::setFramesPerSecond(double fps)
{
  this->Timer->setInterval(static_cast<int>(1000. / std::max(0.5, std::min(fps, 60.))));
}

//-----------------------------------------------------------------------------
void qSlicerAstroAxisScrubber::onTimeout()
{
  const int next = this->currentIndex() + 1;
  this->setCurrentIndex(next >= this->Values.size() ? 0 : next);
}

//-----------------------------------------------------------------------------
void qSlicerAstroAxisScrubber::updatePlayIcon()
{
  this->PlayButton->setIcon(qSlicerAstroTheme::instance()->icon(this->isPlaying() ? "stop" : "run", "on-accent"));
  this->PlayButton->setToolTip(this->isPlaying() ? tr("Pause") : tr("Play through the axis"));
}
