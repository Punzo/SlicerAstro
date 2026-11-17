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

#ifndef __qSlicerAstroAxisScrubber_h
#define __qSlicerAstroAxisScrubber_h

// Qt includes
#include <QVector>
#include <QWidget>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class QLabel;
class QPushButton;
class QSlider;
class QTimer;
class qSlicerAstroAxisScrubberProfile;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Scrubber of an axis of the data: the spectral axis of a cube (channels,
/// velocities), later the time of simulation snapshots. A profile along the
/// axis (e.g. the integrated spectrum) is drawn behind the playhead; the
/// readout shows the value with its unit and the index. Play steps through
/// the axis.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroAxisScrubber
  : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
  Q_PROPERTY(bool playing READ isPlaying WRITE setPlaying NOTIFY playingChanged)
  Q_PROPERTY(double framesPerSecond READ framesPerSecond WRITE setFramesPerSecond)

public:
  explicit qSlicerAstroAxisScrubber(QWidget* parent = nullptr);
  ~qSlicerAstroAxisScrubber() override;

  /// Axis: name of an index ("channel", "snapshot"), unit of the values
  /// ("km/s", "Gyr"), a label of the axis (e.g. "VRAD") and the value of
  /// each index
  Q_INVOKABLE void setAxis(const QString& indexName, const QString& unit, const QString& axisLabel,
                           const QVector<double>& values);
  /// Profile drawn behind the playhead (same length as the values)
  Q_INVOKABLE void setProfile(const QVector<double>& profile);
  Q_INVOKABLE void clear();
  Q_INVOKABLE int count() const;
  Q_INVOKABLE double currentValue() const;

  int currentIndex() const;
  bool isPlaying() const;
  double framesPerSecond() const;
  void setFramesPerSecond(double fps);

public slots:
  void setCurrentIndex(int index);
  void setPlaying(bool playing);
  void togglePlaying();

signals:
  void currentIndexChanged(int index);
  void playingChanged(bool playing);

protected slots:
  void onTimeout();
  void updateReadout();
  void updatePlayIcon();

protected:
  QPushButton* PlayButton;
  qSlicerAstroAxisScrubberProfile* Profile;
  QSlider* Slider;
  QLabel* ValueLabel;
  QLabel* IndexLabel;
  QTimer* Timer;
  QString IndexName;
  QString Unit;
  QString AxisLabel;
  QVector<double> Values;
};

#endif
