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

// Qt includes
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

// VTK includes
#include <vtkNew.h>
#include <vtkStringArray.h>
#include <vtkWeakPointer.h>

// MRML includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLStorageNode.h>
#include <vtkSegmentation.h>

// Segmentations includes
#include <qMRMLNodeComboBox.h>
#include <qMRMLSegmentEditorWidget.h>
#include <qMRMLSegmentsTableView.h>
#include <qMRMLSortFilterProxyModel.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerModuleManager.h>

#include <vector>

#include "qSlicerAstroMasksSection.h"
#include "qSlicerAstroTheme.h"

namespace
{
//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* AstroVolumeLogic()
{
  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerAbstractCoreModule* module = app && app->moduleManager() ? app->moduleManager()->module("AstroVolume") : nullptr;
  return module ? vtkSlicerAstroVolumeLogic::SafeDownCast(module->logic()) : nullptr;
}

//----------------------------------------------------------------------------
QToolButton* IconButton(const QString& iconName, const QString& toolTip, const QString& objectName, bool checkable,
                        QWidget* parent)
{
  QToolButton* button = new QToolButton(parent);
  button->setProperty("astroRole", "toolbar");
  button->setIcon(qSlicerAstroTheme::instance()->icon(iconName, "muted"));
  button->setIconSize(QSize(16, 16));
  button->setToolTip(toolTip);
  button->setCheckable(checkable);
  button->setAutoRaise(true);
  button->setObjectName(objectName);
  return button;
}

//----------------------------------------------------------------------------
QPushButton* TextButton(const QString& text, const QString& iconName, const QString& toolTip,
                        const QString& objectName, QWidget* parent)
{
  QPushButton* button = new QPushButton(text, parent);
  if (!iconName.isEmpty())
    {
    button->setIcon(qSlicerAstroTheme::instance()->icon(iconName, "text"));
    }
  button->setToolTip(toolTip);
  button->setObjectName(objectName);
  return button;
}

//----------------------------------------------------------------------------
QLabel* RowLabel(const QString& text, const QString& toolTip, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  label->setProperty("astroRole", "label");
  label->setToolTip(toolTip);
  return label;
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroMasksSectionPrivate
{
public:
  vtkMRMLSegmentationNode* mask() const;

  vtkWeakPointer<vtkMRMLAstroVolumeNode> Volume;
  vtkWeakPointer<vtkMRMLScene> Scene;
  vtkWeakPointer<vtkMRMLSegmentationNode> ObservedMask;
  qMRMLNodeComboBox* MaskSelector{nullptr};
  qMRMLSegmentsTableView* Table{nullptr};
  QLabel* Empty{nullptr};
  QToolButton* ShowButton{nullptr};
  QPushButton* EditButton{nullptr};
  QToolButton* SaveButton{nullptr};
  QComboBox* Target{nullptr};
  QPushButton* CopyButton{nullptr};
  QPushButton* MoveButton{nullptr};
  QLabel* Status{nullptr};
  QVBoxLayout* EditorHolder{nullptr};
  qMRMLSegmentEditorWidget* Editor{nullptr};
  QTimer* UpdateTimer{nullptr};
};

//-----------------------------------------------------------------------------
vtkMRMLSegmentationNode* qSlicerAstroMasksSectionPrivate::mask() const
{
  return vtkMRMLSegmentationNode::SafeDownCast(this->MaskSelector->currentNode());
}

//-----------------------------------------------------------------------------
qSlicerAstroMasksSection::qSlicerAstroMasksSection(QWidget* parent)
  : QWidget(parent)
  , d_ptr(new qSlicerAstroMasksSectionPrivate)
{
  Q_D(qSlicerAstroMasksSection);
  this->setObjectName("MasksSection");
  this->setToolTip(tr("Mask of the data: one segment per source or region (e.g. a SoFiA mask)."));
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  QHBoxLayout* maskRow = new QHBoxLayout();
  maskRow->setSpacing(8);
  d->MaskSelector = new qMRMLNodeComboBox(this);
  d->MaskSelector->setObjectName("MasksSelector");
  d->MaskSelector->setNodeTypes(QStringList() << "vtkMRMLSegmentationNode");
  d->MaskSelector->setNoneEnabled(true);
  d->MaskSelector->setAddEnabled(true);
  d->MaskSelector->setRemoveEnabled(false);
  d->MaskSelector->setRenameEnabled(true);
  d->MaskSelector->setToolTip(tr("Mask of the data: its own mask, a mask loaded from a FITS file (e.g. SoFiA) or "
                                 "a new one. The chosen mask becomes the mask of the data."));
  QObject::connect(d->MaskSelector, qOverload<vtkMRMLNode*>(&qMRMLNodeComboBox::currentNodeChanged),
                   this, &qSlicerAstroMasksSection::onMaskSelected);
  maskRow->addWidget(RowLabel(tr("Mask"), d->MaskSelector->toolTip(), this));
  maskRow->addWidget(d->MaskSelector, 1);
  layout->addLayout(maskRow);

  d->Table = new qMRMLSegmentsTableView(this);
  d->Table->setObjectName("MasksTable");
  d->Table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  d->Table->setMinimumHeight(110);
  d->Table->setMaximumHeight(170);
  d->Table->setToolTip(tr("Segments of the mask: visibility, colour, name. Select segments to copy or move "
                          "them."));
  layout->addWidget(d->Table);
  d->Empty = new QLabel(tr("No segments"), this);
  d->Empty->setObjectName("MasksEmpty");
  d->Empty->setProperty("astroRole", "label");
  d->Empty->setToolTip(tr("The mask is empty: draw segments with Edit, or load a FITS mask (e.g. SoFiA)."));
  layout->addWidget(d->Empty);

  QHBoxLayout* buttons = new QHBoxLayout();
  buttons->setSpacing(4);
  d->ShowButton = IconButton("eye", tr("Show or hide the mask in the views."), "MasksShow", true, this);
  QObject::connect(d->ShowButton, SIGNAL(toggled(bool)), this, SLOT(setMaskVisible(bool)));
  d->EditButton = TextButton(tr("Edit"), "lasso", tr("Draw, erase and threshold segments (Segment Editor)."),
                             "MasksEdit", this);
  d->EditButton->setCheckable(true);
  QObject::connect(d->EditButton, SIGNAL(toggled(bool)), this, SLOT(setEditing(bool)));
  d->SaveButton = IconButton("download", tr("Save the mask as a FITS mask (integer cube, one label value per "
                                            "segment, with the WCS of the data)."), "MasksSave", false, this);
  QObject::connect(d->SaveButton, SIGNAL(clicked()), this, SLOT(onSaveClicked()));
  buttons->addWidget(d->ShowButton);
  buttons->addWidget(d->EditButton, 1);
  buttons->addWidget(d->SaveButton);
  layout->addLayout(buttons);

  QHBoxLayout* copyRow = new QHBoxLayout();
  copyRow->setSpacing(4);
  d->Target = new QComboBox(this);
  d->Target->setObjectName("MasksCopyTarget");
  d->Target->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  d->Target->setMinimumContentsLength(8);
  d->Target->setToolTip(tr("Data whose mask receives the selected segments (all if none is selected): voxel by "
                           "voxel on the same voxel grid (e.g. a smoothed copy or a cutout), projected along the "
                           "channels on a 2D image of the same sky grid (e.g. a moment map), otherwise reprojected "
                           "with the WCS (nearest neighbour)."));
  d->CopyButton = TextButton(tr("Copy"), "copy", tr("Copy the segments to the mask of the chosen data."),
                             "MasksCopy", this);
  d->MoveButton = TextButton(tr("Move"), QString(), tr("Move the segments to the mask of the chosen data (they are "
                                                       "removed from this mask)."), "MasksMove", this);
  QObject::connect(d->CopyButton, SIGNAL(clicked()), this, SLOT(onCopyClicked()));
  QObject::connect(d->MoveButton, SIGNAL(clicked()), this, SLOT(onMoveClicked()));
  copyRow->addWidget(RowLabel(tr("To"), d->Target->toolTip(), this));
  copyRow->addWidget(d->Target, 1);
  copyRow->addWidget(d->CopyButton);
  copyRow->addWidget(d->MoveButton);
  layout->addLayout(copyRow);

  d->Status = new QLabel(this);
  d->Status->setObjectName("MasksStatus");
  d->Status->setProperty("astroRole", "monoLabel");
  d->Status->setWordWrap(true);
  d->Status->setToolTip(tr("Result of the last action on the mask."));
  layout->addWidget(d->Status);
  d->EditorHolder = new QVBoxLayout();
  d->EditorHolder->setContentsMargins(0, 0, 0, 0);
  layout->addLayout(d->EditorHolder);

  // the buttons follow the edits of the mask, not at each stroke
  d->UpdateTimer = new QTimer(this);
  d->UpdateTimer->setSingleShot(true);
  d->UpdateTimer->setInterval(200);
  QObject::connect(d->UpdateTimer, SIGNAL(timeout()), this, SLOT(updateFromMask()));
  this->setEnabled(false);
}

//-----------------------------------------------------------------------------
qSlicerAstroMasksSection::~qSlicerAstroMasksSection()
{
  Q_D(qSlicerAstroMasksSection);
  if (d->Editor)
    {
    d->Editor->setActiveEffect(nullptr);
    d->Editor->removeViewObservations();
    }
}

//-----------------------------------------------------------------------------
QString qSlicerAstroMasksSection::status() const
{
  Q_D(const qSlicerAstroMasksSection);
  return d->Status->text();
}

//-----------------------------------------------------------------------------
int qSlicerAstroMasksSection::segmentCount() const
{
  Q_D(const qSlicerAstroMasksSection);
  vtkMRMLSegmentationNode* mask = d->mask();
  return mask && mask->GetSegmentation() ? mask->GetSegmentation()->GetNumberOfSegments() : 0;
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroMasksSection::selectedSegmentIDs() const
{
  Q_D(const qSlicerAstroMasksSection);
  return d->Table->selectedSegmentIDs();
}

//-----------------------------------------------------------------------------
qMRMLSegmentEditorWidget* qSlicerAstroMasksSection::segmentEditor() const
{
  Q_D(const qSlicerAstroMasksSection);
  return d->Editor;
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::setVolumeNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroMasksSection);
  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  if (volume && !volume->GetScene())
    {
    // removed from the scene (e.g. while the scene is closed)
    volume = nullptr;
    }
  vtkMRMLScene* scene = volume ? volume->GetScene() : nullptr;
  if (scene != d->Scene.GetPointer())
    {
    this->qvtkReconnect(d->Scene, scene, vtkMRMLScene::NodeAddedEvent, this, SLOT(onSceneNodeAddedOrRemoved()));
    this->qvtkReconnect(d->Scene, scene, vtkMRMLScene::NodeRemovedEvent, this, SLOT(onSceneNodeAddedOrRemoved()));
    d->Scene = scene;
    d->MaskSelector->setMRMLScene(scene);
    d->Table->setMRMLScene(scene);
    if (d->Editor)
      {
      d->Editor->setMRMLScene(scene);
      }
    }
  if (volume != d->Volume.GetPointer())
    {
    d->Status->clear();
    }
  d->Volume = volume;
  this->setEnabled(volume != nullptr);
  this->updateMaskSelector();
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  vtkMRMLSegmentationNode* mask = (logic && volume) ? logic->GetMaskSegmentationForVolume(volume, false) : nullptr;
    {
    const QSignalBlocker blocker(d->MaskSelector);
    d->MaskSelector->setCurrentNode(mask);
    }
  this->onMaskSelected(mask);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::setMaskNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroMasksSection);
  d->MaskSelector->setCurrentNode(node);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::onMaskSelected(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroMasksSection);
  vtkMRMLSegmentationNode* mask = vtkMRMLSegmentationNode::SafeDownCast(node);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  // a new mask (e.g. "Create new" in the selector) gets the volume as
  // reference geometry; the chosen mask becomes the mask of the volume
  if (mask && d->Volume)
    {
    vtkSlicerAstroVolumeLogic::SetMaskReferenceVolume(mask, d->Volume);
    if (vtkSlicerAstroVolumeLogic::IsMaskSegmentation(mask) &&
        mask->GetNodeReference(vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str()) ==
          d->Volume.GetPointer())
      {
      d->Volume->SetNodeReferenceID(vtkSlicerAstroVolumeLogic::GetMaskReferenceRole(), mask->GetID());
      }
    if (logic)
      {
      logic->EnsureFITSMaskStorageNode(mask);
      }
    }
  for (unsigned long event : { vtkSegmentation::SegmentAdded, vtkSegmentation::SegmentRemoved,
                               vtkSegmentation::SegmentModified, vtkSegmentation::RepresentationModified,
                               vtkSegmentation::SourceRepresentationModified })
    {
    this->qvtkReconnect(d->ObservedMask, mask, event, d->UpdateTimer, SLOT(start()));
    }
  this->qvtkReconnect(d->ObservedMask, mask, vtkMRMLDisplayableNode::DisplayModifiedEvent, d->UpdateTimer,
                      SLOT(start()));
  d->ObservedMask = mask;
  d->Table->setSegmentationNode(mask);
  if (d->Editor && d->EditButton->isChecked())
    {
    d->Editor->setSegmentationNode(mask);
    d->Editor->setSourceVolumeNode(d->Volume);
    }
  this->updateFromMask();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::updateFromMask()
{
  Q_D(qSlicerAstroMasksSection);
  vtkMRMLSegmentationNode* mask = d->mask();
  const int count = this->segmentCount();
  d->Table->setVisible(count > 0);
  d->Empty->setVisible(count == 0);
    {
    const QSignalBlocker blocker(d->ShowButton);
    d->ShowButton->setChecked(mask && mask->GetDisplayNode() && mask->GetDisplayNode()->GetVisibility());
    }
  d->ShowButton->setEnabled(count > 0);
  const bool hasVoxels = vtkSlicerAstroVolumeLogic::HasNonEmptySegments(mask);
  d->SaveButton->setEnabled(hasVoxels);
  this->updateTargets();
  d->CopyButton->setEnabled(count > 0 && d->Target->count() > 0);
  d->MoveButton->setEnabled(count > 0 && d->Target->count() > 0);
  d->Target->setEnabled(count > 0 && d->Target->count() > 0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::updateTargets()
{
  Q_D(qSlicerAstroMasksSection);
  const QString current = d->Target->currentData().toString();
  const QSignalBlocker blocker(d->Target);
  d->Target->clear();
  if (!d->Scene || !d->Volume)
    {
    return;
    }
  std::vector<vtkMRMLNode*> volumes;
  d->Scene->GetNodesByClass("vtkMRMLAstroVolumeNode", volumes);
  for (vtkMRMLNode* node : volumes)
    {
    vtkMRMLAstroVolumeNode* other = vtkMRMLAstroVolumeNode::SafeDownCast(node);
    if (other && other != d->Volume.GetPointer() && other->GetImageData() && !other->GetHideFromEditors())
      {
      d->Target->addItem(QString::fromUtf8(other->GetName()), QString::fromUtf8(other->GetID()));
      }
    }
  const int index = d->Target->findData(current);
  d->Target->setCurrentIndex(index >= 0 ? index : 0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::updateMaskSelector()
{
  Q_D(qSlicerAstroMasksSection);
  // only masks (binary labelmaps) in the selector: not the contours
  QStringList hiddenNodeIDs;
  if (d->Scene)
    {
    std::vector<vtkMRMLNode*> nodes;
    d->Scene->GetNodesByClass("vtkMRMLSegmentationNode", nodes);
    for (vtkMRMLNode* node : nodes)
      {
      vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node);
      if (segmentationNode && segmentationNode->GetID() && segmentationNode->GetSegmentation() &&
          !vtkSlicerAstroVolumeLogic::IsMaskSegmentation(segmentationNode))
        {
        hiddenNodeIDs << QString::fromUtf8(segmentationNode->GetID());
        }
      }
    }
  d->MaskSelector->sortFilterProxyModel()->setHiddenNodeIDs(hiddenNodeIDs);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::onSceneNodeAddedOrRemoved()
{
  Q_D(qSlicerAstroMasksSection);
  if (d->Scene && !d->Scene->IsBatchProcessing() && !d->Scene->IsClosing())
    {
    this->updateMaskSelector();
    d->UpdateTimer->start();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::setMaskVisible(bool visible)
{
  Q_D(qSlicerAstroMasksSection);
  vtkMRMLSegmentationNode* mask = d->mask();
  if (mask && mask->GetDisplayNode())
    {
    mask->GetDisplayNode()->SetVisibility(visible);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::setEditing(bool editing)
{
  Q_D(qSlicerAstroMasksSection);
  if (editing && !d->Editor && d->Scene)
    {
    d->Editor = new qMRMLSegmentEditorWidget(this);
    d->Editor->setObjectName("MasksSegmentEditor");
    d->Editor->setMRMLScene(d->Scene);
    vtkMRMLSegmentEditorNode* editorNode = vtkMRMLSegmentEditorNode::SafeDownCast(
      d->Scene->GetSingletonNode("SegmentEditor", "vtkMRMLSegmentEditorNode"));
    if (!editorNode)
      {
      vtkNew<vtkMRMLSegmentEditorNode> newEditorNode;
      newEditorNode->SetSingletonTag("SegmentEditor");
      editorNode = vtkMRMLSegmentEditorNode::SafeDownCast(d->Scene->AddNode(newEditorNode));
      }
    d->Editor->setMRMLSegmentEditorNode(editorNode);
    d->Editor->setSegmentationNodeSelectorVisible(false);
    d->Editor->setSourceVolumeNodeSelectorVisible(false);
    d->Editor->setSwitchToSegmentationsButtonVisible(false);
    // masks are shown in 3D from the Masks section (no 3D surface button)
    d->Editor->setShow3DButtonVisible(false);
    d->EditorHolder->addWidget(d->Editor);
    }
  if (!d->Editor)
    {
    return;
    }
  d->Editor->setVisible(editing);
  if (editing)
    {
    vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
    if (!d->mask() && logic && d->Volume)
      {
      d->MaskSelector->setCurrentNode(logic->GetMaskSegmentationForVolume(d->Volume, true));
      }
    d->Editor->setSegmentationNode(d->mask());
    d->Editor->setSourceVolumeNode(d->Volume);
    d->Editor->setupViewObservations();
    }
  else
    {
    d->Editor->setActiveEffect(nullptr);
    d->Editor->removeViewObservations();
    }
  const QSignalBlocker blocker(d->EditButton);
  d->EditButton->setChecked(editing);
}

//-----------------------------------------------------------------------------
bool qSlicerAstroMasksSection::copySegments(const QString& targetID, bool move)
{
  Q_D(qSlicerAstroMasksSection);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  vtkMRMLSegmentationNode* mask = d->mask();
  vtkMRMLAstroVolumeNode* target = (d->Scene && !targetID.isEmpty()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(d->Scene->GetNodeByID(targetID.toUtf8().constData())) : nullptr;
  if (!logic || !mask || !target)
    {
    d->Status->setText(tr("Choose the data that receives the segments"));
    return false;
    }
  vtkNew<vtkStringArray> segmentIDs;
  for (const QString& segmentID : d->Table->selectedSegmentIDs())
    {
    segmentIDs->InsertNextValue(segmentID.toUtf8().constData());
    }
  const int count = segmentIDs->GetNumberOfValues() > 0 ? static_cast<int>(segmentIDs->GetNumberOfValues()) :
    this->segmentCount();
  QApplication::setOverrideCursor(Qt::WaitCursor);
  vtkMRMLSegmentationNode* targetMask = logic->CopyMaskSegmentsToVolume(mask, segmentIDs, target, move);
  QApplication::restoreOverrideCursor();
  if (!targetMask)
    {
    d->Status->setText(QString::fromStdString(logic->GetLastMaskCopyError()));
    return false;
    }
  const QString segments = count == 1 ? tr("1 segment") : tr("%1 segments").arg(count);
  d->Status->setText((move ? tr("%1 moved to %2") : tr("%1 copied to %2"))
                       .arg(segments, QString::fromUtf8(target->GetName())));
  this->updateFromMask();
  return true;
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::onCopyClicked()
{
  Q_D(qSlicerAstroMasksSection);
  this->copySegments(d->Target->currentData().toString(), false);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::onMoveClicked()
{
  Q_D(qSlicerAstroMasksSection);
  this->copySegments(d->Target->currentData().toString(), true);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::onSaveClicked()
{
  this->saveMask();
}

//-----------------------------------------------------------------------------
bool qSlicerAstroMasksSection::saveMask(const QString& fileName)
{
  Q_D(qSlicerAstroMasksSection);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  vtkMRMLSegmentationNode* mask = d->mask();
  if (!logic || !mask || !d->Scene)
    {
    return false;
    }
  const QString title = tr("Save as FITS mask");
  QString path = fileName;
  if (path.isEmpty())
    {
    // a mask without FITS header takes the one of the data
    if (!vtkSlicerAstroVolumeLogic::HasFITSMaskHeader(mask) && d->Volume &&
        QMessageBox::question(this, title, tr("The mask %1 has no FITS header. Use the header and the geometry of "
                                              "%2?").arg(QString::fromUtf8(mask->GetName()),
                                                          QString::fromUtf8(d->Volume->GetName()))) !=
          QMessageBox::Yes)
      {
      return false;
      }
    vtkMRMLStorageNode* storageNode = mask->GetStorageNode();
    QString defaultFileName = storageNode && storageNode->GetFileName() &&
      storageNode->IsA("vtkMRMLAstroSegmentationStorageNode") ? QString::fromUtf8(storageNode->GetFileName()) :
      QString();
    if (defaultFileName.isEmpty())
      {
      const QString directory = d->Scene->GetRootDirectory() ? QString::fromUtf8(d->Scene->GetRootDirectory()) :
        QDir::homePath();
      defaultFileName = QDir(directory).filePath(QString::fromUtf8(mask->GetName()) + ".fits");
      }
    path = QFileDialog::getSaveFileName(this, title, defaultFileName, tr("FITS mask (*.fits *.fits.gz)"));
    if (path.isEmpty())
      {
      return false;
      }
    }
  QApplication::setOverrideCursor(Qt::WaitCursor);
  const bool saved = logic->SaveFITSMask(mask, d->Volume, path.toUtf8().constData());
  QApplication::restoreOverrideCursor();
  if (saved)
    {
    const char* written = mask->GetStorageNode() ? mask->GetStorageNode()->GetFileName() : nullptr;
    d->Status->setText(tr("Saved %1").arg(written ? QString::fromUtf8(written) : path));
    }
  else
    {
    d->Status->setText(QString::fromStdString(logic->GetLastMaskSaveError()));
    }
  return saved;
}
