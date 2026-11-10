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

// VTK includes
#include <vtkDoubleArray.h>

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

  // The rotation angles are doubles in the scene file
  const char* attributes[] = {"Rz", "30.5", nullptr};
  node1->ReadXMLAttributes(attributes);
  CHECK_DOUBLE(node1->GetRz(), 30.5);

  // Rotation convention of the 3D Gaussian kernel: K(r) = G(Rx Ry Rz r).
  // A kernel elongated along X with Rz = 30 degrees has its major axis
  // along (cos 30, -sin 30, 0).
  vtkNew<vtkMRMLAstroSmoothingParametersNode> node2;
  node2->SetFilter(1);
  node2->SetAccuracy(3);
  node2->SetParameterX(6.);
  node2->SetParameterY(2.);
  node2->SetParameterZ(2.);
  node2->SetRx(0.);
  node2->SetRy(0.);
  node2->SetRz(30.);
  node2->SetGaussianKernels();
  vtkDoubleArray* kernel = node2->GetGaussianKernel3D();
  const int nx = static_cast<int>(node2->GetKernelLengthX());
  const int ny = static_cast<int>(node2->GetKernelLengthY());
  const int nz = static_cast<int>(node2->GetKernelLengthZ());
  CHECK_NOT_NULL(kernel);
  CHECK_BOOL(nx >= 5 && ny >= 3 && nz >= 1, true);
  CHECK_INT(static_cast<int>(kernel->GetNumberOfTuples()), nx * ny * nz);
  auto value = [&](int i, int j, int k)
    {
    return kernel->GetValue(((k + (nz - 1) / 2) * ny + (j + (ny - 1) / 2)) * nx + (i + (nx - 1) / 2));
    };
  CHECK_BOOL(value(2, -1, 0) > value(2, 1, 0), true);
  CHECK_BOOL(value(-2, 1, 0) > value(-2, -1, 0), true);
  CHECK_DOUBLE_TOLERANCE(value(2, -1, 0), value(-2, 1, 0), 1e-12);
  double sum = 0.;
  for (vtkIdType i = 0; i < kernel->GetNumberOfTuples(); ++i)
    {
    sum += kernel->GetValue(i);
    }
  CHECK_DOUBLE_TOLERANCE(sum, 1., 1e-9);

  return EXIT_SUCCESS;
}
