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
#include "vtkMRMLAstroModelingParametersNode.h"

int vtkMRMLAstroModelingParametersNodeTest1(int , char * [] )
{
  vtkNew< vtkMRMLAstroModelingParametersNode > node1;

  EXERCISE_BASIC_OBJECT_METHODS( node1.GetPointer() );

  TEST_SET_GET_STRING(node1.GetPointer(), InputVolumeNodeID);
  TEST_SET_GET_STRING(node1.GetPointer(), OutputVolumeNodeID);
  TEST_SET_GET_STRING(node1.GetPointer(), ResidualVolumeNodeID);
  TEST_SET_GET_STRING(node1.GetPointer(), MaskVolumeNodeID);

  TEST_SET_GET_BOOLEAN(node1.GetPointer(), MaskActive);

  TEST_SET_GET_STRING(node1.GetPointer(), Mode);
  TEST_SET_GET_STRING(node1.GetPointer(), Normalize);

  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), RadSep, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), XCenter, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), YCenter, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), SystemicVelocity, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), RotationVelocity, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), RadialVelocity, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), VelocityDispersion, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), Inclination, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), InclinationError, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), PositionAngle, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), PositionAngleError, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), ScaleHeight, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), RadSep, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), Distance, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), ColumnDensity, 0., 10.);

  TEST_SET_GET_BOOLEAN(node1.GetPointer(), PositionAngleFit);
  TEST_SET_GET_BOOLEAN(node1.GetPointer(), RotationVelocityFit);
  TEST_SET_GET_BOOLEAN(node1.GetPointer(), RadialVelocityFit);
  TEST_SET_GET_BOOLEAN(node1.GetPointer(), VelocityDispersionFit);
  TEST_SET_GET_BOOLEAN(node1.GetPointer(), InclinationFit);
  TEST_SET_GET_BOOLEAN(node1.GetPointer(), XCenterFit);
  TEST_SET_GET_BOOLEAN(node1.GetPointer(), YCenterFit);
  TEST_SET_GET_BOOLEAN(node1.GetPointer(), SystemicVelocityFit);
  TEST_SET_GET_BOOLEAN(node1.GetPointer(), ScaleHeightFit);

  TEST_SET_GET_INT(node1.GetPointer(), LayerType, 0);
  TEST_SET_GET_INT(node1.GetPointer(), FittingFunction, 1);
  TEST_SET_GET_INT(node1.GetPointer(), WeightingFunction, 1);
  TEST_SET_GET_INT(node1.GetPointer(), NumberOfClounds, 0);

  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), CloudsColumnDensity, 0., 10.);

  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), XPosCenterIJK, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), YPosCenterIJK, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), XPosCenterRAS, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), YPosCenterRAS, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), ZPosCenterRAS, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), PVPhi, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), YellowRotOldValue, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), YellowRotValue, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), GreenRotOldValue, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), GreenRotValue, 0., 10.);
  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), PVPhi, 0., 10.);

  TEST_SET_GET_INT(node1.GetPointer(), Operation, 0);
  TEST_SET_GET_INT(node1.GetPointer(), OutputSerial, 1);
  TEST_SET_GET_INT(node1.GetPointer(), Status, 0);

  TEST_SET_GET_BOOLEAN(node1.GetPointer(), FitSuccess);
  TEST_SET_GET_BOOLEAN(node1.GetPointer(), ForceSliceUpdate);

  TEST_SET_GET_INT(node1.GetPointer(), NumberOfRings, 0);

  TEST_SET_GET_DOUBLE_RANGE(node1.GetPointer(), ContourLevel, 0., 10.);

  return EXIT_SUCCESS;
}
