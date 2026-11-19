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
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
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
#include <vtkMRMLAstroPVSliceParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLMarkupsLineNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSliceNode.h>

// AstroVolume includes
#include <qSlicerAstroDatasetSelector.h>
#include <qSlicerAstroMapPreview.h>
#include <qSlicerAstroMessageWidget.h>
#include <qSlicerAstroPanelHeader.h>
#include <qSlicerAstroSegmentedButton.h>
#include <qSlicerAstroTheme.h>
#include <qSlicerAstroValueField.h>
#include <vtkSlicerAstroVolumeLogic.h>

// Logic includes
#include <vtkSlicerAstroPVSliceLogic.h>

// STD includes
#include <cmath>

#include "qSlicerAstroPVSliceModuleWidget.h"

namespace
{
//----------------------------------------------------------------------------
QLabel* Overline(const QString& text, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  qSlicerAstroTheme::setLabelRole(label, "overline");
  return label;
}

//----------------------------------------------------------------------------
/// hh:mm:ss.s and +dd:mm:ss of a sky position (degrees)
QString Sexagesimal(double degrees, bool hours)
{
  const bool negative = degrees < 0.;
  double value = std::fabs(hours ? degrees / 15. : degrees);
  int whole = static_cast<int>(value);
  value = (value - whole) * 60.;
  int minutes = static_cast<int>(value);
  double seconds = (value - minutes) * 60.;
  const int decimals = hours ? 1 : 0;
  // rounding up to 60 seconds
  if (seconds >= 60. - 0.5 * std::pow(10., -decimals))
    {
    seconds = 0.;
    if (++minutes == 60)
      {
      minutes = 0;
      whole++;
      }
    }
  if (hours)
    {
    return QString::fromUtf8("%1h%2m%3s").arg(whole, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0'))
      .arg(seconds, 4, 'f', 1, QChar('0'));
    }
  return QString::fromUtf8("%1%2°%3′%4″").arg(negative ? "−" : "+").arg(whole, 2, 10, QChar('0'))
    .arg(minutes, 2, 10, QChar('0')).arg(static_cast<int>(std::lround(seconds)), 2, 10, QChar('0'));
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroPVSliceModuleWidgetPrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroPVSliceModuleWidget);
protected:
  qSlicerAstroPVSliceModuleWidget* const q_ptr;
public:
  qSlicerAstroPVSliceModuleWidgetPrivate(qSlicerAstroPVSliceModuleWidget& object) : q_ptr(&object) {}
  void init();
  vtkSlicerAstroPVSliceLogic* logic() const;
  vtkMRMLAstroVolumeNode* inputVolume() const;
  vtkMRMLMarkupsLineNode* lineNode() const;
  vtkMRMLAstroVolumeNode* momentMap() const;
  /// moment map and line of the cube (created if needed)
  bool prepareInput();
  /// the PV slice shown in the views (module panel or embedded)
  bool showsViews() const;
  void updateFields();
  void updateEnabled();

  qSlicerAstroPanelHeader* Header{nullptr};
  qSlicerAstroDatasetSelector* InputVolumeNodeSelector{nullptr};
  qSlicerAstroMessageWidget* Message{nullptr};
  qSlicerAstroMapPreview* Preview{nullptr};
  QLabel* PreviewHint{nullptr};
  qSlicerAstroValueField* RightAscension{nullptr};
  qSlicerAstroValueField* Declination{nullptr};
  qSlicerAstroValueField* PositionAngle{nullptr};
  qSlicerAstroValueField* Length{nullptr};
  QLabel* CentreLabel{nullptr};
  QPushButton* PeakButton{nullptr};
  QPushButton* MajorAxisButton{nullptr};
  qSlicerAstroSegmentedButton* CameraButton{nullptr};
  QWidget* Controls{nullptr};

  vtkSmartPointer<vtkMRMLAstroPVSliceParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkWeakPointer<vtkMRMLMarkupsLineNode> ObservedLine;
  bool Updating{false};
};

//-----------------------------------------------------------------------------
vtkSlicerAstroPVSliceLogic* qSlicerAstroPVSliceModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroPVSliceModuleWidget);
  return vtkSlicerAstroPVSliceLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroPVSliceModuleWidgetPrivate::inputVolume() const
{
  Q_Q(const qSlicerAstroPVSliceModuleWidget);
  return (this->parametersNode && q->mrmlScene() && this->parametersNode->GetInputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(q->mrmlScene()->GetNodeByID(this->parametersNode->GetInputVolumeNodeID())) :
    nullptr;
}

//-----------------------------------------------------------------------------
vtkMRMLMarkupsLineNode* qSlicerAstroPVSliceModuleWidgetPrivate::lineNode() const
{
  Q_Q(const qSlicerAstroPVSliceModuleWidget);
  return (this->parametersNode && q->mrmlScene() && this->parametersNode->GetLineNodeID()) ?
    vtkMRMLMarkupsLineNode::SafeDownCast(q->mrmlScene()->GetNodeByID(this->parametersNode->GetLineNodeID())) :
    nullptr;
}

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroPVSliceModuleWidgetPrivate::momentMap() const
{
  Q_Q(const qSlicerAstroPVSliceModuleWidget);
  return (this->parametersNode && q->mrmlScene() && this->parametersNode->GetMomentMapNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(q->mrmlScene()->GetNodeByID(this->parametersNode->GetMomentMapNodeID())) :
    nullptr;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroPVSliceModuleWidgetPrivate::showsViews() const
{
  Q_Q(const qSlicerAstroPVSliceModuleWidget);
  return q->isEntered() || q->property("astroEmbedded").toBool();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroPVSliceModuleWidget);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QVBoxLayout* rootLayout = new QVBoxLayout(q);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  this->Header = new qSlicerAstroPanelHeader(q);
  this->Header->setTitle(qSlicerAstroPVSliceModuleWidget::tr("PV slice"));
  this->Header->setIconName("module-pv-slice");
  rootLayout->addWidget(this->Header);

  QWidget* body = new QWidget(q);
  QVBoxLayout* bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(12, 12, 12, 14);
  bodyLayout->setSpacing(14);
  rootLayout->addWidget(body);

  this->InputVolumeNodeSelector = new qSlicerAstroDatasetSelector(body);
  this->InputVolumeNodeSelector->setObjectName("InputVolumeNodeSelector");
  this->InputVolumeNodeSelector->setRequiredDimensions(3);
  this->InputVolumeNodeSelector->setNoneText(qSlicerAstroPVSliceModuleWidget::tr("Select a data cube"));
  this->InputVolumeNodeSelector->setToolTip(qSlicerAstroPVSliceModuleWidget::tr(
    "Data cube of the position-velocity slice."));
  bodyLayout->addWidget(this->InputVolumeNodeSelector);

  this->Message = new qSlicerAstroMessageWidget(body);
  this->Message->setObjectName("MessageWidget");
  bodyLayout->addWidget(this->Message);

  this->Controls = new QWidget(body);
  QVBoxLayout* controls = new QVBoxLayout(this->Controls);
  controls->setContentsMargins(0, 0, 0, 0);
  controls->setSpacing(14);
  bodyLayout->addWidget(this->Controls);

  // the line on the moment-0 map
  QWidget* lineWidget = new QWidget(this->Controls);
  QVBoxLayout* lineLayout = new QVBoxLayout(lineWidget);
  lineLayout->setContentsMargins(0, 0, 0, 0);
  lineLayout->setSpacing(8);
  lineLayout->addWidget(Overline(qSlicerAstroPVSliceModuleWidget::tr("Line on the moment-0 map"), lineWidget));
  this->Preview = new qSlicerAstroMapPreview(lineWidget);
  this->Preview->setObjectName("PVSlicePreview");
  this->Preview->setToolTip(qSlicerAstroPVSliceModuleWidget::tr(
    "Moment-0 map of the cube (north up) with the line of the slice: drag an end to turn or stretch the "
    "line, its centre to move it."));
  lineLayout->addWidget(this->Preview);
  this->CentreLabel = new QLabel(lineWidget);
  this->CentreLabel->setObjectName("PVSliceCentre");
  this->CentreLabel->setProperty("astroRole", "monoLabel");
  this->CentreLabel->setToolTip(qSlicerAstroPVSliceModuleWidget::tr("Centre of the line on the sky."));
  lineLayout->addWidget(this->CentreLabel);

  QGridLayout* fields = new QGridLayout();
  fields->setHorizontalSpacing(8);
  fields->setVerticalSpacing(6);
  this->RightAscension = new qSlicerAstroValueField(lineWidget);
  this->RightAscension->setObjectName("PVSliceRightAscension");
  this->RightAscension->setLeadingText(qSlicerAstroPVSliceModuleWidget::tr("RA"));
  this->RightAscension->setUnit(QString::fromUtf8("°"));
  this->RightAscension->setDecimals(5);
  this->RightAscension->setRange(0., 360.);
  this->RightAscension->setSingleStep(0.001);
  this->RightAscension->setToolTip(qSlicerAstroPVSliceModuleWidget::tr("Right ascension of the centre of the line."));
  this->Declination = new qSlicerAstroValueField(lineWidget);
  this->Declination->setObjectName("PVSliceDeclination");
  this->Declination->setLeadingText(qSlicerAstroPVSliceModuleWidget::tr("Dec"));
  this->Declination->setUnit(QString::fromUtf8("°"));
  this->Declination->setDecimals(5);
  this->Declination->setRange(-90., 90.);
  this->Declination->setSingleStep(0.001);
  this->Declination->setToolTip(qSlicerAstroPVSliceModuleWidget::tr("Declination of the centre of the line."));
  this->PositionAngle = new qSlicerAstroValueField(lineWidget);
  this->PositionAngle->setObjectName("PVSlicePositionAngle");
  this->PositionAngle->setLeadingText(qSlicerAstroPVSliceModuleWidget::tr("PA"));
  this->PositionAngle->setUnit(QString::fromUtf8("°"));
  this->PositionAngle->setDecimals(1);
  this->PositionAngle->setRange(0., 180.);
  this->PositionAngle->setSingleStep(1.);
  this->PositionAngle->setToolTip(qSlicerAstroPVSliceModuleWidget::tr(
    "Position angle of the line on the sky, from North towards East."));
  this->Length = new qSlicerAstroValueField(lineWidget);
  this->Length->setObjectName("PVSliceLength");
  this->Length->setLeadingText(qSlicerAstroPVSliceModuleWidget::tr("Length"));
  this->Length->setUnit(QString::fromUtf8("′"));
  this->Length->setDecimals(2);
  this->Length->setRange(0.01, 10000.);
  this->Length->setSingleStep(0.1);
  this->Length->setToolTip(qSlicerAstroPVSliceModuleWidget::tr("Length of the line on the sky (arcmin)."));
  fields->addWidget(this->RightAscension, 0, 0);
  fields->addWidget(this->Declination, 0, 1);
  fields->addWidget(this->PositionAngle, 1, 0);
  fields->addWidget(this->Length, 1, 1);
  lineLayout->addLayout(fields);

  QHBoxLayout* placeButtons = new QHBoxLayout();
  placeButtons->setSpacing(6);
  this->PeakButton = new QPushButton(theme->icon("target", "text"),
                                     qSlicerAstroPVSliceModuleWidget::tr("Centre on peak"), lineWidget);
  this->PeakButton->setObjectName("PVSliceCentreOnPeak");
  this->PeakButton->setToolTip(qSlicerAstroPVSliceModuleWidget::tr(
    "Move the line to the brightest pixel of the moment-0 map (same angle and length)."));
  this->MajorAxisButton = new QPushButton(theme->icon("ruler", "text"),
                                          qSlicerAstroPVSliceModuleWidget::tr("Major axis"), lineWidget);
  this->MajorAxisButton->setObjectName("PVSliceMajorAxis");
  this->MajorAxisButton->setToolTip(qSlicerAstroPVSliceModuleWidget::tr(
    "Lay the line along the major axis of the emission: through the intensity-weighted centre of the pixels "
    "above 20% of the peak, along their elongation (same length)."));
  placeButtons->addWidget(this->PeakButton, 1);
  placeButtons->addWidget(this->MajorAxisButton, 1);
  lineLayout->addLayout(placeButtons);
  controls->addWidget(lineWidget);

  // the 3D view
  QWidget* viewWidget = new QWidget(this->Controls);
  QVBoxLayout* viewLayout = new QVBoxLayout(viewWidget);
  viewLayout->setContentsMargins(0, 0, 0, 0);
  viewLayout->setSpacing(8);
  viewLayout->addWidget(Overline(qSlicerAstroPVSliceModuleWidget::tr("3D view"), viewWidget));
  this->CameraButton = new qSlicerAstroSegmentedButton(viewWidget);
  this->CameraButton->setObjectName("PVSliceCamera");
  this->CameraButton->setToolTip(qSlicerAstroPVSliceModuleWidget::tr("Point of view of the 3D view."));
  this->CameraButton->addButton(qSlicerAstroPVSliceModuleWidget::tr("Sky"),
    qSlicerAstroPVSliceModuleWidget::tr("Looking along the velocity axis, north up."));
  this->CameraButton->addButton(qSlicerAstroPVSliceModuleWidget::tr("Along line"),
    qSlicerAstroPVSliceModuleWidget::tr("Looking along the line: the PV plane seen edge-on."));
  this->CameraButton->addButton(qSlicerAstroPVSliceModuleWidget::tr("Face-on"),
    qSlicerAstroPVSliceModuleWidget::tr("Facing the PV plane, velocity up."));
  viewLayout->addWidget(this->CameraButton);
  controls->addWidget(viewWidget);
  bodyLayout->addStretch(1);

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setInputVolume(vtkMRMLNode*)));
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->InputVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));
  for (qSlicerAstroValueField* field : {this->RightAscension, this->Declination, this->PositionAngle, this->Length})
    {
    QObject::connect(field, SIGNAL(editingFinished()), q, SLOT(onSkyFieldEdited()));
    }
  QObject::connect(this->PeakButton, SIGNAL(clicked()), q, SLOT(centerOnPeak()));
  QObject::connect(this->MajorAxisButton, SIGNAL(clicked()), q, SLOT(alignWithMajorAxis()));
  QObject::connect(this->CameraButton, SIGNAL(currentIndexChanged(int)), q, SLOT(setCameraView(int)));
  theme->applyTo(q);
  this->updateEnabled();
}

