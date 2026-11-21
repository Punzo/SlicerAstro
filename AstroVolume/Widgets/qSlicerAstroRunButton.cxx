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
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>

#include "qSlicerAstroRunButton.h"
#include "qSlicerAstroTheme.h"

namespace
{
QPointer<qSlicerAstroRunButton> ActiveRunButton;
}

//-----------------------------------------------------------------------------
/// Progress drawn as the button: amber fill, text and time left
class qSlicerAstroRunProgress : public QWidget
{
public:
  explicit qSlicerAstroRunProgress(QWidget* parent)
    : QWidget(parent)
    {
    this->setMinimumHeight(36);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

  QSize sizeHint() const override
    {
    return QSize(200, 36);
    }

  int Percent{-1};
  QString Text;
  QString Detail;

protected:
  void paintEvent(QPaintEvent* event) override
    {
    Q_UNUSED(event);
    qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(box, 7., 7.);
    painter.fillPath(path, theme->color("accent-soft"));
    if (this->Percent > 0)
      {
      painter.save();
      painter.setClipPath(path);
      painter.fillRect(QRectF(box.left(), box.top(), box.width() * qMin(this->Percent, 100) / 100., box.height()),
                       theme->color("accent-soft-strong"));
      painter.restore();
      }
    painter.setPen(QPen(theme->color("accent"), 1.));
    painter.drawPath(path);
    QFont font = theme->font("body");
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    painter.setPen(theme->color("text"));
    const QRect textRect = this->rect().adjusted(12, 0, -12, 0);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, this->Text);
    painter.setFont(theme->font("monoSmall"));
    painter.setPen(theme->color("muted"));
    painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, this->Detail);
    }
};

//-----------------------------------------------------------------------------
qSlicerAstroRunButton::qSlicerAstroRunButton(QWidget* parent)
  : QWidget(parent)
  , PushButton(new QPushButton(this))
  , CancelButton(new QPushButton(tr("Cancel"), this))
  , Stack(new QStackedWidget(this))
  , Progress(new qSlicerAstroRunProgress(this))
{
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(this->Stack);
  this->PushButton->setProperty("astroRole", "primary");
  this->PushButton->setIcon(theme->icon("run", "on-accent"));
  this->PushButton->setIconSize(QSize(15, 15));
  this->PushButton->setMinimumHeight(36);
  this->PushButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  this->Stack->addWidget(this->PushButton);
  QWidget* running = new QWidget(this->Stack);
  QHBoxLayout* runningLayout = new QHBoxLayout(running);
  runningLayout->setContentsMargins(0, 0, 0, 0);
  runningLayout->setSpacing(8);
  runningLayout->addWidget(this->Progress, 1);
  this->CancelButton->setMinimumHeight(36);
  this->CancelButton->setObjectName("CancelButton");
  runningLayout->addWidget(this->CancelButton);
  this->Stack->addWidget(running);
  this->Stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QObject::connect(this->PushButton, SIGNAL(clicked()), this, SLOT(onClicked()));
  QObject::connect(this->CancelButton, SIGNAL(clicked()), this, SLOT(onCancel()));
}

//-----------------------------------------------------------------------------
qSlicerAstroRunButton::~qSlicerAstroRunButton() = default;

//-----------------------------------------------------------------------------
QString qSlicerAstroRunButton::text() const
{
  return this->PushButton->text();
}

//-----------------------------------------------------------------------------
void qSlicerAstroRunButton::setText(const QString& text)
{
  this->PushButton->setText(text);
}

//-----------------------------------------------------------------------------
QPushButton* qSlicerAstroRunButton::pushButton() const
{
  return this->PushButton;
}

//-----------------------------------------------------------------------------
QPushButton* qSlicerAstroRunButton::cancelButton() const
{
  return this->CancelButton;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroRunButton::isRunning() const
{
  return this->Stack->currentIndex() == 1;
}

//-----------------------------------------------------------------------------
qSlicerAstroRunButton* qSlicerAstroRunButton::activeButton()
{
  return ActiveRunButton;
}

//-----------------------------------------------------------------------------
void qSlicerAstroRunButton::onClicked()
{
  QPointer<qSlicerAstroRunButton> previous = ActiveRunButton;
  ActiveRunButton = this;
  emit this->clicked();
  ActiveRunButton = previous;
}

//-----------------------------------------------------------------------------
void qSlicerAstroRunButton::runAsActive(const std::function<void()>& action)
{
  QPointer<qSlicerAstroRunButton> previous = ActiveRunButton;
  ActiveRunButton = this;
  if (action)
    {
    action();
    }
  ActiveRunButton = previous;
}

//-----------------------------------------------------------------------------
void qSlicerAstroRunButton::setActive(bool active)
{
  static QList<QPointer<qSlicerAstroRunButton> > previous;
  if (active)
    {
    previous.append(ActiveRunButton);
    ActiveRunButton = this;
    }
  else if (ActiveRunButton == this)
    {
    ActiveRunButton = previous.isEmpty() ? nullptr : previous.takeLast();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroRunButton::onCancel()
{
  this->Progress->Text = tr("Cancelling...");
  this->Progress->update();
  this->CancelButton->setEnabled(false);
  emit this->cancelRequested();
}

//-----------------------------------------------------------------------------
void qSlicerAstroRunButton::setRunning(bool running)
{
  if (running == this->isRunning())
    {
    return;
    }
  if (running)
    {
    this->Elapsed.start();
    this->Progress->Percent = -1;
    this->Progress->Detail.clear();
    if (this->Progress->Text.isEmpty())
      {
      this->Progress->Text = tr("Working...");
      }
    this->CancelButton->setEnabled(true);
    }
  else
    {
    this->Progress->Text.clear();
    }
  this->Stack->setCurrentIndex(running ? 1 : 0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroRunButton::setProgress(int percent)
{
  this->Progress->Percent = percent;
  if (percent > 2 && percent < 100 && this->Elapsed.isValid())
    {
    const double elapsed = this->Elapsed.elapsed() / 1000.;
    const int left = qRound(elapsed * (100. - percent) / percent);
    this->Progress->Detail = left >= 60 ?
      tr("%1 % · %2 min left").arg(percent).arg((left + 30) / 60) :
      tr("%1 % · %2 s left").arg(percent).arg(qMax(left, 1));
    }
  else
    {
    this->Progress->Detail = percent >= 0 ? QString("%1 %").arg(percent) : QString();
    }
  this->Progress->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroRunButton::setProgressText(const QString& text)
{
  this->Progress->Text = text;
  this->Progress->update();
}
