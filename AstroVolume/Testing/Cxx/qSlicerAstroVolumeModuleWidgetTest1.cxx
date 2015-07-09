// Qt includes
#include <QTimer>
#include <QWidget>

// SlicerQt includes
#include <qSlicerAbstractModuleRepresentation.h>
#include <qSlicerApplication.h>

// AstroVolume includes
#include "qSlicerAstroVolumeModule.h"
#include "vtkSlicerVolumesLogic.h"

// VTK includes
#include <vtkNew.h>

// ITK includes
#include <itkConfigure.h>
#include <itkFactoryRegistration.h>

//-----------------------------------------------------------------------------
int qSlicerAstroVolumeModuleWidgetTest1( int argc, char * argv[] )
{
  itk::itkFactoryRegistration();

  qSlicerApplication app(argc, argv);

  if (argc < 2)
    {
    std::cerr << "Usage: qSlicerAstroVolumeModuleWidgetTest1 volumeName [-I]" << std::endl;
    return EXIT_FAILURE;
    }

  qSlicerAstroVolumeModule module;
  module.initialize(0);

  vtkNew<vtkMRMLScene> scene;
  vtkNew<vtkSlicerVolumesLogic> VolumesLogic;
  VolumesLogic->SetMRMLScene(scene.GetPointer());

  vtkMRMLVolumeNode* volumeNode = VolumesLogic->AddArchetypeVolume(argv[1], "volume");
  if (!volumeNode)
    {
    std::cerr << "Bad volume file:" << argv[1] << std::endl;
    return EXIT_FAILURE;
    }
  module.setMRMLScene(scene.GetPointer());

  dynamic_cast<QWidget*>(module.widgetRepresentation())->show();

  if (argc < 3 || QString(argv[2]) != "-I")
    {
    QTimer::singleShot(200, &app, SLOT(quit()));
    }
  return app.exec();
}
