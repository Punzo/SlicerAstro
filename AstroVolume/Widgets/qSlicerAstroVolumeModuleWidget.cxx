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
#include <QLabel>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

// VTK includes
#include <vtkNew.h>

// Slicer includes
#include <qSlicerAbstractCoreModule.h>
#include <vtkSlicerApplicationLogic.h>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationNode.h>

// AstroVolume includes
#include <vtkSlicerAstroVolumeLogic.h>

#include "qSlicerAstroContoursSection.h"
#include "qSlicerAstroDatasetSelector.h"
#include "qSlicerAstroDisplaySection.h"
#include "qSlicerAstroFoldSection.h"
#include "qSlicerAstroInfoSection.h"
#include "qSlicerAstroMasksSection.h"
#include "qSlicerAstroPanelHeader.h"
#include "qSlicerAstroRenderingSection.h"
#include "qSlicerAstroSegmentedButton.h"
#include "qSlicerAstroSourcesSection.h"
#include "qSlicerAstroTheme.h"
#include "qSlicerAstroVolumeModuleWidget.h"
#include "qSlicerAstroVolumeModuleWidget_p.h"

namespace
{
//----------------------------------------------------------------------------
/// A page of the tabs: the sections one below the other
QWidget* NewPage(QStackedWidget* pages, const QString& objectName)
{
  QWidget* page = new QWidget(pages);
  page->setObjectName(objectName);
  QVBoxLayout* layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 4, 0, 0);
  layout->setSpacing(14);
  pages->addWidget(page);
  return page;
}

