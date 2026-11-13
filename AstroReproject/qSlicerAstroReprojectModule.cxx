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

// VTK includes
#include <vtkWeakPointer.h>


// AstroReproject includes
#include "qSlicerAstroReprojectModule.h"
#include "qSlicerAstroReprojectModuleWidget.h"


//-----------------------------------------------------------------------------
/// \ingroup SlicerAstro_QtModules_AstroReproject
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
  return tr(
    "<p>Reprojection, resampling and mosaicking of astronomical images and data cubes: the input volumes are "
    "put on a new grid of sky pixels and channels.</p>"
    "<ul>"
    "<li><b>Output grid</b>: sky pixels from a reference volume, from the input (optionally with larger or "
    "smaller pixels), covering all the inputs (mosaic), their common area, or defined manually (celestial "
    "frame, projection, centre, pixel size, rotation, size); channels from the input, a reference volume, "
    "wider or narrower, covering the inputs, or manual (radio/optical velocity, frequency or wavelength, "
    "spectral frame, rest frequency). Celestial frames (ICRS, FK5, FK4, Galactic, Supergalactic) and "
    "spectral frames (topocentric to CMB) are converted.</li>"
    "<li><b>Resampling</b>: nearest, bilinear, bicubic and Lanczos interpolation, or flux-conserving area "
    "average and exact spherical overlap; channel interpolation or flux-conserving channel overlap.</li>"
    "<li><b>Resolution</b>: the inputs can be smoothed to their common beam or to a target beam.</li>"
    "<li><b>Mosaics</b>: several inputs are combined (mean, or linear mosaic with primary beam and noise "
    "weights), with optional weight, sensitivity and Sault-weighted volumes.</li>"
    "</ul>"
    "<p>Open the <i>Quick guide</i> section of the module for the common tasks; each option has a tooltip "
    "and the text under each choice explains the selected option.</p>"
    "<p>The WCS of the FITS headers must be correct (celestial and spectral frames, rest frequency, "
    "observation date and position for topocentric data).</p>");
}

//-----------------------------------------------------------------------------
QString qSlicerAstroReprojectModule::acknowledgementText()const
{
  return "This module was developed by Davide Punzo. <br>"
         "This work was supported by ERC grant nr. 291531, "
         "and Slicer community. <br>";
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
QStringList qSlicerAstroReprojectModule::associatedNodeTypes() const
{
  return QStringList() << "vtkMRMLAstroReprojectParametersNode";
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

  // Register logic
  vtkSlicerAstroReprojectLogic* AstroReprojectLogic =
    vtkSlicerAstroReprojectLogic::SafeDownCast(this->logic());

  // Register AstroVolume module logic
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

  AstroReprojectLogic->SetAstroVolumeLogic(astroVolumeLogic);
  // reprojection service of the AstroVolume module (contours of other data)
  vtkWeakPointer<vtkSlicerAstroReprojectLogic> weakLogic = AstroReprojectLogic;
  astroVolumeLogic->SetReprojectFunction(
    [weakLogic](vtkMRMLAstroReprojectParametersNode* pnode, std::string& error) -> bool
    {
    if (!weakLogic)
      {
      error = "the AstroReproject logic was deleted";
      return false;
      }
    const bool success = weakLogic->Reproject(pnode);
    error = weakLogic->GetLastError();
    return success;
    });
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
