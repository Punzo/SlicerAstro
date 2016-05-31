#ifndef __qSlicerAstroVolumeLayoutSliceViewFactory_h
#define __qSlicerAstroVolumeLayoutSliceViewFactory_h

// SlicerQt includes
#include "qMRMLLayoutManager.h"
#include "qMRMLLayoutManager_p.h"

// VTK includes
#include <vtkWeakPointer.h>

#include "qSlicerAstroVolumeModuleExport.h"

class qSlicerAstroVolumeLayoutSliceViewFactoryPrivate;

/// \ingroup Slicer_QtModules_AstroVolume
class Q_SLICER_QTMODULES_ASTROVOLUME_EXPORT qSlicerAstroVolumeLayoutSliceViewFactory
 : public qMRMLLayoutSliceViewFactory
{
  Q_OBJECT
public:
  typedef qMRMLLayoutSliceViewFactory Superclass;
  qSlicerAstroVolumeLayoutSliceViewFactory(QObject* parent);
  virtual ~qSlicerAstroVolumeLayoutSliceViewFactory();

  virtual QString viewClassName()const;

protected:

  virtual QWidget* createViewFromNode(vtkMRMLAbstractViewNode* viewNode);
  virtual void deleteView(vtkMRMLAbstractViewNode* viewNode);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroVolumeLayoutSliceViewFactory);
  Q_DISABLE_COPY(qSlicerAstroVolumeLayoutSliceViewFactory);
};

#endif
