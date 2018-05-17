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
#include <QDebug>
#include <QMessageBox>

// CTK includes
#include <ctkFlowLayout.h>

// VTK includes
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkProperty.h>
#include <vtkStdString.h>
#include <vtksys/SystemTools.hxx>

// AstroReproject includes
#include "qSlicerAstroReprojectModuleWidget.h"
#include "ui_qSlicerAstroReprojectModuleWidget.h"

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroReprojectLogic.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerAstroVolumeModuleWidget.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerUtils.h>

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLAnnotationROINode.h>
#include <vtkMRMLAstroReprojectParametersNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

#define SigmatoFWHM 2.3548200450309493

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroReproject
class qSlicerAstroReprojectModuleWidgetPrivate: public Ui_qSlicerAstroReprojectModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroReprojectModuleWidget);
protected:
  qSlicerAstroReprojectModuleWidget* const q_ptr;
public:

  qSlicerAstroReprojectModuleWidgetPrivate(qSlicerAstroReprojectModuleWidget& object);
  ~qSlicerAstroReprojectModuleWidgetPrivate();

  void init();
  void cleanPointers();

  vtkSlicerAstroReprojectLogic* logic() const;
  qSlicerAstroVolumeModuleWidget* astroVolumeWidget;
  vtkSmartPointer<vtkMRMLAstroReprojectParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;

};

//-----------------------------------------------------------------------------
// qSlicerAstroReprojectModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroReprojectModuleWidgetPrivate::qSlicerAstroReprojectModuleWidgetPrivate(qSlicerAstroReprojectModuleWidget& object)
  : q_ptr(&object)
{
  this->astroVolumeWidget = 0;
  this->parametersNode = 0;
  this->selectionNode = 0;
}

//-----------------------------------------------------------------------------
qSlicerAstroReprojectModuleWidgetPrivate::~qSlicerAstroReprojectModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroReprojectModuleWidget);
  this->setupUi(q);

  qSlicerApplication* app = qSlicerApplication::application();
  if(!app)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidgetPrivate::init : "
                   "could not find qSlicerApplication!";
    return;
    }

  qSlicerAbstractCoreModule* astroVolume = app->moduleManager()->module("AstroVolume");
  if (!astroVolume)
    {
    qCritical() << "qSlicerAstroReprojectModuleWidgetPrivate::init : "
                   "could not find AstroVolume module.";
    return;
    }

  this->astroVolumeWidget = dynamic_cast<qSlicerAstroVolumeModuleWidget*>
    (astroVolume->widgetRepresentation());

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->ParametersNodeComboBox, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->InputVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->ReferenceVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->OutputVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->NodesCollapsibleButton, SLOT(setEnabled(bool)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->ParametersCollapsibleButton, SLOT(setEnabled(bool)));

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroReprojectParametersNode(vtkMRMLNode*)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->ReferenceVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onReferenceVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->OutputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onOutputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->ReprojectRotationCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onReprojectRotationChanged(bool)));

  QObject::connect(this->ReprojectDataCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onReprojectDataChanged(bool)));

  QObject::connect(this->NearestNeighbourInterpolationRadioButton, SIGNAL(clicked(bool)),
                   q, SLOT(onInterpolationNearestNeighbour(bool)));

  QObject::connect(this->BilinearInterpolationRadioButton, SIGNAL(clicked(bool)),
                   q, SLOT(onInterpolationBilinear(bool)));

  QObject::connect(this->BicubicInterpolationRadioButton, SIGNAL(clicked(bool)),
                   q, SLOT(onInterpolationBicubic(bool)));

  QObject::connect(this->ApplyButton, SIGNAL(clicked()),
                   q, SLOT(onApply()));

  QObject::connect(this->CancelButton, SIGNAL(clicked()),
                   q, SLOT(onComputationCancelled()));


  this->progressBar->hide();
  this->progressBar->setMinimum(0);
  this->progressBar->setMaximum(100);
  this->CancelButton->hide();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::cleanPointers()
{
  Q_Q(const qSlicerAstroReprojectModuleWidget);

  if (!q->mrmlScene())
    {
    return;
    }

  if (this->parametersNode)
    {
    q->mrmlScene()->RemoveNode(this->parametersNode);
    }
  this->parametersNode = 0;
}

