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
#include <QApplication>
#include <QTimer>

// AstroVolume includes
#include "qSlicerAstroVolumeIOOptionsWidget.h"

// STD includes
#include <iostream>

//-----------------------------------------------------------------------------
int qSlicerAstroVolumeIOOptionsWidgetTest1( int argc, char * argv[] )
{
  QApplication app(argc, argv);

  qSlicerAstroVolumeIOOptionsWidget optionsWidget;

  // FITS masks are segmentations: the volume options have no label map option
  optionsWidget.setFileName("WEIN069_mask.fits");
  if (optionsWidget.properties().contains("labelmap"))
    {
    std::cerr << "The volume options must not have a labelmap property" << std::endl;
    return EXIT_FAILURE;
    }
  // astronomical data are always centered: no option
  if (optionsWidget.properties().contains("center") || !optionsWidget.properties().contains("singleFile"))
    {
    std::cerr << "The volume options must have the singleFile property and no center property" << std::endl;
    return EXIT_FAILURE;
    }

  optionsWidget.show();

  if (argc < 2 || QString(argv[2]) != "-I")
    {
    QTimer::singleShot(200, &app, SLOT(quit()));
    }
  return app.exec();
}
