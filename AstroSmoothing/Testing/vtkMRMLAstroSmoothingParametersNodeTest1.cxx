// MRML includes
#include "vtkMRMLCoreTestingMacros.h"
#include "vtkMRMLAstroSmoothingParametersNode.h"

int vtkMRMLAstroSmoothingParametersNodeTest1(int , char * [] )
{
  vtkNew< vtkMRMLAstroSmoothingParametersNode > node1;

  EXERCISE_BASIC_OBJECT_METHODS( node1.GetPointer() );

  std::string InputVolumeNodeID = "WEIN069";
  std::string OutputVolumeNodeID = "WEIN069_filtered";

  TEST_SET_GET_STRING(node1.GetPointer(), InputVolumeNodeID);
  TEST_SET_GET_STRING(node1.GetPointer(), OutputVolumeNodeID);

  return EXIT_SUCCESS;
}
