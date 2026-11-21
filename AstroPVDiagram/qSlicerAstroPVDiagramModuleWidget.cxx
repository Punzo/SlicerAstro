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

// Qt includes
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

// VTK includes
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerLayoutManager.h>
#include <vtkSlicerApplicationLogic.h>

// MRML includes
#include <vtkMRMLAstroPVDiagramParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLInteractionNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLMarkupsCurveNode.h>
#include <vtkMRMLMarkupsDisplayNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>

// AstroVolume includes
#include <qSlicerAstroDatasetSelector.h>
#include <qSlicerAstroMapPreview.h>
#include <qSlicerAstroMessageWidget.h>
#include <qSlicerAstroPanelHeader.h>
#include <qSlicerAstroResultCard.h>
#include <qSlicerAstroSegmentedButton.h>
#include <qSlicerAstroTheme.h>
#include <qSlicerAstroValueField.h>
#include <vtkSlicerAstroVolumeLogic.h>

// Logic includes
#include <vtkSlicerAstroPVDiagramLogic.h>

#include "qSlicerAstroPVDiagramModuleWidget.h"

namespace
{
//----------------------------------------------------------------------------
QLabel* Overline(const QString& text, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  qSlicerAstroTheme::setLabelRole(label, "overline");
  return label;
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroPVDiagramModuleWidgetPrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroPVDiagramModuleWidget);
protected:
  qSlicerAstroPVDiagramModuleWidget* const q_ptr;
public:
  qSlicerAstroPVDiagramModuleWidgetPrivate(qSlicerAstroPVDiagramModuleWidget& object) : q_ptr(&object) {}
  void init();
  vtkSlicerAstroPVDiagramLogic* logic() const;
  vtkMRMLAstroVolumeNode* inputVolume() const;
  vtkMRMLAstroVolumeNode* outputVolume() const;
  vtkMRMLMarkupsCurveNode* path() const;
  vtkMRMLInteractionNode* interactionNode() const;
  /// moment map and path of the cube (created if needed)
  bool prepare();
  /// the moment maps and the PV diagram in the views
  void showViews(bool fit);
  bool embedded() const;
  void updateInfo();
  void updateResult();
  void updateEnabled();

  qSlicerAstroPanelHeader* Header{nullptr};
  qSlicerAstroDatasetSelector* InputVolumeNodeSelector{nullptr};
  qSlicerAstroMessageWidget* Message{nullptr};
  QWidget* Controls{nullptr};
  qSlicerAstroMapPreview* Preview{nullptr};
  QLabel* PathInfo{nullptr};
  QPushButton* DrawButton{nullptr};
  QPushButton* ClearButton{nullptr};
  qSlicerAstroSegmentedButton* ShapeButton{nullptr};
  qSlicerAstroValueField* WidthField{nullptr};
  qSlicerAstroResultCard* Result{nullptr};
  QTimer* UpdateTimer{nullptr};

  vtkSmartPointer<vtkMRMLAstroPVDiagramParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkWeakPointer<vtkMRMLMarkupsCurveNode> ObservedPath;
  bool Updating{false};
};

