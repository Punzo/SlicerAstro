// Qt includes
#include <QtPlugin>
#include <QApplication>
#include <QDoubleSpinBox>

//VTK includes
#include <vtkCollection.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

//CTK includes
#include <ctkVTKVolumePropertyWidget.h>
#include <ctkVTKScalarsToColorsWidget.h>

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
#include "qSlicerVolumeRenderingModuleWidget.h"

// AstroVolume Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// AstroVolume QtModule includes
#include "qSlicerAstroVolumeReader.h"
#include "qSlicerAstroVolumeModule.h"
#include "qSlicerAstroVolumeModuleWidget.h"
#include "qSlicerAstroVolumeLayoutSliceViewFactory.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLApplicationLogic.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLSliceNode.h>

//-----------------------------------------------------------------------------
Q_EXPORT_PLUGIN2(qSlicerAstroVolumeModule, qSlicerAstroVolumeModule);

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroVolume
class qSlicerAstroVolumeModulePrivate
{
public:
};

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModule::qSlicerAstroVolumeModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroVolumeModulePrivate)
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
    "This work was supported by ERC grant .... and the Slicer "
    "Community. See <a href=\"http://www.slicer.org\">http://www.slicer.org"
    "</a> for details.<br>");
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
  this->Superclass::setup();

  qSlicerApplication* app = qSlicerApplication::application();
  // Register the IO module for loading AstroVolumes as a variant of fits files
  qSlicerAbstractCoreModule* volumes = app->moduleManager()->module("Volumes");
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
    qSlicerCoreIOManager* ioManager = app->coreIOManager();
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
  qSlicerAbstractCoreModule* volumeRendering = app->moduleManager()->module("VolumeRendering");
  if (volumeRendering)
    {
    this->modifyRenderingWidgets(volumeRendering);
    }

  //set the Slice Factory
  qMRMLLayoutSliceViewFactory* mrmlSliceViewFactory =
    qobject_cast<qMRMLLayoutSliceViewFactory*>(
    app->layoutManager()->mrmlViewFactory("vtkMRMLSliceNode"));

  qSlicerAstroVolumeLayoutSliceViewFactory* astroSliceViewFactory =
    new qSlicerAstroVolumeLayoutSliceViewFactory(app->layoutManager());
  astroSliceViewFactory->setSliceLogics(mrmlSliceViewFactory->sliceLogics());

  app->layoutManager()->unregisterViewFactory(mrmlSliceViewFactory);
  app->layoutManager()->registerViewFactory(astroSliceViewFactory);
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModule::modifyRenderingWidgets(qSlicerAbstractCoreModule* volumeRendering)
{
  qSlicerVolumeRenderingModuleWidget*  volumeRenderingWidget =
      dynamic_cast<qSlicerVolumeRenderingModuleWidget*>
         (volumeRendering->widgetRepresentation());
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

    XSpinBox->setDecimals(6);

    ctkVTKScalarsToColorsWidget* colorWidget =
        volumePropertyWidget->findChild<ctkVTKScalarsToColorsWidget*>
        (QString("ScalarColorWidget"));

    QDoubleSpinBox* XSpinBox1 = colorWidget->findChild<QDoubleSpinBox*>
        (QString("XSpinBox"));

    XSpinBox1->setDecimals(6);
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
