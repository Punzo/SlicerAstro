// Qt includes
#include <QDesktopServices>
#include <QMainWindow>
#include <QMessageBox>
#include <QSettings>
#include <QSignalMapper>

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
  this->FeedbackCollapsibleWidget->setProperty("source", "qrc:HTML/AstroFeedback.html");
  this->WelcomeAndAboutCollapsibleWidget->setProperty("source", "qrc:HTML/AstroAbout.html");
  this->OverviewCollapsibleWidget->setProperty("source", "qrc:HTML/AstroOverview.html");
  this->AcknowledgmentCollapsibleWidget->setProperty("source", "qrc:HTML/AstroAcknowledgment.html");

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
void qSlicerAstroWelcomeModuleWidget::loadSource(QWidget* widget)
{
  // Lookup fitted browser
  ctkFittedTextBrowser* fittedTextBrowser =
      widget->findChild<ctkFittedTextBrowser*>();
  Q_ASSERT(fittedTextBrowser);
  if (fittedTextBrowser->source().isEmpty())
    {
    fittedTextBrowser->setSource(widget->property("source").toString());
    }
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

  this->Superclass::setup();

  d->FeedbackCollapsibleWidget->setCollapsed(false);
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
  // implement a python module to load it
  return true;
}

