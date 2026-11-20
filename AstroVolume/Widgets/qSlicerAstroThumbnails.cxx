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

// Qt includes
#include <QColor>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>

// VTK includes
#include <vtkImageData.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkSlicerAstroVolumeLogic.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "qSlicerAstroThumbnails.h"

namespace
{
// Colour maps of SlicerAstro (vtkSlicerAstroVolumeLogic, the same tables are
// used by the views)
QColor MapColor(const char* name, double value)
{
  if (!std::isfinite(value))
    {
    return QColor(0x070A12);
    }
  double rgb[3] = {0., 0., 0.};
  vtkSlicerAstroVolumeLogic::GetColorMapColor(name, value, rgb);
  return QColor::fromRgbF(rgb[0], rgb[1], rgb[2]);
}

//----------------------------------------------------------------------------
/// Value at a fraction of the sorted finite values (sampled)
double Percentile(std::vector<double> values, double fraction)
{
  if (values.empty())
    {
    return 0.;
    }
  const size_t index = std::min(values.size() - 1, static_cast<size_t>(fraction * (values.size() - 1)));
  std::nth_element(values.begin(), values.begin() + index, values.end());
  return values[index];
}

//----------------------------------------------------------------------------
/// Map (dims[0] x dims[1]): the plane of 2D data, or the sum over the
/// channels of a cube (sampled in the channels for very large cubes)
template <typename T>
void SumOverChannels(const T* data, const int dims[3], std::vector<double>& map)
{
  const size_t plane = static_cast<size_t>(dims[0]) * dims[1];
  map.assign(plane, std::numeric_limits<double>::quiet_NaN());
  const size_t budget = 40000000;
  const int stride = std::max(1, static_cast<int>(plane * static_cast<size_t>(dims[2]) / budget));
  std::vector<int> counts(plane, 0);
  for (int k = 0; k < dims[2]; k += stride)
    {
    const T* channel = data + static_cast<size_t>(k) * plane;
    for (size_t p = 0; p < plane; p++)
      {
      const double value = static_cast<double>(channel[p]);
      if (std::isfinite(value))
        {
        map[p] = counts[p] ? map[p] + value : value;
        counts[p]++;
        }
      }
    }
}

//----------------------------------------------------------------------------
template <typename T>
void SpectrumOf(const T* data, const int dims[3], QVector<double>& spectrum)
{
  const size_t plane = static_cast<size_t>(dims[0]) * dims[1];
  spectrum.fill(std::numeric_limits<double>::quiet_NaN(), dims[2]);
  // sampled in the sky for very large cubes (a preview)
  const size_t budget = 40000000;
  const int stride = std::max(1, static_cast<int>(std::sqrt(static_cast<double>(plane) * dims[2] / budget)));
  const double scale = static_cast<double>(stride) * stride;
  for (int k = 0; k < dims[2]; k++)
    {
    const T* channel = data + static_cast<size_t>(k) * plane;
    double sum = 0.;
    size_t count = 0;
    for (int j = 0; j < dims[1]; j += stride)
      {
      const T* row = channel + static_cast<size_t>(j) * dims[0];
      for (int i = 0; i < dims[0]; i += stride)
        {
        const double value = static_cast<double>(row[i]);
        if (std::isfinite(value))
          {
          sum += value;
          count++;
          }
        }
      }
    if (count > 0)
      {
      spectrum[k] = sum * scale;
      }
    }
}

//----------------------------------------------------------------------------
struct CacheEntry
{
  vtkMTimeType Time{0};
  QImage Image;
  QVector<double> Spectrum;
  bool HasSpectrum{false};
};

QHash<vtkMRMLVolumeNode*, CacheEntry> Cache;

//----------------------------------------------------------------------------
vtkMTimeType DataTime(vtkMRMLVolumeNode* volume)
{
  vtkImageData* imageData = volume ? volume->GetImageData() : nullptr;
  if (!imageData)
    {
    return 0;
    }
  vtkMTimeType time = std::max(imageData->GetMTime(), volume->GetMTime());
  vtkDataArray* scalars = imageData->GetPointData() ? imageData->GetPointData()->GetScalars() : nullptr;
  if (scalars)
    {
    time = std::max(time, scalars->GetMTime());
    }
  return time;
}

//----------------------------------------------------------------------------
CacheEntry& EntryOf(vtkMRMLVolumeNode* volume)
{
  // previews of deleted nodes are dropped from time to time (keys are
  // pointers: an entry is valid only with the same modification time)
  if (Cache.size() > 64 && !Cache.contains(volume))
    {
    Cache.clear();
    }
  CacheEntry& entry = Cache[volume];
  const vtkMTimeType time = DataTime(volume);
  if (entry.Time != time)
    {
    entry = CacheEntry();
    entry.Time = time;
    }
  return entry;
}

//----------------------------------------------------------------------------
/// Full-resolution preview image of the data
QImage ComputeImage(vtkMRMLVolumeNode* volume)
{
  vtkImageData* imageData = volume ? volume->GetImageData() : nullptr;
  if (!imageData || !imageData->GetPointData() || !imageData->GetPointData()->GetScalars() ||
      imageData->GetNumberOfScalarComponents() != 1)
    {
    return QImage();
    }
  int dims[3];
  imageData->GetDimensions(dims);
  if (dims[0] < 1 || dims[1] < 1)
    {
    return QImage();
    }
  std::vector<double> map;
  switch (imageData->GetScalarType())
    {
    vtkTemplateMacro(SumOverChannels(static_cast<const VTK_TT*>(imageData->GetScalarPointer()), dims, map));
    default:
      return QImage();
    }

  // velocity fields: Doppler colour map, linear; other data: asinh stretch
  const char* datamodel = volume->GetAttribute("SlicerAstro.DATAMODEL");
  const char* bunit = volume->GetAttribute("SlicerAstro.BUNIT");
  const bool dispersion = datamodel && std::string(datamodel) == "SECONDMOMENTMAP";
  const bool velocity = (datamodel && std::string(datamodel) == "FIRSTMOMENTMAP") ||
                        (dims[2] == 1 && bunit && (std::string(bunit) == "km/s" || std::string(bunit) == "m/s") &&
                         !dispersion);
  // maps of velocities (moment 1) and dispersions (moment 2): linear
  const bool linear = velocity || dispersion;
  std::vector<double> sample;
  const size_t step = std::max<size_t>(1, map.size() / 200000);
  for (size_t p = 0; p < map.size(); p += step)
    {
    if (std::isfinite(map[p]))
      {
      sample.push_back(map[p]);
      }
    }
  double low = Percentile(sample, linear ? 0.02 : 0.005);
  double high = Percentile(sample, linear ? 0.98 : 0.998);
  if (!linear)
    {
    // background at the median (the noise is dark)
    low = std::max(low, Percentile(sample, 0.5));
    }
  if (high <= low)
    {
    high = low + 1.;
    }
  QImage image(dims[0], dims[1], QImage::Format_RGB32);
  for (int j = 0; j < dims[1]; j++)
    {
    // FITS images have the first row at the bottom
    QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(dims[1] - 1 - j));
    for (int i = 0; i < dims[0]; i++)
      {
      const double value = map[static_cast<size_t>(j) * dims[0] + i];
      double t = (value - low) / (high - low);
      QColor color;
      if (velocity)
        {
        color = std::isfinite(value) ? qSlicerAstroThumbnails::dopplerColor(t) : QColor(0x070A12);
        }
      else if (dispersion)
        {
        color = std::isfinite(value) ? MapColor("Ember", t) : QColor(0x070A12);
        }
      else if (!std::isfinite(value))
        {
        // blank pixels (e.g. of moment maps): background, not the top of the
        // stretch (std::min(1., NaN) is 1)
        color = QColor(0x070A12);
        }
      else
        {
        t = std::max(0., std::min(1., t));
        color = qSlicerAstroThumbnails::observatoryColor(std::asinh(10. * t) / std::asinh(10.));
        }
      line[i] = color.rgb();
      }
    }
  return image;
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
qSlicerAstroThumbnails::qSlicerAstroThumbnails(QObject* parent)
  : QObject(parent)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroThumbnails::~qSlicerAstroThumbnails() = default;

//-----------------------------------------------------------------------------
qSlicerAstroThumbnails* qSlicerAstroThumbnails::instance()
{
  static qSlicerAstroThumbnails thumbnails;
  return &thumbnails;
}

//-----------------------------------------------------------------------------
QColor qSlicerAstroThumbnails::observatoryColor(double value)
{
  return MapColor("Observatory", value);
}

//-----------------------------------------------------------------------------
QColor qSlicerAstroThumbnails::dopplerColor(double value)
{
  return MapColor("Doppler", value);
}

//-----------------------------------------------------------------------------
QImage qSlicerAstroThumbnails::image(vtkMRMLVolumeNode* volume, int width, int height)
{
  if (!volume || width < 1 || height < 1)
    {
    return QImage();
    }
  CacheEntry& entry = EntryOf(volume);
  if (entry.Image.isNull())
    {
    entry.Image = ComputeImage(volume);
    }
  if (entry.Image.isNull())
    {
    return QImage();
    }
  // cover: scaled to fill, centred crop
  const QImage scaled = entry.Image.scaled(width, height, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  return scaled.copy((scaled.width() - width) / 2, (scaled.height() - height) / 2, width, height);
}

//-----------------------------------------------------------------------------
QPixmap qSlicerAstroThumbnails::rounded(const QImage& image, int width, int height, double radius,
                                       double devicePixelRatio)
{
  const int pixelWidth = qRound(width * devicePixelRatio);
  const int pixelHeight = qRound(height * devicePixelRatio);
  QImage result(pixelWidth, pixelHeight, QImage::Format_ARGB32_Premultiplied);
  result.fill(Qt::transparent);
  QPainter painter(&result);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  QPainterPath path;
  path.addRoundedRect(QRectF(0., 0., pixelWidth, pixelHeight), radius * devicePixelRatio, radius * devicePixelRatio);
  painter.setClipPath(path);
  painter.fillRect(result.rect(), QColor(0x070A12));
  if (!image.isNull())
    {
    painter.drawImage(QRect(0, 0, pixelWidth, pixelHeight), image);
    }
  painter.end();
  QPixmap pixmap = QPixmap::fromImage(result);
  pixmap.setDevicePixelRatio(devicePixelRatio);
  return pixmap;
}

//-----------------------------------------------------------------------------
QPixmap qSlicerAstroThumbnails::roundedPixmap(vtkMRMLVolumeNode* volume, int width, int height, double radius,
                                             double devicePixelRatio)
{
  const QImage preview = this->image(volume, qRound(width * devicePixelRatio), qRound(height * devicePixelRatio));
  return rounded(preview, width, height, radius, devicePixelRatio);
}

//-----------------------------------------------------------------------------
QVector<double> qSlicerAstroThumbnails::integratedSpectrum(vtkMRMLVolumeNode* volume)
{
  vtkImageData* imageData = volume ? volume->GetImageData() : nullptr;
  if (!imageData || !imageData->GetPointData() || !imageData->GetPointData()->GetScalars() ||
      imageData->GetNumberOfScalarComponents() != 1)
    {
    return QVector<double>();
    }
  int dims[3];
  imageData->GetDimensions(dims);
  if (dims[2] < 2)
    {
    return QVector<double>();
    }
  CacheEntry& entry = EntryOf(volume);
  if (!entry.HasSpectrum)
    {
    switch (imageData->GetScalarType())
      {
      vtkTemplateMacro(SpectrumOf(static_cast<const VTK_TT*>(imageData->GetScalarPointer()), dims, entry.Spectrum));
      default:
        break;
      }
    entry.HasSpectrum = true;
    }
  return entry.Spectrum;
}
