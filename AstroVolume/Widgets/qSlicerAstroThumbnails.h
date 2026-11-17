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

==============================================================================*/

#ifndef __qSlicerAstroThumbnails_h
#define __qSlicerAstroThumbnails_h

// Qt includes
#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QVector>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class vtkMRMLVolumeNode;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Previews computed from the data (not screenshots of the views):
/// - cubes: the moment-0 map (sum over the channels), asinh stretch,
///   "Observatory" colour map;
/// - images and maps: the image itself (velocity fields in the "Doppler"
///   colour map);
/// - the integrated spectrum of a cube (sum over each channel).
/// Large cubes are sampled; results are cached until the data change.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroThumbnails
  : public QObject
{
  Q_OBJECT

public:
  explicit qSlicerAstroThumbnails(QObject* parent = nullptr);
  ~qSlicerAstroThumbnails() override;

  static qSlicerAstroThumbnails* instance();

public slots:
  /// Preview of the data, cropped to fill size (object-fit: cover)
  QImage image(vtkMRMLVolumeNode* volume, int width, int height);
  /// Preview with rounded corners, for the device pixel ratio of the screen
  QPixmap roundedPixmap(vtkMRMLVolumeNode* volume, int width, int height, double radius, double devicePixelRatio = 1.);
  /// Sum of each channel of a cube (empty for 2D data)
  QVector<double> integratedSpectrum(vtkMRMLVolumeNode* volume);

  /// Colour of the "Observatory" (intensity) or "Doppler" (velocity) colour
  /// map at value (0 to 1)
  static QColor observatoryColor(double value);
  static QColor dopplerColor(double value);
  /// Rounded copy of an image
  static QPixmap rounded(const QImage& image, int width, int height, double radius, double devicePixelRatio = 1.);
};

#endif
