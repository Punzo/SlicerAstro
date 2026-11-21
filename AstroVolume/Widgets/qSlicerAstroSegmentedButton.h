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

#ifndef __qSlicerAstroSegmentedButton_h
#define __qSlicerAstroSegmentedButton_h

// Qt includes
#include <QFrame>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class QButtonGroup;
class QToolButton;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Segmented control: exclusive choices side by side (e.g. Threshold | Mask,
/// CPU | GPU).
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroSegmentedButton
  : public QFrame
{
  Q_OBJECT
  Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)

public:
  explicit qSlicerAstroSegmentedButton(QWidget* parent = nullptr);
  ~qSlicerAstroSegmentedButton() override;

  /// Add a choice; returns its index
  Q_INVOKABLE int addButton(const QString& text, const QString& toolTip = QString());
  Q_INVOKABLE QToolButton* button(int index) const;
  Q_INVOKABLE int count() const;

  int currentIndex() const;

public slots:
  void setCurrentIndex(int index);
  /// No choice checked (e.g. a value set elsewhere); currentIndex is -1
  void clearSelection();

signals:
  void currentIndexChanged(int index);

protected:
  QButtonGroup* Group;
};

#endif
