// Qt includes
#include <QtPlugin>

// SlicerQt includes
#include "qSlicerAstroWelcomeModule.h"
#include "qSlicerAstroWelcomeModuleWidget.h"


//-----------------------------------------------------------------------------
Q_EXPORT_PLUGIN2(qSlicerAstroWelcomeModule, qSlicerAstroWelcomeModule);

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_SlicerAstroWelcome
class qSlicerAstroWelcomeModulePrivate
{
public:
};

//-----------------------------------------------------------------------------
qSlicerAstroWelcomeModule::qSlicerAstroWelcomeModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroWelcomeModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroWelcomeModule::~qSlicerAstroWelcomeModule()
{
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroWelcomeModule::categories()const
{
  return QStringList() << "Astronomy"<<"";
}

//-----------------------------------------------------------------------------
QIcon qSlicerAstroWelcomeModule::icon()const
{
  return QIcon(":/Icons/SlicerAstroWelcome.png");
}

//-----------------------------------------------------------------------------
QString qSlicerAstroWelcomeModule::helpText()const
{
  return QString();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroWelcomeModule::acknowledgementText()const
{
  return "This module was developed by Davide Punzo. <br>"
         "This work was supported by ERC grant nr. 291531 and the Slicer "
         "Community. See <a href=\"http://www.slicer.org\">http://www.slicer.org"
         "</a> for details.<br>"
         "Special thanks to Steve Pieper (Isomics), Jean-Christophe Fillion-Robin (Kitware)"
         "and Andras Lasso (PerkLab, Queen's).";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroWelcomeModule::contributors()const
{
  QStringList moduleContributors;
  moduleContributors << QString("Davide Punzo (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Thijs van der Hulst (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Jos Roerdink (Johann Bernoulli Institute)");
  moduleContributors << QString("Jean-Christophe Fillion-Robin (Kitware).");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation* qSlicerAstroWelcomeModule::createWidgetRepresentation()
{
  return new qSlicerAstroWelcomeModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerAstroWelcomeModule::createLogic()
{
  return 0;
}
