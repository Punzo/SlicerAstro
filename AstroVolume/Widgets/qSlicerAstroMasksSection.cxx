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
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QStyle>
#include <QTableView>
#include <QAction>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QCursor>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QMessageBox>
#include <QPointer>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

// VTK includes
#include <vtkCommand.h>
#include <vtkImageData.h>
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
#include <qMRMLSegmentEditorWidget.h>
#include <qSlicerSegmentEditorAbstractEffect.h>
#include <qMRMLSegmentsModel.h>
#include <qMRMLSegmentsTableView.h>
#include <qMRMLSortFilterSegmentsProxyModel.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerModuleManager.h>
#include <qSlicerLayoutManager.h>
#include <qMRMLSliceView.h>
#include <qMRMLSliceWidget.h>
#include <qMRMLThreeDView.h>
#include <qMRMLThreeDWidget.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "qSlicerAstroDatasetSelector.h"
#include "qSlicerAstroMasksSection.h"
#include "qSlicerAstroSegmentsTableStyler.h"
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
//----------------------------------------------------------------------------
/// Theme icon of a Segment Editor effect (empty: Slicer's icon is kept)
QString EffectIconName(const QString& effectName)
{
  static const QMap<QString, QString> icons = {
    {"NULL", "pointer"}, {"Threshold", "effectThreshold"}, {"Paint", "effectPaint"}, {"Draw", "effectDraw"},
    {"Erase", "effectErase"}, {"Level tracing", "effectLevelTracing"}, {"Grow from seeds", "effectGrowFromSeeds"},
    {"Fill between slices", "effectFillBetweenSlices"}, {"Margin", "effectMargin"}, {"Hollow", "effectHollow"},
    {"Smoothing", "smoothing"}, {"Scissors", "crop"}, {"Islands", "effectIslands"},
    {"Logical operators", "effectLogicalOperators"}, {"Mask volume", "effectMaskVolume"},
    {"AstroCloudLasso", "lasso"}};
  return icons.value(effectName);
}

//----------------------------------------------------------------------------
/// The layout (root or nested) that holds the widget
QLayout* LayoutContaining(QLayout* root, QWidget* widget)
{
  if (!root)
    {
    return nullptr;
    }
  if (root->indexOf(widget) >= 0)
    {
    return root;
    }
  for (int index = 0; index < root->count(); index++)
    {
    QLayout* found = LayoutContaining(root->itemAt(index)->layout(), widget);
    if (found)
      {
      return found;
      }
    }
  return nullptr;
}

/// Lengths of the effects of the Segment Editor shown in pixels: effect,
/// parameter, index of Slicer's spin box in the options, decimals
struct PixelParameter
{
  const char* Effect;
  const char* Parameter;
  int SpinBoxIndex;
  int Decimals;
};
const PixelParameter PixelParameters[] = {
  {"Margin", "MarginSizeMm", 0, 0}, {"Hollow", "ShellThicknessMm", 0, 0},
  {"Smoothing", "KernelSizeMm", 0, 0}, {"Smoothing", "GaussianStandardDeviationMm", 1, 1}};
const int PixelParameterCount = 4;

/// Effects per row of the effects bar
const int EffectColumns = 7;
} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroMasksSectionPrivate
{
public:
  vtkMRMLSegmentationNode* mask() const;

  vtkWeakPointer<vtkMRMLAstroVolumeNode> Volume;
  vtkWeakPointer<vtkMRMLScene> Scene;
  vtkWeakPointer<vtkMRMLSegmentationNode> ObservedMask;
  vtkWeakPointer<vtkMRMLSegmentationNode> Mask;
  qMRMLSegmentsTableView* Table{nullptr};
  QLabel* Empty{nullptr};
  QToolButton* EyeButton{nullptr};
  QPushButton* EditButton{nullptr};
  QPushButton* SaveButton{nullptr};
  qSlicerAstroDatasetSelector* Target{nullptr};
  QPushButton* CopyButton{nullptr};
  QWidget* CopyRow{nullptr};
  QWidget* SegmentsHeader{nullptr};
  QToolButton* AddButton{nullptr};
  QToolButton* RemoveButton{nullptr};
  QFrame* EffectsBar{nullptr};
  bool SyncingSelection{false};
  QPushButton* MoveButton{nullptr};
  QLabel* Status{nullptr};
  QVBoxLayout* EditorHolder{nullptr};
  qMRMLSegmentEditorWidget* Editor{nullptr};
  QTimer* UpdateTimer{nullptr};
  vtkWeakPointer<vtkMRMLSegmentEditorNode> EditorNode;
  QString ActiveEffect;
};

