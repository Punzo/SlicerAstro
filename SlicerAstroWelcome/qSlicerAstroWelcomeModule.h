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
  and was supported through the European Research Consil grant nr. 291531.

==============================================================================*/

#ifndef __qSlicerAstroWelcomeModule_h
#define __qSlicerAstroWelcomeModule_h

// CTK includes
#include <ctkPimpl.h>

// SlicerQt includes
#include "qSlicerLoadableModule.h"
#include "qSlicerAstroWelcomeModuleExport.h"

class qSlicerAbstractModuleWidget;
class qSlicerAstroWelcomeModulePrivate;

/// \ingroup Slicer_QtModules_SlicerAstroWelcome
class Q_SLICER_QTMODULES_ASTROWELCOME_EXPORT qSlicerAstroWelcomeModule :
  public qSlicerLoadableModule
{
  Q_OBJECT
  Q_INTERFACES(qSlicerLoadableModule);

public:

  typedef qSlicerLoadableModule Superclass;
  qSlicerAstroWelcomeModule(QObject *parent=0);
  virtual ~qSlicerAstroWelcomeModule();

  qSlicerGetTitleMacro(QTMODULE_TITLE);

  virtual QStringList categories()const;
  virtual QIcon icon()const;

  /// Help to use the module
  virtual QString helpText()const;
  virtual QString acknowledgementText()const;
  virtual QStringList contributors()const;

protected:

  /// Create and return the widget representation associated to this module
  virtual qSlicerAbstractModuleRepresentation * createWidgetRepresentation();

  /// Create and return the logic associated to this module
  virtual vtkMRMLAbstractLogic* createLogic();

protected:
  QScopedPointer<qSlicerAstroWelcomeModulePrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroWelcomeModule);
  Q_DISABLE_COPY(qSlicerAstroWelcomeModule);
};

#endif
