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

#ifndef __qSlicerAstroWelcomeModuleWidget_h
#define __qSlicerAstroWelcomeModuleWidget_h

// CTK includes
#include <ctkPimpl.h>

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"
#include "qSlicerAstroWelcomeModuleExport.h"

class qSlicerAstroWelcomeModuleWidgetPrivate;
class qSlicerAstroWelcomeWidget;

/// \ingroup SlicerAstro_QtModules_SlicerAstroWelcome
/// Welcome module panel: the welcome page of SlicerAstro
/// (qSlicerAstroWelcomeWidget, also the home page of the AstroWorkbench).
class Q_SLICERASTRO_QTMODULES_ASTROWELCOME_EXPORT qSlicerAstroWelcomeModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroWelcomeModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroWelcomeModuleWidget();

  virtual void enter();

  /// The welcome page
  Q_INVOKABLE qSlicerAstroWelcomeWidget* welcomeWidget() const;

protected:
  virtual void setup();

  QScopedPointer<qSlicerAstroWelcomeModuleWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroWelcomeModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroWelcomeModuleWidget);
};

#endif
