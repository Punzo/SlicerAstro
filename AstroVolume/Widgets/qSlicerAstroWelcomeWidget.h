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

#ifndef __qSlicerAstroWelcomeWidget_h
#define __qSlicerAstroWelcomeWidget_h

// Qt includes
#include <QWidget>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qSlicerAstroWelcomeWidgetPrivate;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Welcome page of SlicerAstro: what it is, the theme, quick actions (open
/// data, the AstroWorkbench or the workspaces, sky search, documentation),
/// the recent files (File > Recent of Slicer) and workspaces, and the
/// sample data (downloaded once, then cached). One UI for:
/// - the Welcome module panel (narrow: one column);
/// - the home page of the AstroWorkbench (wide: a dashboard over the views,
///   setOverlayTarget, with its own scroll area).
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroWelcomeWidget : public QWidget
{
  Q_OBJECT
  /// Dashboard layout (cards in one row, recent files beside the samples)
  Q_PROPERTY(bool wide READ isWide WRITE setWide)
  /// In the AstroWorkbench: a Workspaces card instead of the Workbench card
  Q_PROPERTY(bool workbenchMode READ workbenchMode WRITE setWorkbenchMode)

public:
  explicit qSlicerAstroWelcomeWidget(QWidget* parent = nullptr);
  ~qSlicerAstroWelcomeWidget() override;

  bool isWide() const;
  void setWide(bool wide);
  bool workbenchMode() const;
  void setWorkbenchMode(bool workbench);

  /// Shown over target, covering it (e.g. the views of the main window);
  /// nullptr: not an overlay
  Q_INVOKABLE void setOverlayTarget(QWidget* target);

  /// Recent files and workspaces listed (at most 8, newest first): their
  /// number and names; samples
  Q_INVOKABLE int recentFileCount() const;
  Q_INVOKABLE QStringList recentNames() const;
  Q_INVOKABLE int sampleCount() const;
  Q_INVOKABLE QStringList sampleNames() const;

public slots:
  /// The Add data dialog of Slicer (FITS files, scenes, ...)
  void openData();
  /// The AstroWorkbench module
  void openWorkbench();
  /// The SlicerAstro documentation in the web browser
  void openDocumentation();
  /// The About window of SlicerAstro (versions, shortcuts, credits)
  void showAbout();
  void editApplicationSettings();
  /// Download (once) and load a sample; returns true if it was loaded
  bool loadSample(const QString& name);
  /// Load the recent file or open the recent workspace at index (0: the
  /// most recent)
  bool loadRecentFile(int index);
  /// Clear the history: the recent files (also File > Recent) and the recent
  /// workspaces (not the workspaces)
  void clearRecentFiles();
  /// The recently loaded files (from File > Recent) and the samples
  void updateLists();
  /// Theme of the SlicerAstro panels: "dark", "light" or "system"
  void setThemeMode(const QString& mode);

signals:
  /// Data were loaded from the page (a sample, a recent file)
  void dataLoaded();
  /// The Workspaces card (workbench mode)
  void workspacesRequested();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void showEvent(QShowEvent* event) override;

  QScopedPointer<qSlicerAstroWelcomeWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroWelcomeWidget);
  Q_DISABLE_COPY(qSlicerAstroWelcomeWidget);
};

#endif
