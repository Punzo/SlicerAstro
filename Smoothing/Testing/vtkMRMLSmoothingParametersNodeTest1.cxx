// MRML includes
#include "vtkMRMLCoreTestingMacros.h"
#include "vtkMRMLSmoothingParametersNode.h"

int vtkMRMLSmoothingParametersNodeTest1(int , char * [] )
{
  vtkNew< vtkMRMLSmoothingParametersNode > node1;

  EXERCISE_BASIC_OBJECT_METHODS( node1.GetPointer() );

  std::string InputVolumeNodeID = "WEIN069";
  std::string OutputVolumeNodeID = "WEIN069_filtered";

  TEST_SET_GET_STRING(node1.GetPointer(), InputVolumeNodeID);
  TEST_SET_GET_STRING(node1.GetPointer(), OutputVolumeNodeID);

  return EXIT_SUCCESS;
}
