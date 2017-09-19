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
  and was supported through the European Research Consil grant nr. 291531.

==============================================================================*/

// Qt includes
#include <QDebug>
#include <QtPlugin>

// Slicer includes
#include <qSlicerCoreApplication.h>
#include <qSlicerModuleManager.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroMomentMapsLogic.h>

// AstroMomentMaps includes
#include "qSlicerAstroMomentMapsModule.h"
#include "qSlicerAstroMomentMapsModuleWidget.h"

//-----------------------------------------------------------------------------
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <QtPlugin>
Q_EXPORT_PLUGIN2(qSlicerAstroMomentMapsModule, qSlicerAstroMomentMapsModule);
#endif

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroMomentMaps
class qSlicerAstroMomentMapsModulePrivate
{
public:
  qSlicerAstroMomentMapsModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerAstroMomentMapsModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroMomentMapsModulePrivate::qSlicerAstroMomentMapsModulePrivate()
{
}

//-----------------------------------------------------------------------------
// qSlicerAstroMomentMapsModule methods

//-----------------------------------------------------------------------------
qSlicerAstroMomentMapsModule::qSlicerAstroMomentMapsModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroMomentMapsModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroMomentMapsModule::~qSlicerAstroMomentMapsModule()
{
}

//-----------------------------------------------------------------------------
QString qSlicerAstroMomentMapsModule::helpText()const
{
  return "AstroMomentMaps module generates Moment Maps.";
}

//-----------------------------------------------------------------------------
QString qSlicerAstroMomentMapsModule::acknowledgementText()const
{
  return "This module was developed by Davide Punzo. <br>"
         "This work was supported by ERC grant nr. 291531, "
         "and Slicer community. <br>";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroMomentMapsModule::contributors()const
{
  QStringList moduleContributors;
  moduleContributors << QString("Davide Punzo (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Thijs van der Hulst (Kapteyn Astronomical Institute)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerAstroMomentMapsModule::icon()const
{
  return QIcon(":/Icons/AstroMomentMaps.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroMomentMapsModule::categories()const
{
  return QStringList() << "Astronomy";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroMomentMapsModule::dependencies()const
{
  return QStringList() << "AstroVolume" << "Segmentations" ;
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModule::setup()
{
  this->Superclass::setup();
  vtkSlicerAstroMomentMapsLogic* AstroMomentMapsLogic =
    vtkSlicerAstroMomentMapsLogic::SafeDownCast(this->logic());

  qSlicerAbstractCoreModule* astroVolumeModule =
    qSlicerCoreApplication::application()->moduleManager()->module("AstroVolume");
  if (!astroVolumeModule)
    {
    qCritical() << "AstroVolume module is not found";
    return;
    }
  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(astroVolumeModule->logic());
  AstroMomentMapsLogic->SetAstroVolumeLogic(astroVolumeLogic);
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation * qSlicerAstroMomentMapsModule::createWidgetRepresentation()
{
  return new qSlicerAstroMomentMapsModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerAstroMomentMapsModule::createLogic()
{
  return vtkSlicerAstroMomentMapsLogic::New();
}
