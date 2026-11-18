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
/// show or hide, the Segment Editor in the section, save as a FITS mask, and
/// copy or move segments to the mask of other data.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroMasksSection : public QWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  explicit qSlicerAstroMasksSection(QWidget* parent = nullptr);
  ~qSlicerAstroMasksSection() override;

  /// Text of the last result (segments copied, file saved, or why not)
  Q_INVOKABLE QString status() const;
  /// Number of segments of the mask
  Q_INVOKABLE int segmentCount() const;
  /// IDs of the selected segments
  Q_INVOKABLE QStringList selectedSegmentIDs() const;
  /// The Segment Editor of the section (nullptr until editing starts)
  Q_INVOKABLE qMRMLSegmentEditorWidget* segmentEditor() const;

public slots:
  /// The volume whose mask is shown (nullptr: none)
  void setVolumeNode(vtkMRMLNode* node);
  /// A mask of the volume (it becomes the mask of the volume)
  void setMaskNode(vtkMRMLNode* node);
  void setMaskVisible(bool visible);
  /// Shows the Segment Editor in the section, on the mask and the volume
  void setEditing(bool editing);
  /// Copies (or moves) the selected segments (all if none is selected) to
  /// the mask of the volume targetID; returns true on success
  bool copySegments(const QString& targetID, bool move);
  /// Saves the mask as a FITS mask (fileName empty: asks for it)
  bool saveMask(const QString& fileName = QString());
  /// Updates the widgets from the mask
  void updateFromMask();

protected slots:
  void onMaskSelected(vtkMRMLNode* node);
  void onSceneNodeAddedOrRemoved();
  void onCopyClicked();
  void onMoveClicked();
  void onSaveClicked();

protected:
  void updateTargets();
  void updateMaskSelector();

  QScopedPointer<qSlicerAstroMasksSectionPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroMasksSection);
  Q_DISABLE_COPY(qSlicerAstroMasksSection);
};

#endif
