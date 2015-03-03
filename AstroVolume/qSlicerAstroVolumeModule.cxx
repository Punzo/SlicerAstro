/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Jean-Christophe Fillion-Robin, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

// Qt includes
#include <QtPlugin>

// SlicerQt includes
#include <qSlicerCoreApplication.h>
#include <qSlicerIOManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerNodeWriter.h>

// AstroVolume Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// AstroVolume QTModule includes
#include "qSlicerAstroVolumeReader.h"
#include "qSlicerAstroVolumeModule.h"
#include "qSlicerAstroVolumeModuleWidget.h"

// MRML Logic includes
#include <vtkMRMLColorLogic.h>

// MRML includes
#include <vtkMRMLScene.h>

// SubjectHierarchy Plugins includes
//#include "qSlicerSubjectHierarchyPluginHandler.h"
//#include "qSlicerSubjectHierarchyAstroVolumePlugin.h"


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
    "<td><img src=\":Logos/kapteyn.png\" alt\"Kapteyn Astronomical Institute\"></td>"
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
  moduleDependencies << "Colors" << "Units";
  return moduleDependencies;
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModule::setup()
{
  this->Superclass::setup();
  vtkSlicerAstroVolumeLogic* AstroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
  qSlicerAbstractCoreModule* colorsModule =
    qSlicerCoreApplication::application()->moduleManager()->module("Colors");
  if (colorsModule)
    {
    vtkMRMLColorLogic* colorLogic =
      vtkMRMLColorLogic::SafeDownCast(colorsModule->logic());
    AstroVolumeLogic->SetColorLogic(colorLogic);
    }

  qSlicerCoreIOManager* ioManager =
    qSlicerCoreApplication::application()->coreIOManager();
  ioManager->registerIO(new qSlicerAstroVolumeReader(AstroVolumeLogic,this));
  ioManager->registerIO(new qSlicerNodeWriter(
    "AstroVolume", QString("VolumeFile"),
    QStringList() << "vtkMRMLVolumeNode", this));

  // Register Subject Hierarchy core plugins
  //qSlicerSubjectHierarchyPluginHandler::instance()->registerPlugin(new qSlicerSubjectHierarchyAstroVolumePlugin());
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
