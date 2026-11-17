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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace
{
/// An arrow of the compass: direction on the screen, label and colour
struct CompassArrow
{
  const double* Direction;
  const char* Label;
  QColor Colour;
};
}

#include "qSlicerAstroTheme.h"
#include "qSlicerAstroViewFrame.h"

//-----------------------------------------------------------------------------
qSlicerAstroViewFrame::qSlicerAstroViewFrame(QWidget* parent)
  : QWidget(parent)
{
  this->setObjectName("AstroViewFrame");
  this->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  this->setAttribute(Qt::WA_NoSystemBackground, true);
  this->setAttribute(Qt::WA_TranslucentBackground, true);
  this->setFocusPolicy(Qt::NoFocus);
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(update()));
}

//-----------------------------------------------------------------------------
void qSlicerAstroViewFrame::attach(QWidget* container, QWidget* view)
{
  if (this->Container)
    {
    this->Container->removeEventFilter(this);
    }
  if (this->View)
    {
    this->View->removeEventFilter(this);
    }
  this->Container = container;
  this->View = view;
  if (container)
    {
    this->setParent(container);
    container->installEventFilter(this);
    this->setGeometry(container->rect());
    }
  if (view)
    {
    view->setAcceptDrops(true);
    view->installEventFilter(this);
    }
  this->raise();
  this->show();
}

//-----------------------------------------------------------------------------
qSlicerAstroViewFrame::~qSlicerAstroViewFrame()
{
  if (this->View)
    {
    this->View->removeEventFilter(this);
    }
  if (this->Container)
    {
    this->Container->removeEventFilter(this);
    }
}

//-----------------------------------------------------------------------------
QString qSlicerAstroViewFrame::mimeType()
{
  return QString("application/x-slicerastro-node-id");
}

