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
#include <vtkSlicerAstroReprojectLogic.h>


// AstroReproject includes
#include "qSlicerAstroReprojectModule.h"
#include "qSlicerAstroReprojectModuleWidget.h"

//-----------------------------------------------------------------------------
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <QtPlugin>
Q_EXPORT_PLUGIN2(qSlicerAstroReprojectModule, qSlicerAstroReprojectModule);
#endif

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroReproject
class qSlicerAstroReprojectModulePrivate
{
public:
  qSlicerAstroReprojectModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerAstroReprojectModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroReprojectModulePrivate::qSlicerAstroReprojectModulePrivate()
{
}

//-----------------------------------------------------------------------------
// qSlicerAstroReprojectModule methods

//-----------------------------------------------------------------------------
qSlicerAstroReprojectModule::qSlicerAstroReprojectModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroReprojectModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroReprojectModule::~qSlicerAstroReprojectModule()
{
}

//-----------------------------------------------------------------------------
QString qSlicerAstroReprojectModule::helpText()const
{
  return "AstroReproject module filters a Volume using several techniques. "
         "The algorithms are optmized for Astronomical Neutral Hydrogen (HI) data.";
}

//-----------------------------------------------------------------------------
QString qSlicerAstroReprojectModule::acknowledgementText()const
{
  return  "The AstroReproject module implements image reprojection (resampling) methods "
          "for astronomical datasets. Specifically, the methods have been designed to "
          "reproject the spatial axis, (e.g., 2D images or 3D datacubes over 2D images)."
          "For example in the case of datacubes, the algorithm reprojects each slice "
          "(spatial celestial axis) of the datacube with the reference data (treating each slice as independent). "
          "However, overlaying two 3D astronomical datasets requires also to resample the velocity axes, "
          "which is not implemented in this module. "
          "The AstroReproject module requires that the WCS information contained in the fits header are correct. ";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroReprojectModule::contributors()const
{
  QStringList moduleContributors;
  moduleContributors << QString("Davide Punzo (Kapteyn Astronomical Institute)");
  moduleContributors << QString("Thijs van der Hulst (Kapteyn Astronomical Institute)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerAstroReprojectModule::icon()const
{
  return QIcon(":/Icons/AstroReproject.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroReprojectModule::categories()const
{
  return QStringList() << "Astronomy" << "Registration";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroReprojectModule::dependencies()const
{
  return QStringList() << "AstroVolume" ;
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModule::setup()
{
  this->Superclass::setup();
  vtkSlicerAstroReprojectLogic* AstroReprojectLogic =
    vtkSlicerAstroReprojectLogic::SafeDownCast(this->logic());

  qSlicerAbstractCoreModule* astroVolumeModule =
    qSlicerCoreApplication::application()->moduleManager()->module("AstroVolume");
  if (!astroVolumeModule)
    {
    qCritical() << "AstroVolume module is not found";
    return;
    }
  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(astroVolumeModule->logic());
  AstroReprojectLogic->SetAstroVolumeLogic(astroVolumeLogic);
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation * qSlicerAstroReprojectModule::createWidgetRepresentation()
{
  return new qSlicerAstroReprojectModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerAstroReprojectModule::createLogic()
{
  return vtkSlicerAstroReprojectLogic::New();
}
