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

// Fatal errors of BBarolo must not terminate SlicerAstro: libbbarolo is built
// with BBAROLO_THROW_ON_ERROR, so they are thrown as bbarolo::Error carrying
// the error message (reported by vtkSlicerAstroModelingLogic in the log).

// BBarolo includes
#include <Arrays/cube.hh>
#include <Utilities/fatal.hh>

// STD includes
#include <cstdlib>
#include <iostream>
#include <string>

int vtkSlicerAstroModelingBBaroloErrorTest1(int, char*[])
{
  int dimensions[3] = {-2, 3, 4};
  try
    {
    Cube<float> cube(dimensions);
    std::cerr << "FAILED: no error for a cube of negative size" << std::endl;
    return EXIT_FAILURE;
    }
  catch (const bbarolo::Error& error)
    {
    const std::string message(error.what());
    if (message.find("Negative size") == std::string::npos ||
        message.find("cube.cpp:") == std::string::npos)
      {
      std::cerr << "FAILED: unexpected error message: " << message << std::endl;
      return EXIT_FAILURE;
      }
    std::cout << "bbarolo::Error: " << message << std::endl;
    }
  return EXIT_SUCCESS;
}
