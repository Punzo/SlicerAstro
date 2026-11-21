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

#ifndef __qSlicerAstroSourcesSection_h
#define __qSlicerAstroSourcesSection_h

// Qt includes
#include <QScopedPointer>
#include <QWidget>

// CTK includes
#include <ctkVTKObject.h>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class QModelIndex;
class qSlicerAstroSourcesSectionPrivate;
class vtkMRMLNode;
class vtkObject;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Source catalogues of an astronomical volume, a reusable section
/// (AstroVolume panel, Workbench): the catalogues placed on the volume (SoFiA
/// or other catalogues with positions), their table, load, points, labels
/// and 2D display of the sources, their colours and size; a click on a
/// source centres the views on it, selects its point and its segment in the
/// mask; several sources can be selected (Ctrl/Shift) and deleted.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroSourcesSection : public QWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  explicit qSlicerAstroSourcesSection(QWidget* parent = nullptr);
  ~qSlicerAstroSourcesSection() override;

  /// Number of catalogues of the volume, and of sources of the catalogue shown
  Q_INVOKABLE int catalogueCount() const;
  Q_INVOKABLE int sourceCount() const;
  /// The selected source, or the result of the last load
  Q_INVOKABLE QString status() const;
  /// Segment of the selected source in the mask of the volume ("" if none)
  Q_INVOKABLE QString selectedSegmentID() const;
  /// Rows of the sources selected in the table
  Q_INVOKABLE QList<int> selectedRows() const;

public slots:
  /// The volume whose catalogues are shown (nullptr: none)
  void setVolumeNode(vtkMRMLNode* node);
  /// The catalogue shown (a table of the volume)
  void setCatalogueNode(vtkMRMLNode* node);
  /// Load a catalogue file and place its sources on the volume (fileName
  /// empty: asks for it); returns true on success
  bool loadCatalogue(const QString& fileName = QString());
  /// Select the source of a row of the table
  void selectSource(int row);
  void setPointsVisible(bool visible);
  void setLabelsVisible(bool visible);
  void setSlicesVisible(bool visible);
  /// Points moved with the mouse (unlocked) or not
  void setPointsMovable(bool movable);
  /// Colour of the points, and of the points of the selected sources
  void setPointColor(const QColor& color);
  void setSelectedPointColor(const QColor& color);
  /// Size of the points (glyph scale of the markups)
  void setPointSize(double size);
  /// Delete the selected sources (rows of the table and points)
  void deleteSelectedSources();
  /// Remove the catalogue shown from the scene (ask: after a confirmation)
  void removeCatalogue(bool ask = true);
  /// Updates the list of the catalogues and the buttons
  void updateFromScene();

protected slots:
  void onCatalogueChanged(int index);
  void onCurrentRowChanged(const QModelIndex& current, const QModelIndex& previous);
  void onSelectionChanged();

protected:
  QScopedPointer<qSlicerAstroSourcesSectionPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroSourcesSection);
  Q_DISABLE_COPY(qSlicerAstroSourcesSection);
};

#endif