//-----------------------------------------------------------------------------
vtkSlicerAstroReprojectLogic* qSlicerAstroReprojectModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroReprojectModuleWidget);
  return vtkSlicerAstroReprojectLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
// qSlicerAstroReprojectModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroReprojectModuleWidget::qSlicerAstroReprojectModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroReprojectModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroReprojectModuleWidget::~qSlicerAstroReprojectModuleWidget()
{
}

//----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::enter()
{
  this->Superclass::enter();

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app || !app->layoutManager() || !app->layoutManager()->layoutLogic()
     || !app->layoutManager()->layoutLogic()->GetLayoutNode())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "qSlicerApplication not found.";
    return;
    }

  app->layoutManager()->layoutLogic()->GetLayoutNode()->SetViewArrangement
          (vtkMRMLLayoutNode::SlicerLayoutFourUpView);
}

//----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::exit()
{
  this->Superclass::exit();
}

namespace
{
//----------------------------------------------------------------------------
template <typename T> T StringToNumber(const char* num)
{
  std::stringstream ss;
  ss << num;
  T result;
  return ss >> result ? result : 0;
}

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

//----------------------------------------------------------------------------
template <typename T> std::string NumberToString(T V)
{
  std::string stringValue;
  std::stringstream strstream;
  strstream << V;
  strstream >> stringValue;
  return stringValue;
}

//----------------------------------------------------------------------------
std::string IntToString(int Value)
{
  return NumberToString<int>(Value);
}
} // end namespace

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  if (!scene)
    {
    return;
    }

  this->Superclass::setMRMLScene(scene);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroReprojectModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroReprojectModuleWidget::setMRMLScene : selectionNode not found!";
    return;
    }

  this->initializeNodes();

  this->qvtkReconnect(scene, vtkMRMLScene::EndCloseEvent,
                      this, SLOT(onEndCloseEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::StartImportEvent,
                      this, SLOT(onStartImportEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent,
                      this, SLOT(onEndImportEvent()));
  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));

  this->onMRMLSelectionNodeModified(d->selectionNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::initializeNodes(bool forceNew /*= false*/)
{
  this->initializeParameterNode(forceNew);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroReprojectModuleWidget::onEndCloseEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onEndCloseEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes(true);
  this->onMRMLAstroReprojectParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onEndImportEvent()
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroReprojectModuleWidget::onEndImportEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes();
  this->onMRMLAstroReprojectParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  if (!this->mrmlScene() || !d->selectionNode)
    {
    return;
    }

  vtkMRMLAstroReprojectParametersNode *astroParametersNode = NULL;
  unsigned int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroReprojectParametersNode");
  if(numNodes > 0 && !forceNew)
    {
    astroParametersNode = vtkMRMLAstroReprojectParametersNode::SafeDownCast
      (this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroReprojectParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> parametersNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLAstroReprojectParametersNode");
    parametersNode.TakeReference(foo);
    this->mrmlScene()->AddNode(parametersNode);

    astroParametersNode = vtkMRMLAstroReprojectParametersNode::SafeDownCast(parametersNode);
    int wasModifying = astroParametersNode->StartModify();
    astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
    astroParametersNode->EndModify(wasModifying);
    }

  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onMRMLSelectionNodeModified(vtkObject* sender)
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  if (!sender)
    {
    return;
    }

  vtkMRMLSelectionNode *selectionNode =
      vtkMRMLSelectionNode::SafeDownCast(sender);

  if (!selectionNode || !d->parametersNode)
    {
    return;
    }

  if (d->parametersNode->GetInputVolumeNodeID() && selectionNode->GetActiveVolumeID())
    {
    if(!strcmp(d->parametersNode->GetInputVolumeNodeID(), selectionNode->GetActiveVolumeID()))
      {
      return;
      }
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetInputVolumeNodeID(selectionNode->GetActiveVolumeID());
  d->parametersNode->EndModify(wasModifying);
}

//--------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setMRMLAstroReprojectParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLAstroReprojectParametersNode* AstroReprojectParaNode =
      vtkMRMLAstroReprojectParametersNode::SafeDownCast(mrmlNode);

  this->qvtkReconnect(d->parametersNode, AstroReprojectParaNode, vtkCommand::ModifiedEvent,
                this, SLOT(onMRMLAstroReprojectParametersNodeModified()));

  d->parametersNode = AstroReprojectParaNode;

  this->onMRMLAstroReprojectParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  if (!d->parametersNode || !d->selectionNode)
    {
    return;
    }

  if (mrmlNode)
    {
    d->selectionNode->SetReferenceActiveVolumeID(mrmlNode->GetID());
    d->selectionNode->SetActiveVolumeID(mrmlNode->GetID());
    }
  else
    {
    d->selectionNode->SetReferenceActiveVolumeID(NULL);
    d->selectionNode->SetActiveVolumeID(NULL);
  }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onInterpolationNearestNeighbour(bool toggled)
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (toggled)
    {
    d->parametersNode->SetInterpolationOrder(vtkMRMLAstroReprojectParametersNode::NearestNeighbour);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onInterpolationBilinear(bool toggled)
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (toggled)
    {
    d->parametersNode->SetInterpolationOrder(vtkMRMLAstroReprojectParametersNode::Bilinear);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onInterpolationBicubic(bool toggled)
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (toggled)
    {
    d->parametersNode->SetInterpolationOrder(vtkMRMLAstroReprojectParametersNode::Bicubic);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onOutputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (mrmlNode)
    {
    d->parametersNode->SetOutputVolumeNodeID(mrmlNode->GetID());
    }
  else
    {
    d->parametersNode->SetOutputVolumeNodeID(NULL);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onReferenceVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (mrmlNode)
    {
    d->parametersNode->SetReferenceVolumeNodeID(mrmlNode->GetID());
    }
  else
    {
    d->parametersNode->SetReferenceVolumeNodeID(NULL);
  }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onReprojectDataChanged(bool toggled)
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetReprojectData(toggled);

  if (toggled)
    {
    d->parametersNode->SetReprojectRotation(!toggled);
    }

  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onReprojectRotationChanged(bool toggled)
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetReprojectRotation(toggled);

  if (toggled)
    {
    d->parametersNode->SetReprojectData(!toggled);
    }

  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onMRMLAstroReprojectParametersNodeModified()
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int status = d->parametersNode->GetStatus();

  char *inputVolumeNodeID = d->parametersNode->GetInputVolumeNodeID();
  vtkMRMLAstroVolumeNode *inputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(inputVolumeNodeID));
  d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);

  char *referenceVolumeNodeID = d->parametersNode->GetReferenceVolumeNodeID();
  vtkMRMLAstroVolumeNode *referenceVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(referenceVolumeNodeID));
  d->ReferenceVolumeNodeSelector->setCurrentNode(referenceVolumeNode);

  char *outputVolumeNodeID = d->parametersNode->GetOutputVolumeNodeID();
  vtkMRMLAstroVolumeNode *outputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(outputVolumeNodeID));
  d->OutputVolumeNodeSelector->setCurrentNode(outputVolumeNode);

  d->ReprojectDataCheckBox->setChecked(d->parametersNode->GetReprojectData());

  if (d->parametersNode->GetReprojectData())
    {
    d->ReferenceVolumeNodeSelector->show();
    d->ReferenceVolumeLabel->show();
    }
  else
    {
    d->ReferenceVolumeNodeSelector->hide();
    d->ReferenceVolumeLabel->hide();
    }

  d->ReprojectRotationCheckBox->setChecked(d->parametersNode->GetReprojectRotation());

  if (d->parametersNode->GetInterpolationOrder() == vtkMRMLAstroReprojectParametersNode::NearestNeighbour)
    {
    d->NearestNeighbourInterpolationRadioButton->setChecked(true);
    }
  else if  (d->parametersNode->GetInterpolationOrder() == vtkMRMLAstroReprojectParametersNode::Bilinear)
    {
    d->BilinearInterpolationRadioButton->setChecked(true);
    }
  else if  (d->parametersNode->GetInterpolationOrder() == vtkMRMLAstroReprojectParametersNode::Bicubic)
    {
    d->BicubicInterpolationRadioButton->setChecked(true);
    }
  else
    {
    d->BilinearInterpolationRadioButton->setChecked(true);
    }

  if(status == 0)
    {
    this->onComputationFinished();
    }
  else
    {
    if(status == 1)
      {
      this->onComputationStarted();
      }
    if(status != -1)
      {
      this->updateProgress(status);
      qSlicerApplication::application()->processEvents();
      }
  }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroReprojectModuleWidget);

  d->cleanPointers();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onApply()
{
  Q_D(const qSlicerAstroReprojectModuleWidget);

  if (!d->parametersNode)
    {
    qCritical() << "qSlicerAstroReprojectModuleWidget::onApply() : "
                   "parametersNode not found!";
    return;
    }

  vtkSlicerAstroReprojectLogic *logic = d->logic();
  if (!logic)
    {
    qCritical() <<"qSlicerAstroReprojectModuleWidget::onApply() : astroReprojectLogic not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  d->parametersNode->SetStatus(1);

  vtkMRMLScene *scene = this->mrmlScene();
  if(!scene)
    {
    qCritical() <<"qSlicerAstroReprojectModuleWidget::onApply"
                  " : scene not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    qCritical() <<"qSlicerAstroReprojectModuleWidget::onApply"
                  " : inputVolume not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkMRMLAstroVolumeNode *referenceVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetReferenceVolumeNodeID()));

  bool ReprojectToReference = false;

  std::string ReferenceID = "";

  if(referenceVolume && referenceVolume->GetImageData())
    {
    ReprojectToReference = true;
    ReferenceID = d->parametersNode->GetReferenceVolumeNodeID();
    }

  const char* outputNameReference = "_Reprojected_";

  std::ostringstream outSS;
  outSS << inputVolume->GetName() << outputNameReference;

  int serial = d->parametersNode->GetOutputSerial();
  outSS<< IntToString(serial);
  serial++;
  d->parametersNode->SetOutputSerial(serial);

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));

  outputVolume = logic->GetAstroVolumeLogic()->CloneAstroVolume
    (this->mrmlScene(), inputVolume, outputVolume, outputNameReference, outSS.str().c_str());
  if(!outputVolume || !outputVolume->GetImageData())
    {
    qCritical() <<"qSlicerAstroReprojectModuleWidget::onApply"
                  " : outputVolume not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  d->parametersNode->SetOutputVolumeNodeID(outputVolume->GetID());

  vtkMRMLAstroVolumeDisplayNode *outputVolumeDisplay =
    outputVolume->GetAstroVolumeDisplayNode();
  if (!outputVolumeDisplay || !outputVolumeDisplay->GetWCSStruct())
    {
    qCritical() <<"qSlicerAstroReprojectModuleWidget::onApply"
                  " : outputVolumeDisplay not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (d->parametersNode->GetReprojectRotation())
    {
    int n = outputVolumeDisplay->GetWCSStruct()->naxis;
    double rot = StringToDouble(outputVolume->GetAttribute("SlicerAstro.CROTA1")) * PI / 180.;
    double rotCos = cos(rot);
    double rotSin = sin(rot);
    outputVolumeDisplay->GetWCSStruct()->flag = 0;
    for (int ii = 0; ii < n; ii++)
      {
      outputVolumeDisplay->GetWCSStruct()->crota[ii] = 0.;
      }

    double temp[4] = {0.};
    for (int ii = 0; ii < 4; ii++)
      {
      temp[ii] = outputVolumeDisplay->GetWCSStruct()->pc[ii];
      }
    outputVolumeDisplay->GetWCSStruct()->pc[0] = temp[0] * rotCos + temp[1] * rotSin;
    outputVolumeDisplay->GetWCSStruct()->pc[1] = -temp[0] * rotSin + temp[1] * rotCos;
    outputVolumeDisplay->GetWCSStruct()->pc[2] = temp[2] * rotCos + temp[3] * rotSin;
    outputVolumeDisplay->GetWCSStruct()->pc[3] = -temp[2] * rotSin + temp[3] * rotCos;

    outputVolumeDisplay->SetWCSStatus(wcsset(outputVolumeDisplay->GetWCSStruct()));
    if (outputVolumeDisplay->GetWCSStatus())
      {
      qCritical() <<"qSlicerAstroReprojectModuleWidget::onApply :"
                    "wcsset ERROR "<<outputVolumeDisplay->GetWCSStatus()<<":\n"<<
                    "Message from "<<outputVolumeDisplay->GetWCSStruct()->err->function<<
                    "at line "<<outputVolumeDisplay->GetWCSStruct()->err->line_no<<
                    " of file "<<outputVolumeDisplay->GetWCSStruct()->err->file<<
                    ": \n"<<outputVolumeDisplay->GetWCSStruct()->err->msg<<"\n";
      }

    d->parametersNode->SetReferenceVolumeNodeID(outputVolume->GetID());

    if (logic->Reproject(d->parametersNode))
      {
      outputVolume->SetAttribute("SlicerAstro.CROTA1", "0.");
      outputVolume->SetAttribute("SlicerAstro.CROTA2", "0.");

      if (n > 2)
        {
        outputVolume->SetAttribute("SlicerAstro.CROTA3", "0.");
        }
      d->parametersNode->SetReferenceVolumeNodeID("");
      d->astroVolumeWidget->setThreeComparativeView
        (inputVolume->GetID(), outputVolume->GetID(), outputVolume->GetID());
      }
    else
      {
      scene->RemoveNode(outputVolume);
      d->parametersNode->SetOutputVolumeNodeID("");
      }
    }

  if (ReprojectToReference)
    {
    if (logic->Reproject(d->parametersNode))
      {
      vtkNew<vtkStringArray> outputAttributes;
      outputVolume->GetAttributeNames(outputAttributes);
      for (int AttributeIndex = 0; AttributeIndex < outputAttributes->GetNumberOfValues(); AttributeIndex++)
        {
        std::string AttributeKey = outputAttributes->GetValue(AttributeIndex).c_str();
        if (AttributeKey.find("_COMMENT") != std::string::npos ||
            AttributeKey.find("_HISTORY") != std::string::npos)
          {
          continue;
          }
        else if (AttributeKey.find("BMAJ") != std::string::npos ||
                 AttributeKey.find("BMIN") != std::string::npos ||
                 AttributeKey.find("BPA") != std::string::npos ||
                 AttributeKey.find("CDELT1") != std::string::npos ||
                 AttributeKey.find("CDELT2") != std::string::npos ||
                 AttributeKey.find("CROTA1") != std::string::npos ||
                 AttributeKey.find("CROTA2") != std::string::npos ||
                 AttributeKey.find("CRPIX1") != std::string::npos ||
                 AttributeKey.find("CRPIX2") != std::string::npos ||
                 AttributeKey.find("CRVAL1") != std::string::npos ||
                 AttributeKey.find("CRVAL2") != std::string::npos ||
                 AttributeKey.find("CTYPE1") != std::string::npos ||
                 AttributeKey.find("CTYPE2") != std::string::npos ||
                 AttributeKey.find("CUNIT1") != std::string::npos ||
                 AttributeKey.find("CUNIT2") != std::string::npos ||
                 AttributeKey.find("NAXIS1") != std::string::npos ||
                 AttributeKey.find("NAXIS2") != std::string::npos)
          {
          outputVolume->RemoveAttribute(AttributeKey.c_str());
          }
        }

      vtkNew<vtkStringArray> referenceAttributes;
      referenceVolume->GetAttributeNames(referenceAttributes);
      for (int AttributeIndex = 0; AttributeIndex < referenceAttributes->GetNumberOfValues(); AttributeIndex++)
        {
        std::string AttributeKey = referenceAttributes->GetValue(AttributeIndex).c_str();
        if (AttributeKey.find("_COMMENT") != std::string::npos ||
            AttributeKey.find("_HISTORY") != std::string::npos)
          {
          continue;
          }
        else if (AttributeKey.find("BMAJ") != std::string::npos ||
                 AttributeKey.find("BMIN") != std::string::npos ||
                 AttributeKey.find("BPA") != std::string::npos ||
                 AttributeKey.find("CDELT1") != std::string::npos ||
                 AttributeKey.find("CDELT2") != std::string::npos ||
                 AttributeKey.find("CROTA1") != std::string::npos ||
                 AttributeKey.find("CROTA2") != std::string::npos ||
                 AttributeKey.find("CRPIX1") != std::string::npos ||
                 AttributeKey.find("CRPIX2") != std::string::npos ||
                 AttributeKey.find("CRVAL1") != std::string::npos ||
                 AttributeKey.find("CRVAL2") != std::string::npos ||
                 AttributeKey.find("CTYPE1") != std::string::npos ||
                 AttributeKey.find("CTYPE2") != std::string::npos ||
                 AttributeKey.find("CUNIT1") != std::string::npos ||
                 AttributeKey.find("CUNIT2") != std::string::npos ||
                 AttributeKey.find("NAXIS1") != std::string::npos ||
                 AttributeKey.find("NAXIS2") != std::string::npos)
          {
          std::string AttributeValue = referenceVolume->GetAttribute(AttributeKey.c_str());
          outputVolume->SetAttribute(AttributeKey.c_str(), AttributeValue.c_str());
          }
        }

      vtkMRMLAstroVolumeDisplayNode* outputDisplayNode = outputVolume->GetAstroVolumeDisplayNode();
      vtkMRMLAstroVolumeDisplayNode* referenceDisplayNode = referenceVolume->GetAstroVolumeDisplayNode();
      outputDisplayNode->SetFitSlices(referenceDisplayNode->GetFitSlices());
      outputDisplayNode->SetContoursColor(referenceDisplayNode->GetContoursColor());
      outputDisplayNode->SetSpaceQuantities(referenceDisplayNode->GetSpaceQuantities());
      outputDisplayNode->SetSpace(referenceDisplayNode->GetSpace());
      outputDisplayNode->CopySpatialWCS(referenceDisplayNode);

      d->astroVolumeWidget->setThreeComparativeView
        (inputVolume->GetID(), referenceVolume->GetID(), outputVolume->GetID());
      }
    else
      {
      scene->RemoveNode(outputVolume);
      d->parametersNode->SetOutputVolumeNodeID("");
      }
    }

  d->parametersNode->SetStatus(0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  d->CancelButton->hide();
  d->progressBar->hide();
  d->ApplyButton->show();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  d->parametersNode->SetStatus(-1);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  d->progressBar->setValue(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onComputationStarted()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  d->ApplyButton->hide();
  d->progressBar->show();
  d->CancelButton->show();
}

//---------------------------------------------------------------------------
vtkMRMLAstroReprojectParametersNode* qSlicerAstroReprojectModuleWidget::
mrmlAstroReprojectParametersNode()const
{
  Q_D(const qSlicerAstroReprojectModuleWidget);
  return d->parametersNode;
}
