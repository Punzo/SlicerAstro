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
#include "vtkMRMLAstroPVSliceParametersNode.h"

int vtkMRMLAstroPVSliceParametersNodeTest1(int , char * [] )
{
  vtkNew< vtkMRMLAstroPVSliceParametersNode > node1;

  EXERCISE_BASIC_OBJECT_METHODS( node1.GetPointer() );

  TEST_SET_GET_STRING(node1.GetPointer(), InputVolumeNodeID);
  TEST_SET_GET_STRING(node1.GetPointer(), MomentMapNodeID);
  TEST_SET_GET_STRING(node1.GetPointer(), LineNodeID);

  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), LineAngle, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), LineOldAngle, 0., 10.);

  return EXIT_SUCCESS;
}
