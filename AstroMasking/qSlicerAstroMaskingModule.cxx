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
#include <vtkSlicerAstroMaskingLogic.h>

// AstroMasking includes
#include "qSlicerAstroMaskingModule.h"
#include "qSlicerAstroMaskingModuleWidget.h"

//-----------------------------------------------------------------------------
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <QtPlugin>
Q_EXPORT_PLUGIN2(qSlicerAstroMaskingModule, qSlicerAstroMaskingModule);
#endif

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroMasking
class qSlicerAstroMaskingModulePrivate
{
public:
  qSlicerAstroMaskingModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerAstroMaskingModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroMaskingModulePrivate::qSlicerAstroMaskingModulePrivate()
{
}

//-----------------------------------------------------------------------------
// qSlicerAstroMaskingModule methods

//-----------------------------------------------------------------------------
qSlicerAstroMaskingModule::qSlicerAstroMaskingModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroMaskingModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroMaskingModule::~qSlicerAstroMaskingModule()
{
}

//-----------------------------------------------------------------------------
QString qSlicerAstroMaskingModule::helpText()const
{
  return "AstroMasking module generates Moment Maps.";
}

//-----------------------------------------------------------------------------
QString qSlicerAstroMaskingModule::acknowledgementText()const
{
  return "This module was developed by Davide Punzo. <br>"
         "This work was supported by ERC grant nr. 291531, "
         "and Slicer community. <br>";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroMaskingModule::contributors()const
{
  QStringList moduleContributors;
  moduleContributors << QString("Davide Punzo (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Thijs van der Hulst (Kapteyn Astronomical Institute)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerAstroMaskingModule::icon()const
{
  return QIcon(":/Icons/AstroMasking.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroMaskingModule::categories()const
{
  return QStringList() << "Astronomy";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroMaskingModule::dependencies()const
{
  return QStringList() << "AstroVolume" << "Segmentations" ;
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModule::setup()
{
  this->Superclass::setup();
  vtkSlicerAstroMaskingLogic* AstroMaskingLogic =
    vtkSlicerAstroMaskingLogic::SafeDownCast(this->logic());

  qSlicerAbstractCoreModule* astroVolumeModule =
    qSlicerCoreApplication::application()->moduleManager()->module("AstroVolume");
  if (!astroVolumeModule)
    {
    qCritical() << "AstroVolume module is not found";
    return;
    }
  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(astroVolumeModule->logic());
  AstroMaskingLogic->SetAstroVolumeLogic(astroVolumeLogic);
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation * qSlicerAstroMaskingModule::createWidgetRepresentation()
{
  return new qSlicerAstroMaskingModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerAstroMaskingModule::createLogic()
{
  return vtkSlicerAstroMaskingLogic::New();
}
