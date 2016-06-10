// Qt includes
#include <QApplication>
#include <QDoubleSpinBox>
#include <QtPlugin>
#include <QSettings>

//VTK includes
#include <vtkCollection.h>
#include <vtkInstantiator.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

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

// AstroVolume Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// AstroVolume QtModule includes
#include <qSlicerAstroVolumeLayoutSliceViewFactory.h>
#include <qSlicerAstroVolumeModule.h>
#include <qSlicerAstroVolumeModuleWidget.h>
#include <qSlicerAstroVolumeReader.h>

// AstroVolume MRML includes
#include <vtkMRMLAstroTwoDAxesDisplayableManager.h>

// MRML includes
#include <vtkMRMLApplicationLogic.h>
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

vtkInstantiatorNewMacro(vtkMRMLAstroTwoDAxesDisplayableManager)

//-----------------------------------------------------------------------------
Q_EXPORT_PLUGIN2(qSlicerAstroVolumeModule, qSlicerAstroVolumeModule);

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
  moduleContributors << QString("Jean-Christophe Fillion-Robin (Kitware)");
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
  moduleDependencies << "Data" << "Volumes" << "VolumeRendering";
  return moduleDependencies;
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModule::setup()
{
  Q_D(qSlicerAstroVolumeModule);
  this->Superclass::setup();

  d->app = qSlicerApplication::application();

  // Register the IO module for loading AstroVolumes as a variant of fits files
  if(d->app->mrmlScene())
    {
    this->setMRMLScene(d->app->mrmlScene());
    }

  qSlicerAbstractCoreModule* volumes = d->app->moduleManager()->module("Volumes");
  if (volumes)
    {
    vtkSlicerVolumesLogic* volumesLogic =
      vtkSlicerVolumesLogic::SafeDownCast(volumes->logic());
    vtkSlicerAstroVolumeLogic* logic =
      vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
    if (volumesLogic && logic)
      {
      logic->RegisterArchetypeVolumeNodeSetFactory( volumesLogic );
      }
    qSlicerCoreIOManager* ioManager = d->app->coreIOManager();
    ioManager->registerIO(new qSlicerAstroVolumeReader(volumesLogic,this));
    ioManager->registerIO(new qSlicerNodeWriter(
      "AstroVolume", QString("AstroVolumeFile"),
      QStringList() << "vtkMRMLVolumeNode", true, this));
    }

  //removing Volumes action from mainWindow interface:
  //for the moment I just disable the widget creation,
  //i.e. the action is till present on mainWindows.
  //For the moment is satisfactory.
  volumes->setWidgetRepresentationCreationEnabled(false);

  //modify precision in VolumeRenderingWidgets
  d->volumeRendering = d->app->moduleManager()->module("VolumeRendering");

  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
    this->mrmlScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if(selectionNode)
    {
    vtkMRMLUnitNode* unitNodeIntensity = selectionNode->GetUnitNode("intensity");
    this->qvtkConnect(unitNodeIntensity, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLUnitNodeIntensityModified(vtkObject*)));
    this->onMRMLUnitNodeIntensityModified(unitNodeIntensity);
    }

  qSlicerVolumeRenderingModuleWidget*  volumeRenderingWidget =
      dynamic_cast<qSlicerVolumeRenderingModuleWidget*>
         (d->volumeRendering->widgetRepresentation());

  if(volumeRenderingWidget)
    {
    qSlicerPresetComboBox* PresetsNodeComboBox =
        volumeRenderingWidget->findChild<qSlicerPresetComboBox*>
        (QString("PresetsNodeComboBox"));
    PresetsNodeComboBox->setEnabled(false);
    }

  // modify velocity and frequancy nodes
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNodeLength = selectionNode->GetUnitNode("length");
    unitNodeLength->SetMaximumValue(180.);
    unitNodeLength->SetMinimumValue(-180.);
    unitNodeLength->SetDisplayCoefficient(1.);
    unitNodeLength->SetPrefix("");
    unitNodeLength->SetSuffix("\xB0");
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
    unitNodeVelocity->SetPrecision(3);
    unitNodeVelocity->SetAttribute("DisplayHint","");
    selectionNode->SetUnitNodeID("velocity", unitNodeVelocity->GetID());

    vtkMRMLUnitNode* unitNodeFrequency = selectionNode->GetUnitNode("frequency");
    unitNodeFrequency->SetDisplayCoefficient(0.000001);
    unitNodeFrequency->SetPrefix("");
    unitNodeFrequency->SetPrecision(2);
    unitNodeFrequency->SetSuffix("MHz");
    unitNodeFrequency->SetAttribute("DisplayHint","");
    selectionNode->SetUnitNodeID("frequency", unitNodeFrequency->GetID());
    }

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
  if(layoutNode)
    {
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
    }

  // unregister RulerDisplayableManager
  vtkMRMLThreeDViewDisplayableManagerFactory::GetInstance()->
    UnRegisterDisplayableManager("vtkMRMLRulerDisplayableManager");
  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->
    UnRegisterDisplayableManager("vtkMRMLRulerDisplayableManager");

  // register AstroTwoDAxesDisplayableManager
  vtkInstantiator::RegisterInstantiator("vtkMRMLAstroTwoDAxesDisplayableManager",
                                        vtkInstantiatorvtkMRMLAstroTwoDAxesDisplayableManagerNew);

  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->
    RegisterDisplayableManager("vtkMRMLAstroTwoDAxesDisplayableManager");
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
