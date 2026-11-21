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

#ifndef __qSlicerAstroSubCubeDialog_h
#define __qSlicerAstroSubCubeDialog_h

// Qt includes
#include <QDialog>
#include <QScopedPointer>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qSlicerAstroSubCubeDialogPrivate;
class vtkMRMLNode;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Which part of a FITS file to load: the whole data or a sub-cube (pixel
/// ranges on the sky and a range of channels, with their velocities), with
/// the memory each takes. Asked when the data of a file would take a large
/// part of the memory, and from the Info section (load a part of the file).
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroSubCubeDialog : public QDialog
{
  Q_OBJECT

public:
  /// Result of ask(): cancelled, the whole data, or the sub-cube
  enum Choice
    {
    Cancelled = 0,
    WholeData,
    SubCube
    };

  explicit qSlicerAstroSubCubeDialog(QWidget* parent = nullptr);
  ~qSlicerAstroSubCubeDialog() override;

  /// The file and its HDU (0: the first image HDU with data); false if its
  /// header can not be read
  /// The file and its image extension (HDU; 0: the first image HDU with
  /// data); the Extension row lists the image extensions of files with several
  Q_INVOKABLE bool setFile(const QString& fileName, int hduNumber = 0);
  /// The chosen extension (HDU number)
  Q_INVOKABLE int hduNumber() const;
  /// Loads the choice (the whole data or the part of the chosen extension) as
  /// a new dataset, made active; returns it (nullptr: cancelled or failed)
  Q_INVOKABLE vtkMRMLNode* load();
  /// Sub-cube (i0, i1, j0, j1, k0, k1; 0-based, inclusive)
  Q_INVOKABLE void setSubExtent(int i0, int i1, int j0, int j1, int k0, int k1);
  Q_INVOKABLE QList<int> subExtent() const;
  /// Bytes of the whole data and of the sub-cube when loaded
  Q_INVOKABLE double wholeBytes() const;
  Q_INVOKABLE double subCubeBytes() const;
  Q_INVOKABLE int choice() const;

  /// Physical memory of the computer (bytes)
  static double physicalMemory();
  /// Bytes of the data of a FITS HDU when loaded (header only; 0 if unknown)
  static double dataBytes(const QString& fileName, int hduNumber = 0);
  /// Asks which part to load; subExtent receives the sub-cube
  /// Asks which extension and part of the file to load: hduNumber (in and
  /// out) and subExtent (out); returns the choice
  static int ask(const QString& fileName, int* hduNumber, QWidget* parent, int subExtent[6]);
  /// The image extensions of a file (HDU numbers and "NAME · 134×70×83 ·
  /// Jy/beam" texts), read from the headers and cached by file and date
  static void imageExtensions(const QString& fileName, QList<int>& numbers, QStringList& texts);

protected slots:
  void updateSummary();
  void onWholeClicked();
  void onPartClicked();
  void onExtensionChanged(int index);

protected:
  /// Dimensions and spectral axis of an extension in the ranges
  bool readDescription(int hduNumber);

  QScopedPointer<qSlicerAstroSubCubeDialogPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroSubCubeDialog);
  Q_DISABLE_COPY(qSlicerAstroSubCubeDialog);
};

#endif
