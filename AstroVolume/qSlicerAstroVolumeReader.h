/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Davide Punzo, Kapteyn Astronomical Institute.

==============================================================================*/

#ifndef __qSlicerAstroVolumeReader_h
#define __qSlicerAstroVolumeReader_h

// SlicerQt includes
#include "qSlicerFileReader.h"

class qSlicerAstroVolumeReaderPrivate;
class vtkSlicerVolumesLogic;

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroVolume
class qSlicerAstroVolumeReader
  : public qSlicerFileReader
{
  Q_OBJECT
public:
  typedef qSlicerFileReader Superclass;
  qSlicerAstroVolumeReader(QObject* parent = 0);
  qSlicerAstroVolumeReader(vtkSlicerVolumesLogic* logic, QObject* parent = 0);
  virtual ~qSlicerAstroVolumeReader();

  vtkSlicerVolumesLogic* logic()const;
  void setLogic(vtkSlicerVolumesLogic* logic);

  virtual QString description()const;
  virtual IOFileType fileType()const;
  virtual QStringList extensions()const;
  virtual qSlicerIOOptions* options()const;

  virtual bool load(const IOProperties& properties);
protected:
  QScopedPointer<qSlicerAstroVolumeReaderPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroVolumeReader);
  Q_DISABLE_COPY(qSlicerAstroVolumeReader);
};

#endif
