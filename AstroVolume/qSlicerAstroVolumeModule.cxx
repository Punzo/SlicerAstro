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
#include <QApplication>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QtPlugin>
#include <QSettings>

//VTK includes
#include <vtkCollection.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkLookupTable.h>

//CTK includes
#include <ctkDoubleRangeSlider.h>
#include <ctkRangeWidget.h>
#include <ctkVTKScalarsToColorsWidget.h>
#include <ctkVTKVolumePropertyWidget.h>

// Slicer includes
#include <vtkSlicerConfigure.h>
#include <vtkSlicerVolumesLogic.h>
#include <vtkSlicerUnitsLogic.h>

// SlicerQt includes
#include <qMRMLLayoutManager.h>
#include <qMRMLLayoutManager_p.h>
#include <qMRMLVolumePropertyNodeWidget.h>
#include <qMRMLSliceWidget.h>
#include <qMRMLSliceView.h>
#include <qMRMLVolumeThresholdWidget.h>
#include <qSlicerAbstractModuleFactoryManager.h>
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerIOManager.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleFactoryManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerNodeWriter.h>
#include <qSlicerPresetComboBox.h>
#include <qSlicerScriptedLoadableModuleWidget.h>
#include <qSlicerUtils.h>
#include <qSlicerVolumeRenderingModuleWidget.h>
#include <qSlicerVolumeRenderingPresetComboBox.h>
#include <vtkSlicerVersionConfigure.h> // For Slicer_VERSION_MAJOR, Slicer_VERSION_MINOR

// AstroVolume Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// AstroVolume QtModule includes
#include <qSlicerAstroVolumeLayoutSliceViewFactory.h>
#include <qSlicerAstroVolumeModule.h>
#include <qSlicerAstroVolumeModuleWidget.h>
#include <qSlicerAstroVolumeReader.h>

// AstroVolume MRMLDM includes
#include <vtkMRMLAstroTwoDAxesDisplayableManager.h>
#include <vtkMRMLAstroBeamDisplayableManager.h>

// Segment editor effects includes
#include "qSlicerSegmentEditorEffectFactory.h"
#include "qSlicerSegmentEditorAstroCloudLassoEffect.h"
#include "qSlicerSegmentEditorAstroContoursEffect.h"

// MRML includes
#include <vtkMRMLApplicationLogic.h>
#include <vtkMRMLColorTableNode.h>
#include <vtkMRMLPlotChartNode.h>
#include <vtkMRMLProceduralColorNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSliceViewDisplayableManagerFactory.h>
#include <vtkMRMLThreeDViewDisplayableManagerFactory.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLViewNode.h>

// DisplayableManager initialization
#if Slicer_VERSION_MAJOR == 4 && Slicer_VERSION_MINOR >= 9
#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkSlicerAstroVolumeModuleMRMLDisplayableManager)
#endif

//-----------------------------------------------------------------------------
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <QtPlugin>
Q_EXPORT_PLUGIN2(qSlicerAstroVolumeModule, qSlicerAstroVolumeModule);
#endif

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroVolume
class qSlicerAstroVolumeModulePrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroVolumeModule);
protected:
  qSlicerAstroVolumeModule* const q_ptr;