//-----------------------------------------------------------------------------
QWidget* qSlicerAstroViewFrame::view() const
{
  return this->View;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroViewFrame::isActive() const
{
  return this->Active;
}

//-----------------------------------------------------------------------------
void qSlicerAstroViewFrame::setActive(bool active)
{
  this->Active = active;
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroViewFrame::setCompass(bool visible, double northX, double northY, double eastX, double eastY)
{
  this->CompassVisible = visible;
  this->North[0] = northX;
  this->North[1] = northY;
  this->East[0] = eastX;
  this->East[1] = eastY;
  this->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroViewFrame::setScaleBar(double pixels, const QString& label)
{
  this->ScaleBarPixels = pixels;
  this->ScaleBarLabel = label;
  this->update();
}

//-----------------------------------------------------------------------------
double qSlicerAstroViewFrame::cornerRadius() const
{
  return this->CornerRadius;
}

//-----------------------------------------------------------------------------
void qSlicerAstroViewFrame::setCornerRadius(double radius)
{
  this->CornerRadius = radius;
  this->update();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroViewFrame::cornerToken() const
{
  return this->CornerToken;
}

//-----------------------------------------------------------------------------
void qSlicerAstroViewFrame::setCornerToken(const QString& token)
{
  this->CornerToken = token;
  this->update();
}

//-----------------------------------------------------------------------------
bool qSlicerAstroViewFrame::eventFilter(QObject* watched, QEvent* event)
{
  if (watched == this->Container && this->Container &&
      (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::ChildAdded))
    {
    this->setGeometry(this->Container->rect());
    this->raise();
    return QWidget::eventFilter(watched, event);
    }
  if (watched != this->View)
    {
    return QWidget::eventFilter(watched, event);
    }
  switch (event->type())
    {
    case QEvent::MouseButtonPress:
      // the view keeps the event (interaction)
      emit this->activated();
      break;
    case QEvent::DragEnter:
      {
      QDragEnterEvent* dragEvent = static_cast<QDragEnterEvent*>(event);
      if (dragEvent->mimeData() && dragEvent->mimeData()->hasFormat(mimeType()))
        {
        dragEvent->acceptProposedAction();
        this->DropTarget = true;
        this->update();
        return true;
        }
      break;
      }
    case QEvent::DragMove:
      {
      QDragMoveEvent* dragEvent = static_cast<QDragMoveEvent*>(event);
      if (dragEvent->mimeData() && dragEvent->mimeData()->hasFormat(mimeType()))
        {
        dragEvent->acceptProposedAction();
        return true;
        }
      break;
      }
    case QEvent::DragLeave:
      if (this->DropTarget)
        {
        this->DropTarget = false;
        this->update();
        }
      break;
    case QEvent::Drop:
      {
      QDropEvent* dropEvent = static_cast<QDropEvent*>(event);
      if (dropEvent->mimeData() && dropEvent->mimeData()->hasFormat(mimeType()))
        {
        dropEvent->acceptProposedAction();
        this->DropTarget = false;
        this->update();
        emit this->nodeDropped(QString::fromUtf8(dropEvent->mimeData()->data(mimeType())));
        return true;
        }
      break;
      }
    default:
      break;
    }
  return QWidget::eventFilter(watched, event);
}

//-----------------------------------------------------------------------------
void qSlicerAstroViewFrame::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF outer(this->rect());
  QPainterPath rounded;
  rounded.addRoundedRect(outer, this->CornerRadius, this->CornerRadius);
  // corners: the colour around the views outside of the rounded rectangle
  if (this->CornerRadius > 0.)
    {
    QPainterPath corners;
    corners.addRect(outer);
    painter.fillPath(corners.subtracted(rounded), theme->color(this->CornerToken));
    }
  if (this->DropTarget)
    {
    QColor fill = theme->color("view-data");
    fill.setAlphaF(0.10);
    painter.fillPath(rounded, fill);
    }
  // overlays in the area of the view (below the bar, left of the slider)
  if (this->View && this->Container && (this->CompassVisible || this->ScaleBarPixels > 0.))
    {
    const QRectF view(QPointF(this->View->mapTo(this->Container, QPoint(0, 0))), QSizeF(this->View->size()));
    const QColor text = theme->color("view-text");
    QColor shadow = theme->color("view");
    shadow.setAlphaF(0.85);
    if (this->CompassVisible)
      {
      const QPointF origin(view.right() - 40., view.top() + 40.);
      const double length = 22.;
      painter.setFont(theme->font("monoSmall"));
      const CompassArrow arrows[2] =
        {{this->North, "N", theme->color("accent")}, {this->East, "E", text}};
      for (const CompassArrow& arrow : arrows)
        {
        const QPointF tip(origin.x() + length * arrow.Direction[0], origin.y() + length * arrow.Direction[1]);
        painter.setPen(QPen(shadow, 4., Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(origin, tip);
        painter.setPen(QPen(arrow.Colour, 1.6, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(origin, tip);
        const QPointF label(origin.x() + (length + 9.) * arrow.Direction[0], origin.y() + (length + 9.) * arrow.Direction[1]);
        painter.drawText(QRectF(label.x() - 8., label.y() - 8., 16., 16.), Qt::AlignCenter, arrow.Label);
        }
      }
    if (this->ScaleBarPixels > 0.)
      {
      const double right = view.right() - 22.;
      const double left = right - this->ScaleBarPixels;
      const double y = view.bottom() - 52.;
      painter.setPen(QPen(shadow, 4., Qt::SolidLine, Qt::FlatCap));
      painter.drawLine(QPointF(left, y), QPointF(right, y));
      painter.setPen(QPen(text, 1.6, Qt::SolidLine, Qt::FlatCap));
      painter.drawLine(QPointF(left, y), QPointF(right, y));
      painter.drawLine(QPointF(left, y - 4.), QPointF(left, y + 4.));
      painter.drawLine(QPointF(right, y - 4.), QPointF(right, y + 4.));
      painter.setFont(theme->font("monoSmall"));
      painter.drawText(QRectF(left, y - 20., right - left, 14.), Qt::AlignCenter, this->ScaleBarLabel);
      }
    }
  if (this->DropTarget || this->Active)
    {
    const QColor color = this->DropTarget ? theme->color("view-data") : theme->color("accent");
    painter.setPen(QPen(color, 2.));
    painter.setBrush(Qt::NoBrush);
    const double radius = std::max(0., this->CornerRadius - 1.);
    painter.drawRoundedRect(outer.adjusted(1., 1., -1., -1.), radius, radius);
    }
}
