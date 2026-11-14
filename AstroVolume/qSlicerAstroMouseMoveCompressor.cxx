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

  This file was originally developed by Davide Punzo, Kapteyn Astronomical Institute,
  and was supported through the European Research Council grant nr. 291531.
==============================================================================*/


// AstroVolume includes
#include "qSlicerAstroMouseMoveCompressor.h"

// Qt includes
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QTimer>

//-----------------------------------------------------------------------------
namespace
{
QMouseEvent* CloneMouseEvent(const QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return static_cast<QMouseEvent*>(event->clone());
#else
  return new QMouseEvent(event->type(), event->localPos(), event->windowPos(), event->screenPos(),
                         event->button(), event->buttons(), event->modifiers(), event->source());
#endif
}

//-----------------------------------------------------------------------------
bool IsInputEvent(QEvent::Type type)
{
  switch (type)
    {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::Wheel:
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::Enter:
    case QEvent::Leave:
    case QEvent::FocusOut:
    case QEvent::ContextMenu:
      return true;
    default:
      return false;
    }
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
qSlicerAstroMouseMoveCompressor::qSlicerAstroMouseMoveCompressor(QObject* parent)
  : QObject(parent)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroMouseMoveCompressor::~qSlicerAstroMouseMoveCompressor() = default;

//-----------------------------------------------------------------------------
bool qSlicerAstroMouseMoveCompressor::isNeeded()
{
  const QByteArray setting = qgetenv("SLICERASTRO_COMPRESS_MOUSE_MOVES");
  if (!setting.isEmpty())
    {
    return setting != "0";
    }
  return QGuiApplication::platformName().startsWith("wayland");
}

//-----------------------------------------------------------------------------
int qSlicerAstroMouseMoveCompressor::compressedMoves() const
{
  return this->CompressedMoves;
}

//-----------------------------------------------------------------------------
int qSlicerAstroMouseMoveCompressor::droppedHoverMoves() const
{
  return this->DroppedHoverMoves;
}

//-----------------------------------------------------------------------------
void qSlicerAstroMouseMoveCompressor::setCompressMoves(bool compress)
{
  if (!compress)
    {
    this->flush();
    }
  this->CompressMoves = compress;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroMouseMoveCompressor::compressMoves() const
{
  return this->CompressMoves;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroMouseMoveCompressor::eventFilter(QObject* watched, QEvent* event)
{
  if (!watched || !event)
    {
    return false;
    }
  const QEvent::Type type = event->type();
  // hover moves duplicate the mouse moves (also those replayed here)
  if (type == QEvent::HoverMove && watched->inherits("QVTKOpenGLNativeWidget"))
    {
    this->DroppedHoverMoves++;
    return true;
    }
  if (this->Replaying || !this->CompressMoves)
    {
    return false;
    }
  if (type == QEvent::MouseMove && watched->inherits("QVTKOpenGLNativeWidget"))
    {
    if (this->PendingEvent && this->PendingReceiver != watched)
      {
      this->flush();
      }
    if (this->PendingEvent)
      {
      this->CompressedMoves++;
      }
    this->PendingReceiver = watched;
    this->PendingEvent.reset(CloneMouseEvent(static_cast<QMouseEvent*>(event)));
    if (!this->FlushScheduled)
      {
      this->FlushScheduled = true;
      QTimer::singleShot(0, this, SLOT(flush()));
      }
    return true;
    }
  if (this->PendingEvent && IsInputEvent(type))
    {
    this->flush();
    }
  return false;
}

//-----------------------------------------------------------------------------
void qSlicerAstroMouseMoveCompressor::flush()
{
  this->FlushScheduled = false;
  if (!this->PendingEvent)
    {
    return;
    }
  std::unique_ptr<QMouseEvent> event = std::move(this->PendingEvent);
  QPointer<QObject> receiver = this->PendingReceiver;
  this->PendingReceiver = nullptr;
  if (!receiver)
    {
    return;
    }
  this->Replaying = true;
  QCoreApplication::sendEvent(receiver, event.get());
  this->Replaying = false;
}
