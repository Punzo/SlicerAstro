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
