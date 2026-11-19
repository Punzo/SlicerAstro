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
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>

// STD includes
#include <algorithm>

#include "qSlicerAstroScrollAreaFitter.h"

//-----------------------------------------------------------------------------
qSlicerAstroScrollAreaFitter::qSlicerAstroScrollAreaFitter(QObject* parent)
  : QObject(parent)
{
  this->setObjectName("AstroScrollAreaFitter");
  this->setScrollArea(qobject_cast<QScrollArea*>(parent));
}

//-----------------------------------------------------------------------------
void qSlicerAstroScrollAreaFitter::setScrollArea(QScrollArea* scrollArea)
{
  if (this->ScrollArea == scrollArea)
    {
    return;
    }
  if (this->ScrollArea)
    {
    this->ScrollArea->removeEventFilter(this);
    }
  this->ScrollArea = scrollArea;
  if (scrollArea)
    {
    scrollArea->installEventFilter(this);
    }
  QTimer::singleShot(0, this, SLOT(fit()));
}

//-----------------------------------------------------------------------------
qSlicerAstroScrollAreaFitter::~qSlicerAstroScrollAreaFitter() = default;

//-----------------------------------------------------------------------------
qSlicerAstroScrollAreaFitter* qSlicerAstroScrollAreaFitter::install(QScrollArea* scrollArea)
{
  if (!scrollArea)
    {
    return nullptr;
    }
  qSlicerAstroScrollAreaFitter* fitter =
    scrollArea->findChild<qSlicerAstroScrollAreaFitter*>("AstroScrollAreaFitter", Qt::FindDirectChildrenOnly);
  return fitter ? fitter : new qSlicerAstroScrollAreaFitter(scrollArea);
}

//-----------------------------------------------------------------------------
int qSlicerAstroScrollAreaFitter::requiredWidth() const
{
  QWidget* contents = this->ScrollArea ? this->ScrollArea->widget() : nullptr;
  if (!contents)
    {
    return 0;
    }
  const QMargins margins = this->ScrollArea->contentsMargins();
  int scrollBarWidth = 0;
  if (this->ScrollArea->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
    {
    QScrollBar* scrollBar = this->ScrollArea->verticalScrollBar();
    scrollBarWidth = scrollBar ? std::max(scrollBar->sizeHint().width(),
      this->ScrollArea->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, scrollBar)) : 0;
    }
  return contents->minimumSizeHint().width() + scrollBarWidth + margins.left() + margins.right() +
         2 * this->ScrollArea->frameWidth();
}

//-----------------------------------------------------------------------------
void qSlicerAstroScrollAreaFitter::fit()
{
  if (!this->ScrollArea || this->Fitting)
    {
    return;
    }
  this->Fitting = true;
  // contents replaced (e.g. the module panel shows another module)
  QWidget* contents = this->ScrollArea->widget();
  if (contents != this->Contents)
    {
    if (this->Contents)
      {
      this->Contents->removeEventFilter(this);
      }
    this->Contents = contents;
    if (contents)
      {
      contents->installEventFilter(this);
      }
    }
  const int width = this->ScrollArea->isVisible() ? this->requiredWidth() : 0;
  if (this->ScrollArea->minimumWidth() != width)
    {
    this->ScrollArea->setMinimumWidth(width);
    }
  this->Fitting = false;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroScrollAreaFitter::eventFilter(QObject* watched, QEvent* event)
{
  switch (event->type())
    {
    case QEvent::LayoutRequest:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::ChildAdded:
    case QEvent::ChildRemoved:
    case QEvent::StyleChange:
      // after the layouts are updated
      QTimer::singleShot(0, this, SLOT(fit()));
      break;
    default:
      break;
    }
  return QObject::eventFilter(watched, event);
}
