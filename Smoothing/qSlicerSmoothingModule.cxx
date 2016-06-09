// Qt includes
#include <QDebug>
#include <QtPlugin>

// Slicer includes
#include <qSlicerCoreApplication.h>
#include <qSlicerModuleManager.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerSmoothingLogic.h>


// Smoothing includes
#include "qSlicerSmoothingModule.h"
#include "qSlicerSmoothingModuleWidget.h"

//-----------------------------------------------------------------------------
Q_EXPORT_PLUGIN2(qSlicerSmoothingModule, qSlicerSmoothingModule);

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_Smoothing
class qSlicerSmoothingModulePrivate
{
public:
  qSlicerSmoothingModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerSmoothingModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerSmoothingModulePrivate::qSlicerSmoothingModulePrivate()
{
}

//-----------------------------------------------------------------------------
// qSlicerSmoothingModule methods

//-----------------------------------------------------------------------------
qSlicerSmoothingModule::qSlicerSmoothingModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerSmoothingModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerSmoothingModule::~qSlicerSmoothingModule()
{
}

//-----------------------------------------------------------------------------
QString qSlicerSmoothingModule::helpText()const
{
  return "Smoothing module filters a Volume using several techniques. "
         "The algorithms are optmized for Astronomical Neutral Hydoren (HI) data.";
}

//-----------------------------------------------------------------------------
QString qSlicerSmoothingModule::acknowledgementText()const
{
  return "This module was developed by Davide Punzo. "
         "This work was supported by ERC grant nr. 291531, "
         "and Slicer community."
         " Special thanks to Steve Pieper (Isomics) for support"
         " regarding the GPU (OpenGL) implementation of the filters.";
}

//-----------------------------------------------------------------------------
QStringList qSlicerSmoothingModule::contributors()const
{
  QStringList moduleContributors;
  moduleContributors << QString("Davide Punzo (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Jean-Christophe Fillion-Robin (Kitware)");
  moduleContributors << QString("Steve Pieper (Isomics)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerSmoothingModule::icon()const
{
  return QIcon(":/Icons/Smoothing.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerSmoothingModule::categories()const
{
  return QStringList() << "Astronomy" << "Filtering";
}

//-----------------------------------------------------------------------------
QStringList qSlicerSmoothingModule::dependencies()const
{
  return QStringList() << "AstroVolume";
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModule::setup()
{
  this->Superclass::setup();

  vtkSlicerSmoothingLogic* smoothingLogic =
    vtkSlicerSmoothingLogic::SafeDownCast(this->logic());

  qSlicerAbstractCoreModule* astroVolumeModule =
    qSlicerCoreApplication::application()->moduleManager()->module("AstroVolume");
  if (astroVolumeModule)
    {
    vtkSlicerAstroVolumeLogic* astroVolumeLogic =
      vtkSlicerAstroVolumeLogic::SafeDownCast(astroVolumeModule->logic());
    smoothingLogic->SetAstroVolumeLogic(astroVolumeLogic);
    }
  else
    {
    qWarning() << "AstroVolume module is not found";
    }
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation * qSlicerSmoothingModule::createWidgetRepresentation()
{
  return new qSlicerSmoothingModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerSmoothingModule::createLogic()
{
  return vtkSlicerSmoothingLogic::New();
}
