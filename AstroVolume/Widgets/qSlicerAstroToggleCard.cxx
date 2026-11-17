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
#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

#include "qSlicerAstroTheme.h"
#include "qSlicerAstroToggleCard.h"

//-----------------------------------------------------------------------------
qSlicerAstroToggleCard::qSlicerAstroToggleCard(QWidget* parent)
  : QAbstractButton(parent)
{
  this->setCheckable(true);
  this->setAttribute(Qt::WA_Hover, true);
  this->setCursor(Qt::PointingHandCursor);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(update()));
}

//-----------------------------------------------------------------------------
qSlicerAstroToggleCard::qSlicerAstroToggleCard(const QString& title, const QString& caption, QWidget* parent)
  : qSlicerAstroToggleCard(parent)
{
  this->setText(title);
  this->Caption = caption;
}

//-----------------------------------------------------------------------------
qSlicerAstroToggleCard::~qSlicerAstroToggleCard() = default;

//-----------------------------------------------------------------------------
QString qSlicerAstroToggleCard::caption() const
{
  return this->Caption;
}

//-----------------------------------------------------------------------------
void qSlicerAstroToggleCard::setCaption(const QString& caption)
{
  this->Caption = caption;
  this->updateGeometry();
  this->update();
}

//-----------------------------------------------------------------------------
QSize qSlicerAstroToggleCard::sizeHint() const
{
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QFont titleFont = theme->font("body");
  titleFont.setWeight(QFont::Bold);
  const QFontMetrics titleMetrics(titleFont);
  const QFontMetrics captionMetrics(theme->font("label"));
  const int width = qMax(titleMetrics.horizontalAdvance(this->text()),
                         captionMetrics.horizontalAdvance(this->Caption)) + 18;
  return QSize(qMax(width, 60), titleMetrics.height() + captionMetrics.height() + 18);
}

//-----------------------------------------------------------------------------
QSize qSlicerAstroToggleCard::minimumSizeHint() const
{
  return QSize(48, this->sizeHint().height());
}

//-----------------------------------------------------------------------------
bool qSlicerAstroToggleCard::event(QEvent* event)
{
  if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave)
    {
    this->update();
    }
  return QAbstractButton::event(event);
}

//-----------------------------------------------------------------------------
void qSlicerAstroToggleCard::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  QPainterPath path;
  path.addRoundedRect(box, 9., 9.);
  const bool enabled = this->isEnabled();
  QColor background = Qt::transparent;
  QColor border = theme->color("line");
  if (this->isChecked() && enabled)
    {
    background = theme->color("accent-soft");
    border = theme->color("accent");
    }
  else if (this->underMouse() && enabled)
    {
    background = theme->color("hover");
    }
  painter.fillPath(path, background);
  painter.setPen(QPen(border, 1.));
  painter.drawPath(path);
  if (this->hasFocus())
    {
    painter.setPen(QPen(theme->color("data"), 1.));
    painter.drawRoundedRect(box.adjusted(1.5, 1.5, -1.5, -1.5), 7.5, 7.5);
    }

  QFont titleFont = theme->font("body");
  titleFont.setWeight(QFont::Bold);
  const QFontMetrics titleMetrics(titleFont);
  painter.setFont(titleFont);
  painter.setPen(enabled ? theme->color("text") : theme->color("subtle"));
  const int x = 9;
  int y = 8;
  painter.drawText(QRect(x, y, this->width() - 2 * x, titleMetrics.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, this->text());
  y += titleMetrics.height() + 1;
  const QFont captionFont = theme->font("label");
  painter.setFont(captionFont);
  painter.setPen(enabled ? theme->color("muted") : theme->color("subtle"));
  const QFontMetrics captionMetrics(captionFont);
  painter.drawText(QRect(x, y, this->width() - 2 * x, captionMetrics.height()),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   captionMetrics.elidedText(this->Caption, Qt::ElideRight, this->width() - 2 * x));
}
