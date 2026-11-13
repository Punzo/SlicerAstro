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
/// Application event filter compressing the mouse moves of the VTK views
/// (slice and 3D views): consecutive moves are replaced by the last one,
/// delivered once per pass of the event loop, as Qt does on X11
/// (Qt::AA_CompressHighFrequencyEvents). On Wayland every pointer event is
/// delivered (up to 1000 per second), and each one pans, zooms or updates
/// the views. Other input events (buttons, wheel, keys, enter and leave)
/// deliver the pending move first, so the order of the events is kept.
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

  /// Moves not delivered (replaced by a later move)
  int compressedMoves() const;

public slots:
  /// Deliver the pending move
  void flush();

protected:
  QPointer<QObject> PendingReceiver;
  std::unique_ptr<QMouseEvent> PendingEvent;
  bool Replaying{false};
  bool FlushScheduled{false};
  int CompressedMoves{0};
};

#endif
