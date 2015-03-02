/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

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

// AstroVolume includes
#include "qSlicerAstroVolumeModule.h"
#include "qSlicerAstroVolumeModuleWidget.h"

// MRML Logic includes
#include <vtkMRMLColorLogic.h>

// MRML includes
#include <vtkMRMLScene.h>

//-----------------------------------------------------------------------------
Q_EXPORT_PLUGIN2(qSlicerAstroVolumeModule, qSlicerAstroVolumeModule);

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_ExtensionTemplate
class qSlicerAstroVolumeModulePrivate
{
public:
  qSlicerAstroVolumeModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerAstroVolumeModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModulePrivate::qSlicerAstroVolumeModulePrivate()
{
}

//-----------------------------------------------------------------------------
// qSlicerAstroVolumeModule methods

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
QString qSlicerAstroVolumeModule::helpText() const
{
  return "This is a a volume module for astronomical data";
}

//-----------------------------------------------------------------------------
QString qSlicerAstroVolumeModule::acknowledgementText() const
{
  return "This work was partially funded by ERC Grant Agreement nr. 291531";
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroVolumeModule::contributors() const
{
  QStringList moduleContributors;
  moduleContributors << QString("Davide Punzo (Kapteyn Astronomical Institute)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerAstroVolumeModule::icon() const
{
  return QIcon(":/Icons/AstroVolume.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroVolumeModule::categories() const
{
  return QStringList() << "Astro";
}


//-----------------------------------------------------------------------------
QStringList qSlicerAstroVolumeModule::dependencies() const
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModule::setup()
{

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