//-----------------------------------------------------------------------------
bool qSlicerAstroPVSliceModuleWidgetPrivate::prepareInput()
{
  Q_Q(qSlicerAstroPVSliceModuleWidget);
  vtkSlicerAstroPVSliceLogic* logic = this->logic();
  vtkMRMLAstroVolumeNode* cube = this->inputVolume();
  if (!logic || !cube || !cube->GetImageData() || !q->mrmlScene())
    {
    return false;
    }
  int dims[3] = {0, 0, 0};
  cube->GetImageData()->GetDimensions(dims);
  if (dims[2] < 2)
    {
    this->Message->showMessage("info", qSlicerAstroPVSliceModuleWidget::tr(
      "A PV slice needs a data cube (two axes on the sky and the spectral axis)."));
    return false;
    }
  this->Message->clearMessage();
  // the moment-0 map of the cube shared by the modules
  vtkMRMLAstroVolumeNode* map = vtkSlicerAstroVolumeLogic::GetMomentMap(cube, 0);
  if (map)
    {
    this->parametersNode->SetMomentMapNodeID(map->GetID());
    }
  else if (!logic->Calculate0thMomentMap(this->parametersNode))
    {
    this->Message->showMessage("error", qSlicerAstroPVSliceModuleWidget::tr(
      "The moment-0 map of the cube could not be computed (see the error log)."));
    return false;
    }
  // the line (one PVSliceLine in the scene), placed on the new map
  vtkMRMLMarkupsLineNode* line = vtkMRMLMarkupsLineNode::SafeDownCast(
    q->mrmlScene()->GetFirstNodeByName("PVSliceLine"));
  if (!line)
    {
    logic->CreateAndSetLine(this->parametersNode);
    }
  else
    {
    this->parametersNode->SetLineNodeID(line->GetID());
    logic->InitializeLine(this->parametersNode);
    }
  return this->lineNode() != nullptr && this->momentMap() != nullptr;
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidgetPrivate::updateEnabled()
{
  const bool ready = this->lineNode() && this->momentMap() && this->inputVolume();
  this->Controls->setEnabled(ready);
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidgetPrivate::updateFields()
{
  vtkSlicerAstroPVSliceLogic* logic = this->logic();
  double center[2] = {0., 0.};
  double positionAngle = 0.;
  double length = 0.;
  const bool known = logic && logic->GetLineSky(this->parametersNode, center, &positionAngle, &length);
  for (qSlicerAstroValueField* field : {this->RightAscension, this->Declination, this->PositionAngle, this->Length})
    {
    field->setEnabled(known);
    }
  if (!known)
    {
    this->CentreLabel->clear();
    return;
    }
  const QSignalBlocker blockRA(this->RightAscension);
  const QSignalBlocker blockDec(this->Declination);
  const QSignalBlocker blockPA(this->PositionAngle);
  const QSignalBlocker blockLength(this->Length);
  this->RightAscension->setValue(center[0]);
  this->Declination->setValue(center[1]);
  this->PositionAngle->setValue(positionAngle);
  this->Length->setValue(length);
  this->CentreLabel->setText(QString::fromUtf8("%1  %2").arg(Sexagesimal(center[0], true),
                                                               Sexagesimal(center[1], false)));
}

//-----------------------------------------------------------------------------
// qSlicerAstroPVSliceModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroPVSliceModuleWidget::qSlicerAstroPVSliceModuleWidget(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroPVSliceModuleWidgetPrivate(*this))
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroPVSliceModuleWidget::~qSlicerAstroPVSliceModuleWidget() = default;

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
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
  this->onEndImportEvent();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  // a new parameters node for the new scene
  d->parametersNode = nullptr;
  this->onEndImportEvent();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onEndImportEvent()
{
  vtkMRMLScene* scene = this->mrmlScene();
  if (!scene)
    {
    return;
    }
  vtkMRMLNode* parametersNode = scene->GetFirstNodeByClass("vtkMRMLAstroPVSliceParametersNode");
  if (!parametersNode)
    {
    vtkSmartPointer<vtkMRMLNode> node = vtkSmartPointer<vtkMRMLNode>::Take(
      scene->CreateNodeByClass("vtkMRMLAstroPVSliceParametersNode"));
    parametersNode = scene->AddNode(node);
    }
  this->setMRMLAstroPVSliceParametersNode(parametersNode);
  this->onSelectionNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::setMRMLAstroPVSliceParametersNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  vtkMRMLAstroPVSliceParametersNode* parametersNode = vtkMRMLAstroPVSliceParametersNode::SafeDownCast(node);
  this->qvtkReconnect(d->parametersNode, parametersNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onParametersNodeModified()));
  d->parametersNode = parametersNode;
  this->onParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onSelectionNodeModified()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  // the module follows the active cube
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
void qSlicerAstroPVSliceModuleWidget::setInputVolume(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  const char* id = node ? node->GetID() : nullptr;
  const char* current = d->parametersNode->GetInputVolumeNodeID();
  const bool changed = (id && current) ? strcmp(id, current) != 0 : id != current;
  if (changed)
    {
    // a new cube: its moment map and the line on it
    d->parametersNode->SetMomentMapNodeID(nullptr);
    d->parametersNode->SetInputVolumeNodeID(id);
    }
  if (!d->lineNode() || !d->momentMap() || changed)
    {
    d->prepareInput();
    }
  this->onParametersNodeModified();
  if (d->showsViews())
    {
    vtkSlicerAstroPVSliceLogic* logic = d->logic();
    if (logic && d->lineNode() && d->momentMap())
      {
      logic->SetMomentMapOnRedWidget(d->parametersNode);
      logic->InitializePV(d->parametersNode);
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onParametersNodeModified()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  vtkMRMLAstroVolumeNode* cube = d->inputVolume();
  {
  const QSignalBlocker blocker(d->InputVolumeNodeSelector);
  d->InputVolumeNodeSelector->setCurrentNode(cube);
  }
  vtkMRMLMarkupsLineNode* line = d->lineNode();
  if (line != d->ObservedLine.GetPointer())
    {
    for (unsigned long event : {static_cast<unsigned long>(vtkMRMLMarkupsNode::PointModifiedEvent),
                                static_cast<unsigned long>(vtkMRMLMarkupsNode::PointPositionDefinedEvent)})
      {
      this->qvtkReconnect(d->ObservedLine, line, event, this, SLOT(onLineModified()));
      }
    d->ObservedLine = line;
    }
  d->Preview->setVolumeNode(d->momentMap());
  d->Preview->setMarkupsNode(line);
  d->updateEnabled();
  d->updateFields();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onLineModified()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  vtkSlicerAstroPVSliceLogic* logic = d->logic();
  if (!logic || d->Updating || !d->parametersNode)
    {
    return;
    }
  d->Updating = true;
  // on the plane of the map (e.g. dragged in a view), then the PV slice
  if (logic->ConstrainLine(d->parametersNode) && d->showsViews())
    {
    logic->UpdatePV(d->parametersNode);
    }
  d->Updating = false;
  d->updateFields();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onSkyFieldEdited()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  this->setLineSky(d->RightAscension->value(), d->Declination->value(), d->PositionAngle->value(),
                   d->Length->value());
}

//-----------------------------------------------------------------------------
QList<double> qSlicerAstroPVSliceModuleWidget::lineSky() const
{
  Q_D(const qSlicerAstroPVSliceModuleWidget);
  vtkSlicerAstroPVSliceLogic* logic = d->logic();
  double center[2] = {0., 0.};
  double positionAngle = 0.;
  double length = 0.;
  if (!logic || !logic->GetLineSky(d->parametersNode, center, &positionAngle, &length))
    {
    return QList<double>();
    }
  return QList<double>() << center[0] << center[1] << positionAngle << length;
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::setLineSky(double rightAscension, double declination,
                                                 double positionAngle, double length)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  vtkSlicerAstroPVSliceLogic* logic = d->logic();
  const double center[2] = {rightAscension, declination};
  if (!logic || !logic->SetLineSky(d->parametersNode, center, positionAngle, length))
    {
    d->updateFields();
    return;
    }
  this->onLineModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::centerOnPeak()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  vtkSlicerAstroPVSliceLogic* logic = d->logic();
  if (logic && logic->CenterLineOnPeak(d->parametersNode))
    {
    this->onLineModified();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::alignWithMajorAxis()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  vtkSlicerAstroPVSliceLogic* logic = d->logic();
  if (!logic)
    {
    return;
    }
  if (logic->AlignLineWithMajorAxis(d->parametersNode))
    {
    this->onLineModified();
    }
  else
    {
    d->Message->showMessage("info", tr("No emission above 20% of the peak of the moment-0 map."));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::setCameraView(int view)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  {
  const QSignalBlocker blocker(d->CameraButton);
  d->CameraButton->setCurrentIndex(view);
  }
  vtkSlicerAstroPVSliceLogic* logic = d->logic();
  if (logic)
    {
    logic->SetPVCamera(d->parametersNode, view);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::enter()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  this->Superclass::enter();
  this->onSelectionNodeModified();
  if (!d->lineNode() || !d->momentMap())
    {
    d->prepareInput();
    this->onParametersNodeModified();
    }
  vtkMRMLScene* scene = this->mrmlScene();
  vtkMRMLMarkupsLineNode* line = d->lineNode();
  if (line)
    {
    line->SetDisplayVisibility(1);
    }
  // classic Slicer: four views (moment-0 map with the line, PV slice, 3D);
  // embedded (e.g. Workbench): the views are not rearranged
  qSlicerApplication* app = qSlicerApplication::application();
  vtkMRMLLayoutNode* layoutNode = (app && app->layoutManager() && app->layoutManager()->layoutLogic()) ?
    app->layoutManager()->layoutLogic()->GetLayoutNode() : nullptr;
  if (layoutNode && !this->property("astroEmbedded").toBool() &&
      layoutNode->GetViewArrangement() != vtkMRMLLayoutNode::SlicerLayoutFourUpView)
    {
    layoutNode->SetViewArrangement(vtkMRMLLayoutNode::SlicerLayoutFourUpView);
    }
  vtkMRMLSliceNode* yellow = scene ? vtkMRMLSliceNode::SafeDownCast(scene->GetNodeByID("vtkMRMLSliceNodeYellow")) :
                                     nullptr;
  if (yellow)
    {
    yellow->SetSliceVisible(1);
    }
  vtkSlicerAstroPVSliceLogic* logic = d->logic();
  if (logic && line && d->momentMap())
    {
    logic->SetMomentMapOnRedWidget(d->parametersNode);
    logic->InitializePV(d->parametersNode);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::exit()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  this->Superclass::exit();
  vtkMRMLMarkupsLineNode* line = d->lineNode();
  if (line && !this->property("astroEmbedded").toBool())
    {
    line->SetDisplayVisibility(0);
    }
  vtkMRMLScene* scene = this->mrmlScene();
  vtkMRMLSliceNode* yellow = scene ? vtkMRMLSliceNode::SafeDownCast(scene->GetNodeByID("vtkMRMLSliceNodeYellow")) :
                                     nullptr;
  if (yellow)
    {
    yellow->SetSliceVisible(0);
    }
}
