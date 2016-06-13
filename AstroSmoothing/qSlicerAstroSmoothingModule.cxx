// Qt includes
#include <QDebug>
#include <QtPlugin>

// Slicer includes
#include <qSlicerCoreApplication.h>
#include <qSlicerModuleManager.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroSmoothingLogic.h>


// AstroSmoothing includes
#include "qSlicerAstroSmoothingModule.h"
#include "qSlicerAstroSmoothingModuleWidget.h"

//-----------------------------------------------------------------------------
Q_EXPORT_PLUGIN2(qSlicerAstroSmoothingModule, qSlicerAstroSmoothingModule);

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroSmoothing
class qSlicerAstroSmoothingModulePrivate
{
public:
  qSlicerAstroSmoothingModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerAstroSmoothingModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroSmoothingModulePrivate::qSlicerAstroSmoothingModulePrivate()
{
}

//-----------------------------------------------------------------------------
// qSlicerAstroSmoothingModule methods

//-----------------------------------------------------------------------------
qSlicerAstroSmoothingModule::qSlicerAstroSmoothingModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroSmoothingModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroSmoothingModule::~qSlicerAstroSmoothingModule()
{
}

//-----------------------------------------------------------------------------
QString qSlicerAstroSmoothingModule::helpText()const
{
  return "AstroSmoothing module filters a Volume using several techniques. "
         "The algorithms are optmized for Astronomical Neutral Hydoren (HI) data.";
}

//-----------------------------------------------------------------------------
QString qSlicerAstroSmoothingModule::acknowledgementText()const
{
  return "This module was developed by Davide Punzo. <br>"
         "This work was supported by ERC grant nr. 291531, "
         "and Slicer community. <br>"
         " Special thanks to Steve Pieper (Isomics) for support"
         " regarding the GPU (OpenGL) implementation of the filters.";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroSmoothingModule::contributors()const
{
  QStringList moduleContributors;
  moduleContributors << QString("Davide Punzo (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Thijs van der Hulst (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Jos Roerdink (Johann Bernoulli Institute)");
  moduleContributors << QString("Jean-Christophe Fillion-Robin (Kitware)");
  moduleContributors << QString("Steve Pieper (Isomics).");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerAstroSmoothingModule::icon()const
{
  return QIcon(":/Icons/AstroSmoothing.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroSmoothingModule::categories()const
{
  return QStringList() << "Astronomy" << "Filtering";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroSmoothingModule::dependencies()const
{
  return QStringList() << "AstroVolume";
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModule::setup()
{
  this->Superclass::setup();
  vtkSlicerAstroSmoothingLogic* AstroSmoothingLogic =
    vtkSlicerAstroSmoothingLogic::SafeDownCast(this->logic());

  qSlicerAbstractCoreModule* astroVolumeModule =
    qSlicerCoreApplication::application()->moduleManager()->module("AstroVolume");
  if (!astroVolumeModule)
    {
    qCritical() << "AstroVolume module is not found";
    return;
    }
  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(astroVolumeModule->logic());
  AstroSmoothingLogic->SetAstroVolumeLogic(astroVolumeLogic);
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation * qSlicerAstroSmoothingModule::createWidgetRepresentation()
{
  return new qSlicerAstroSmoothingModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerAstroSmoothingModule::createLogic()
{
  return vtkSlicerAstroSmoothingLogic::New();
}
