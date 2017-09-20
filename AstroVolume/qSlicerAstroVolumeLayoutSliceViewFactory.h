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
