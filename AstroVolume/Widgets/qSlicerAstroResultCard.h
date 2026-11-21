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

#ifndef __qSlicerAstroResultCard_h
#define __qSlicerAstroResultCard_h

// Qt includes
#include <QFrame>
#include <QList>

// CTK includes
#include <ctkVTKObject.h>

// VTK includes
#include <vtkWeakPointer.h>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class QLabel;
class QPushButton;
class QToolButton;
class vtkMRMLSegmentationNode;
class vtkMRMLVolumeNode;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// A result of a panel (e.g. a moment map): preview, name, unit, a Show
/// button that shows it in place of its dataset in the views (clicked again,
/// Hide: the dataset back; vtkSlicerAstroVolumeLogic::ShowProductInViews), an eye button for the mask made from the
/// result (shown only when there is one, e.g. "Make a mask" of Smoothing) and
/// a delete button that removes the result from the scene.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroResultCard
  : public QFrame
{
  Q_OBJECT
  QVTK_OBJECT
  Q_PROPERTY(QString title READ title WRITE setTitle)
  Q_PROPERTY(QString subtitle READ subtitle WRITE setSubtitle)

public:
  explicit qSlicerAstroResultCard(QWidget* parent = nullptr);
  ~qSlicerAstroResultCard() override;

  QString title() const;
  void setTitle(const QString& title);
  QString subtitle() const;
  void setSubtitle(const QString& subtitle);

  /// Preview, name and unit of a volume (and its mask, if any)
  Q_INVOKABLE void setVolume(vtkMRMLVolumeNode* volume);
  Q_INVOKABLE vtkMRMLVolumeNode* volume() const;
  /// Mask made from the result (vtkSlicerAstroVolumeLogic::GetThresholdMaskOf)
  Q_INVOKABLE vtkMRMLSegmentationNode* maskNode() const;
  Q_INVOKABLE QPushButton* showButton() const;
  Q_INVOKABLE QToolButton* maskButton() const;
  Q_INVOKABLE QToolButton* deleteButton() const;

  /// Show or hide result cards and apply the layouts of their parents at
  /// once: a panel growing with its results is never painted squashed (the
  /// layout requests of the parents, e.g. a scroll area, are otherwise
  /// handled after a paint).
  static void setCardsVisible(const QList<qSlicerAstroResultCard*>& cards, const QList<bool>& visible);

public slots:
  /// "Show" or "Hide": the result shown in place of its dataset or not
  /// (vtkSlicerAstroVolumeLogic::IsProductShownInViews)
  void updateShowButton();
  /// Show or hide the mask of the result in the views
  void setMaskVisible(bool visible);
  /// Remove the result from the scene (no confirmation; the delete button
  /// asks first)
  void deleteResult();

signals:
  void showRequested();
  /// The result was removed from the scene
  void deleted();

protected slots:
  void onDeleteClicked();

protected:
  QLabel* ThumbnailLabel;
  QLabel* TitleLabel;
  QLabel* SubtitleLabel;
  QPushButton* ShowButton;
  QToolButton* MaskButton;
  QToolButton* DeleteButton;
  vtkWeakPointer<vtkMRMLVolumeNode> Volume;
  vtkWeakPointer<vtkMRMLSegmentationNode> Mask;
};

#endif
