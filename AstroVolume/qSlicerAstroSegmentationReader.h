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

#ifndef __qSlicerAstroSegmentationReader_h
#define __qSlicerAstroSegmentationReader_h

// SlicerQt includes
#include "qSlicerFileReader.h"

#include "qSlicerAstroVolumeModuleExport.h"

/// \brief Reader of FITS masks as segmentations.
///
/// FITS masks (see vtkFITSReader::IsMaskFile, e.g. the masks of SoFiA) are
/// loaded as segmentation nodes, one segment per source, by default: the
/// confidence of this reader is higher than the one of the AstroVolume
/// reader for masks and lower for the other FITS files. The segmentation is
/// saved back as a FITS mask (vtkMRMLAstroSegmentationStorageNode).
///
/// Properties: fileName, name, center (default true: as the astronomical
/// volumes loaded with the center option).
/// \ingroup SlicerAstro_QtModules_AstroVolume
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_EXPORT qSlicerAstroSegmentationReader
  : public qSlicerFileReader
{
  Q_OBJECT
public:
  typedef qSlicerFileReader Superclass;
  qSlicerAstroSegmentationReader(QObject* parent = nullptr);
  ~qSlicerAstroSegmentationReader() override;

  QString description() const override;
  IOFileType fileType() const override;
  QStringList extensions() const override;
  double canLoadFileConfidence(const QString& file) const override;
  bool load(const IOProperties& properties) override;

private:
  Q_DISABLE_COPY(qSlicerAstroSegmentationReader);
};

#endif
