// Qt includes
#include <QtPlugin>
#include <QApplication>
#include <QDoubleSpinBox>

//VTK includes
#include <vtkCollection.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkInstantiator.h>

//CTK includes
#include <ctkVTKVolumePropertyWidget.h>
#include <ctkVTKScalarsToColorsWidget.h>
#include <ctkRangeWidget.h>
#include <ctkDoubleRangeSlider.h>

// Slicer includes
#include <vtkSlicerVolumesLogic.h>
#include <vtkSlicerUnitsLogic.h>
#include <vtkMRMLSliceLogic.h>

// SlicerQt includes
#include <qSlicerCoreApplication.h>
#include <qSlicerIOManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerNodeWriter.h>
#include <qSlicerUtils.h>
#include <qSlicerModuleManager.h>
#include <qSlicerScriptedLoadableModuleWidget.h>
#include <qSlicerApplication.h>
#include <qSlicerLayoutManager.h>
#include <vtkSlicerConfigure.h>
#include <qMRMLLayoutManager.h>
#include <qMRMLLayoutManager_p.h>
#include <qMRMLVolumeThresholdWidget.h>
#include <qMRMLSliceWidget.h>
#include <qSlicerVolumeRenderingModuleWidget.h>
#include <qSlicerPresetComboBox.h>
#include <qMRMLSliceView.h>

// AstroVolume Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// AstroVolume QtModule includes
#include <qSlicerAstroVolumeReader.h>
#include <qSlicerAstroVolumeModule.h>
#include <qSlicerAstroVolumeModuleWidget.h>
#include <qSlicerAstroVolumeLayoutSliceViewFactory.h>

// AstroVolume MRML includes
#include <vtkMRMLAstroTwoDAxesDisplayableManager.h>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLApplicationLogic.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLViewNode.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLThreeDViewDisplayableManagerFactory.h>
#include <vtkMRMLSliceViewDisplayableManagerFactory.h>

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
    "<center><table border=\"0\"><tr>"
    "<td><img src=\":://Logos/kapteyn.png\" alt\"Kapteyn Astronomical Institute\"></td>"
    "</tr></table></center>"
    "This work was supported by ERC grant nr. 29153Fikl and the Slicer "
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
    vtkSmartPointer<vtkSlicerVolumesLogic> volumesLogic =
      vtkSlicerVolumesLogic::SafeDownCast(volumes->logic());
    vtkSmartPointer<vtkSlicerAstroVolumeLogic> logic =
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

  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
    this->mrmlScene()->GetNthNodeByClass(0, "vtkMRMLSelectionNode"));
  if(selectionNode)
    {
    vtkSmartPointer<vtkMRMLUnitNode> unitNode = selectionNode->GetUnitNode("intensity");
    this->qvtkConnect(unitNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLUnitModified(vtkObject*)));
    this->onMRMLUnitModified(unitNode);
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
  /*
  //set the Slice Factory
  qMRMLLayoutSliceViewFactory* mrmlSliceViewFactory =
    qobject_cast<qMRMLLayoutSliceViewFactory*>(
    d->app->layoutManager()->mrmlViewFactory("vtkMRMLSliceNode"));

  qSlicerAstroVolumeLayoutSliceViewFactory* astroSliceViewFactory =
    new qSlicerAstroVolumeLayoutSliceViewFactory(d->app->layoutManager());
  astroSliceViewFactory->setSliceLogics(mrmlSliceViewFactory->sliceLogics());

  d->app->layoutManager()->unregisterViewFactory(mrmlSliceViewFactory);
  d->app->layoutManager()->registerViewFactory(astroSliceViewFactory);*/

  //unregister RulerDisplayableManager
  vtkMRMLThreeDViewDisplayableManagerFactory::GetInstance()->
    UnRegisterDisplayableManager("vtkMRMLRulerDisplayableManager");
  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->
    UnRegisterDisplayableManager("vtkMRMLRulerDisplayableManager");

  //register AstroTwoDAxesDisplayableManager
  vtkInstantiator::RegisterInstantiator("vtkMRMLAstroTwoDAxesDisplayableManager",
                                        vtkInstantiatorvtkMRMLAstroTwoDAxesDisplayableManagerNew);

  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->
    RegisterDisplayableManager("vtkMRMLAstroTwoDAxesDisplayableManager");
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModule::onMRMLUnitModified(vtkObject* sender)
{
  Q_D(qSlicerAstroVolumeModule);
  if(!sender && d->volumeRendering)
    {
    return;
    }

  vtkSmartPointer<vtkMRMLUnitNode> unitNode = vtkMRMLUnitNode::SafeDownCast(sender);

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
