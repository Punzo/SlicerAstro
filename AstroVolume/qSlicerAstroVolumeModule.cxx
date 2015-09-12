// Qt includes
#include <QtPlugin>

// Slicer includes
#include <vtkSlicerVolumesLogic.h>
#include <vtkSlicerUnitsLogic.h>

// SlicerQt includes
#include <qSlicerCoreApplication.h>
#include <qSlicerIOManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerNodeWriter.h>
#include <qSlicerUtils.h>
#include <qSlicerModuleManager.h>
#include <qSlicerScriptedLoadableModuleWidget.h>
#include <qSlicerApplication.h>
#include <vtkSlicerConfigure.h>

// AstroVolume Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// AstroVolume QTModule includes
#include "qSlicerAstroVolumeReader.h"
#include "qSlicerAstroVolumeModule.h"
#include "qSlicerAstroVolumeModuleWidget.h"


// MRML includes
#include <vtkMRMLScene.h>

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
  return QStringList() << "Astronomy";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroVolumeModule::dependencies() const
{
  QStringList moduleDependencies;
  moduleDependencies << "Volumes";
  return moduleDependencies;
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModule::setup()
{
  this->Superclass::setup();

  // Register the IO module for loading AstroVolumes as a variant of fits files
  qSlicerAbstractCoreModule* volumes = qSlicerApplication::application()->moduleManager()->module("Volumes");
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
    qSlicerCoreIOManager* ioManager =
      qSlicerCoreApplication::application()->coreIOManager();
    ioManager->registerIO(new qSlicerAstroVolumeReader(volumesLogic,this));
    ioManager->registerIO(new qSlicerNodeWriter(
      "AstroVolume", QString("AstroVolumeFile"),
      QStringList() << "vtkMRMLAstroVolumeNode", true, this));
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