//-----------------------------------------------------------------------------
vtkSlicerAstroPVDiagramLogic* qSlicerAstroPVDiagramModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroPVDiagramModuleWidget);
  return vtkSlicerAstroPVDiagramLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroPVDiagramModuleWidgetPrivate::inputVolume() const
{
  Q_Q(const qSlicerAstroPVDiagramModuleWidget);
  return (this->parametersNode && q->mrmlScene() && this->parametersNode->GetInputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(q->mrmlScene()->GetNodeByID(this->parametersNode->GetInputVolumeNodeID())) :
    nullptr;
}

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroPVDiagramModuleWidgetPrivate::outputVolume() const
{
  Q_Q(const qSlicerAstroPVDiagramModuleWidget);
  vtkMRMLAstroVolumeNode* output = (this->parametersNode && q->mrmlScene() &&
                                    this->parametersNode->GetOutputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(q->mrmlScene()->GetNodeByID(this->parametersNode->GetOutputVolumeNodeID())) :
    nullptr;
  return (output && output->GetImageData()) ? output : nullptr;
}

//-----------------------------------------------------------------------------
vtkMRMLMarkupsCurveNode* qSlicerAstroPVDiagramModuleWidgetPrivate::path() const
{
  vtkSlicerAstroPVDiagramLogic* logic = this->logic();
  return logic ? logic->GetPath(this->parametersNode) : nullptr;
}

//-----------------------------------------------------------------------------
vtkMRMLInteractionNode* qSlicerAstroPVDiagramModuleWidgetPrivate::interactionNode() const
{
  Q_Q(const qSlicerAstroPVDiagramModuleWidget);
  vtkSlicerApplicationLogic* appLogic = q->module() ? q->module()->appLogic() : nullptr;
  return appLogic ? appLogic->GetInteractionNode() : nullptr;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroPVDiagramModuleWidgetPrivate::embedded() const
{
  Q_Q(const qSlicerAstroPVDiagramModuleWidget);
  return q->property("astroEmbedded").toBool();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroPVDiagramModuleWidget);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QVBoxLayout* rootLayout = new QVBoxLayout(q);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  this->Header = new qSlicerAstroPanelHeader(q);
  this->Header->setTitle(qSlicerAstroPVDiagramModuleWidget::tr("PV diagram"));
  this->Header->setIconName("module-pv-diagram");
  rootLayout->addWidget(this->Header);

  QWidget* body = new QWidget(q);
  QVBoxLayout* bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(12, 12, 12, 14);
  bodyLayout->setSpacing(14);
  rootLayout->addWidget(body);

  this->InputVolumeNodeSelector = new qSlicerAstroDatasetSelector(body);
  this->InputVolumeNodeSelector->setObjectName("InputVolumeNodeSelector");
  this->InputVolumeNodeSelector->setRequiredDimensions(3);
  this->InputVolumeNodeSelector->setNoneText(qSlicerAstroPVDiagramModuleWidget::tr("Select a data cube"));
  this->InputVolumeNodeSelector->setToolTip(qSlicerAstroPVDiagramModuleWidget::tr(
    "Data cube of the position-velocity diagram."));
  bodyLayout->addWidget(this->InputVolumeNodeSelector);

  this->Message = new qSlicerAstroMessageWidget(body);
  this->Message->setObjectName("MessageWidget");
  bodyLayout->addWidget(this->Message);

  this->Controls = new QWidget(body);
  QVBoxLayout* controls = new QVBoxLayout(this->Controls);
  controls->setContentsMargins(0, 0, 0, 0);
  controls->setSpacing(8);
  bodyLayout->addWidget(this->Controls);

  // the path on the moment-0 map
  controls->addWidget(Overline(qSlicerAstroPVDiagramModuleWidget::tr("Path on the moment-0 map"), this->Controls));
  this->Preview = new qSlicerAstroMapPreview(this->Controls);
  this->Preview->setObjectName("PVDiagramPreview");
  this->Preview->setToolTip(qSlicerAstroPVDiagramModuleWidget::tr(
    "Moment-0 map of the cube (north up) with the path of the PV diagram. Click to add a point at the end of "
    "the path, drag a point to move it, right-click a point to remove it. The PV diagram starts at the first "
    "point (offset 0)."));
  controls->addWidget(this->Preview);
  this->PathInfo = new QLabel(this->Controls);
  this->PathInfo->setObjectName("PVDiagramPathInfo");
  this->PathInfo->setProperty("astroRole", "monoLabel");
  this->PathInfo->setToolTip(qSlicerAstroPVDiagramModuleWidget::tr(
    "Points of the path, its length on the sky and the columns of the PV diagram (one per pixel along the path)."));
  controls->addWidget(this->PathInfo);

  QHBoxLayout* pathButtons = new QHBoxLayout();
  pathButtons->setSpacing(6);
  this->DrawButton = new QPushButton(theme->icon("curve", "text"),
                                     qSlicerAstroPVDiagramModuleWidget::tr("Draw in views"), this->Controls);
  this->DrawButton->setObjectName("PVDiagramDrawButton");
  this->DrawButton->setCheckable(true);
  this->DrawButton->setToolTip(qSlicerAstroPVDiagramModuleWidget::tr(
    "Add points to the path by clicking on the moment-0 map in the views; click again or right-click in a view "
    "to stop."));
  this->ClearButton = new QPushButton(theme->icon("trash", "text"),
                                      qSlicerAstroPVDiagramModuleWidget::tr("Clear"), this->Controls);
  this->ClearButton->setObjectName("PVDiagramClearButton");
  this->ClearButton->setToolTip(qSlicerAstroPVDiagramModuleWidget::tr(
    "Remove the points of the path (the PV diagram is kept until a new path has two points)."));
  pathButtons->addWidget(this->DrawButton, 1);
  pathButtons->addWidget(this->ClearButton, 1);
  controls->addLayout(pathButtons);

  QHBoxLayout* shapeRow = new QHBoxLayout();
  shapeRow->setSpacing(8);
  this->ShapeButton = new qSlicerAstroSegmentedButton(this->Controls);
  this->ShapeButton->setObjectName("PVDiagramShape");
  this->ShapeButton->setToolTip(qSlicerAstroPVDiagramModuleWidget::tr("Shape of the path through its points."));
  this->ShapeButton->addButton(qSlicerAstroPVDiagramModuleWidget::tr("Straight"),
    qSlicerAstroPVDiagramModuleWidget::tr("Straight segments between the points."));
  this->ShapeButton->addButton(qSlicerAstroPVDiagramModuleWidget::tr("Spline"),
    qSlicerAstroPVDiagramModuleWidget::tr("A smooth curve (cardinal spline) through the points."));
  this->WidthField = new qSlicerAstroValueField(this->Controls);
  this->WidthField->setObjectName("PVDiagramWidth");
  this->WidthField->setLeadingText(qSlicerAstroPVDiagramModuleWidget::tr("Width"));
  this->WidthField->setUnit(qSlicerAstroPVDiagramModuleWidget::tr("px"));
  this->WidthField->setDecimals(0);
  this->WidthField->setRange(1., 999.);
  this->WidthField->setSingleStep(1.);
  this->WidthField->setToolTip(qSlicerAstroPVDiagramModuleWidget::tr(
    "Width of the path (pixels of the map): each column of the PV diagram is the mean of the spectra across "
    "the path, one pixel apart (blank pixels left out). 1: the spectra on the path only."));
  shapeRow->addWidget(this->ShapeButton, 1);
  shapeRow->addWidget(this->WidthField, 1);
  controls->addLayout(shapeRow);
  bodyLayout->addStretch(1);

  // the result
  QFrame* footer = new QFrame(q);
  footer->setProperty("astroRole", "panelFooter");
  QVBoxLayout* footerLayout = new QVBoxLayout(footer);
  footerLayout->setContentsMargins(12, 12, 12, 12);
  footerLayout->setSpacing(8);
  this->Result = new qSlicerAstroResultCard(footer);
  this->Result->setObjectName("PVDiagramResult");
  this->Result->hide();
  footerLayout->addWidget(this->Result);
  rootLayout->addWidget(footer);
  rootLayout->addStretch(1);

  // the PV diagram follows the path once the pending events are processed
  // (one update for the moves of a drag)
  this->UpdateTimer = new QTimer(q);
  this->UpdateTimer->setSingleShot(true);
  this->UpdateTimer->setInterval(0);

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setInputVolume(vtkMRMLNode*)));
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->InputVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(this->DrawButton, SIGNAL(toggled(bool)), q, SLOT(setDrawing(bool)));
  QObject::connect(this->ClearButton, SIGNAL(clicked()), q, SLOT(clearPath()));
  QObject::connect(this->ShapeButton, &qSlicerAstroSegmentedButton::currentIndexChanged,
                   q, [q](int index) { q->setInterpolation(index == 1); });
  QObject::connect(this->WidthField, &qSlicerAstroValueField::valueChanged,
                   q, [q](double value) { q->setWidth(static_cast<int>(value + 0.5)); });
  QObject::connect(this->UpdateTimer, SIGNAL(timeout()), q, SLOT(updatePVDiagram()));
  QObject::connect(this->Result, SIGNAL(showRequested()), q, SLOT(onShowResult()));
  QObject::connect(this->Result, SIGNAL(deleted()), q, SLOT(onResultDeleted()));
  theme->applyTo(q);
  this->updateEnabled();
  this->updateInfo();
}

