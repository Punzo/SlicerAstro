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

#ifndef __qSlicerAstroMasksSection_h
#define __qSlicerAstroMasksSection_h

// Qt includes
#include <QScopedPointer>
#include <QStringList>
#include <QCursor>
#include <QPixmap>
#include <QWidget>

// CTK includes
#include <ctkVTKObject.h>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qMRMLSegmentEditorWidget;
class qSlicerAstroMasksSectionPrivate;
class vtkMRMLNode;
class vtkObject;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Mask of an astronomical volume, a reusable section (AstroVolume panel,
/// Workbench): the mask (a segmentation, e.g. a SoFiA mask), its segments,
/// show or hide, the Segment Editor in the section, export as a FITS mask, and
/// copy or move segments to the mask of other data on the same grid.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroMasksSection : public QWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  explicit qSlicerAstroMasksSection(QWidget* parent = nullptr);
  ~qSlicerAstroMasksSection() override;

  /// Text of the last result (segments copied, file saved, or why not)
  Q_INVOKABLE QString status() const;
  /// The mask of the data (one per dataset, "<data>_mask")
  Q_INVOKABLE vtkMRMLNode* maskNode() const;
  /// Number of segments of the mask
  Q_INVOKABLE int segmentCount() const;
  /// IDs of the selected segments
  Q_INVOKABLE QStringList selectedSegmentIDs() const;
  /// The Segment Editor of the section (nullptr until editing starts)
  Q_INVOKABLE qMRMLSegmentEditorWidget* segmentEditor() const;
  /// True while the Segment Editor is shown (Edit checked)
  Q_INVOKABLE bool isEditing() const;
  /// Name of the active Segment Editor effect (empty: none, or not editing)
  Q_INVOKABLE QString activeEffect() const;
  /// Width the Segment Editor needs to show the options of the active effect
  /// without clipping (0: not editing)
  Q_INVOKABLE int editorWidthHint() const;
  /// The cursor of the views for an effect of the Segment Editor (an arrow
  /// with the icon of the effect in the theme)
  static QCursor effectCursor(const QString& effectName);
  /// The same for scripts (the pixmap of the cursor)
  Q_INVOKABLE QPixmap effectCursorPixmap(const QString& effectName) const;

signals:
  /// An effect of the Segment Editor was activated or deactivated (empty
  /// name: none, or editing ended)
  void activeEffectChanged(const QString& effect);
  /// The Segment Editor was shown (Edit) or hidden
  void editingChanged(bool editing);

public slots:
  /// The volume whose mask is shown (nullptr: none)
  void setVolumeNode(vtkMRMLNode* node);
  /// A mask of the volume (it becomes the mask of the volume)
  void setMaskNode(vtkMRMLNode* node);
  void setMaskVisible(bool visible);
  /// Shows (or hides) all the segments of the mask
  void setAllSegmentsVisible(bool visible);
  /// All the segments shown if none is shown, otherwise all hidden
  void toggleAllSegments();
  /// A new segment in the mask (selected: the segment edited); its ID
  QString addSegment();
  /// Removes the selected segments from the mask
  void removeSelectedSegments();
  /// The segment dialog of SlicerAstro (name, type, colour) for a segment
  bool editSegment(const QString& segmentID);
  /// Sets the cursor of the active effect on the views
  void applyEffectCursor();
  /// Shows the Segment Editor in the section, on the mask and the volume
  void setEditing(bool editing);
  /// Copies (or moves) the selected segments (all if none is selected) to
  /// the mask of the volume targetID; returns true on success
  bool copySegments(const QString& targetID, bool move);
  /// Exports the mask as a FITS mask file (fileName empty: asks for it)
  bool saveMask(const QString& fileName = QString());
  /// Updates the widgets from the mask
  void updateFromMask();

protected slots:
  void onMaskSelected(vtkMRMLNode* node);
  void onSceneNodeAddedOrRemoved();
  void onCopyClicked();
  void onMoveClicked();
  void onSaveClicked();
  void onEditorNodeModified();
  void restyleEditor();
  void adjustEffectOptions();
  void syncPixelSpinBoxes();
  void onTableSelectionChanged();
  void onEditorSegmentChanged(const QString& segmentID);

protected:
  void updateTargets();

  QScopedPointer<qSlicerAstroMasksSectionPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroMasksSection);
  Q_DISABLE_COPY(qSlicerAstroMasksSection);
};

#endif
