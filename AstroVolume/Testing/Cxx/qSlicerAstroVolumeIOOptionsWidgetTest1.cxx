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

//-----------------------------------------------------------------------------
int qSlicerAstroVolumeIOOptionsWidgetTest1( int argc, char * argv[] )
{
  QApplication app(argc, argv);

  qSlicerAstroVolumeIOOptionsWidget optionsWidget;

  optionsWidget.setFileName("mylabelmap-seg.fits");
  if (!optionsWidget.properties()["labelmap"].toBool())
    {
    std::cerr << "Must be a labelmap" << std::endl;
    return EXIT_FAILURE;
    }

  optionsWidget.setFileName("./mylabelmap_seg_1.fits");
  if (!optionsWidget.properties()["labelmap"].toBool())
    {
    std::cerr << "Must be a labelmap" << std::endl;
    return EXIT_FAILURE;
    }

  optionsWidget.setFileName("./segment.fits");
  if (optionsWidget.properties()["labelmap"].toBool())
    {
    std::cerr << "Not a labelmap" << std::endl;
    return EXIT_FAILURE;
    }

  optionsWidget.show();

  if (argc < 2 || QString(argv[2]) != "-I")
    {
    QTimer::singleShot(200, &app, SLOT(quit()));
    }
  return app.exec();
}