//----------------------------------------------------------------------------
/// A section folded under the first one of a page (e.g. Contours under
/// Display)
qSlicerAstroFoldSection* NewFold(QWidget* page, QWidget* section, const QString& title, const QString& summary)
{
  qSlicerAstroFoldSection* fold = new qSlicerAstroFoldSection(page);
  fold->setObjectName(QString("Section_%1").arg(title));
  fold->setTitle(title);
  fold->setSummary(summary);
  fold->setToolTip(section->toolTip());
  fold->contentLayout()->addWidget(section);
  page->layout()->addWidget(fold);
  return fold;
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModuleWidgetPrivate::qSlicerAstroVolumeModuleWidgetPrivate(qSlicerAstroVolumeModuleWidget& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* qSlicerAstroVolumeModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroVolumeModuleWidget);
  return vtkSlicerAstroVolumeLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
vtkMRMLSegmentEditorNode* qSlicerAstroVolumeModuleWidgetPrivate::ensureSegmentEditorNode()
{
  Q_Q(qSlicerAstroVolumeModuleWidget);
  vtkMRMLScene* scene = q->mrmlScene();
  if (!scene || scene->IsClosing())
    {
    return nullptr;
    }
  vtkMRMLSegmentEditorNode* editorNode = vtkMRMLSegmentEditorNode::SafeDownCast(
    scene->GetSingletonNode("SegmentEditor", "vtkMRMLSegmentEditorNode"));
  if (!editorNode)
    {
    vtkNew<vtkMRMLSegmentEditorNode> newEditorNode;
    newEditorNode->SetSingletonTag("SegmentEditor");
    editorNode = vtkMRMLSegmentEditorNode::SafeDownCast(scene->AddNode(newEditorNode));
    }
  this->segmentEditorNode = editorNode;
  return editorNode;
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidgetPrivate::setupUi()
{
  Q_Q(qSlicerAstroVolumeModuleWidget);
  q->setObjectName("qSlicerAstroVolumeModuleWidget");
  QVBoxLayout* rootLayout = new QVBoxLayout(q);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  this->Header = new qSlicerAstroPanelHeader(q);
  this->Header->setTitle(qSlicerAstroVolumeModuleWidget::tr("Volume"));
  this->Header->setIconName("module-volume");
  this->AutoShowButton = new QToolButton(this->Header);
  this->AutoShowButton->setObjectName("AutoShowButton");
  this->AutoShowButton->setProperty("astroRole", "toolbar");
  this->AutoShowButton->setCheckable(true);
  this->AutoShowButton->setChecked(true);
  this->AutoShowButton->setAutoRaise(true);
  this->AutoShowButton->setIcon(qSlicerAstroTheme::instance()->icon("layers", "muted"));
  this->AutoShowButton->setIconSize(QSize(16, 16));
  this->AutoShowButton->setToolTip(qSlicerAstroVolumeModuleWidget::tr(
    "Show the active data automatically: in the 2D views, and cubes in the 3D views (the other cubes are "
    "hidden)."));
  this->Header->addWidget(this->AutoShowButton);
  rootLayout->addWidget(this->Header);

  QWidget* body = new QWidget(q);
  QVBoxLayout* bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(12, 12, 12, 14);
  bodyLayout->setSpacing(12);
  rootLayout->addWidget(body);

  this->DatasetSelector = new qSlicerAstroDatasetSelector(body);
  this->DatasetSelector->setObjectName("DatasetSelector");
  this->DatasetSelector->setNoneText(qSlicerAstroVolumeModuleWidget::tr("Select data"));
  this->DatasetSelector->setToolTip(qSlicerAstroVolumeModuleWidget::tr(
    "Data shown and edited in the panel (the active volume of the views)."));
  bodyLayout->addWidget(this->DatasetSelector);

  this->Tabs = new qSlicerAstroSegmentedButton(body);
  this->Tabs->setObjectName("Tabs");
  this->Tabs->setToolTip(qSlicerAstroVolumeModuleWidget::tr("Parts of the panel."));
  this->Tabs->addButton(qSlicerAstroVolumeModuleWidget::tr("Display"),
                        qSlicerAstroVolumeModuleWidget::tr("2D colours, window, noise and contours."));
  this->Tabs->addButton(qSlicerAstroVolumeModuleWidget::tr("3D"),
                        qSlicerAstroVolumeModuleWidget::tr("Volume rendering of the cube."));
  this->Tabs->addButton(qSlicerAstroVolumeModuleWidget::tr("Masks"),
                        qSlicerAstroVolumeModuleWidget::tr("Mask segments and source catalogues."));
  this->Tabs->addButton(qSlicerAstroVolumeModuleWidget::tr("Info"),
                        qSlicerAstroVolumeModuleWidget::tr("Facts, coordinate conventions, beam and FITS header."));
  bodyLayout->addWidget(this->Tabs);

  this->Pages = new QStackedWidget(body);
  this->Pages->setObjectName("Pages");
  bodyLayout->addWidget(this->Pages);
  bodyLayout->addStretch(1);

  QWidget* displayPage = NewPage(this->Pages, "DisplayPage");
  this->DisplaySection = new qSlicerAstroDisplaySection(displayPage);
  displayPage->layout()->addWidget(this->DisplaySection);
  this->ContoursSection = new qSlicerAstroContoursSection(displayPage);
  NewFold(displayPage, this->ContoursSection, qSlicerAstroVolumeModuleWidget::tr("Contours"),
          qSlicerAstroVolumeModuleWidget::tr("levels over the data"));

  QWidget* renderingPage = NewPage(this->Pages, "RenderingPage");
  this->RenderingSection = new qSlicerAstroRenderingSection(renderingPage);
  renderingPage->layout()->addWidget(this->RenderingSection);

  QWidget* masksPage = NewPage(this->Pages, "MasksPage");
  this->MasksSection = new qSlicerAstroMasksSection(masksPage);
  masksPage->layout()->addWidget(this->MasksSection);
  this->SourcesSection = new qSlicerAstroSourcesSection(masksPage);
  NewFold(masksPage, this->SourcesSection, qSlicerAstroVolumeModuleWidget::tr("Sources"),
          qSlicerAstroVolumeModuleWidget::tr("catalogues"));

  QWidget* infoPage = NewPage(this->Pages, "InfoPage");
  this->InfoSection = new qSlicerAstroInfoSection(infoPage);
  infoPage->layout()->addWidget(this->InfoSection);

  // the pages are as tall as the page shown (not the tallest page)
  // the sections at the top of their page
  for (int page = 0; page < this->Pages->count(); page++)
    {
    qobject_cast<QVBoxLayout*>(this->Pages->widget(page)->layout())->addStretch(1);
    }
  QObject::connect(this->Tabs, &qSlicerAstroSegmentedButton::currentIndexChanged, this->Pages, [this](int index)
    {
    for (int page = 0; page < this->Pages->count(); page++)
      {
      this->Pages->widget(page)->setSizePolicy(QSizePolicy::Preferred,
        page == index ? QSizePolicy::Preferred : QSizePolicy::Ignored);
      }
    this->Pages->setCurrentIndex(index);
    this->Pages->updateGeometry();
    });
  QObject::connect(this->DatasetSelector, &qSlicerAstroDatasetSelector::currentNodeChanged,
                   q, &qSlicerAstroVolumeModuleWidget::onDatasetSelected);
  QObject::connect(this->AutoShowButton, SIGNAL(toggled(bool)), q, SLOT(setAutoShowActiveVolume(bool)));
  this->Tabs->setCurrentIndex(qSlicerAstroVolumeModuleWidget::DisplayTab);
  for (int page = 1; page < this->Pages->count(); page++)
    {
    this->Pages->widget(page)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    }
  qSlicerAstroTheme::instance()->applyTo(q);
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModuleWidget::qSlicerAstroVolumeModuleWidget(QWidget* parent)
  : Superclass(parent)
  , d_ptr(new qSlicerAstroVolumeModuleWidgetPrivate(*this))
{
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModuleWidget::~qSlicerAstroVolumeModuleWidget() = default;

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setup()
{
  Q_D(qSlicerAstroVolumeModuleWidget);
  d->setupUi();
  this->Superclass::setup();
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroVolumeModuleWidget);
  this->Superclass::setMRMLScene(scene);
  d->DatasetSelector->setMRMLScene(scene);
  if (!scene)
    {
    return;
    }
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent, this, SLOT(onMRMLSceneEndImportEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndCloseEvent, this, SLOT(onMRMLSceneEndCloseEvent()));
  vtkSlicerApplicationLogic* appLogic = this->module() ? this->module()->appLogic() : nullptr;
  vtkMRMLSelectionNode* selectionNode = appLogic ? appLogic->GetSelectionNode() : nullptr;
  this->qvtkReconnect(d->selectionNode, selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onSelectionNodeModified()));
  d->selectionNode = selectionNode;
  d->ensureSegmentEditorNode();
  {
  const QSignalBlocker blocker(d->AutoShowButton);
  d->AutoShowButton->setChecked(this->autoShowActiveVolume());
  }
  this->onSelectionNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onSelectionNodeModified()
{
  Q_D(qSlicerAstroVolumeModuleWidget);
  vtkMRMLScene* scene = this->mrmlScene();
  if (!scene || !d->selectionNode || scene->IsImporting() || scene->IsClosing())
    {
    // the volume is set at the end of the import, when its references (e.g.
    // its 3D color function) are restored
    return;
    }
  const char* activeID = d->selectionNode->GetActiveVolumeID();
  vtkMRMLAstroVolumeNode* volume = activeID ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(activeID)) : nullptr;
  if (volume == d->astroVolumeNode.GetPointer())
    {
    return;
    }
  d->astroVolumeNode = volume;
  vtkSlicerAstroVolumeLogic* logic = d->logic();
  if (logic)
    {
    logic->updateIntensityUnitsNode(volume);
    // the logic shows the new active volume (3D rendering prepared and,
    // with automatic display, shown in the views)
    }
  {
  const QSignalBlocker blocker(d->DatasetSelector);
  d->DatasetSelector->setCurrentNode(volume);
  }
  d->DisplaySection->setVolumeNode(volume);
  d->ContoursSection->setVolumeNode(volume);
  d->RenderingSection->setVolumeNode(volume);
  d->MasksSection->setVolumeNode(volume);
  d->SourcesSection->setVolumeNode(volume);
  d->InfoSection->setVolumeNode(volume);
  // the mask of the cube in the Segment Editor when it edits no mask yet
  vtkMRMLSegmentationNode* mask = (logic && volume) ? logic->GetMaskSegmentationForVolume(volume, false) : nullptr;
  if (mask && d->segmentEditorNode && !d->segmentEditorNode->GetSegmentationNode())
    {
    d->segmentEditorNode->SetAndObserveSegmentationNode(mask);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onDatasetSelected(vtkMRMLNode* node)
{
  this->setMRMLVolumeNode(node);
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setMRMLVolumeNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroVolumeModuleWidget);
  if (!d->selectionNode)
    {
    return;
    }
  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  d->selectionNode->SetActiveVolumeID(volume ? volume->GetID() : nullptr);
  if (!volume)
    {
    d->selectionNode->SetActiveLabelVolumeID(nullptr);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLSceneEndImportEvent()
{
  Q_D(qSlicerAstroVolumeModuleWidget);
  d->astroVolumeNode = nullptr;
  d->ensureSegmentEditorNode();
  this->onSelectionNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLSceneEndCloseEvent()
{
  Q_D(qSlicerAstroVolumeModuleWidget);
  d->astroVolumeNode = nullptr;
  d->ensureSegmentEditorNode();
  this->onSelectionNodeModified();
  d->DisplaySection->setVolumeNode(nullptr);
  d->ContoursSection->setVolumeNode(nullptr);
  d->RenderingSection->setVolumeNode(nullptr);
  d->MasksSection->setVolumeNode(nullptr);
  d->SourcesSection->setVolumeNode(nullptr);
  d->InfoSection->setVolumeNode(nullptr);
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::enter()
{
  this->Superclass::enter();
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::exit()
{
  Q_D(qSlicerAstroVolumeModuleWidget);
  // no editing of the mask outside the module
  d->MasksSection->setEditing(false);
  this->Superclass::exit();
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setCurrentTab(int tab)
{
  Q_D(qSlicerAstroVolumeModuleWidget);
  d->Tabs->setCurrentIndex(tab);
}

//-----------------------------------------------------------------------------
int qSlicerAstroVolumeModuleWidget::currentTab() const
{
  Q_D(const qSlicerAstroVolumeModuleWidget);
  return d->Tabs->currentIndex();
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setAutoShowActiveVolume(bool show)
{
  Q_D(qSlicerAstroVolumeModuleWidget);
  vtkSlicerAstroVolumeLogic* logic = d->logic();
  if (logic)
    {
    logic->SetAutoShowActiveVolume(show);
    }
  if (d->AutoShowButton && d->AutoShowButton->isChecked() != show)
    {
    const QSignalBlocker blocker(d->AutoShowButton);
    d->AutoShowButton->setChecked(show);
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroVolumeModuleWidget::autoShowActiveVolume() const
{
  Q_D(const qSlicerAstroVolumeModuleWidget);
  vtkSlicerAstroVolumeLogic* logic = d->logic();
  return logic ? logic->GetAutoShowActiveVolume() : true;
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::prepareVolumeRendering(vtkMRMLAstroVolumeNode* volume)
{
  Q_D(qSlicerAstroVolumeModuleWidget);
  vtkSlicerAstroVolumeLogic* logic = d->logic();
  if (logic)
    {
    logic->PrepareVolumeRendering(volume);
    }
}

//-----------------------------------------------------------------------------
vtkMRMLVolumeRenderingDisplayNode* qSlicerAstroVolumeModuleWidget::volumeRenderingDisplay() const
{
  Q_D(const qSlicerAstroVolumeModuleWidget);
  vtkSlicerAstroVolumeLogic* logic = d->logic();
  return logic ? logic->GetVolumeRenderingDisplayNode(d->astroVolumeNode, false) : nullptr;
}

//-----------------------------------------------------------------------------
qSlicerAstroDisplaySection* qSlicerAstroVolumeModuleWidget::displaySection() const
{
  Q_D(const qSlicerAstroVolumeModuleWidget);
  return d->DisplaySection;
}

//-----------------------------------------------------------------------------
qSlicerAstroContoursSection* qSlicerAstroVolumeModuleWidget::contoursSection() const
{
  Q_D(const qSlicerAstroVolumeModuleWidget);
  return d->ContoursSection;
}

//-----------------------------------------------------------------------------
qSlicerAstroRenderingSection* qSlicerAstroVolumeModuleWidget::renderingSection() const
{
  Q_D(const qSlicerAstroVolumeModuleWidget);
  return d->RenderingSection;
}

//-----------------------------------------------------------------------------
qSlicerAstroMasksSection* qSlicerAstroVolumeModuleWidget::masksSection() const
{
  Q_D(const qSlicerAstroVolumeModuleWidget);
  return d->MasksSection;
}

//-----------------------------------------------------------------------------
qSlicerAstroSourcesSection* qSlicerAstroVolumeModuleWidget::sourcesSection() const
{
  Q_D(const qSlicerAstroVolumeModuleWidget);
  return d->SourcesSection;
}

//-----------------------------------------------------------------------------
qSlicerAstroInfoSection* qSlicerAstroVolumeModuleWidget::infoSection() const
{
  Q_D(const qSlicerAstroVolumeModuleWidget);
  return d->InfoSection;
}
