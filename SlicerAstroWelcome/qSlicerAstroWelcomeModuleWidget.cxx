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

// Qt includes
#include <QDebug>
#include <QDesktopServices>
#include <QMainWindow>
#include <QMessageBox>
#include <QSettings>
#include <QSignalMapper>
#include <QLatin1String>
#include <QTextStream>

// Slicer includes
#include "vtkSlicerConfigure.h" // For Slicer_BUILD_DICOM_SUPPORT
#include "vtkSlicerVersionConfigure.h"

// SlicerQt includes
#include "qSlicerAstroWelcomeModuleWidget.h"
#include "ui_qSlicerAstroWelcomeModuleWidget.h"
#include "qSlicerApplication.h"
#include "qSlicerIO.h"
#include "qSlicerIOManager.h"
#include "qSlicerLayoutManager.h"
#include "qSlicerModuleManager.h"
#include "qSlicerAbstractCoreModule.h"
#include "qSlicerModulePanel.h"
#include "qSlicerUtils.h"

// CTK includes
#include "ctkButtonGroup.h"

class qSlicerAppMainWindow;

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_SlicerAstroWelcome
class qSlicerAstroWelcomeModuleWidgetPrivate: public Ui_qSlicerAstroWelcomeModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroWelcomeModuleWidget);
protected:
  qSlicerAstroWelcomeModuleWidget* const q_ptr;
public:
  qSlicerAstroWelcomeModuleWidgetPrivate(qSlicerAstroWelcomeModuleWidget& object);
  void setupUi(qSlicerWidget* widget);

  bool selectModule(const QString& moduleName);

  QSignalMapper CollapsibleButtonMapper;
};

//-----------------------------------------------------------------------------
// qSlicerAstroWelcomeModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroWelcomeModuleWidgetPrivate::qSlicerAstroWelcomeModuleWidgetPrivate(qSlicerAstroWelcomeModuleWidget& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeModuleWidgetPrivate::setupUi(qSlicerWidget* widget)
{
  Q_Q(qSlicerAstroWelcomeModuleWidget);

  this->Ui_qSlicerAstroWelcomeModuleWidget::setupUi(widget);

  this->IconLabel->setPixmap(QPixmap(QLatin1String(":/Images/SlicerAstroIcon.png")));
  // Create the button group ensuring that only one collabsibleWidgetButton will be open at a time
  ctkButtonGroup * group = new ctkButtonGroup(widget);

  // Add all collabsibleWidgetButton to a button group
  QList<ctkCollapsibleButton*> collapsibles = widget->findChildren<ctkCollapsibleButton*>();
  foreach(ctkCollapsibleButton* collapsible, collapsibles)
    {
    group->addButton(collapsible);
    }

  // Lazily set the fitted browser source to avoid overhead when the module
  // is loaded.
  this->FeedbackCollapsibleWidget->setProperty("source", ":HTML/AstroFeedback.html");
  this->WelcomeAndAboutCollapsibleWidget->setProperty("source", ":HTML/AstroAbout.html");
  this->OverviewCollapsibleWidget->setProperty("source", ":HTML/AstroOverview.html");
  this->AcknowledgmentCollapsibleWidget->setProperty("source", ":HTML/AstroAcknowledgment.html");

  foreach(QWidget* widget, QWidgetList()
          << this->FeedbackCollapsibleWidget
          << this->WelcomeAndAboutCollapsibleWidget
          << this->OverviewCollapsibleWidget
          << this->AcknowledgmentCollapsibleWidget
          )
    {
    this->CollapsibleButtonMapper.setMapping(widget, widget);
    QObject::connect(widget, SIGNAL(contentsCollapsed(bool)),
                     &this->CollapsibleButtonMapper, SLOT(map()));
    }

  QObject::connect(&this->CollapsibleButtonMapper, SIGNAL(mapped(QWidget*)),
                   q, SLOT(loadSource(QWidget*)));
}

//-----------------------------------------------------------------------------
bool qSlicerAstroWelcomeModuleWidgetPrivate::selectModule(const QString& moduleName)
{
  Q_Q(qSlicerAstroWelcomeModuleWidget);
  qSlicerModuleManager * moduleManager = qSlicerCoreApplication::application()->moduleManager();
  if (!moduleManager)
    {
    return false;
    }
  qSlicerAbstractCoreModule * module = moduleManager->module(moduleName);
  if(!module)
    {
    QMessageBox::warning(
          q, q->tr("Raising %1 Module:").arg(moduleName),
          q->tr("Unfortunately, this requested module is not available in this Slicer session."),
          QMessageBox::Ok);
    return false;
    }
  qSlicerLayoutManager * layoutManager = qSlicerApplication::application()->layoutManager();
  if (!layoutManager)
    {
    return false;
    }
  layoutManager->setCurrentModule(moduleName);
  return true;
}

//-----------------------------------------------------------------------------
// qSlicerAstroWelcomeModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroWelcomeModuleWidget::qSlicerAstroWelcomeModuleWidget(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroWelcomeModuleWidgetPrivate(*this))
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
  d->setupUi(this);

  connect(d->LoadNonDicomDataButton, SIGNAL(clicked()),
          this, SLOT (loadNonDicomData()));
  connect(d->LoadSampleDataButton, SIGNAL(clicked()),
          this, SLOT (loadRemoteSampleData()));
  connect(d->EditApplicationSettingsButton, SIGNAL(clicked()),
          this, SLOT (editApplicationSettings()));
  connect(d->pushToSlicerWelcom, SIGNAL(clicked()),
          this, SLOT (navigateToSlicerWelcom()));

  this->Superclass::setup();
  d->FeedbackCollapsibleWidget->setCollapsed(false);
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeModuleWidget::editApplicationSettings()
{
  qSlicerApplication::application()->settingsDialog()->exec();
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeModuleWidget::loadSource(QWidget* widget)
{
  // Lookup fitted browser
  ctkFittedTextBrowser* fittedTextBrowser =
      widget->findChild<ctkFittedTextBrowser*>();
  Q_ASSERT(fittedTextBrowser);
  if (fittedTextBrowser->source().isEmpty())
    {
    // Read content
    QString url = widget->property("source").toString();
    QFile source(url);
    if(!source.open(QIODevice::ReadOnly))
      {
      qWarning() << Q_FUNC_INFO << ": Failed to read" << url;
      return;
      }
    QTextStream in(&source);
    QString html = in.readAll();
    source.close();

    qSlicerCoreApplication* app = qSlicerCoreApplication::application();

    // Update occurences of wiki URLs
    QString wikiVersion = "Nightly";
    if (app->isRelease())
      {
      wikiVersion = QString("%1.%2").arg(app->majorVersion()).arg(app->minorVersion());
      }
      html = qSlicerUtils::replaceWikiUrlVersion(html, wikiVersion);

    fittedTextBrowser->setHtml(html);
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroWelcomeModuleWidget::loadNonDicomData()
{
  qSlicerIOManager *ioManager = qSlicerApplication::application()->ioManager();
  if (!ioManager)
    {
    return false;
    }
  return ioManager->openAddDataDialog();
}


//-----------------------------------------------------------------------------
bool qSlicerAstroWelcomeModuleWidget::loadRemoteSampleData()
{
  Q_D(qSlicerAstroWelcomeModuleWidget);
  return d->selectModule("AstroSampleData");
}

//-----------------------------------------------------------------------------
bool qSlicerAstroWelcomeModuleWidget::navigateToSlicerWelcom()
{
  Q_D(qSlicerAstroWelcomeModuleWidget);
  return d->selectModule("Welcome");
}

