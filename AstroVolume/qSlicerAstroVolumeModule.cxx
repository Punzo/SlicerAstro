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
#include <QLabel>
#include <QtPlugin>
#include <QSettings>
#include <QMainWindow>

//VTK includes
#include <vtkCollection.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkLookupTable.h>

//CTK includes
#include <ctkCollapsibleButton.h>

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

// MRML includes
#include <vtkMRMLApplicationLogic.h>
#include <vtkMRMLCameraNode.h>
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
/// \ingroup SlicerAstro_QtModules_AstroVolume
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
  return "The AstroVolume Module loads and adjusts display parameters of astronomical datasets (FITS file).";
}

//-----------------------------------------------------------------------------
QString qSlicerAstroVolumeModule::acknowledgementText()const
{
  QString acknowledgement = QString(
    "This module was developed by Davide Punzo. <br>"
    "This work was supported by ERC grant nr. 291531 and the Slicer "
    "Community. See <a href=\"http://www.slicer.org\">http://www.slicer.org"
    "</a> for details.<br>"
    "Special thanks to Steve Pieper (Isomics) and Andras Lasso (PerkLab, Queen's).<br>"
    "This module uses the following C libraries: <br>"
    "cfitsio: https://heasarc.gsfc.nasa.gov/fitsio/fitsio.html <br>"
    "wcslib: http://www.atnf.csiro.au/people/mcalabre/WCS/");
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
  return QStringList() << "Data" << "Volumes" << "VolumeRendering" << "Segmentations" << "SegmentEditor";
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

  // Get Volumes logic
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

  // Register I/O manager
  logic->RegisterArchetypeVolumeNodeSetFactory( volumesLogic );
  qSlicerCoreIOManager* ioManager = d->app->coreIOManager();
  ioManager->registerIO(new qSlicerAstroVolumeReader(volumesLogic,this));
  ioManager->registerIO(new qSlicerNodeWriter(
    "AstroVolume", QString("AstroVolumeFile"),
    QStringList() << "vtkMRMLVolumeNode", true, this));

  // Set Module GUI (dockWidgetContents) size policy
  if (d->app->mainWindow())
    {
    ctkCollapsibleButton *DataProbeCollapsibleWidget =
      d->app->mainWindow()->findChild<ctkCollapsibleButton*>
        (QString("DataProbeCollapsibleWidget"));
    QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    DataProbeCollapsibleWidget->setSizePolicy(sizePolicy);
    DataProbeCollapsibleWidget->setMinimumWidth(620);
    }

  // Removing Volumes action from mainWindow interface:
  // this disable the creration of teh widget for the volumes module.
  volumes->setWidgetRepresentationCreationEnabled(false);

  // Modify VolumeRenderingWidgets
  d->volumeRendering = d->app->moduleManager()->module("VolumeRendering");
  if(!d->volumeRendering)
    {
    qCritical() << "qSlicerAstroVolumeModule::setup() : volumeRendering module not found!";
    return;
    }

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

  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
    this->mrmlScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if(!selectionNode)
    {
    qCritical() << "qSlicerAstroVolumeModule::setup() : selectionNode not found!";
    return;
    }

  // Modify units nodes
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

  // Set Slice Default Node
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

  // Modify SliceNodes already allocated
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

  // Set the Slice Factory
  /*qMRMLLayoutSliceViewFactory* mrmlSliceViewFactory =
    qobject_cast<qMRMLLayoutSliceViewFactory*>(
    d->app->layoutManager()->mrmlViewFactory("vtkMRMLSliceNode"));*/

  qSlicerAstroVolumeLayoutSliceViewFactory* astroSliceViewFactory =
    new qSlicerAstroVolumeLayoutSliceViewFactory(d->app->layoutManager());

  //d->app->layoutManager()->unregisterViewFactory(mrmlSliceViewFactory);
  // TO DO: this should be disabled, but SlicerAstro with the last Slicer version (16/02/2021)
  // crash if the default slice view factory is disabled
  d->app->layoutManager()->registerViewFactory(astroSliceViewFactory);

  // Modify orietation in default Layouts
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


  // Unregister RulerDisplayableManager
  vtkMRMLThreeDViewDisplayableManagerFactory::GetInstance()->
    UnRegisterDisplayableManager("vtkMRMLRulerDisplayableManager");
  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->
    UnRegisterDisplayableManager("vtkMRMLRulerDisplayableManager");

  // Register AstroTwoDAxesDisplayableManager
  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->
    RegisterDisplayableManager("vtkMRMLAstroTwoDAxesDisplayableManager");

  // Register AstroTwoDAxesDisplayableManager
  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->
    RegisterDisplayableManager("vtkMRMLAstroBeamDisplayableManager");

  // Register Astro Editor Effects in the Segmentation Editor
  qSlicerSegmentEditorEffectFactory::instance()->registerEffect(new qSlicerSegmentEditorAstroCloudLassoEffect());

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
    << "vtkMRMLAstroVolumeNode"
    << "vtkMRMLAstroVolumeDisplayNode"
    << "vtkMRMLAstroLabelMapVolumeNode"
    << "vtkMRMLAstroLabelMapVolumeDisplayNode";
}
