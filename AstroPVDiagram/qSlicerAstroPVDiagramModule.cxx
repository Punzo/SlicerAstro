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
#include <QDebug>
#include <QtPlugin>

// Slicer includes
#include <qSlicerCoreApplication.h>
#include <qSlicerModuleManager.h>

// Logic includes
#include <vtkSlicerAstroMomentMapsLogic.h>
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroPVDiagramLogic.h>

// AstroPVDiagram includes
#include "qSlicerAstroPVDiagramModule.h"
#include "qSlicerAstroPVDiagramModuleWidget.h"

//-----------------------------------------------------------------------------
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <QtPlugin>
Q_EXPORT_PLUGIN2(qSlicerAstroPVDiagramModule, qSlicerAstroPVDiagramModule);
#endif

//-----------------------------------------------------------------------------
/// \ingroup SlicerAstro_QtModules_AstroPVDiagram
class qSlicerAstroPVDiagramModulePrivate
{
public:
  qSlicerAstroPVDiagramModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerAstroPVDiagramModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroPVDiagramModulePrivate::qSlicerAstroPVDiagramModulePrivate()
{
}

//-----------------------------------------------------------------------------
// qSlicerAstroPVDiagramModule methods

//-----------------------------------------------------------------------------
qSlicerAstroPVDiagramModule::qSlicerAstroPVDiagramModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroPVDiagramModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroPVDiagramModule::~qSlicerAstroPVDiagramModule()
{
}

//-----------------------------------------------------------------------------
QString qSlicerAstroPVDiagramModule::helpText()const
{
  return "The AstroPVDiagram module generates a Position Velocity (PV) diagram. "
         "The user can manually place fiducials in the 3D view or on a moment map "
         "in the red 2D view. Such fiducials will be connected and a PV Diagram from "
         "this selection will be generated.";
}

//-----------------------------------------------------------------------------
QString qSlicerAstroPVDiagramModule::acknowledgementText()const
{
  return "This module was developed by Davide Punzo. <br>"
         "This work was supported by ERC grant nr. 291531, "
         "and Slicer community. <br>";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroPVDiagramModule::contributors()const
{
  QStringList moduleContributors;
  moduleContributors << QString("Davide Punzo (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Thijs van der Hulst (Kapteyn Astronomical Institute)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerAstroPVDiagramModule::icon()const
{
  return QIcon(":/Icons/AstroPVDiagram.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroPVDiagramModule::categories()const
{
  return QStringList() << "Astronomy" << "Registration";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroPVDiagramModule::dependencies()const
{
  return QStringList() << "AstroVolume" << "AstroMomentMaps" << "Markups";
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModule::setup()
{
  this->Superclass::setup();
  vtkSlicerAstroPVDiagramLogic* AstroPVDiagramLogic =
    vtkSlicerAstroPVDiagramLogic::SafeDownCast(this->logic());

  qSlicerAbstractCoreModule* astroVolumeModule =
    qSlicerCoreApplication::application()->moduleManager()->module("AstroVolume");
  if (!astroVolumeModule)
    {
    qCritical() << "AstroVolume module is not found";
    return;
    }
  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(astroVolumeModule->logic());
  AstroPVDiagramLogic->SetAstroVolumeLogic(astroVolumeLogic);

  qSlicerAbstractCoreModule* astroMomentMapsModule =
    qSlicerCoreApplication::application()->moduleManager()->module("AstroMomentMaps");
  if (!astroMomentMapsModule)
    {
    qCritical() << "astroMomentMaps module is not found";
    return;
    }
  vtkSlicerAstroMomentMapsLogic* astroMomentMapsLogic =
    vtkSlicerAstroMomentMapsLogic::SafeDownCast(astroMomentMapsModule->logic());
  AstroPVDiagramLogic->SetAstroMomentMapsLogic(astroMomentMapsLogic);
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation * qSlicerAstroPVDiagramModule::createWidgetRepresentation()
{
  return new qSlicerAstroPVDiagramModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerAstroPVDiagramModule::createLogic()
{
  return vtkSlicerAstroPVDiagramLogic::New();
}
