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
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

#include "qSlicerAstroMessageWidget.h"
#include "qSlicerAstroTheme.h"

//-----------------------------------------------------------------------------
qSlicerAstroMessageWidget::qSlicerAstroMessageWidget(QWidget* parent)
  : QFrame(parent)
  , IconLabel(new QLabel(this))
  , TextLabel(new QLabel(this))
  , Actions(new QWidget(this))
  , ActionsLayout(new QHBoxLayout(this->Actions))
{
  this->setProperty("astroRole", "message");
  this->setProperty("kind", "info");
  QGridLayout* layout = new QGridLayout(this);
  layout->setContentsMargins(11, 9, 11, 9);
  layout->setHorizontalSpacing(9);
  layout->setVerticalSpacing(8);
  this->IconLabel->setFixedSize(18, 18);
  this->TextLabel->setWordWrap(true);
  this->TextLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  this->TextLabel->setObjectName("MessageTextLabel");
  this->ActionsLayout->setContentsMargins(0, 0, 0, 0);
  this->ActionsLayout->setSpacing(8);
  this->ActionsLayout->addStretch(1);
  this->Actions->hide();
  layout->addWidget(this->IconLabel, 0, 0, Qt::AlignTop);
  layout->addWidget(this->TextLabel, 0, 1);
  layout->addWidget(this->Actions, 1, 1);
  layout->setColumnStretch(1, 1);
  this->hide();
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(updateIcon()));
}

//-----------------------------------------------------------------------------
qSlicerAstroMessageWidget::~qSlicerAstroMessageWidget() = default;

//-----------------------------------------------------------------------------
QString qSlicerAstroMessageWidget::text() const
{
  return this->TextLabel->text();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroMessageWidget::kind() const
{
  return this->Kind;
}

//-----------------------------------------------------------------------------
void qSlicerAstroMessageWidget::updateIcon()
{
  QString icon("info");
  QString token("data");
  if (this->Kind == "warning")
    {
    icon = "warning";
    token = "warn";
    }
  else if (this->Kind == "error")
    {
    icon = "warning";
    token = "err";
    }
  else if (this->Kind == "success")
    {
    icon = "check";
    token = "ok";
    }
  const qreal ratio = this->devicePixelRatioF();
  QPixmap pixmap = qSlicerAstroTheme::instance()->icon(icon, token).pixmap(QSize(18 * ratio, 18 * ratio));
  pixmap.setDevicePixelRatio(ratio);
  this->IconLabel->setPixmap(pixmap);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMessageWidget::showMessage(const QString& kind, const QString& text)
{
  this->Kind = (kind == "warning" || kind == "error" || kind == "success") ? kind : QString("info");
  this->setProperty("kind", this->Kind);
  this->style()->unpolish(this);
  this->style()->polish(this);
  this->TextLabel->setText(text);
  // previous actions are removed
  while (this->ActionsLayout->count() > 1)
    {
    QLayoutItem* item = this->ActionsLayout->takeAt(0);
    if (item->widget())
      {
      item->widget()->deleteLater();
      }
    delete item;
    }
  this->Actions->hide();
  this->updateIcon();
  this->setVisible(!text.isEmpty());
}

//-----------------------------------------------------------------------------
void qSlicerAstroMessageWidget::clearMessage()
{
  this->showMessage(this->Kind, QString());
}

//-----------------------------------------------------------------------------
QPushButton* qSlicerAstroMessageWidget::addAction(const QString& name, const QString& text, bool primary)
{
  QPushButton* button = new QPushButton(text, this->Actions);
  button->setObjectName(name);
  button->setProperty("astroRole", primary ? "" : "ghost");
  button->setMinimumHeight(28);
  this->ActionsLayout->insertWidget(this->ActionsLayout->count() - 1, button);
  this->Actions->show();
  QObject::connect(button, &QPushButton::clicked, this, [this, name]()
    {
    emit this->actionTriggered(name);
    });
  return button;
}
