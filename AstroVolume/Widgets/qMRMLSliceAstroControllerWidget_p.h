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

// Qt includes
#include <QLabel>

// vtk includes
#include <vtkCollection.h>
#include <vtkSmartPointer.h>

class qSlicerApplication;

//-----------------------------------------------------------------------------
class qMRMLSliceAstroControllerWidgetPrivate
  : public qMRMLSliceControllerWidgetPrivate
{
  Q_OBJECT
  QVTK_OBJECT
  Q_DECLARE_PUBLIC(qMRMLSliceAstroControllerWidget);

public slots:
  /// Update widget state using the associated MRML slice node
  void updateCoordinateWidgetFromMRMLSliceNode();

protected:
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void setMRMLSliceNodeInternal(vtkMRMLSliceNode* sliceNode);

public:
  typedef qMRMLSliceControllerWidgetPrivate Superclass;
  qMRMLSliceAstroControllerWidgetPrivate(qMRMLSliceAstroControllerWidget& object);
  virtual ~qMRMLSliceAstroControllerWidgetPrivate();

  virtual void init();

  QLabel*     WCSDisplay;
  qSlicerApplication* app;
  vtkSmartPointer<vtkCollection> col;
  vtkSmartPointer<vtkMRMLSliceLogic> sliceLogic;
};

#endif

