#ifndef __qSlicerAstroVolumeReader_h
#define __qSlicerAstroVolumeReader_h

// SlicerQt includes
#include "qSlicerFileReader.h"

#include "qSlicerAstroVolumeModuleExport.h"

class qSlicerAstroVolumeReaderPrivate;
class vtkSlicerVolumesLogic;

/// \ingroup Slicer_QtModules_AstroVolume
class Q_SLICER_QTMODULES_ASTROVOLUME_EXPORT qSlicerAstroVolumeReader
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
