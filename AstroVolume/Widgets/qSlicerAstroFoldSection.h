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

#ifndef __qSlicerAstroFoldSection_h
#define __qSlicerAstroFoldSection_h

// Qt includes
#include <QWidget>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class QLabel;
class QToolButton;
class QVBoxLayout;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Progressive disclosure: a folded section shows a one-line summary of what
/// it contains ("Advanced · output names, upper limit"). Folded by default;
/// the state is remembered in the settings when a settings key is set.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroFoldSection
  : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(QString title READ title WRITE setTitle)
  Q_PROPERTY(QString summary READ summary WRITE setSummary)
  Q_PROPERTY(bool collapsed READ isCollapsed WRITE setCollapsed NOTIFY collapsedChanged)
  Q_PROPERTY(QString settingsKey READ settingsKey WRITE setSettingsKey)

public:
  explicit qSlicerAstroFoldSection(QWidget* parent = nullptr);
  ~qSlicerAstroFoldSection() override;

  QString title() const;
  void setTitle(const QString& title);
  QString summary() const;
  void setSummary(const QString& summary);
  bool isCollapsed() const;
  QString settingsKey() const;
  /// Restores the remembered state
  void setSettingsKey(const QString& key);

  /// Widget holding the content (with a vertical layout)
  Q_INVOKABLE QWidget* contentWidget() const;
  Q_INVOKABLE QVBoxLayout* contentLayout() const;

public slots:
  void setCollapsed(bool collapsed);
  void toggle();

signals:
  void collapsedChanged(bool collapsed);

protected slots:
  void updateIcon();

protected:
  QToolButton* Header;
  QLabel* IconLabel{nullptr};
  QLabel* TitleLabel{nullptr};
  QLabel* SummaryLabel;
  QWidget* Content;
  QVBoxLayout* ContentLayout;
  QString Title;
  QString SettingsKey;
};

#endif
