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

// Qt includes
#include <QVBoxLayout>

// AstroVolume includes
#include <qSlicerAstroPanelHeader.h>
#include <qSlicerAstroTheme.h>
#include <qSlicerAstroWelcomeWidget.h>

#include "qSlicerAstroWelcomeModuleWidget.h"

//-----------------------------------------------------------------------------
/// \ingroup SlicerAstro_QtModules_SlicerAstroWelcome
class qSlicerAstroWelcomeModuleWidgetPrivate
{
public:
  qSlicerAstroWelcomeWidget* Welcome{nullptr};
};

//-----------------------------------------------------------------------------
qSlicerAstroWelcomeModuleWidget::qSlicerAstroWelcomeModuleWidget(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroWelcomeModuleWidgetPrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroWelcomeModuleWidget::~qSlicerAstroWelcomeModuleWidget()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeModuleWidget::setup()
{
  Q_D(qSlicerAstroWelcomeModuleWidget);
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  qSlicerAstroPanelHeader* header = new qSlicerAstroPanelHeader(this);
  header->setTitle(tr("Welcome"));
  header->setIconName("module-welcome");
  header->setToolTip(tr("Welcome to SlicerAstro."));
  layout->addWidget(header);
  // the narrow layout of the welcome page (the AstroWorkbench shows the wide one)
  d->Welcome = new qSlicerAstroWelcomeWidget(this);
  layout->addWidget(d->Welcome);
  layout->addStretch(1);
  qSlicerAstroTheme::instance()->applyTo(this);
  this->Superclass::setup();
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeModuleWidget::enter()
{
  Q_D(qSlicerAstroWelcomeModuleWidget);
  this->Superclass::enter();
  d->Welcome->updateLists();
}

//-----------------------------------------------------------------------------
qSlicerAstroWelcomeWidget* qSlicerAstroWelcomeModuleWidget::welcomeWidget() const
{
  Q_D(const qSlicerAstroWelcomeModuleWidget);
  return d->Welcome;
}
