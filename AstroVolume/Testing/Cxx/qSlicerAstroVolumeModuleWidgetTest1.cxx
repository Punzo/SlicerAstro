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
#include <QTimer>
#include <QWidget>

// SlicerQt includes
#include <qSlicerAbstractModuleRepresentation.h>
#include <qSlicerApplication.h>

// AstroVolume includes
#include "qSlicerAstroVolumeModule.h"
#include "vtkSlicerAstroVolumeLogic.h"
#include "vtkSlicerVolumesLogic.h"

// VTK includes
#include <vtkNew.h>

//-----------------------------------------------------------------------------
int qSlicerAstroVolumeModuleWidgetTest1( int argc, char * argv[] )
{
  qSlicerApplication app(argc, argv);

  if (argc < 2)
    {
    std::cerr << "Usage: qSlicerAstroVolumeModuleWidgetTest1 volumeName [-I]" << std::endl;
    return EXIT_FAILURE;
    }

  qSlicerAstroVolumeModule module;

  vtkNew<vtkMRMLScene> scene;
  vtkNew<vtkSlicerVolumesLogic> VolumesLogic;
  VolumesLogic->SetMRMLScene(scene.GetPointer());
  vtkNew<vtkSlicerAstroVolumeLogic> astroVolumesLogic;

  astroVolumesLogic->RegisterArchetypeVolumeNodeSetFactory(VolumesLogic.GetPointer());

  vtkMRMLVolumeNode* volumeNode = VolumesLogic->AddArchetypeVolume(argv[1], "volume");
  if (!volumeNode)
    {
    std::cerr << "Bad volume file:" << argv[1] << std::endl;
    return EXIT_FAILURE;
    }
  module.setMRMLScene(scene.GetPointer());

  if (argc < 3 || QString(argv[2]) != "-I")
    {
    QTimer::singleShot(200, &app, SLOT(quit()));
    }
  return app.exec();
}
