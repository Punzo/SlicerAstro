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
#include "vtkMRMLAstroStatisticsParametersNode.h"

int vtkMRMLAstroStatisticsParametersNodeTest1(int , char * [] )
{
  vtkNew< vtkMRMLAstroStatisticsParametersNode > node1;

  EXERCISE_BASIC_OBJECT_METHODS( node1.GetPointer() );

  std::string InputVolumeNodeID = "WEIN069";

  TEST_SET_GET_STRING(node1.GetPointer(), InputVolumeNodeID);

  return EXIT_SUCCESS;
}
