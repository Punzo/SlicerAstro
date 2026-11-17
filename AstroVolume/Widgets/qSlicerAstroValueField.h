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

#ifndef __qSlicerAstroValueField_h
#define __qSlicerAstroValueField_h

// Qt includes
#include <QFrame>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class QDoubleSpinBox;
class QLabel;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// A number with its unit in one field (e.g. "270.6 km/s", "above 3.0 σ"):
/// tabular figures, the unit in a quieter colour, an optional leading word.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroValueField
  : public QFrame
{
  Q_OBJECT
  Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
  Q_PROPERTY(QString unit READ unit WRITE setUnit)
  Q_PROPERTY(QString leadingText READ leadingText WRITE setLeadingText)
  Q_PROPERTY(int decimals READ decimals WRITE setDecimals)

public:
  explicit qSlicerAstroValueField(QWidget* parent = nullptr);
  ~qSlicerAstroValueField() override;

  double value() const;
  QString unit() const;
  void setUnit(const QString& unit);
  QString leadingText() const;
  void setLeadingText(const QString& text);
  int decimals() const;
  void setDecimals(int decimals);
  Q_INVOKABLE void setRange(double minimum, double maximum);
  Q_INVOKABLE void setSingleStep(double step);
  Q_INVOKABLE QDoubleSpinBox* spinBox() const;

public slots:
  void setValue(double value);

signals:
  void valueChanged(double value);
  /// The user finished editing (Enter or focus out)
  void editingFinished();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

  QDoubleSpinBox* SpinBox;
  QLabel* LeadingLabel;
  QLabel* UnitLabel;
};

#endif