//-----------------------------------------------------------------------------
vtkMRMLSegmentationNode* qSlicerAstroMasksSectionPrivate::mask() const
{
  return this->Mask;
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


  // segments: title, and Add / Remove while editing
  d->SegmentsHeader = new QWidget(this);
  QHBoxLayout* segmentsHeader = new QHBoxLayout(d->SegmentsHeader);
  segmentsHeader->setContentsMargins(0, 4, 0, 0);
  segmentsHeader->setSpacing(2);
  QLabel* segmentsTitle = new QLabel(tr("SEGMENTS"), d->SegmentsHeader);
  qSlicerAstroTheme::setLabelRole(segmentsTitle, "overline");
  segmentsHeader->addWidget(segmentsTitle, 1);
  d->EyeButton = IconButton("eye", tr("Show or hide all the segments of the mask."), "MasksShowAll", false,
                            d->SegmentsHeader);
  QObject::connect(d->EyeButton, SIGNAL(clicked()), this, SLOT(toggleAllSegments()));
  segmentsHeader->addWidget(d->EyeButton);
  d->AddButton = IconButton("plus", tr("Add a segment to the mask (it becomes the segment being edited)."),
                            "MasksAddSegment", false, d->SegmentsHeader);
  d->RemoveButton = IconButton("trash", tr("Remove the selected segment from the mask."), "MasksRemoveSegment",
                               false, d->SegmentsHeader);
  QObject::connect(d->AddButton, SIGNAL(clicked()), this, SLOT(addSegment()));
  QObject::connect(d->RemoveButton, SIGNAL(clicked()), this, SLOT(removeSelectedSegments()));
  segmentsHeader->addWidget(d->AddButton);
  segmentsHeader->addWidget(d->RemoveButton);
  d->AddButton->hide();
  d->RemoveButton->hide();
  layout->addWidget(d->SegmentsHeader);

  d->Table = new qMRMLSegmentsTableView(this);
  d->Table->setObjectName("MasksTable");
  d->Table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  d->Table->setMinimumHeight(110);
  d->Table->setMaximumHeight(200);
  d->Table->setToolTip(tr("Segments of the mask: visibility, colour and name (double-click to rename). The "
                          "selected segment is the one edited."));
  // a plain list (visibility, colour, name), eyes of the theme, the segment
  // dialog of SlicerAstro on the colour swatch
  qSlicerAstroSegmentsTableStyler* styler = qSlicerAstroSegmentsTableStyler::styleTable(d->Table);
  QObject::connect(styler, SIGNAL(segmentEdited(QString)), this, SLOT(updateFromMask()));
  QObject::connect(d->Table, SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                   this, SLOT(onTableSelectionChanged()));
  layout->addWidget(d->Table);
  d->Empty = new QLabel(tr("No segments"), this);
  d->Empty->setObjectName("MasksEmpty");
  d->Empty->setProperty("astroRole", "label");
  d->Empty->setToolTip(tr("The mask is empty: draw segments with Edit, or load a FITS mask (e.g. SoFiA)."));
  layout->addWidget(d->Empty);

  QHBoxLayout* buttons = new QHBoxLayout();
  buttons->setSpacing(4);
  d->EditButton = TextButton(tr("Edit"), "lasso", tr("Draw, erase and threshold segments (Segment Editor)."),
                             "MasksEdit", this);
  d->EditButton->setCheckable(true);
  QObject::connect(d->EditButton, SIGNAL(toggled(bool)), this, SLOT(setEditing(bool)));
  d->SaveButton = TextButton(tr("Export"), "download", tr("Export the mask as a FITS mask file (integer cube, one "
                                                         "label value per segment, with the WCS of the data)."),
                             "MasksSave", this);
  QObject::connect(d->SaveButton, SIGNAL(clicked()), this, SLOT(onSaveClicked()));
  buttons->addWidget(d->EditButton, 1);
  buttons->addWidget(d->SaveButton);
  layout->addLayout(buttons);

  // copy / move of segments to the mask of data on the same grid: kept for
  // scripts and tests, hidden in the panel for now
  d->CopyRow = new QWidget(this);
  d->CopyRow->setObjectName("MasksCopyRow");
  QHBoxLayout* copyRow = new QHBoxLayout(d->CopyRow);
  copyRow->setContentsMargins(0, 0, 0, 0);
  copyRow->setSpacing(4);
  d->Target = new qSlicerAstroDatasetSelector(d->CopyRow);
  d->Target->setObjectName("MasksCopyTarget");
  d->Target->setNodeTypes(QStringList() << "vtkMRMLAstroVolumeNode");
  d->Target->setNoneText(tr("No data on the same grid"));
  d->Target->setToolTip(tr("Data whose mask receives the selected segments (all if none is selected). Only data "
                           "on the same voxel grid are listed (e.g. a smoothed copy or a cutout: voxel by voxel; "
                           "a 2D map of the same sky grid such as a moment map: projected along the channels). "
                           "Reproject other data first."));
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
  for (QPushButton* button : {d->CopyButton, d->MoveButton})
    {
    button->setFixedHeight(d->Target->sizeHint().height());
    }
  layout->addWidget(d->CopyRow);
  d->CopyRow->hide();

  d->Status = new QLabel(this);
  d->Status->setObjectName("MasksStatus");
  d->Status->setProperty("astroRole", "monoLabel");
  d->Status->setWordWrap(true);
  d->Status->setToolTip(tr("Result of the last action on the mask."));
  layout->addWidget(d->Status);
  // the effects of the Segment Editor (moved here from the editor: a row
  // above its options, full width) and the editor
  d->EffectsBar = new QFrame(this);
  d->EffectsBar->setObjectName("MasksEffects");
  d->EffectsBar->setProperty("astroRole", "card");
  QHBoxLayout* effectsLayout = new QHBoxLayout(d->EffectsBar);
  effectsLayout->setContentsMargins(6, 6, 6, 6);
  effectsLayout->setSpacing(6);
  d->EffectsBar->hide();
  layout->addWidget(d->EffectsBar);
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
vtkMRMLNode* qSlicerAstroMasksSection::maskNode() const
{
  Q_D(const qSlicerAstroMasksSection);
  return d->mask();
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
    d->Table->setMRMLScene(scene);
    d->Target->setMRMLScene(scene);
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
  // the mask of the data (one per dataset, "<data>_mask"; created when the
  // data are loaded, or by Edit / Add)
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  vtkMRMLSegmentationNode* mask = (logic && volume) ? logic->GetMaskSegmentationForVolume(volume, false) : nullptr;
  this->onMaskSelected(mask);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::setMaskNode(vtkMRMLNode* node)
{
  this->onMaskSelected(node);
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
  d->Mask = mask;
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
  // the eye of all the segments: open when the mask and a segment are shown
  bool anyVisible = false;
  vtkMRMLSegmentationDisplayNode* displayNode = mask ?
    vtkMRMLSegmentationDisplayNode::SafeDownCast(mask->GetDisplayNode()) : nullptr;
  if (displayNode && displayNode->GetVisibility())
    {
    std::vector<std::string> segmentIDs;
    mask->GetSegmentation()->GetSegmentIDs(segmentIDs);
    for (const std::string& segmentID : segmentIDs)
      {
      anyVisible = anyVisible || displayNode->GetSegmentVisibility(segmentID);
      }
    }
  d->EyeButton->setIcon(qSlicerAstroTheme::instance()->icon(anyVisible ? "eye" : "eyeOff", "muted"));
  d->EyeButton->setToolTip(anyVisible ? tr("Hide all the segments of the mask.") :
                                        tr("Show all the segments of the mask."));
  d->EyeButton->setEnabled(count > 0);
  const bool hasVoxels = vtkSlicerAstroVolumeLogic::HasNonEmptySegments(mask);
  d->SaveButton->setEnabled(hasVoxels);
  this->updateTargets();
  const bool hasTarget = d->Target->currentNode() != nullptr;
  d->CopyButton->setEnabled(count > 0 && hasTarget);
  d->MoveButton->setEnabled(count > 0 && hasTarget);
  d->Target->setEnabled(count > 0 && hasTarget);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::updateTargets()
{
  Q_D(qSlicerAstroMasksSection);
  // only the data on the grid of the volume (IsMaskCopyTarget)
  const QString current = d->Target->currentNodeID();
  QStringList targetIDs;
  if (d->Scene && d->Volume)
    {
    std::vector<vtkMRMLNode*> volumes;
    d->Scene->GetNodesByClass("vtkMRMLAstroVolumeNode", volumes);
    for (vtkMRMLNode* node : volumes)
      {
      vtkMRMLAstroVolumeNode* other = vtkMRMLAstroVolumeNode::SafeDownCast(node);
      if (other && other->GetID() && !other->GetHideFromEditors() &&
          vtkSlicerAstroVolumeLogic::IsMaskCopyTarget(d->Volume, other))
        {
        targetIDs << QString::fromUtf8(other->GetID());
        }
      }
    }
  const QSignalBlocker blocker(d->Target);
  // an empty filter lists everything: a filter that matches no node
  d->Target->setNodeIDFilter(targetIDs.isEmpty() ? QStringList() << QString("None") : targetIDs);
  d->Target->setCurrentNodeID(targetIDs.contains(current) ? current :
                              (targetIDs.isEmpty() ? QString() : targetIDs.first()));
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::onSceneNodeAddedOrRemoved()
{
  Q_D(qSlicerAstroMasksSection);
  if (d->Scene && !d->Scene->IsBatchProcessing() && !d->Scene->IsClosing())
    {
    // the mask of the data created, replaced or removed
    vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
    vtkMRMLSegmentationNode* mask = (logic && d->Volume) ? logic->GetMaskSegmentationForVolume(d->Volume, false) :
                                                            nullptr;
    if (mask != d->Mask.GetPointer())
      {
      this->onMaskSelected(mask);
      }
    d->UpdateTimer->start();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::restyleEditor()
{
  Q_D(qSlicerAstroMasksSection);
  if (!d->Editor)
    {
    return;
    }
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  // the segments are listed by the section: the editor shows the effects
  // (in the bar of the section) and their options, full width
  for (const char* name : {"SegmentsTableResizableFrame", "AddSegmentButton", "RemoveSegmentButton"})
    {
    QWidget* widget = d->Editor->findChild<QWidget*>(name);
    if (widget)
      {
      widget->hide();
      }
    }
  // effects of no use on astronomical masks
  const QStringList hiddenEffects = QStringList() << "Scissors" << "Mask volume" << "Level tracing" << "Draw";
  d->Editor->setProperty("astroHiddenEffects", hiddenEffects);
  QStringList order;
  const QStringList available = d->Editor->availableEffectNames();
  for (const QString& name : d->Editor->effectNameOrder() + available)
    {
    if (available.contains(name) && !hiddenEffects.contains(name) && !order.contains(name))
      {
      order << name;
      }
    }
  if (order != d->Editor->effectNameOrder() || d->Editor->unorderedEffectsVisible())
    {
    d->Editor->setEffectNameOrder(order);
    d->Editor->setUnorderedEffectsVisible(false);
    }
  this->adjustEffectOptions();
  QFrame* effects = d->Editor->findChild<QFrame*>("EffectsGroupBox");
  QFrame* undoRedo = d->Editor->findChild<QFrame*>("UndoRedoGroupBox");
  QHBoxLayout* barLayout = qobject_cast<QHBoxLayout*>(d->EffectsBar->layout());
  if (d->Editor->effectColumnCount() != EffectColumns)
    {
    d->Editor->setEffectColumnCount(EffectColumns);
    }
  for (QFrame* frame : {effects, undoRedo})
    {
    if (!frame || !barLayout)
      {
      continue;
      }
    frame->setFrameShape(QFrame::NoFrame);
    frame->setStyleSheet(QString());
    if (frame->layout())
      {
      frame->layout()->setContentsMargins(0, 0, 0, 0);
      frame->layout()->setSpacing(2);
      }
    if (barLayout->indexOf(frame) < 0)
      {
      if (frame == undoRedo)
        {
        barLayout->addStretch(1);
        }
      frame->setParent(d->EffectsBar);
      barLayout->addWidget(frame, 0, Qt::AlignTop);
      frame->show();
      }
    }
  // the icons of the theme on the effects, undo and redo
  QList<QToolButton*> buttons;
  if (effects)
    {
    buttons << effects->findChildren<QToolButton*>();
    }
  if (undoRedo)
    {
    buttons << undoRedo->findChildren<QToolButton*>();
    }
  for (QToolButton* button : buttons)
    {
    QString iconName = EffectIconName(button->objectName());
    if (button->objectName() == "UndoButton")
      {
      iconName = "undo";
      }
    else if (button->objectName() == "RedoButton")
      {
      iconName = "redo";
      }
    button->setProperty("astroRole", "toolbar");
    button->setAutoRaise(true);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(18, 18));
    button->setFixedSize(32, 32);
    if (!iconName.isEmpty())
      {
      button->setIcon(theme->icon(iconName, "muted"));
      }
    }
  // the options of the effects without the frames of Slicer's group boxes
  // (also the Masking group: editable area, intensity range, overwrite)
  for (const char* name : {"OptionsGroupBox", "MaskingGroupBox"})
    {
    QWidget* group = d->Editor->findChild<QWidget*>(name);
    if (group && group->property("astroRole").toString() != "plain")
      {
      group->setProperty("astroRole", "plain");
      group->style()->unpolish(group);
      group->style()->polish(group);
      }
    }
  theme->applyTo(d->EffectsBar);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::adjustEffectOptions()
{
  Q_D(qSlicerAstroMasksSection);
  if (!d->Editor)
    {
    return;
    }
  // Threshold: decimals for the range of the data (e.g. mJy/beam in Jy/beam),
  // no local histogram
  qSlicerSegmentEditorAbstractEffect* threshold = d->Editor->effectByName("Threshold");
  QFrame* thresholdOptions = threshold ? threshold->optionsFrame() : nullptr;
  if (thresholdOptions)
    {
    int decimals = 2;
    vtkImageData* image = d->Volume ? d->Volume->GetImageData() : nullptr;
    if (image)
      {
      double range[2] = {0., 0.};
      image->GetScalarRange(range);
      const double step = (range[1] - range[0]) / 1000.;
      if (step > 0. && std::isfinite(step))
        {
        decimals = std::max(2, std::min(8, static_cast<int>(std::ceil(-std::log10(step)))));
        }
      }
    for (QWidget* widget : thresholdOptions->findChildren<QWidget*>())
      {
      if (widget->inherits("ctkRangeWidget"))
        {
        widget->setProperty("decimals", decimals);
        }
      else if (widget->inherits("ctkTransferFunctionView"))
        {
        // the group box of the local histogram
        for (QWidget* parent = widget->parentWidget(); parent && parent != thresholdOptions;
             parent = parent->parentWidget())
          {
          if (parent->inherits("ctkCollapsibleGroupBox"))
            {
            parent->hide();
            break;
            }
          }
        }
      }
    }
  // Margin, Hollow and Smoothing: sizes in pixels (the voxels of
  // astronomical volumes are 1 x 1 x 1 in the views, so Slicer's lengths
  // are pixels): a spin box in pixels drives Slicer's one (hidden), without
  // the "Actual: ... mm" lines
  for (int index = 0; index < PixelParameterCount; index++)
    {
    const PixelParameter& entry = PixelParameters[index];
    qSlicerSegmentEditorAbstractEffect* effect = d->Editor->effectByName(entry.Effect);
    QFrame* options = effect ? effect->optionsFrame() : nullptr;
    const QString pixelsName = QString("AstroPixels_%1").arg(entry.Parameter);
    if (!options || options->findChild<QDoubleSpinBox*>(pixelsName))
      {
      continue;
      }
    // Slicer's spin boxes of the effect, in the order of creation
    QList<QWidget*> slicerSpinBoxes;
    for (QWidget* widget : options->findChildren<QWidget*>())
      {
      if (widget->inherits("qMRMLSpinBox"))
        {
        slicerSpinBoxes << widget;
        }
      QLabel* label = qobject_cast<QLabel*>(widget);
      if (label && (label->toolTip().contains("pixel") || label->toolTip().contains("achievable thickness")))
        {
        label->hide();
        }
      }
    QWidget* slicerSpinBox = slicerSpinBoxes.value(entry.SpinBoxIndex);
    QBoxLayout* row = slicerSpinBox ? qobject_cast<QBoxLayout*>(LayoutContaining(options->layout(), slicerSpinBox))
                                    : nullptr;
    QFormLayout* form = (slicerSpinBox && !row) ?
      qobject_cast<QFormLayout*>(LayoutContaining(options->layout(), slicerSpinBox)) : nullptr;
    if (!row && !form)
      {
      continue;
      }
    QDoubleSpinBox* pixels = new QDoubleSpinBox(slicerSpinBox->parentWidget());
    pixels->setObjectName(pixelsName);
    pixels->setDecimals(entry.Decimals);
    pixels->setSingleStep(entry.Decimals > 0 ? 0.5 : 1.);
    pixels->setRange(entry.Decimals > 0 ? 0.1 : 1., 1000.);
    pixels->setSuffix(tr(" px"));
    pixels->setToolTip(slicerSpinBox->toolTip());
    QPointer<QWidget> target(slicerSpinBox);
    QObject::connect(pixels, qOverload<double>(&QDoubleSpinBox::valueChanged), pixels, [target](double value)
      {
      if (target)
        {
        target->setProperty("value", value);
        }
      });
    if (row)
      {
      row->insertWidget(row->indexOf(slicerSpinBox), pixels);
      }
    else
      {
      // a row of a form: the spin box in pixels takes its place
      int formRow = -1;
      QFormLayout::ItemRole role = QFormLayout::FieldRole;
      form->getWidgetPosition(slicerSpinBox, &formRow, &role);
      form->removeWidget(slicerSpinBox);
      QWidget* field = new QWidget(slicerSpinBox->parentWidget());
      QHBoxLayout* fieldLayout = new QHBoxLayout(field);
      fieldLayout->setContentsMargins(0, 0, 0, 0);
      fieldLayout->addWidget(pixels, 1);
      fieldLayout->addWidget(slicerSpinBox);
      form->setWidget(formRow, role, field);
      }
    slicerSpinBox->hide();
    }
  this->syncPixelSpinBoxes();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::syncPixelSpinBoxes()
{
  Q_D(qSlicerAstroMasksSection);
  if (!d->Editor)
    {
    return;
    }
  for (int index = 0; index < PixelParameterCount; index++)
    {
    const PixelParameter& entry = PixelParameters[index];
    qSlicerSegmentEditorAbstractEffect* effect = d->Editor->effectByName(entry.Effect);
    QDoubleSpinBox* pixels = effect && effect->optionsFrame() ? effect->optionsFrame()->findChild<QDoubleSpinBox*>(
      QString("AstroPixels_%1").arg(entry.Parameter)) : nullptr;
    if (pixels)
      {
      const QSignalBlocker blocker(pixels);
      pixels->setValue(std::fabs(effect->doubleParameter(entry.Parameter)));
      }
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroMasksSection::editSegment(const QString& segmentID)
{
  Q_D(qSlicerAstroMasksSection);
  return qSlicerAstroSegmentsTableStyler::styleTable(d->Table)->editSegment(segmentID);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::onTableSelectionChanged()
{
  Q_D(qSlicerAstroMasksSection);
  d->RemoveButton->setEnabled(!d->Table->selectedSegmentIDs().isEmpty());
  if (!d->Editor || !d->EditButton->isChecked() || d->SyncingSelection)
    {
    return;
    }
  // the segment edited by the Segment Editor: the selected one, otherwise
  // the first of the mask
  QStringList selected = d->Table->selectedSegmentIDs();
  vtkMRMLSegmentationNode* mask = d->mask();
  if (selected.isEmpty() && mask && mask->GetSegmentation() && mask->GetSegmentation()->GetNumberOfSegments() > 0)
    {
    selected << QString::fromStdString(mask->GetSegmentation()->GetNthSegmentID(0));
    d->SyncingSelection = true;
    d->Table->setSelectedSegmentIDs(selected);
    d->SyncingSelection = false;
    }
  if (!selected.isEmpty() && d->Editor->currentSegmentID() != selected.first())
    {
    d->Editor->setCurrentSegmentID(selected.first());
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::onEditorSegmentChanged(const QString& segmentID)
{
  Q_D(qSlicerAstroMasksSection);
  if (d->SyncingSelection || segmentID.isEmpty() || d->Table->selectedSegmentIDs().contains(segmentID))
    {
    return;
    }
  d->SyncingSelection = true;
  d->Table->setSelectedSegmentIDs(QStringList() << segmentID);
  d->SyncingSelection = false;
}

//-----------------------------------------------------------------------------
QString qSlicerAstroMasksSection::addSegment()
{
  Q_D(qSlicerAstroMasksSection);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (!d->mask() && logic && d->Volume)
    {
    this->onMaskSelected(logic->GetMaskSegmentationForVolume(d->Volume, true));
    }
  vtkMRMLSegmentationNode* mask = d->mask();
  if (!mask || !mask->GetSegmentation())
    {
    return QString();
    }
  QString segmentID;
  QPushButton* editorAdd = d->Editor ? d->Editor->findChild<QPushButton*>("AddSegmentButton") : nullptr;
  if (editorAdd && d->EditButton->isChecked())
    {
    // as the Segment Editor adds segments (default terminology and colour)
    editorAdd->click();
    segmentID = d->Editor->currentSegmentID();
    }
  else
    {
    segmentID = QString::fromStdString(mask->GetSegmentation()->AddEmptySegment());
    }
  if (!segmentID.isEmpty())
    {
    d->Table->setSelectedSegmentIDs(QStringList() << segmentID);
    }
  this->updateFromMask();
  return segmentID;
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::removeSelectedSegments()
{
  Q_D(qSlicerAstroMasksSection);
  vtkMRMLSegmentationNode* mask = d->mask();
  const QStringList selected = d->Table->selectedSegmentIDs();
  if (!mask || !mask->GetSegmentation() || selected.isEmpty())
    {
    return;
    }
  {
  MRMLNodeModifyBlocker blocker(mask);
  for (const QString& segmentID : selected)
    {
    mask->GetSegmentation()->RemoveSegment(segmentID.toStdString());
    }
  }
  this->updateFromMask();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::setAllSegmentsVisible(bool visible)
{
  Q_D(qSlicerAstroMasksSection);
  vtkMRMLSegmentationNode* mask = d->mask();
  vtkMRMLSegmentationDisplayNode* displayNode = mask ?
    vtkMRMLSegmentationDisplayNode::SafeDownCast(mask->GetDisplayNode()) : nullptr;
  if (!displayNode)
    {
    return;
    }
  {
  MRMLNodeModifyBlocker blocker(displayNode);
  if (visible)
    {
    displayNode->SetVisibility(true);
    }
  std::vector<std::string> segmentIDs;
  mask->GetSegmentation()->GetSegmentIDs(segmentIDs);
  for (const std::string& segmentID : segmentIDs)
    {
    displayNode->SetSegmentVisibility(segmentID, visible);
    }
  }
  this->updateFromMask();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::toggleAllSegments()
{
  Q_D(qSlicerAstroMasksSection);
  // all shown if none is shown, otherwise all hidden
  bool anyVisible = false;
  vtkMRMLSegmentationNode* mask = d->mask();
  vtkMRMLSegmentationDisplayNode* displayNode = mask ?
    vtkMRMLSegmentationDisplayNode::SafeDownCast(mask->GetDisplayNode()) : nullptr;
  if (displayNode && displayNode->GetVisibility())
    {
    std::vector<std::string> segmentIDs;
    mask->GetSegmentation()->GetSegmentIDs(segmentIDs);
    for (const std::string& segmentID : segmentIDs)
      {
      anyVisible = anyVisible || displayNode->GetSegmentVisibility(segmentID);
      }
    }
  this->setAllSegmentsVisible(!anyVisible);
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
  const bool wasEditing = d->EditButton->isChecked() && d->Editor && !d->Editor->isHidden();
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
    // the active effect (its options need more width, see editorWidthHint)
    this->qvtkReconnect(d->EditorNode, editorNode, vtkCommand::ModifiedEvent, this, SLOT(onEditorNodeModified()));
    d->EditorNode = editorNode;
    d->Editor->setSegmentationNodeSelectorVisible(false);
    d->Editor->setSourceVolumeNodeSelectorVisible(false);
    d->Editor->setSwitchToSegmentationsButtonVisible(false);
    // masks are shown in 3D from the Masks section (no 3D surface button)
    d->Editor->setShow3DButtonVisible(false);
    QObject::connect(d->Editor, SIGNAL(currentSegmentIDChanged(QString)), this,
                     SLOT(onEditorSegmentChanged(QString)));
    d->EditorHolder->addWidget(d->Editor);
    this->restyleEditor();
    }
  if (!d->Editor)
    {
    return;
    }
  d->Editor->setVisible(editing);
  d->EffectsBar->setVisible(editing);
  d->AddButton->setVisible(editing);
  d->RemoveButton->setVisible(editing);
  if (editing)
    {
    vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
    if (!d->mask() && logic && d->Volume)
      {
      this->onMaskSelected(logic->GetMaskSegmentationForVolume(d->Volume, true));
      }
    d->Editor->setSegmentationNode(d->mask());
    d->Editor->setSourceVolumeNode(d->Volume);
    d->Editor->setupViewObservations();
    this->restyleEditor();
    // the edited segment: the selected one, otherwise the first
    this->onTableSelectionChanged();
    }
  else
    {
    d->Editor->setActiveEffect(nullptr);
    d->Editor->removeViewObservations();
    }
  {
  const QSignalBlocker blocker(d->EditButton);
  d->EditButton->setChecked(editing);
  }
  this->onEditorNodeModified();
  if (editing != wasEditing)
    {
    emit editingChanged(editing);
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroMasksSection::isEditing() const
{
  Q_D(const qSlicerAstroMasksSection);
  return d->EditButton->isChecked();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroMasksSection::activeEffect() const
{
  Q_D(const qSlicerAstroMasksSection);
  return d->ActiveEffect;
}

//-----------------------------------------------------------------------------
int qSlicerAstroMasksSection::editorWidthHint() const
{
  Q_D(const qSlicerAstroMasksSection);
  // (hidden: not editing; not isVisible, which is false while the section
  // itself is not on screen)
  if (!d->Editor || d->Editor->isHidden())
    {
    return 0;
    }
  // the effect options (e.g. Paint: diameter, sphere brush, ...) set the width
  int width = d->Editor->minimumSizeHint().width();
  QWidget* options = d->Editor->findChild<QWidget*>("OptionsGroupBox");
  if (options && !options->isHidden())
    {
    const QMargins margins = d->Editor->contentsMargins();
    width = std::max(width, options->sizeHint().width() + margins.left() + margins.right());
    }
  return width;
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::onEditorNodeModified()
{
  Q_D(qSlicerAstroMasksSection);
  QString effect;
  if (d->Editor && d->EditButton->isChecked() && d->EditorNode && d->EditorNode->GetActiveEffectName())
    {
    effect = QString::fromUtf8(d->EditorNode->GetActiveEffectName());
    }
  // the pixel sizes of Margin and Hollow follow their parameters
  this->syncPixelSpinBoxes();
  if (effect != d->ActiveEffect)
    {
    d->ActiveEffect = effect;
    this->adjustEffectOptions();
    // after the Segment Editor has set its cursor on the views
    QTimer::singleShot(0, this, SLOT(applyEffectCursor()));
    emit activeEffectChanged(effect);
    }
}

//-----------------------------------------------------------------------------
QCursor qSlicerAstroMasksSection::effectCursor(const QString& effectName)
{
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  const QString iconName = EffectIconName(effectName);
  const int size = 32;
  const qreal ratio = 2.;
  QPixmap pixmap(static_cast<int>(size * ratio), static_cast<int>(size * ratio));
  pixmap.setDevicePixelRatio(ratio);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  // the arrow: light with a dark outline, readable on the data and the sky
  QPainterPath arrow;
  arrow.moveTo(2., 2.);
  arrow.lineTo(2., 19.5);
  arrow.lineTo(6.6, 15.3);
  arrow.lineTo(9.8, 22.3);
  arrow.lineTo(12.6, 21.1);
  arrow.lineTo(9.5, 14.2);
  arrow.lineTo(15.6, 14.2);
  arrow.closeSubpath();
  painter.setPen(QPen(QColor("#070A12"), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(QColor("#F4F6FA"));
  painter.drawPath(arrow);
  if (!iconName.isEmpty())
    {
    // the badge of the effect: its icon on the theme colours, bottom right
    const QRectF badge(14.5, 14.5, 16., 16.);
    QPainterPath badgePath;
    badgePath.addRoundedRect(badge, 4.5, 4.5);
    painter.setPen(QPen(theme->color("accent"), 1.2));
    painter.setBrush(QColor("#111726"));
    painter.drawPath(badgePath);
    theme->icon(iconName, "accent").paint(&painter, badge.adjusted(2., 2., -2., -2.).toRect());
    }
  painter.end();
  return QCursor(pixmap, 2, 2);
}

//-----------------------------------------------------------------------------
QPixmap qSlicerAstroMasksSection::effectCursorPixmap(const QString& effectName) const
{
  return qSlicerAstroMasksSection::effectCursor(effectName).pixmap();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::applyEffectCursor()
{
  Q_D(qSlicerAstroMasksSection);
  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerLayoutManager* layoutManager = app ? app->layoutManager() : nullptr;
  if (!layoutManager || d->ActiveEffect.isEmpty() || EffectIconName(d->ActiveEffect).isEmpty())
    {
    // no effect: Slicer's default cursors (set by the Segment Editor)
    return;
    }
  const QCursor cursor = qSlicerAstroMasksSection::effectCursor(d->ActiveEffect);
  for (const QString& name : layoutManager->sliceViewNames())
    {
    qMRMLSliceWidget* sliceWidget = layoutManager->sliceWidget(name);
    if (sliceWidget && sliceWidget->sliceView())
      {
      sliceWidget->sliceView()->setViewCursor(cursor);
      sliceWidget->sliceView()->setDefaultViewCursor(cursor);
      }
    }
  for (int index = 0; index < layoutManager->threeDViewCount(); index++)
    {
    qMRMLThreeDWidget* threeDWidget = layoutManager->threeDWidget(index);
    if (threeDWidget && threeDWidget->threeDView())
      {
      threeDWidget->threeDView()->setViewCursor(cursor);
      threeDWidget->threeDView()->setDefaultViewCursor(cursor);
      }
    }
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
  this->copySegments(d->Target->currentNodeID(), false);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMasksSection::onMoveClicked()
{
  Q_D(qSlicerAstroMasksSection);
  this->copySegments(d->Target->currentNodeID(), true);
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
  const QString title = tr("Export as FITS mask");
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
    d->Status->setText(tr("Exported %1").arg(written ? QString::fromUtf8(written) : path));
    }
  else
    {
    d->Status->setText(QString::fromStdString(logic->GetLastMaskSaveError()));
    }
  return saved;
}
