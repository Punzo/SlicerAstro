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

#ifndef __qSlicerAstroRingPlot_h
#define __qSlicerAstroRingPlot_h

// Qt includes
#include <QVector>
#include <QWidget>

#include "qSlicerAstroModelingModuleExport.h"

/// \ingroup SlicerAstro_QtModules_AstroModeling
/// Plot of a parameter of the rings of a tilted-ring model against the
/// radius (e.g. the rotation curve), in the colours of the SlicerAstro
/// theme; the ring under the mouse is highlighted with its values.
class Q_SLICERASTRO_QTMODULES_ASTROMODELING_EXPORT qSlicerAstroRingPlot : public QWidget
{
  Q_OBJECT

public:
  explicit qSlicerAstroRingPlot(QWidget* parent = nullptr);
  ~qSlicerAstroRingPlot() override;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  /// Radii (arcsec) and values of the rings; unit of the values (e.g. km/s)
  Q_INVOKABLE void setRings(const QVector<double>& radii, const QVector<double>& values, const QString& unit);
  Q_INVOKABLE void clearRings();
  Q_INVOKABLE QVector<double> radii() const;
  Q_INVOKABLE QVector<double> values() const;
  /// Text shown without rings
  Q_INVOKABLE void setPlaceholderText(const QString& text);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

  QRectF plotRect() const;
  QPointF toWidget(double radius, double value) const;

  QVector<double> Radii;
  QVector<double> Values;
  QString Unit;
  QString PlaceholderText;
  double XMin{0.};
  double XMax{1.};
  double YMin{0.};
  double YMax{1.};
  int HoverIndex{-1};
};

#endif
