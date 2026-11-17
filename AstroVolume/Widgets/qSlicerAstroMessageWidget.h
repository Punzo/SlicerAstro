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

#ifndef __qSlicerAstroMessageWidget_h
#define __qSlicerAstroMessageWidget_h

// Qt includes
#include <QFrame>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class QHBoxLayout;
class QLabel;
class QPushButton;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// A message shown next to the control that caused it, instead of a dialog:
/// "info" (cyan), "warning" (orange), "error" (red), "success" (check). It
/// can carry action buttons (e.g. "Create aligned copy", "Keep rotation").
/// Hidden while empty.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroMessageWidget
  : public QFrame
{
  Q_OBJECT
  Q_PROPERTY(QString text READ text)
  Q_PROPERTY(QString kind READ kind)

public:
  explicit qSlicerAstroMessageWidget(QWidget* parent = nullptr);
  ~qSlicerAstroMessageWidget() override;

  QString text() const;
  QString kind() const;

  /// Add an action button under the text; actionTriggered(name) when clicked
  Q_INVOKABLE QPushButton* addAction(const QString& name, const QString& text, bool primary = false);

public slots:
  /// kind: "info", "warning", "error" or "success"; removes the actions
  void showMessage(const QString& kind, const QString& text);
  void clearMessage();

signals:
  void actionTriggered(const QString& name);

protected slots:
  void updateIcon();

protected:
  QLabel* IconLabel;
  QLabel* TextLabel;
  QWidget* Actions;
  QHBoxLayout* ActionsLayout;
  QString Kind;
};

#endif
