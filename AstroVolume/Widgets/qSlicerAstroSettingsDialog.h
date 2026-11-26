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

#ifndef __qSlicerAstroSettingsDialog_h
#define __qSlicerAstroSettingsDialog_h

// Qt includes
#include <QDialog>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qSlicerAstroSettingsDialogPrivate;
class vtkSlicerAstroVolumeLogic;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Settings of SlicerAstro (a few, for the work with astronomical data):
/// theme, saving of new data as
/// compressed FITS, scenes folder, quality of the 3D rendering, size
/// of the cubes shown in 3D automatically, size of the files for which a
/// part is offered at loading. The changes apply at once and are kept in
/// the application settings (keys SlicerAstro/...).
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroSettingsDialog : public QDialog
{
  Q_OBJECT

public:
  explicit qSlicerAstroSettingsDialog(QWidget* parent = nullptr);
  ~qSlicerAstroSettingsDialog() override;

  /// The settings applied to the AstroVolume logic (at startup and after a
  /// change); nullptr: the logic of the AstroVolume module
  static void applySettings(vtkSlicerAstroVolumeLogic* logic = nullptr);

  /// Show the dialog (modal) for the application
  static void showSettings(QWidget* parent = nullptr);

  /// Values (for scripts and tests)
  static bool saveCompressed();
  static int volumeRenderingQuality();
  static double autoShow3DMaximumMegabytes();
  static double largeDataPercent();
  static QString scenesDirectory();

public slots:
  void setSaveCompressed(bool compressed);
  /// vtkMRMLViewNode::VolumeRenderingQuality (0 adaptive, 1 normal, 2 maximum)
  void setVolumeRenderingQuality(int quality);
  void setAutoShow3DMaximumMegabytes(double megabytes);
  void setLargeDataPercent(double percent);
  void setScenesDirectory(const QString& directory);
  /// The fields from the settings
  void updateFromSettings();

protected:
  QScopedPointer<qSlicerAstroSettingsDialogPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroSettingsDialog);
  Q_DISABLE_COPY(qSlicerAstroSettingsDialog);
};

#endif
