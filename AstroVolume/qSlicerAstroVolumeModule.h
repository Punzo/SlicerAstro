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

#ifndef __qSlicerAstroVolumeModule_h
#define __qSlicerAstroVolumeModule_h

// CTK includes
#include <ctkVTKObject.h>

// SlicerQt includes
#include "qSlicerLoadableModule.h"

#include "qSlicerAstroVolumeModuleExport.h"

class qSlicerAbstractModuleWidget;
class qSlicerAstroVolumeModulePrivate;
class vtkMRMLSliceLogic;
class vtkObject;

/// \ingroup Slicer_QtModules_AstroVolume
class Q_SLICER_QTMODULES_ASTROVOLUME_EXPORT qSlicerAstroVolumeModule :
  public qSlicerLoadableModule
{
  Q_OBJECT
  QVTK_OBJECT
#ifdef Slicer_HAVE_QT5
  Q_PLUGIN_METADATA(IID "org.slicer.modules.loadable.qSlicerLoadableModule/1.0");
#endif
  Q_INTERFACES(qSlicerLoadableModule);

public:

  typedef qSlicerLoadableModule Superclass;
  qSlicerAstroVolumeModule(QObject *parent=0);
  virtual ~qSlicerAstroVolumeModule();

  virtual QString helpText()const;
  virtual QString acknowledgementText()const;
  virtual QStringList contributors()const;
  virtual QIcon icon()const;
  virtual QStringList categories()const;
  virtual QStringList dependencies()const;
  qSlicerGetTitleMacro(QTMODULE_TITLE);

protected:
  /// Initialize the module. Register the AstroVolume reader/writer
  virtual void setup();

  /// Create and return the widget representation associated to this module
  virtual qSlicerAbstractModuleRepresentation* createWidgetRepresentation();

  /// Create and return the logic associated to this module
  virtual vtkMRMLAbstractLogic* createLogic();

  /// Specify editable node types
  virtual QStringList associatedNodeTypes()const;

  QScopedPointer<qSlicerAstroVolumeModulePrivate> d_ptr;

protected slots:

  void onMRMLUnitNodeIntensityModified(vtkObject* sender);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroVolumeModule);
  Q_DISABLE_COPY(qSlicerAstroVolumeModule);
};

#endif
