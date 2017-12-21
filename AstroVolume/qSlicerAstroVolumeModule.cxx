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
#include <vtkSlicerVersionConfigure.h> // For Slicer_VERSION_MAJOR, Slicer_VERSION_MINOR

// AstroVolume Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// AstroVolume QtModule includes
#include <qSlicerAstroVolumeLayoutSliceViewFactory.h>
#include <qSlicerAstroVolumeModule.h>
#include <qSlicerAstroVolumeModuleWidget.h>
#include <qSlicerAstroVolumeReader.h>

// AstroVolume MRML includes
#include <vtkMRMLAstroTwoDAxesDisplayableManager.h>

// Segment editor effects includes
#include "qSlicerSegmentEditorEffectFactory.h"
#include "qSlicerSegmentEditorAstroCloudLassoEffect.h"
#include "qSlicerSegmentEditorAstroContoursEffect.h"

// MRML includes
#include <vtkMRMLApplicationLogic.h>
#include <vtkMRMLColorTableNode.h>
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

  qSlicerPresetComboBox* PresetsNodeComboBox =
      volumeRenderingWidget->findChild<qSlicerPresetComboBox*>
      (QString("PresetsNodeComboBox"));
  PresetsNodeComboBox->setEnabled(false);

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
    vtkMRMLNode * foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLSliceNode");
    defaultNode.TakeReference(foo);
    this->mrmlScene()->AddDefaultNode(defaultNode);
    }
  vtkMRMLSliceNode * defaultSliceNode = vtkMRMLSliceNode::SafeDownCast(defaultNode);
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
    qCritical() << "qSlicerAstroVolumeModule::setup() : selectionNode not found!";
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

  // register Astro Editor Effects in the Segmentation Editor
  qSlicerSegmentEditorEffectFactory::instance()->registerEffect(new qSlicerSegmentEditorAstroCloudLassoEffect());
  qSlicerSegmentEditorEffectFactory::instance()->registerEffect(new qSlicerSegmentEditorAstroContoursEffect());


  // Add Astro 2D color functions
  vtkNew<vtkMRMLColorTableNode> HeatColorTableNode;
  HeatColorTableNode->SetType(vtkMRMLColorTableNode::User);
  HeatColorTableNode->SetName("Heat");
  HeatColorTableNode->SetDescription("A scale from red to yellow.");
  HeatColorTableNode->SetNumberOfColors(256);

  // Red component
  vtkNew<vtkLookupTable> RedLookupTable;
  RedLookupTable->SetNumberOfTableValues(85);
  RedLookupTable->SetTableRange(0, 85);
  RedLookupTable->SetHueRange(0, 0);
  RedLookupTable->SetSaturationRange(1,1);
  RedLookupTable->SetValueRange(0.,1);
  RedLookupTable->SetRampToLinear();
  RedLookupTable->ForceBuild();

  // Green component
  vtkNew<vtkLookupTable> GreenLookupTable;
  GreenLookupTable->SetNumberOfTableValues(256);
  GreenLookupTable->SetTableRange(0, 256);
  GreenLookupTable->SetHueRange(0.333, 0.333);
  GreenLookupTable->SetSaturationRange(1,1);
  GreenLookupTable->SetValueRange(0.,1);
  GreenLookupTable->SetRampToLinear();
  GreenLookupTable->ForceBuild();

  // Blue component
  vtkNew<vtkLookupTable> BlueLookupTable;
  BlueLookupTable->SetNumberOfTableValues(85);
  BlueLookupTable->SetTableRange(0, 85);
  BlueLookupTable->SetHueRange(0.667, 0.667);
  BlueLookupTable->SetSaturationRange(1,1);
  BlueLookupTable->SetValueRange(0,1);
  BlueLookupTable->SetRampToLinear();
  BlueLookupTable->ForceBuild();

  for (int ii = 0; ii < 85; ii++)
    {
    double RGBRed[3], RGBGreen[3], RGBA[4];
    RedLookupTable->GetTableValue(ii, RGBRed);
    GreenLookupTable->GetTableValue(ii, RGBGreen);

    RGBA[0] = RGBRed[0];
    RGBA[1] = RGBGreen[1];
    RGBA[2] = 0.;
    RGBA[3] = 1.;
    HeatColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }
  for (int ii = 85; ii < 171; ii++)
    {
    double RGBGreen[3], RGBA[4];
    GreenLookupTable->GetTableValue(ii, RGBGreen);

    RGBA[0] = 1.;
    RGBA[1] = RGBGreen[1];
    RGBA[2] = 0.;
    RGBA[3] = 1.;
    HeatColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }
  for (int ii = 171; ii < 256; ii++)
    {
    double RGBGreen[3], RGBBlue[3], RGBA[4];
    GreenLookupTable->GetTableValue(ii, RGBGreen);
    BlueLookupTable->GetTableValue(ii - 171, RGBBlue);

    RGBA[0] = 1.;
    RGBA[1] = RGBGreen[1];
    RGBA[2] = RGBBlue[2];
    RGBA[3] = 1.;
    HeatColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }

  HeatColorTableNode->SetNamesFromColors();
  this->mrmlScene()->AddNode(HeatColorTableNode.GetPointer());

  vtkNew<vtkMRMLColorTableNode> RonekersColorTableNode;
  RonekersColorTableNode->SetType(vtkMRMLColorTableNode::User);
  RonekersColorTableNode->SetName("Ronekers");
  RonekersColorTableNode->SetDescription("Discrete rainbow color function. Very useful to visualize for Astro HI datasets.");
  RonekersColorTableNode->SetNumberOfColors(256);

  for (int ii = 0; ii < 28; ii++)
    {
    double RGBA[4];

    RGBA[0] = 0.199;
    RGBA[1] = 0.199;
    RGBA[2] = 0.199;
    RGBA[3] = 1.;
    RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }
  for (int ii = 28; ii < 53; ii++)
    {
    double RGBA[4];

    RGBA[0] = 0.473;
    RGBA[1] = 0.;
    RGBA[2] = 0.606;
    RGBA[3] = 1.;
    RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }
  for (int ii = 53; ii < 78; ii++)
    {
    double RGBA[4];

    RGBA[0] = 0.;
    RGBA[1] = 0.;
    RGBA[2] = 0.781;
    RGBA[3] = 1.;
    RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }
  for (int ii = 78; ii < 103; ii++)
    {
    double RGBA[4];

    RGBA[0] = 0.371;
    RGBA[1] = 0.652;
    RGBA[2] = 0.922;
    RGBA[3] = 1.;
    RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }
  for (int ii = 103; ii < 128; ii++)
    {
    double RGBA[4];

    RGBA[0] = 0.;
    RGBA[1] = 0.566;
    RGBA[2] = 0.;
    RGBA[3] = 1.;
    RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }
  for (int ii = 128; ii < 153; ii++)
    {
    double RGBA[4];

    RGBA[0] = 0.;
    RGBA[1] = 0.961;
    RGBA[2] = 0.;
    RGBA[3] = 1.;
    RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }
  for (int ii = 153; ii < 178; ii++)
    {
    double RGBA[4];

    RGBA[0] = 1.;
    RGBA[1] = 1.;
    RGBA[2] = 0.;
    RGBA[3] = 1.;
    RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }
  for (int ii = 178; ii < 203; ii++)
    {
    double RGBA[4];

    RGBA[0] = 1.;
    RGBA[1] = 0.691;
    RGBA[2] = 0.;
    RGBA[3] = 1.;
    RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }
  for (int ii = 203; ii < 228; ii++)
    {
    double RGBA[4];

    RGBA[0] = 1.;
    RGBA[1] = 0.;
    RGBA[2] = 0.;
    RGBA[3] = 1.;
    RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }
  for (int ii = 228; ii < 256; ii++)
    {
    double RGBA[4];

    RGBA[0] = 1.;
    RGBA[1] = 1.;
    RGBA[2] = 1.;
    RGBA[3] = 1.;
    RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }

  RonekersColorTableNode->SetNamesFromColors();
  this->mrmlScene()->AddNode(RonekersColorTableNode.GetPointer());

  vtkNew<vtkMRMLColorTableNode> VelocityFieldColorTableNode;
  VelocityFieldColorTableNode->SetType(vtkMRMLColorTableNode::User);
  VelocityFieldColorTableNode->SetName("Velocity Field");
  VelocityFieldColorTableNode->SetDescription("A scale from blue to red.");
  VelocityFieldColorTableNode->SetNumberOfColors(256);

  // Red component
  RedLookupTable->SetNumberOfTableValues(128);
  RedLookupTable->SetTableRange(0, 128);
  RedLookupTable->SetHueRange(0, 0);
  RedLookupTable->SetSaturationRange(1,1);
  RedLookupTable->SetValueRange(0.,1);
  RedLookupTable->SetRampToLinear();
  RedLookupTable->ForceBuild();

  // Green component
  GreenLookupTable->SetNumberOfTableValues(128);
  GreenLookupTable->SetTableRange(0, 128);
  GreenLookupTable->SetHueRange(0.333, 0.333);
  GreenLookupTable->SetSaturationRange(1,1);
  GreenLookupTable->SetValueRange(0.,1);
  GreenLookupTable->SetRampToLinear();
  GreenLookupTable->ForceBuild();

  // Blue component
  BlueLookupTable->SetNumberOfTableValues(128);
  BlueLookupTable->SetTableRange(0, 128);
  BlueLookupTable->SetHueRange(0.667, 0.667);
  BlueLookupTable->SetSaturationRange(1,1);
  BlueLookupTable->SetValueRange(0,1);
  BlueLookupTable->SetRampToLinear();
  BlueLookupTable->ForceBuild();

  for (int ii = 0; ii < 128; ii++)
    {
    double RGBBlue[3], RGBGreen[3], RGBA[4];
    int blueIndex = 128 - ii;
    BlueLookupTable->GetTableValue(blueIndex, RGBBlue);
    GreenLookupTable->GetTableValue(ii, RGBGreen);

    if (ii == 0)
      {
      RGBA[0] = 0.;
      RGBA[1] = 0.;
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      }
    else
      {
      RGBA[0] = 0.;
      RGBA[1] = RGBGreen[1];
      RGBA[2] = RGBBlue[2];
      RGBA[3] = 1.;
      }
    VelocityFieldColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }
  for (int ii = 128; ii < 256; ii++)
    {
    double RGBGreen[3], RGBRed[3], RGBA[4];
    int redIndex = ii - 128;
    int greeIndex = 128 - (redIndex);
    GreenLookupTable->GetTableValue(greeIndex, RGBGreen);
    RedLookupTable->GetTableValue(redIndex, RGBRed);

    RGBA[0] = RGBRed[0];
    RGBA[1] = RGBGreen[1];
    RGBA[2] = 0.;
    RGBA[3] = 1.;
    VelocityFieldColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
    }

  VelocityFieldColorTableNode->SetNamesFromColors();
  this->mrmlScene()->AddNode(VelocityFieldColorTableNode.GetPointer());

  // Remove unwanted 2D color functions
  vtkSmartPointer<vtkCollection> ColorTableNodeCol =
    vtkSmartPointer<vtkCollection>::Take(
      this->mrmlScene()->GetNodesByClass("vtkMRMLColorTableNode"));
  for (int ii = 0; ii < ColorTableNodeCol->GetNumberOfItems(); ii++)
    {
    vtkMRMLColorTableNode* tempColorTableNode = vtkMRMLColorTableNode::SafeDownCast
            (ColorTableNodeCol->GetItemAsObject(ii));
    if (!tempColorTableNode)
      {
      continue;
      }
    if (!strcmp(tempColorTableNode->GetName(), "Grey") ||
        !strcmp(tempColorTableNode->GetName(), "Heat") ||
        !strcmp(tempColorTableNode->GetName(), "Ronekers") ||
        !strcmp(tempColorTableNode->GetName(), "Velocity Field"))
      {
      tempColorTableNode->SetAttribute("SlicerAstro.AddFunctions", "on");
      tempColorTableNode->SetAttribute("SlicerAstro.Reverse", "off");
      tempColorTableNode->SetAttribute("SlicerAstro.Inverse", "off");
      tempColorTableNode->SetAttribute("SlicerAstro.Log", "off");
      continue;
      }
    if (!strcmp(tempColorTableNode->GetName(), "GenericColors") ||
        !strcmp(tempColorTableNode->GetName(), "MediumChartColors"))
      {
      vtkNew<vtkMRMLColorTableNode> tempTableNode;
      tempTableNode->Copy(tempColorTableNode);
      this->mrmlScene()->RemoveNode(tempColorTableNode);
      tempTableNode->SetAttribute("SlicerAstro.AddFunctions", "off");
      this->mrmlScene()->AddNode(tempTableNode.GetPointer());
      continue;
      }
    if (!strcmp(tempColorTableNode->GetName(), "Rainbow"))
      {
      this->mrmlScene()->RemoveNode(tempColorTableNode);
      vtkNew<vtkMRMLColorTableNode> RainbowTableNode;
      RainbowTableNode->SetName("Rainbow");
      RainbowTableNode->SetType(vtkMRMLColorTableNode::User);
      RainbowTableNode->SetDescription("Goes from red to purple, passing through the colors of the rainbow in between.");
      RainbowTableNode->SetNumberOfColors(256);
      RainbowTableNode->GetLookupTable()->SetNumberOfTableValues(256);
      RainbowTableNode->GetLookupTable()->SetHueRange(0., 0.8);
      RainbowTableNode->GetLookupTable()->SetSaturationRange(1,1);
      RainbowTableNode->GetLookupTable()->SetValueRange(1,1);
      RainbowTableNode->GetLookupTable()->SetRampToLinear();
      RainbowTableNode->GetLookupTable()->ForceBuild();
      RainbowTableNode->SetColor(0, 0, 0, 0);
      RainbowTableNode->SetNamesFromColors();
      RainbowTableNode->SetAttribute("SlicerAstro.AddFunctions", "on");
      RainbowTableNode->SetAttribute("SlicerAstro.Reverse", "off");
      RainbowTableNode->SetAttribute("SlicerAstro.Inverse", "off");
      RainbowTableNode->SetAttribute("SlicerAstro.Log", "off");
      this->mrmlScene()->AddNode(RainbowTableNode.GetPointer());
      continue;
      }
    this->mrmlScene()->RemoveNode(tempColorTableNode);
    }

  vtkSmartPointer<vtkCollection> ProceduralColorTableNodeCol =
    vtkSmartPointer<vtkCollection>::Take(
      this->mrmlScene()->GetNodesByClass("vtkMRMLProceduralColorNode"));
  for (int ii = 0; ii < ProceduralColorTableNodeCol->GetNumberOfItems(); ii++)
    {
    vtkMRMLProceduralColorNode* tempProceduralColorTableNode = vtkMRMLProceduralColorNode::SafeDownCast
            (ProceduralColorTableNodeCol->GetItemAsObject(ii));
    if (!tempProceduralColorTableNode)
      {
      continue;
      }
    if (!strcmp(tempProceduralColorTableNode->GetName(), "RandomIntegers"))
      {
      vtkNew<vtkMRMLProceduralColorNode> tempTableNode;
      tempTableNode->Copy(tempProceduralColorTableNode);
      this->mrmlScene()->RemoveNode(tempProceduralColorTableNode);
      tempTableNode->SetAttribute("SlicerAstro.AddFunctions", "off");
      this->mrmlScene()->AddNode(tempTableNode.GetPointer());
      continue;
      }
    this->mrmlScene()->RemoveNode(tempProceduralColorTableNode);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModule::onMRMLUnitNodeIntensityModified(vtkObject* sender)
{
  Q_D(qSlicerAstroVolumeModule);
  if(!sender && d->volumeRendering)
    {
    return;
    }

  vtkMRMLUnitNode* unitNode = vtkMRMLUnitNode::SafeDownCast(sender);

  qSlicerVolumeRenderingModuleWidget*  volumeRenderingWidget =
      dynamic_cast<qSlicerVolumeRenderingModuleWidget*>
         (d->volumeRendering->widgetRepresentation());

  if(volumeRenderingWidget)
    {  
    ctkVTKVolumePropertyWidget* volumePropertyWidget =
        volumeRenderingWidget->findChild<ctkVTKVolumePropertyWidget*>
        (QString("VolumeProperty"));

    ctkVTKScalarsToColorsWidget* opacityWidget =
        volumePropertyWidget->findChild<ctkVTKScalarsToColorsWidget*>
        (QString("ScalarOpacityWidget"));

    QDoubleSpinBox* XSpinBox = opacityWidget->findChild<QDoubleSpinBox*>
        (QString("XSpinBox"));

    XSpinBox->setDecimals(unitNode->GetPrecision());

    ctkDoubleRangeSlider* XRangeSlider = opacityWidget->findChild<ctkDoubleRangeSlider*>
        (QString("XRangeSlider"));

    XRangeSlider->setRange(unitNode->GetMinimumValue(), unitNode->GetMaximumValue());
    XRangeSlider->setSingleStep((unitNode->GetMaximumValue() - unitNode->GetMinimumValue()) / 100.);

    opacityWidget->setXRange(unitNode->GetMinimumValue(), unitNode->GetMaximumValue());

    ctkVTKScalarsToColorsWidget* colorWidget =
        volumePropertyWidget->findChild<ctkVTKScalarsToColorsWidget*>
        (QString("ScalarColorWidget"));

    QDoubleSpinBox* XSpinBox1 = colorWidget->findChild<QDoubleSpinBox*>
        (QString("XSpinBox"));

    XSpinBox1->setDecimals(unitNode->GetPrecision());

    ctkDoubleRangeSlider* XRangeSlider1 = colorWidget->findChild<ctkDoubleRangeSlider*>
        (QString("XRangeSlider"));

    XRangeSlider1->setRange(unitNode->GetMinimumValue(), unitNode->GetMaximumValue());
    XRangeSlider1->setSingleStep((unitNode->GetMaximumValue() - unitNode->GetMinimumValue()) / 100.);

    colorWidget->setXRange(unitNode->GetMinimumValue(), unitNode->GetMaximumValue());
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
    << "vtkMRMLVolumeNode"
    << "vtkMRMLVolumeDisplayNode";
}
