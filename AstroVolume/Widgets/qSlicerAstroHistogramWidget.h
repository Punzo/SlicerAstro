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

#ifndef __qSlicerAstroHistogramWidget_h
#define __qSlicerAstroHistogramWidget_h

// Qt includes
#include <QString>
#include <QVector>
#include <QWidget>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Histogram of the data with the 2D colour window and the noise: counts on a
/// logarithmic axis, values in the units of the data, the window of the 2D
/// colour function (amber band, drag its edges or move it) and the noise σ
/// used by the 3D colour functions (white dashed line, drag it). The value under the
/// pointer is shown with its multiple of σ and its count.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroHistogramWidget
  : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(double windowMinimum READ windowMinimum)
  Q_PROPERTY(double windowMaximum READ windowMaximum)
  Q_PROPERTY(double noise READ noise)

public:
  explicit qSlicerAstroHistogramWidget(QWidget* parent = nullptr);
  ~qSlicerAstroHistogramWidget() override;

  /// Bins: centres (increasing, equal widths) and counts
  Q_INVOKABLE void setHistogram(const QVector<double>& centres, const QVector<double>& counts);
  Q_INVOKABLE void clearHistogram();
  Q_INVOKABLE QVector<double> binCentres() const;
  Q_INVOKABLE QVector<double> binCounts() const;
  /// Unit of the values (shown in the readout and the axis)
  Q_INVOKABLE void setUnit(const QString& unit);
  /// Values of the window and noise, without signals; NaN hides the noise
  Q_INVOKABLE void setWindow(double minimum, double maximum);
  Q_INVOKABLE void setNoise(double noise);
  /// The noise line can be dragged (e.g. not for 2D images)
  Q_INVOKABLE void setNoiseEditable(bool editable);

  double windowMinimum() const;
  double windowMaximum() const;
  double noise() const;
  /// Limits of the value axis
  Q_INVOKABLE double minimumValue() const;
  Q_INVOKABLE double maximumValue() const;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

signals:
  /// While dragging, and when released
  void windowChanging(double minimum, double maximum);
  void windowChanged(double minimum, double maximum);
  void noiseChanging(double noise);
  void noiseChanged(double noise);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

  QRectF plotRect() const;
  double valueToPixel(double value) const;
  double pixelToValue(double pixel) const;
  double countToPixel(double count) const;

  QVector<double> Centres;
  QVector<double> Counts;
  QString Unit;
  double XMin{0.};
  double XMax{1.};
  double LogMax{1.};
  double WindowMin{0.};
  double WindowMax{1.};
  double Noise;
  bool NoiseEditable{true};
  int HoverIndex{-1};

  enum DragMode { None, LowerEdge, UpperEdge, Shift, NoiseLine };
  DragMode Drag{None};
  double DragAnchor{0.};
  double DragWindowMin{0.};
  double DragWindowMax{0.};
};

#endif