//-----------------------------------------------------------------------------
bool qSlicerAstroPVDiagramModuleWidgetPrivate::prepare()
{
  vtkSlicerAstroPVDiagramLogic* logic = this->logic();
  if (!logic || !this->parametersNode || !this->inputVolume())
    {
    return false;
    }
  if (!logic->PreparePath(this->parametersNode))
    {
    this->Message->showMessage("info", QString::fromStdString(logic->GetLastError()));
    return false;
    }
  this->Message->clearMessage();
  vtkMRMLMarkupsCurveNode* path = this->path();
  vtkMRMLMarkupsDisplayNode* display = path ? path->GetMarkupsDisplayNode() : nullptr;
  if (display)
    {
    display->SetPointLabelsVisibility(false);
    display->SetPropertiesLabelVisibility(false);
    }
  return path != nullptr;
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidgetPrivate::showViews(bool fit)
{
  Q_Q(qSlicerAstroPVDiagramModuleWidget);
  vtkSlicerAstroPVDiagramLogic* logic = this->logic();
  if (!logic || !this->parametersNode || !q->isEntered())
    {
    return;
    }
  // classic Slicer: the 2nd moment map in the Green view too; embedded (e.g.
  // the Workbench): only the views of the path and of the PV diagram
  logic->SetMomentMapsOnViews(this->parametersNode, !this->embedded());
  if (this->outputVolume())
    {
    logic->SetPVDiagramOnViews(this->parametersNode, fit);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidgetPrivate::updateEnabled()
{
  vtkSlicerAstroPVDiagramLogic* logic = this->logic();
  const bool ready = logic && this->path() && this->inputVolume() && this->parametersNode &&
    this->parametersNode->GetMomentMapNodeID();
  this->Controls->setEnabled(ready);
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidgetPrivate::updateInfo()
{
  vtkSlicerAstroPVDiagramLogic* logic = this->logic();
  vtkMRMLMarkupsCurveNode* path = this->path();
  const int points = path ? path->GetNumberOfDefinedControlPoints() : 0;
  double length = 0.;
  int columns = 0;
  if (logic && points >= 2 && logic->GetPathInfo(this->parametersNode, &length, &columns))
    {
    this->PathInfo->setText(qSlicerAstroPVDiagramModuleWidget::tr("%1 points · %2′ · %3 columns")
                              .arg(points).arg(length, 0, 'f', 2).arg(columns));
    }
  else if (points == 1)
    {
    this->PathInfo->setText(qSlicerAstroPVDiagramModuleWidget::tr("1 point · add the next one"));
    }
  else
    {
    this->PathInfo->setText(qSlicerAstroPVDiagramModuleWidget::tr("Click on the map to start the path"));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidgetPrivate::updateResult()
{
  vtkMRMLAstroVolumeNode* output = this->outputVolume();
  if (output)
    {
    this->Result->setVolume(output);
    int dims[3] = {0, 0, 0};
    output->GetImageData()->GetDimensions(dims);
    this->Result->setSubtitle(qSlicerAstroPVDiagramModuleWidget::tr("%1 columns × %2 channels")
                                .arg(dims[0]).arg(dims[1]));
    }
  qSlicerAstroResultCard::setCardsVisible(QList<qSlicerAstroResultCard*>() << this->Result,
                                          QList<bool>() << (output != nullptr));
}

//-----------------------------------------------------------------------------
// qSlicerAstroPVDiagramModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroPVDiagramModuleWidget::qSlicerAstroPVDiagramModuleWidget(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroPVDiagramModuleWidgetPrivate(*this))
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroPVDiagramModuleWidget::~qSlicerAstroPVDiagramModuleWidget() = default;

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  this->Superclass::setMRMLScene(scene);
  if (!scene)
    {
    return;
    }
  vtkSlicerApplicationLogic* appLogic = this->module() ? this->module()->appLogic() : nullptr;
  d->selectionNode = appLogic ? appLogic->GetSelectionNode() : nullptr;
  this->qvtkReconnect(scene, vtkMRMLScene::EndCloseEvent, this, SLOT(onEndCloseEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent, this, SLOT(onEndImportEvent()));
  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent, this, SLOT(onSelectionNodeModified()));
  this->qvtkReconnect(d->interactionNode(), vtkMRMLInteractionNode::InteractionModeChangedEvent,
                      this, SLOT(onInteractionNodeModified()));
  this->onEndImportEvent();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  // a new parameters node for the new scene
  d->UpdateTimer->stop();
  d->parametersNode = nullptr;
  this->onEndImportEvent();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onEndImportEvent()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  vtkMRMLScene* scene = this->mrmlScene();
  if (!scene)
    {
    return;
    }
  // the parameters of a loaded scene (the last one with a cube) rather than
  // the empty node made when the previous scene was closed, which is removed
  std::vector<vtkMRMLNode*> nodes;
  scene->GetNodesByClass("vtkMRMLAstroPVDiagramParametersNode", nodes);
  vtkMRMLNode* parametersNode = nullptr;
  for (vtkMRMLNode* node : nodes)
    {
    vtkMRMLAstroPVDiagramParametersNode* candidate = vtkMRMLAstroPVDiagramParametersNode::SafeDownCast(node);
    if (candidate && (candidate->GetInputVolumeNodeID() || !parametersNode))
      {
      parametersNode = candidate;
      }
    }
  for (vtkMRMLNode* node : nodes)
    {
    vtkMRMLAstroPVDiagramParametersNode* other = vtkMRMLAstroPVDiagramParametersNode::SafeDownCast(node);
    if (other && other != parametersNode && !other->GetInputVolumeNodeID())
      {
      if (other == d->parametersNode.GetPointer())
        {
        this->setMRMLAstroPVDiagramParametersNode(nullptr);
        }
      scene->RemoveNode(other);
      }
    }
  if (!parametersNode)
    {
    vtkSmartPointer<vtkMRMLNode> node = vtkSmartPointer<vtkMRMLNode>::Take(
      scene->CreateNodeByClass("vtkMRMLAstroPVDiagramParametersNode"));
    parametersNode = scene->AddNode(node);
    }
  this->setMRMLAstroPVDiagramParametersNode(parametersNode);
  // a loaded scene: its path on its moment map
  if (d->inputVolume())
    {
    d->prepare();
    this->onParametersNodeModified();
    }
  this->onSelectionNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::setMRMLAstroPVDiagramParametersNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  vtkMRMLAstroPVDiagramParametersNode* parametersNode = vtkMRMLAstroPVDiagramParametersNode::SafeDownCast(node);
  this->qvtkReconnect(d->parametersNode, parametersNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onParametersNodeModified()));
  this->qvtkReconnect(d->parametersNode, parametersNode,
                      vtkMRMLAstroPVDiagramParametersNode::InterpolationModifiedEvent,
                      this, SLOT(onInterpolationModified()));
  d->parametersNode = parametersNode;
  this->onParametersNodeModified();
}

//-----------------------------------------------------------------------------
vtkMRMLAstroPVDiagramParametersNode* qSlicerAstroPVDiagramModuleWidget::mrmlAstroPVDiagramParametersNode() const
{
  Q_D(const qSlicerAstroPVDiagramModuleWidget);
  return d->parametersNode;
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onSelectionNodeModified()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  // the module follows the active cube while it is shown (a scene loaded
  // while another module is shown keeps its PV diagram)
  if (!this->isEntered())
    {
    return;
    }
  const char* activeID = d->selectionNode ? d->selectionNode->GetActiveVolumeID() : nullptr;
  vtkMRMLAstroVolumeNode* active = (activeID && this->mrmlScene()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->GetNodeByID(activeID)) : nullptr;
  if (active && active->GetImageData() && active->GetImageData()->GetDimensions()[2] > 1 &&
      active != d->inputVolume())
    {
    this->setInputVolume(active);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::setInputVolume(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  const char* id = node ? node->GetID() : nullptr;
  const char* current = d->parametersNode->GetInputVolumeNodeID();
  const bool changed = (id && current) ? strcmp(id, current) != 0 : id != current;
  if (changed)
    {
    // a new cube: its moment map, its PV diagram and the path on its map
    const int wasModifying = d->parametersNode->StartModify();
    d->parametersNode->SetMomentMapNodeID(nullptr);
    d->parametersNode->SetOutputVolumeNodeID(nullptr);
    d->parametersNode->SetNodeReferenceID(vtkSlicerAstroPVDiagramLogic::GetSecondMomentMapReferenceRole(), nullptr);
    d->parametersNode->SetInputVolumeNodeID(id);
    d->parametersNode->EndModify(wasModifying);
    }
  if (changed || !d->path() || !d->parametersNode->GetMomentMapNodeID())
    {
    d->prepare();
    }
  this->onParametersNodeModified();
  if (changed)
    {
    d->showViews(true);
    this->updatePVDiagram();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onParametersNodeModified()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  if (!d->parametersNode)
    {
    d->updateEnabled();
    return;
    }
  vtkMRMLAstroVolumeNode* cube = d->inputVolume();
  {
  const QSignalBlocker blocker(d->InputVolumeNodeSelector);
  d->InputVolumeNodeSelector->setCurrentNode(cube);
  }
  vtkMRMLMarkupsCurveNode* path = d->path();
  if (path != d->ObservedPath.GetPointer())
    {
    for (unsigned long event : {static_cast<unsigned long>(vtkMRMLMarkupsNode::PointModifiedEvent),
                                static_cast<unsigned long>(vtkMRMLMarkupsNode::PointAddedEvent),
                                static_cast<unsigned long>(vtkMRMLMarkupsNode::PointRemovedEvent),
                                static_cast<unsigned long>(vtkMRMLMarkupsNode::PointPositionDefinedEvent),
                                static_cast<unsigned long>(vtkMRMLMarkupsNode::PointPositionUndefinedEvent)})
      {
      this->qvtkReconnect(d->ObservedPath, path, event, this, SLOT(onPathModified()));
      }
    d->ObservedPath = path;
    }
  vtkMRMLNode* map = (this->mrmlScene() && d->parametersNode->GetMomentMapNodeID()) ?
    this->mrmlScene()->GetNodeByID(d->parametersNode->GetMomentMapNodeID()) : nullptr;
  d->Preview->setVolumeNode(map);
  d->Preview->setMarkupsNode(path);
  {
  const QSignalBlocker blockShape(d->ShapeButton);
  d->ShapeButton->setCurrentIndex(d->parametersNode->GetInterpolation() ? 1 : 0);
  const QSignalBlocker blockWidth(d->WidthField);
  d->WidthField->setValue(d->parametersNode->GetWidth());
  }
  d->updateEnabled();
  d->updateInfo();
  d->updateResult();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onInterpolationModified()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  vtkSlicerAstroPVDiagramLogic* logic = d->logic();
  if (logic && d->parametersNode)
    {
    logic->UpdatePathCurveType(d->parametersNode);
    }
  d->Preview->update();
  d->UpdateTimer->start();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onPathModified()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  vtkSlicerAstroPVDiagramLogic* logic = d->logic();
  if (!logic || d->Updating || !d->parametersNode)
    {
    return;
    }
  // on the plane and in the extent of the map (e.g. placed in a view)
  d->Updating = true;
  logic->ConstrainPath(d->parametersNode);
  d->Updating = false;
  d->updateInfo();
  d->UpdateTimer->start();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::updatePVDiagram()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  d->UpdateTimer->stop();
  vtkSlicerAstroPVDiagramLogic* logic = d->logic();
  vtkMRMLMarkupsCurveNode* path = d->path();
  if (!logic || !d->parametersNode || !path || path->GetNumberOfDefinedControlPoints() < 2)
    {
    d->updateInfo();
    return;
    }
  const bool existed = d->outputVolume() != nullptr;
  if (!logic->UpdatePVDiagram(d->parametersNode))
    {
    d->Message->showMessage("error", QString::fromStdString(logic->GetLastError()));
    return;
    }
  if (this->isEntered())
    {
    // the view is fitted to a new PV diagram only (the zoom of the user kept)
    logic->SetPVDiagramOnViews(d->parametersNode, !existed);
    }
  d->updateInfo();
  d->updateResult();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::setInterpolation(bool spline)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  if (d->parametersNode)
    {
    d->parametersNode->SetInterpolation(spline);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::setWidth(int width)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  if (!d->parametersNode || d->parametersNode->GetWidth() == width)
    {
    return;
    }
  d->parametersNode->SetWidth(width);
  d->UpdateTimer->start();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::setDrawing(bool drawing)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  vtkMRMLInteractionNode* interactionNode = d->interactionNode();
  vtkMRMLMarkupsCurveNode* path = d->path();
  if (!interactionNode || !d->selectionNode)
    {
    return;
    }
  const bool placing = interactionNode->GetCurrentInteractionMode() == vtkMRMLInteractionNode::Place && path &&
    d->selectionNode->GetActivePlaceNodeID() && !strcmp(d->selectionNode->GetActivePlaceNodeID(), path->GetID());
  if (drawing && path && !placing)
    {
    d->selectionNode->SetReferenceActivePlaceNodeClassName("vtkMRMLMarkupsCurveNode");
    d->selectionNode->SetActivePlaceNodeID(path->GetID());
    interactionNode->SetPlaceModePersistence(1);
    interactionNode->SetCurrentInteractionMode(vtkMRMLInteractionNode::Place);
    }
  else if (!drawing && placing)
    {
    interactionNode->SetPlaceModePersistence(0);
    interactionNode->SetCurrentInteractionMode(vtkMRMLInteractionNode::ViewTransform);
    }
  this->onInteractionNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onInteractionNodeModified()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  vtkMRMLInteractionNode* interactionNode = d->interactionNode();
  vtkMRMLMarkupsCurveNode* path = d->path();
  const bool placing = interactionNode && path && d->selectionNode &&
    interactionNode->GetCurrentInteractionMode() == vtkMRMLInteractionNode::Place &&
    d->selectionNode->GetActivePlaceNodeID() && !strcmp(d->selectionNode->GetActivePlaceNodeID(), path->GetID());
  const QSignalBlocker blocker(d->DrawButton);
  d->DrawButton->setChecked(placing);
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::clearPath()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  vtkSlicerAstroPVDiagramLogic* logic = d->logic();
  if (logic)
    {
    logic->ClearPath(d->parametersNode);
    }
  d->updateInfo();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onShowResult()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  vtkSlicerAstroPVDiagramLogic* logic = d->logic();
  vtkSlicerAstroVolumeLogic* astroVolumeLogic = logic ? logic->GetAstroVolumeLogic() : nullptr;
  if (astroVolumeLogic && d->outputVolume())
    {
    astroVolumeLogic->ToggleProductInViews(d->outputVolume(), d->inputVolume());
    d->Result->updateShowButton();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onResultDeleted()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  if (d->parametersNode)
    {
    d->parametersNode->SetOutputVolumeNodeID(nullptr);
    }
  d->updateResult();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::enter()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  this->Superclass::enter();
  this->onSelectionNodeModified();
  if (!d->path() || !d->parametersNode || !d->parametersNode->GetMomentMapNodeID())
    {
    d->prepare();
    this->onParametersNodeModified();
    }
  vtkMRMLMarkupsCurveNode* path = d->path();
  if (path)
    {
    path->SetDisplayVisibility(1);
    }
  // classic Slicer: four views (moment-0 map with the path, 2nd moment map,
  // PV diagram, 3D); embedded (e.g. Workbench): the layout is kept
  qSlicerApplication* app = qSlicerApplication::application();
  vtkMRMLLayoutNode* layoutNode = (app && app->layoutManager() && app->layoutManager()->layoutLogic()) ?
    app->layoutManager()->layoutLogic()->GetLayoutNode() : nullptr;
  if (layoutNode && !d->embedded() && layoutNode->GetViewArrangement() != vtkMRMLLayoutNode::SlicerLayoutFourUpView)
    {
    layoutNode->SetViewArrangement(vtkMRMLLayoutNode::SlicerLayoutFourUpView);
    }
  if (path && path->GetNumberOfDefinedControlPoints() >= 2)
    {
    vtkSlicerAstroPVDiagramLogic* logic = d->logic();
    if (logic)
      {
      logic->UpdatePVDiagram(d->parametersNode);
      }
    }
  d->showViews(true);
  d->updateInfo();
  d->updateResult();
  this->onInteractionNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::exit()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  this->setDrawing(false);
  this->Superclass::exit();
  // the path is shown only while the module (or its Workbench step) is open
  vtkMRMLMarkupsCurveNode* path = d->path();
  if (path)
    {
    path->SetDisplayVisibility(0);
    }
}
