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


#ifndef __qSlicerAstroMouseMoveCompressor_h
#define __qSlicerAstroMouseMoveCompressor_h

// Qt includes
#include <QObject>
#include <QPointer>

// STD includes
#include <memory>

class QMouseEvent;

/// \ingroup SlicerAstro_QtModules_AstroVolume
/// Application event filter for the mouse moves of the VTK views (slice and
/// 3D views):
/// - hover moves are dropped: QVTKOpenGLNativeWidget has mouse tracking and
///   Qt::WA_Hover, and QVTKInteractorAdapter turns both QEvent::MouseMove and
///   QEvent::HoverMove into a vtkCommand::MouseMoveEvent. Qt 6 sends a hover
///   move with every mouse move, so every pointer motion was processed twice
///   (interaction, displayable managers and a render of all the views).
/// - optionally (by default on Wayland) consecutive mouse moves are replaced
///   by the last one, delivered once per pass of the event loop, as Qt does on
///   X11 (Qt::AA_CompressHighFrequencyEvents). On Wayland every pointer event
///   is delivered (up to 1000 per second). Other input events (buttons, wheel,
///   keys, enter and leave) deliver the pending move first, so the order of
///   the events is kept.
class qSlicerAstroMouseMoveCompressor : public QObject
{
  Q_OBJECT

public:
  explicit qSlicerAstroMouseMoveCompressor(QObject* parent = nullptr);
  ~qSlicerAstroMouseMoveCompressor() override;

  /// Whether the compression is needed: platforms without compression of
  /// the mouse moves (Wayland). The environment variable
  /// SLICERASTRO_COMPRESS_MOUSE_MOVES (0 or 1) overrides it.
  static bool isNeeded();

  bool eventFilter(QObject* watched, QEvent* event) override;

  /// Compression of the mouse moves (hover moves are always dropped)
  void setCompressMoves(bool compress);
  bool compressMoves() const;

  /// Moves not delivered (replaced by a later move), hover moves dropped
  int compressedMoves() const;
  int droppedHoverMoves() const;

public slots:
  /// Deliver the pending move
  void flush();

protected:
  QPointer<QObject> PendingReceiver;
  std::unique_ptr<QMouseEvent> PendingEvent;
  bool Replaying{false};
  bool FlushScheduled{false};
  int CompressedMoves{0};
  int DroppedHoverMoves{0};
  bool CompressMoves{true};
};

#endif
