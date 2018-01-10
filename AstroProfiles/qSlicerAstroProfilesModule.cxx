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
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroProfilesLogic.h>

// AstroProfiles includes
#include "qSlicerAstroProfilesModule.h"
#include "qSlicerAstroProfilesModuleWidget.h"

//-----------------------------------------------------------------------------
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <QtPlugin>
Q_EXPORT_PLUGIN2(qSlicerAstroProfilesModule, qSlicerAstroProfilesModule);
#endif

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroProfiles
class qSlicerAstroProfilesModulePrivate
{
public:
  qSlicerAstroProfilesModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerAstroProfilesModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroProfilesModulePrivate::qSlicerAstroProfilesModulePrivate()
{
}

//-----------------------------------------------------------------------------
// qSlicerAstroProfilesModule methods

//-----------------------------------------------------------------------------
qSlicerAstroProfilesModule::qSlicerAstroProfilesModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroProfilesModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroProfilesModule::~qSlicerAstroProfilesModule()
{
}

//-----------------------------------------------------------------------------
QString qSlicerAstroProfilesModule::helpText()const
{
  return "AstroProfiles module generates Moment Maps.";
}

//-----------------------------------------------------------------------------
QString qSlicerAstroProfilesModule::acknowledgementText()const
{
  return "This module was developed by Davide Punzo. <br>"
         "This work was supported by ERC grant nr. 291531, "
         "and Slicer community. <br>";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroProfilesModule::contributors()const
{
  QStringList moduleContributors;
  moduleContributors << QString("Davide Punzo (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Thijs van der Hulst (Kapteyn Astronomical Institute)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerAstroProfilesModule::icon()const
{
  return QIcon(":/Icons/AstroProfiles.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroProfilesModule::categories()const
{
  return QStringList() << "Astronomy";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroProfilesModule::dependencies()const
{
  return QStringList() << "AstroVolume" << "Segmentations" ;
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModule::setup()
{
  this->Superclass::setup();
  vtkSlicerAstroProfilesLogic* AstroProfilesLogic =
    vtkSlicerAstroProfilesLogic::SafeDownCast(this->logic());

  qSlicerAbstractCoreModule* astroVolumeModule =
    qSlicerCoreApplication::application()->moduleManager()->module("AstroVolume");
  if (!astroVolumeModule)
    {
    qCritical() << "AstroVolume module is not found";
    return;
    }
  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(astroVolumeModule->logic());
  AstroProfilesLogic->SetAstroVolumeLogic(astroVolumeLogic);
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation * qSlicerAstroProfilesModule::createWidgetRepresentation()
{
  return new qSlicerAstroProfilesModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerAstroProfilesModule::createLogic()
{
  return vtkSlicerAstroProfilesLogic::New();
}
