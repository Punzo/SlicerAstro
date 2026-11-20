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

#ifndef __qSlicerAstroSpectrumRangeWidget_h
#define __qSlicerAstroSpectrumRangeWidget_h

// Qt includes
#include <QVector>
#include <QWidget>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// A range of the spectral axis chosen on the integrated spectrum: the
/// spectrum in the data colour on the dark view background, the range in
/// amber. Drag an edge to move it, drag inside the range to shift it, drag
/// outside to draw a new range. Values are in the units of the x axis (e.g.
/// km/s); the x values may decrease with the channel.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroSpectrumRangeWidget
  : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(double minimumValue READ minimumValue)
  Q_PROPERTY(double maximumValue READ maximumValue)

public:
  explicit qSlicerAstroSpectrumRangeWidget(QWidget* parent = nullptr);
  ~qSlicerAstroSpectrumRangeWidget() override;

  /// Spectrum: x (e.g. velocity of each channel) and y; NaN values of y are
  /// gaps
  Q_INVOKABLE void setSpectrum(const QVector<double>& x, const QVector<double>& y);
  Q_INVOKABLE void clearSpectrum();
  Q_INVOKABLE QVector<double> spectrumX() const;
  Q_INVOKABLE QVector<double> spectrumY() const;

  double minimumValue() const;
  double maximumValue() const;
  /// Limits of the x axis
  Q_INVOKABLE double minimumX() const;
  Q_INVOKABLE double maximumX() const;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  /// The channels of the line ("Auto from line"): where the spectrum is
  /// above its noise around its peak (baseline and noise from the median and
  /// the median absolute deviation; channels above 1.5 sigma around the peak,
  /// plus two). Sets the range and emits rangeChanged. Returns an empty text,
  /// or why the range is unchanged (too few channels, no line).
  Q_INVOKABLE QString setRangeFromLine();

public slots:
  /// Range in x units (clamped to the x axis); no signal
  void setRange(double minimum, double maximum);

signals:
  /// While dragging
  void rangeChanging(double minimum, double maximum);
  /// The user set a range (mouse released)
  void rangeChanged(double minimum, double maximum);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

  double xToPixel(double x) const;
  double pixelToX(double pixel) const;
  QRectF plotRect() const;

  QVector<double> X;
  QVector<double> Y;
  double XMin{0.};
  double XMax{1.};
  double YMin{0.};
  double YMax{1.};
  double RangeMin{0.};
  double RangeMax{1.};

  enum DragMode { None, LowerEdge, UpperEdge, Shift, Draw };
  DragMode Drag{None};
  double DragAnchor{0.};
  double DragRangeMin{0.};
  double DragRangeMax{0.};
};

#endif
