#ifndef __qMRMLSliceAstroControllerWidget_p_h
#define __qMRMLSliceAstroControllerWidget_p_h

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Slicer API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

// CTK includes
#include <ctkPimpl.h>
#include <ctkVTKObject.h>

// qMRML includes
#include "qMRMLSliceAstroControllerWidget.h"
#include "qMRMLSliceControllerWidget_p.h"

//-----------------------------------------------------------------------------
class qMRMLSliceAstroControllerWidgetPrivate
  : public qMRMLSliceControllerWidgetPrivate
{
  Q_OBJECT
  QVTK_OBJECT
  Q_DECLARE_PUBLIC(qMRMLSliceAstroControllerWidget);

public:
  typedef qMRMLSliceControllerWidgetPrivate Superclass;
  qMRMLSliceAstroControllerWidgetPrivate(qMRMLSliceAstroControllerWidget& object);
  virtual ~qMRMLSliceAstroControllerWidgetPrivate();

  virtual void init();

};

#endif