public:
  qSlicerApplication* app;
  qSlicerAbstractCoreModule *volumeRendering;

  qSlicerAstroVolumeModulePrivate(qSlicerAstroVolumeModule& object);
  virtual ~qSlicerAstroVolumeModulePrivate();
};

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModulePrivate::qSlicerAstroVolumeModulePrivate(
  qSlicerAstroVolumeModule& object)
  : q_ptr(&object)
{
  this->app = 0;
  this->volumeRendering = 0;
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModulePrivate::~qSlicerAstroVolumeModulePrivate()
{
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModule::qSlicerAstroVolumeModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroVolumeModulePrivate(*this))
{
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModule::~qSlicerAstroVolumeModule()
{
}

//-----------------------------------------------------------------------------
QString qSlicerAstroVolumeModule::helpText()const
{
  QString help = QString(
    "The AstroVolume Module loads and adjusts display parameters of volume data.<br>"
    "<a href=\"%1/Documentation/%2.%3/Modules/AstroVolume\">"
    "%1/Documentation/%2.%3/Modules/AstroVolume</a><br>");
  return help.arg(this->slicerWikiUrl()).arg(Slicer_VERSION_MAJOR).arg(Slicer_VERSION_MINOR);
}

//-----------------------------------------------------------------------------
QString qSlicerAstroVolumeModule::acknowledgementText()const
{
  QString acknowledgement = QString(
    "This module was developed by Davide Punzo. <br>"
    "This work was supported by ERC grant nr. 291531 and the Slicer "
    "Community. See <a href=\"http://www.slicer.org\">http://www.slicer.org"
    "</a> for details.<br>"
    "Special thanks to Steve Pieper (Isomics), Jean-Christophe Fillion-Robin (Kitware)"
    "and Andras Lasso (PerkLab, Queen's).");
  return acknowledgement;
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroVolumeModule::contributors()const
{
  QStringList moduleContributors;
  moduleContributors << QString("Davide Punzo (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Thijs van der Hulst (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Jos Roerdink (Johann Bernoulli Institute)");
  moduleContributors << QString("Jean-Christophe Fillion-Robin (Kitware).");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerAstroVolumeModule::icon()const
{
  return QIcon(":/Icons/Medium/SlicerVolumes.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroVolumeModule::categories() const
{
  return QStringList() << "Astronomy" << "";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroVolumeModule::dependencies() const
{
  QStringList moduleDependencies;
  moduleDependencies << "Data" << "Volumes" << "VolumeRendering" << "Segmentations" << "SegmentEditor";
  return moduleDependencies;
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModule::setup()
{
  Q_D(qSlicerAstroVolumeModule);
  this->Superclass::setup();

  d->app = qSlicerApplication::application();

  if(!d->app)
    {
    qCritical() << "qSlicerAstroVolumeModule::setup() : qSlicerApplication not found!";
    return;
    }

  // Register the IO module for loading AstroVolumes as a variant of fits files
  if(!d->app->mrmlScene())
    {
    qCritical() << "qSlicerAstroVolumeModule::setup() : mrmlScene not preset.";
    return;
    }

  this->setMRMLScene(d->app->mrmlScene());

  qSlicerAbstractCoreModule* volumes = d->app->moduleManager()->module("Volumes");
  if (!volumes)
    {
    qCritical() << "qSlicerAstroVolumeModule::setup() : Volumes module not found!";
    return;
    }

  vtkSlicerVolumesLogic* volumesLogic =
    vtkSlicerVolumesLogic::SafeDownCast(volumes->logic());
  vtkSlicerAstroVolumeLogic* logic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
  if (!volumesLogic || !logic)
    {
    qCritical() << "qSlicerAstroVolumeModule::setup() : logics not found!";
    return;
    }

  logic->RegisterArchetypeVolumeNodeSetFactory( volumesLogic );
  qSlicerCoreIOManager* ioManager = d->app->coreIOManager();
  ioManager->registerIO(new qSlicerAstroVolumeReader(volumesLogic,this));
  ioManager->registerIO(new qSlicerNodeWriter(
    "AstroVolume", QString("AstroVolumeFile"),
    QStringList() << "vtkMRMLVolumeNode", true, this));


  //removing Volumes action from mainWindow interface:
  //for the moment I just disable the widget creation,
  //i.e. the action is still present on mainWindows.
  //For the moment it is satisfactory.
  volumes->setWidgetRepresentationCreationEnabled(false);

  //modify precision in VolumeRenderingWidgets
  d->volumeRendering = d->app->moduleManager()->module("VolumeRendering");
  if(!d->volumeRendering)
    {
    qCritical() << "qSlicerAstroVolumeModule::setup() : volumeRendering module not found!";
    return;
    }

  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
    this->mrmlScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if(!selectionNode)
    {
    qCritical() << "qSlicerAstroVolumeModule::setup() : selectionNode not found!";
    return;
    }

  vtkMRMLUnitNode* unitNodeIntensity = selectionNode->GetUnitNode("intensity");
  this->qvtkConnect(unitNodeIntensity, vtkCommand::ModifiedEvent,
                    this, SLOT(onMRMLUnitNodeIntensityModified(vtkObject*)));
  this->onMRMLUnitNodeIntensityModified(unitNodeIntensity);

  qSlicerVolumeRenderingModuleWidget*  volumeRenderingWidget =
      dynamic_cast<qSlicerVolumeRenderingModuleWidget*>
         (d->volumeRendering->widgetRepresentation());

  if(!volumeRenderingWidget)
    {
    qCritical() << "qSlicerAstroVolumeModule::setup() : VolumeReneringWidget not found!";
    return;
    }

  qSlicerVolumeRenderingPresetComboBox *PresetComboBox =
    volumeRenderingWidget->findChild<qSlicerVolumeRenderingPresetComboBox*>
      (QString("PresetComboBox"));
  if (!PresetComboBox)
    {
    qCritical() << "qSlicerAstroVolumeModule::setup() : PresetComboBox not found!";
    return;
    }

  QLabel* PresetsNodeLabel =
    PresetComboBox->findChild<QLabel*>
      (QString("PresetsLabel"));
  if (!PresetsNodeLabel)
    {
    qCritical() << "qSlicerAstroVolumeModule::setup() : PresetsNodeLabel not found!";
    return;
    }
  PresetsNodeLabel->hide();

  qSlicerPresetComboBox* PresetsNodeComboBox =
    PresetComboBox->findChild<qSlicerPresetComboBox*>
      (QString("PresetComboBox"));
  if (!PresetsNodeComboBox)
    {
    qCritical() << "qSlicerAstroVolumeModule::setup() : PresetsNodeComboBox not found!";
    return;
    }
  PresetsNodeComboBox->hide();

  // modify units nodes
  vtkMRMLUnitNode* unitNodeLength = selectionNode->GetUnitNode("length");
  unitNodeLength->SetMaximumValue(180.);
  unitNodeLength->SetMinimumValue(-180.);
  unitNodeLength->SetDisplayCoefficient(1.);
  unitNodeLength->SetPrefix("");
  unitNodeLength->SetSuffix("\u00B0");
  unitNodeLength->SetAttribute("DisplayHint","DegreeAsArcMinutesArcSeconds");
  unitNodeLength->SetPrecision(3);
  selectionNode->SetUnitNodeID("length", unitNodeLength->GetID());

  vtkMRMLUnitNode* unitNodeTime = selectionNode->GetUnitNode("time");
  unitNodeTime->SetMaximumValue(360);
  unitNodeTime->SetMinimumValue(0);
  unitNodeTime->SetDisplayCoefficient(0.066666666666667);
  unitNodeTime->SetPrefix("");
  unitNodeTime->SetSuffix("h");
  unitNodeTime->SetAttribute("DisplayHint","hoursAsMinutesSeconds");
  unitNodeTime->SetPrecision(3);
  selectionNode->SetUnitNodeID("time", unitNodeTime->GetID());

  vtkMRMLUnitNode* unitNodeVelocity = selectionNode->GetUnitNode("velocity");
  unitNodeVelocity->SetDisplayCoefficient(0.001);
  unitNodeVelocity->SetSuffix("km/s");
  unitNodeVelocity->SetPrefix("");
  unitNodeVelocity->SetPrecision(1);
  unitNodeVelocity->SetAttribute("DisplayHint","");
  selectionNode->SetUnitNodeID("velocity", unitNodeVelocity->GetID());

  vtkMRMLUnitNode* unitNodeFrequency = selectionNode->GetUnitNode("frequency");
  unitNodeFrequency->SetDisplayCoefficient(0.000001);
  unitNodeFrequency->SetPrefix("");
  unitNodeFrequency->SetPrecision(2);
  unitNodeFrequency->SetSuffix("MHz");
  unitNodeFrequency->SetAttribute("DisplayHint","");
  selectionNode->SetUnitNodeID("frequency", unitNodeFrequency->GetID());

  // set Slice Default Node
  vtkSmartPointer<vtkMRMLNode> defaultNode = vtkMRMLSliceNode::SafeDownCast
      (this->mrmlScene()->GetDefaultNodeByClass("vtkMRMLSliceNode"));
  if (!defaultNode)
    {
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLSliceNode");
    defaultNode.TakeReference(foo);
    this->mrmlScene()->AddDefaultNode(defaultNode);
    }
  vtkMRMLSliceNode *defaultSliceNode = vtkMRMLSliceNode::SafeDownCast(defaultNode);
  defaultSliceNode->RenameSliceOrientationPreset("Axial", "XZ");
  defaultSliceNode->RenameSliceOrientationPreset("Sagittal", "ZY");
  defaultSliceNode->RenameSliceOrientationPreset("Coronal", "XY");

  // modify SliceNodes already allocated
  vtkSmartPointer<vtkCollection> sliceNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLSliceNode"));

  for(int i = 0; i < sliceNodes->GetNumberOfItems(); i++)
    {
    vtkMRMLSliceNode* sliceNode =
        vtkMRMLSliceNode::SafeDownCast(sliceNodes->GetItemAsObject(i));
    if (sliceNode)
      {
      sliceNode->DisableModifiedEventOn();
      sliceNode->RenameSliceOrientationPreset("Axial", "XZ");
      sliceNode->RenameSliceOrientationPreset("Sagittal", "ZY");
      sliceNode->RenameSliceOrientationPreset("Coronal", "XY");
      sliceNode->DisableModifiedEventOff();
      }
    }

  // set the Slice Factory
  qMRMLLayoutSliceViewFactory* mrmlSliceViewFactory =
    qobject_cast<qMRMLLayoutSliceViewFactory*>(
    d->app->layoutManager()->mrmlViewFactory("vtkMRMLSliceNode"));

  qSlicerAstroVolumeLayoutSliceViewFactory* astroSliceViewFactory =
    new qSlicerAstroVolumeLayoutSliceViewFactory(d->app->layoutManager());
  astroSliceViewFactory->setSliceLogics(mrmlSliceViewFactory->sliceLogics());

  d->app->layoutManager()->unregisterViewFactory(mrmlSliceViewFactory);
  d->app->layoutManager()->registerViewFactory(astroSliceViewFactory);

  // modify orietation in default Layouts
  vtkMRMLLayoutNode* layoutNode =  vtkMRMLLayoutNode::SafeDownCast(
    this->mrmlScene()->GetSingletonNode("vtkMRMLLayoutNode","vtkMRMLLayoutNode"));
  if(!layoutNode)
    {
    qCritical() << "qSlicerAstroVolumeModule::setup() : layoutNode not found!";
    return;
    }

  std::vector<std::string> oldOrietation;
  oldOrietation.push_back("Coronal");
  oldOrietation.push_back("Axial");
  oldOrietation.push_back("Sagittal");
  std::vector<std::string> newOrietation;
  newOrietation.push_back("XY");
  newOrietation.push_back("XZ");
  newOrietation.push_back("ZY");
  for(int i = 1 ; i < 36; i++)
    {
    if (i == 5 || i == 11 || i == 13 || i == 18 || i == 20)
      {
      continue;
      }

    std::string layoutDescription = layoutNode->GetLayoutDescription(i);
    std::vector<std::string>::const_iterator it;
    std::vector<std::string>::const_iterator jt;
    for(it = oldOrietation.begin(), jt = newOrietation.begin(); it != oldOrietation.end() && jt != newOrietation.end(); ++it, ++jt)
      {
      size_t found = layoutDescription.find(*it);
      while (found!=std::string::npos)
        {
        layoutDescription.replace(found, it->size(), *jt);
        found = layoutDescription.find(*it);
        }
      }
    layoutNode->SetLayoutDescription(i, layoutDescription.c_str());
    }


  // unregister RulerDisplayableManager
  vtkMRMLThreeDViewDisplayableManagerFactory::GetInstance()->
    UnRegisterDisplayableManager("vtkMRMLRulerDisplayableManager");
  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->
    UnRegisterDisplayableManager("vtkMRMLRulerDisplayableManager");

  // register AstroTwoDAxesDisplayableManager
  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->
    RegisterDisplayableManager("vtkMRMLAstroTwoDAxesDisplayableManager");

  // register AstroTwoDAxesDisplayableManager
  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->
    RegisterDisplayableManager("vtkMRMLAstroBeamDisplayableManager");

  // register Astro Editor Effects in the Segmentation Editor
  qSlicerSegmentEditorEffectFactory::instance()->registerEffect(new qSlicerSegmentEditorAstroCloudLassoEffect());
  qSlicerSegmentEditorEffectFactory::instance()->registerEffect(new qSlicerSegmentEditorAstroContoursEffect());

  // Set Default ColorNodeTable
  defaultNode = vtkMRMLColorTableNode::SafeDownCast
      (this->mrmlScene()->GetDefaultNodeByClass("vtkMRMLPlotChartNode"));
  if (!defaultNode)
    {
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLColorTableNode");
    defaultNode.TakeReference(foo);
    this->mrmlScene()->AddDefaultNode(defaultNode);
    }
  vtkMRMLColorTableNode *colorTableDefaultNode = vtkMRMLColorTableNode::SafeDownCast(defaultNode);
  colorTableDefaultNode->SetAttribute("SlicerAstro.AddFunctions", "on");
  colorTableDefaultNode->SetAttribute("SlicerAstro.Reverse", "on");
  colorTableDefaultNode->SetAttribute("SlicerAstro.Inverse", "on");
  colorTableDefaultNode->SetAttribute("SlicerAstro.Log", "on");

  // Modify ColorNodeTable already allocated
  vtkSmartPointer<vtkCollection> ColorTableNodeCol = vtkSmartPointer<vtkCollection>::Take(
      this->mrmlScene()->GetNodesByClass("vtkMRMLColorTableNode"));
  for (int ii = 0; ii < ColorTableNodeCol->GetNumberOfItems(); ii++)
    {
    vtkMRMLColorTableNode* tempColorTableNode = vtkMRMLColorTableNode::SafeDownCast
            (ColorTableNodeCol->GetItemAsObject(ii));
    if (!tempColorTableNode)
      {
      continue;
      }

    tempColorTableNode->SetAttribute("SlicerAstro.AddFunctions", "on");
    tempColorTableNode->SetAttribute("SlicerAstro.Reverse", "on");
    tempColorTableNode->SetAttribute("SlicerAstro.Inverse", "on");
    tempColorTableNode->SetAttribute("SlicerAstro.Log", "on");
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModule::onMRMLUnitNodeIntensityModified(vtkObject* sender)
{
  Q_D(qSlicerAstroVolumeModule);
  if(!sender || !d->volumeRendering)
    {
    return;
    }

  vtkMRMLUnitNode* unitNode = vtkMRMLUnitNode::SafeDownCast(sender);
  if (!unitNode)
    {
    return;
    }

  qSlicerVolumeRenderingModuleWidget*  volumeRenderingWidget =
      dynamic_cast<qSlicerVolumeRenderingModuleWidget*>
         (d->volumeRendering->widgetRepresentation());
  if(!volumeRenderingWidget)
    {
    return;
    }

  qMRMLVolumePropertyNodeWidget* volumePropertyNodeWidget =
    volumeRenderingWidget->findChild<qMRMLVolumePropertyNodeWidget*>
      (QString("VolumePropertyNodeWidget"));
  if (!volumePropertyNodeWidget)
    {
    return;
    }

  ctkVTKVolumePropertyWidget* volumePropertyWidget =
      volumePropertyNodeWidget->findChild<ctkVTKVolumePropertyWidget*>
      (QString("VolumePropertyWidget"));
  if (!volumePropertyWidget)
    {
    return;
    }

  ctkVTKScalarsToColorsWidget* opacityWidget =
      volumePropertyWidget->findChild<ctkVTKScalarsToColorsWidget*>
      (QString("ScalarOpacityWidget"));
  if (!opacityWidget)
    {
    return;
    }

  QDoubleSpinBox* XSpinBox = opacityWidget->findChild<QDoubleSpinBox*>
      (QString("XSpinBox"));
  if (!opacityWidget)
    {
    return;
    }

  XSpinBox->setDecimals(unitNode->GetPrecision());

  ctkDoubleRangeSlider* XRangeSlider = opacityWidget->findChild<ctkDoubleRangeSlider*>
      (QString("XRangeSlider"));
  if (!XRangeSlider)
    {
    return;
    }

  XRangeSlider->setRange(unitNode->GetMinimumValue(), unitNode->GetMaximumValue());
  XRangeSlider->setSingleStep((unitNode->GetMaximumValue() - unitNode->GetMinimumValue()) / 100.);

  opacityWidget->setXRange(unitNode->GetMinimumValue(), unitNode->GetMaximumValue());

  ctkVTKScalarsToColorsWidget* colorWidget =
      volumePropertyWidget->findChild<ctkVTKScalarsToColorsWidget*>
      (QString("ScalarColorWidget"));
  if (!colorWidget)
    {
    return;
    }

  QDoubleSpinBox* XSpinBox1 = colorWidget->findChild<QDoubleSpinBox*>
      (QString("XSpinBox"));
  if (!colorWidget)
    {
    return;
    }

  XSpinBox1->setDecimals(unitNode->GetPrecision());

  ctkDoubleRangeSlider* XRangeSlider1 = colorWidget->findChild<ctkDoubleRangeSlider*>
      (QString("XRangeSlider"));
  if (!XRangeSlider1)
    {
    return;
    }

  XRangeSlider1->setRange(unitNode->GetMinimumValue(), unitNode->GetMaximumValue());
  XRangeSlider1->setSingleStep((unitNode->GetMaximumValue() - unitNode->GetMinimumValue()) / 100.);

  colorWidget->setXRange(unitNode->GetMinimumValue(), unitNode->GetMaximumValue());
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation* qSlicerAstroVolumeModule::createWidgetRepresentation()
{
  return new qSlicerAstroVolumeModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerAstroVolumeModule::createLogic()
{
  return vtkSlicerAstroVolumeLogic::New();
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroVolumeModule::associatedNodeTypes() const
{
  return QStringList()
    << "vtkMRMLVolumeNode"
    << "vtkMRMLVolumeDisplayNode";
}
