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

#ifndef __qSlicerAstroContoursSection_h
#define __qSlicerAstroContoursSection_h

// Qt includes
#include <QScopedPointer>
#include <QWidget>

// CTK includes
#include <ctkVTKObject.h>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qSlicerAstroContoursSectionPrivate;
class vtkMRMLNode;
class vtkObject;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Contour levels drawn over an astronomical volume, a reusable section
/// (AstroVolume panel, Workbench): the data of the contours (the volume, or
/// other data reprojected on its sky grid), levels in units of the noise, of
/// HI column density or of the data, colour family, draw, show, smooth,
/// delete, and the contours of each source with their visibility.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroContoursSection : public QWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  explicit qSlicerAstroContoursSection(QWidget* parent = nullptr);
  ~qSlicerAstroContoursSection() override;

  enum LevelUnit { Sigma = 0, ColumnDensity, DataUnit };
  Q_ENUM(LevelUnit);

  /// Text of the last result (levels drawn, or why not)
  Q_INVOKABLE QString status() const;

public slots:
  /// The volume the contours are drawn over (nullptr: none)
  void setVolumeNode(vtkMRMLNode* node);
  /// Data of the contours (nullptr: the volume itself)
  void setSourceNode(vtkMRMLNode* node);
  /// Levels separated by commas, in unit (LevelUnit)
  void setLevels(const QString& levels, int unit);
  /// Draws the levels; returns the number of levels drawn
  int drawContours();
  void setContoursVisible(bool visible);
  void setContoursSmooth(bool smooth);
  void deleteContours();
  /// Updates the list of the sources and the buttons
  void updateSources();

protected slots:
  void onSourceChanged(int index);
  void onUnitChanged(int index);
  void onSourceVisibilityToggled(bool visible);
  void onSceneNodeAddedOrRemoved();

protected:
  QScopedPointer<qSlicerAstroContoursSectionPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroContoursSection);
  Q_DISABLE_COPY(qSlicerAstroContoursSection);
};

#endif
