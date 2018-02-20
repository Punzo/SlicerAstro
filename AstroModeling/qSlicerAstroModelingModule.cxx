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
#include <vtkSlicerAstroModelingLogic.h>
#include <vtkSlicerMarkupsLogic.h>

// AstroModeling includes
#include "qSlicerAstroModelingModule.h"
#include "qSlicerAstroModelingModuleWidget.h"

//-----------------------------------------------------------------------------
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <QtPlugin>
Q_EXPORT_PLUGIN2(qSlicerAstroModelingModule, qSlicerAstroModelingModule);
#endif

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroModeling
class qSlicerAstroModelingModulePrivate
{
public:
  qSlicerAstroModelingModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerAstroModelingModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroModelingModulePrivate::qSlicerAstroModelingModulePrivate()
{
}

//-----------------------------------------------------------------------------
// qSlicerAstroModelingModule methods

//-----------------------------------------------------------------------------
qSlicerAstroModelingModule::qSlicerAstroModelingModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroModelingModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroModelingModule::~qSlicerAstroModelingModule()
{
}

//-----------------------------------------------------------------------------
QString qSlicerAstroModelingModule::helpText()const
{
  return "AstroModeling module fits tilted-ring models on a Volume using 3DBarolo"
         " (http://editeodoro.github.io/Bbarolo/)";
}

//-----------------------------------------------------------------------------
QString qSlicerAstroModelingModule::acknowledgementText()const
{
  return "This module was developed by Davide Punzo. <br>"
         "This work was supported by ERC grant nr. 291531, "
         "and Slicer community. <br>"
         " Special thanks to Enrico di Teodoro (Australian National University)"
         " for support regarding 3DBarolo wrapping in SlicerAstro. <br>";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroModelingModule::contributors()const
{
  QStringList moduleContributors;
  moduleContributors << QString("Davide Punzo (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Thijs van der Hulst (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Jos Roerdink (Johann Bernoulli Institute)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerAstroModelingModule::icon()const
{
  return QIcon(":/Icons/AstroModeling.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroModelingModule::categories()const
{
  return QStringList() << "Astronomy" << "Quantification";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroModelingModule::dependencies()const
{
  return QStringList() << "AstroVolume" << "Markups" << "Segmentations" ;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModule::setup()
{
  this->Superclass::setup();
  vtkSlicerAstroModelingLogic* AstroModelingLogic =
    vtkSlicerAstroModelingLogic::SafeDownCast(this->logic());

  qSlicerAbstractCoreModule* astroVolumeModule =
    qSlicerCoreApplication::application()->moduleManager()->module("AstroVolume");
  if (!astroVolumeModule)
    {
    qCritical() << "AstroVolume module is not found";
    return;
    }
  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(astroVolumeModule->logic());
  if (!astroVolumeLogic)
    {
    qCritical() << "AstroVolume logic is not found";
    return;
    }
  AstroModelingLogic->SetAstroVolumeLogic(astroVolumeLogic);

  qSlicerAbstractCoreModule* markupsModule =
    qSlicerCoreApplication::application()->moduleManager()->module("Markups");
  if (!markupsModule)
    {
    qCritical() << "Markups module is not found";
    return;
    }
  vtkSlicerMarkupsLogic* markupsLogic =
    vtkSlicerMarkupsLogic::SafeDownCast(markupsModule->logic());
  if (!markupsLogic)
    {
    qCritical() << "Markups logic is not found";
    return;
    }
  AstroModelingLogic->SetMarkupsLogic(markupsLogic);
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation * qSlicerAstroModelingModule::createWidgetRepresentation()
{
  return new qSlicerAstroModelingModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerAstroModelingModule::createLogic()
{
  return vtkSlicerAstroModelingLogic::New();
}
