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

// MRML includes
#include "vtkMRMLCoreTestingMacros.h"
#include "vtkMRMLAstroSmoothingParametersNode.h"

int vtkMRMLAstroSmoothingParametersNodeTest1(int , char * [] )
{
  vtkNew< vtkMRMLAstroSmoothingParametersNode > node1;

  EXERCISE_BASIC_OBJECT_METHODS( node1.GetPointer() );

  TEST_SET_GET_STRING(node1.GetPointer(), InputVolumeNodeID);
  TEST_SET_GET_STRING(node1.GetPointer(), OutputVolumeNodeID);
  TEST_SET_GET_STRING(node1.GetPointer(), Mode);
  TEST_SET_GET_STRING(node1.GetPointer(), MasksCommand);

  TEST_SET_GET_INT(node1.GetPointer(), OutputSerial, 1);
  TEST_SET_GET_INT(node1.GetPointer(), Status, 0);
  TEST_SET_GET_INT(node1.GetPointer(), Filter, 2);
  TEST_SET_GET_INT(node1.GetPointer(), Hardware, 0);
  TEST_SET_GET_INT(node1.GetPointer(), Cores, 0);

  TEST_SET_GET_BOOLEAN(node1.GetPointer(), Link);
  TEST_SET_GET_BOOLEAN(node1.GetPointer(), AutoRun);

  TEST_SET_GET_INT(node1.GetPointer(), Accuracy, 20);

  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), TimeStep, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), K, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), ParameterX, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), ParameterY, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), ParameterZ, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), Rx, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), Ry, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), Rz, 0., 10.);

  TEST_SET_GET_INT(node1.GetPointer(), KernelLengthX, 0);
  TEST_SET_GET_INT(node1.GetPointer(), KernelLengthY, 0);
  TEST_SET_GET_INT(node1.GetPointer(), KernelLengthZ, 0);

  return EXIT_SUCCESS;
}
