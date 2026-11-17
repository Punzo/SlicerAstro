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

#ifndef __qSlicerAstroSpectrumPlot_h
#define __qSlicerAstroSpectrumPlot_h

// Qt includes
#include <QString>
#include <QVector>
#include <QWidget>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// A spectrum with axes, drawn in the panel: the line in the data colour on
/// the view background, tick labels on both axes, optional width markers
/// (W50 as an amber band, W20 as dashed lines) and a readout of the channel
/// under the pointer.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroSpectrumPlot
  : public QWidget
{
  Q_OBJECT

public:
  explicit qSlicerAstroSpectrumPlot(QWidget* parent = nullptr);
  ~qSlicerAstroSpectrumPlot() override;

  /// Spectrum: x (e.g. velocity of each channel) and y; NaN values of y are
  /// gaps
  Q_INVOKABLE void setSpectrum(const QVector<double>& x, const QVector<double>& y);
  Q_INVOKABLE void clearSpectrum();
  Q_INVOKABLE QVector<double> spectrumX() const;
  Q_INVOKABLE QVector<double> spectrumY() const;

  /// Units of the axes, shown with the readout
  Q_INVOKABLE void setUnits(const QString& xUnit, const QString& yUnit);

  /// Edges of the line at 50% (band) and 20% (dashed lines) of the peak;
  /// NaN hides a marker
  Q_INVOKABLE void setWidthMarkers(double low50, double high50, double low20, double high20);
  Q_INVOKABLE void clearWidthMarkers();

  /// Text shown without a spectrum
  Q_INVOKABLE void setPlaceholderText(const QString& text);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

  QRectF plotRect() const;
  double xToPixel(double x) const;
  double yToPixel(double y) const;
  /// Index of the sample nearest to a pixel column (-1 without spectrum)
  int sampleAt(double pixel) const;

  QVector<double> X;
  QVector<double> Y;
  QString XUnit;
  QString YUnit;
  QString PlaceholderText;
  double XMin{0.};
  double XMax{1.};
  double YMin{0.};
  double YMax{1.};
  double Low50;
  double High50;
  double Low20;
  double High20;
  int HoverIndex{-1};
};

#endif
