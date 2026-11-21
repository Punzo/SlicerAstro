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

==============================================================================*/

// Qt includes
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

// STD includes
#include <algorithm>

// VTK includes
#include <vtkCollection.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkTable.h>
#include <vtkWeakPointer.h>

// MRML includes
#include <qMRMLTableView.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLMarkupsDisplayNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLMessageCollection.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLTableNode.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerCoreIOManager.h>
#include <qSlicerModuleManager.h>

#include "qSlicerAstroColorPicker.h"
#include "qSlicerAstroSourcesSection.h"
#include "qSlicerAstroValueField.h"
#include "qSlicerAstroTheme.h"

namespace
{
//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* AstroVolumeLogic()
{
  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerAbstractCoreModule* module = app && app->moduleManager() ? app->moduleManager()->module("AstroVolume") : nullptr;
  return module ? vtkSlicerAstroVolumeLogic::SafeDownCast(module->logic()) : nullptr;
}

//----------------------------------------------------------------------------
QToolButton* OptionButton(const QString& text, const QString& toolTip, const QString& objectName, QWidget* parent)
{
  QToolButton* button = new QToolButton(parent);
  button->setProperty("astroRole", "actionChip");
  button->setText(text);
  button->setCheckable(true);
  button->setToolTip(toolTip);
  button->setObjectName(objectName);
  button->setCursor(Qt::PointingHandCursor);
  return button;
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroSourcesSectionPrivate
{
public:
  vtkMRMLTableNode* catalogue() const;
  vtkMRMLMarkupsDisplayNode* pointsDisplay() const;

  vtkWeakPointer<vtkMRMLAstroVolumeNode> Volume;
  vtkWeakPointer<vtkMRMLScene> Scene;
  QComboBox* Catalogue{nullptr};
  QPushButton* LoadButton{nullptr};
  QToolButton* RemoveButton{nullptr};
  qMRMLTableView* Table{nullptr};
  QLabel* Empty{nullptr};
  QToolButton* PointsButton{nullptr};
  QToolButton* LabelsButton{nullptr};
  QToolButton* SlicesButton{nullptr};
  QToolButton* MoveButton{nullptr};
  QToolButton* ColorButton{nullptr};
  QToolButton* SelectedColorButton{nullptr};
  qSlicerAstroValueField* SizeField{nullptr};
  QPushButton* DeleteButton{nullptr};
  QWidget* PointsRow{nullptr};
  QLabel* Status{nullptr};
  QTimer* UpdateTimer{nullptr};
  QString SelectedSegmentID;
};

//-----------------------------------------------------------------------------
vtkMRMLTableNode* qSlicerAstroSourcesSectionPrivate::catalogue() const
{
  const QString nodeID = this->Catalogue->currentData().toString();
  return (this->Scene && !nodeID.isEmpty()) ?
    vtkMRMLTableNode::SafeDownCast(this->Scene->GetNodeByID(nodeID.toUtf8().constData())) : nullptr;
}

//-----------------------------------------------------------------------------
vtkMRMLMarkupsDisplayNode* qSlicerAstroSourcesSectionPrivate::pointsDisplay() const
{
  vtkMRMLMarkupsFiducialNode* points = vtkSlicerAstroVolumeLogic::GetSourceCataloguePoints(this->catalogue());
  return points ? vtkMRMLMarkupsDisplayNode::SafeDownCast(points->GetDisplayNode()) : nullptr;
}

//-----------------------------------------------------------------------------
qSlicerAstroSourcesSection::qSlicerAstroSourcesSection(QWidget* parent)
  : QWidget(parent)
  , d_ptr(new qSlicerAstroSourcesSectionPrivate)
{
  Q_D(qSlicerAstroSourcesSection);
  this->setObjectName("SourcesSection");
  this->setToolTip(tr("Catalogues of sources placed on the data (e.g. SoFiA)."));
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  QHBoxLayout* row = new QHBoxLayout();
  row->setSpacing(4);
  d->Catalogue = new QComboBox(this);
  d->Catalogue->setObjectName("SourcesCatalogue");
  d->Catalogue->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  d->Catalogue->setMinimumContentsLength(8);
  d->Catalogue->setToolTip(tr("Catalogue shown in the table."));
  QObject::connect(d->Catalogue, SIGNAL(currentIndexChanged(int)), this, SLOT(onCatalogueChanged(int)));
  d->LoadButton = new QPushButton(tr("Import"), this);
  d->LoadButton->setObjectName("SourcesLoad");
  d->LoadButton->setIcon(qSlicerAstroTheme::instance()->icon("folder", "text"));
  d->LoadButton->setToolTip(tr("Import a catalogue (SoFiA text or VOTable, CSV or text with RA, Dec and velocity "
                               "or frequency) and place its sources on the data."));
  QObject::connect(d->LoadButton, &QPushButton::clicked, this, [this]() { this->loadCatalogue(); });
  d->RemoveButton = new QToolButton(this);
  d->RemoveButton->setObjectName("SourcesRemoveCatalogue");
  d->RemoveButton->setProperty("astroRole", "iconButton");
  d->RemoveButton->setIcon(qSlicerAstroTheme::instance()->icon("trash", "muted"));
  d->RemoveButton->setToolTip(tr("Remove the catalogue from the scene (its table and the points of its sources)."));
  QObject::connect(d->RemoveButton, &QToolButton::clicked, this, [this]() { this->removeCatalogue(true); });
  row->addWidget(d->Catalogue, 1);
  row->addWidget(d->LoadButton);
  row->addWidget(d->RemoveButton);
  layout->addLayout(row);

  d->Table = new qMRMLTableView(this);
  d->Table->setObjectName("SourcesTable");
  d->Table->setMinimumHeight(140);
  d->Table->setMaximumHeight(240);
  d->Table->setMinimumWidth(80);
  d->Table->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->Table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  d->Table->setToolTip(tr("Sources of the catalogue: click a source to centre the views on it (Ctrl/Shift: "
                          "several sources, drawn with the selected colour)."));
  layout->addWidget(d->Table);
  if (d->Table->selectionModel())
    {
    QObject::connect(d->Table->selectionModel(), &QItemSelectionModel::currentRowChanged,
                     this, &qSlicerAstroSourcesSection::onCurrentRowChanged);
    QObject::connect(d->Table->selectionModel(), &QItemSelectionModel::selectionChanged,
                     this, &qSlicerAstroSourcesSection::onSelectionChanged);
    }
  d->Empty = new QLabel(tr("No catalogue"), this);
  d->Empty->setObjectName("SourcesEmpty");
  d->Empty->setProperty("astroRole", "label");
  d->Empty->setToolTip(tr("Import a catalogue of sources (e.g. the SoFiA output of these data)."));
  layout->addWidget(d->Empty);

  QHBoxLayout* toggles = new QHBoxLayout();
  toggles->setSpacing(4);
  d->PointsButton = OptionButton(tr("Points"), tr("Show the positions of the sources in the 3D views."),
                                 "SourcesPoints", this);
  d->LabelsButton = OptionButton(tr("Labels"), tr("Show the names of the sources next to their points."),
                                 "SourcesLabels", this);
  d->SlicesButton = OptionButton(tr("2D"), tr("Show the sources in the 2D views (only where a slice passes through "
                                              "them)."), "SourcesSlices", this);
  QObject::connect(d->PointsButton, SIGNAL(toggled(bool)), this, SLOT(setPointsVisible(bool)));
  QObject::connect(d->LabelsButton, SIGNAL(toggled(bool)), this, SLOT(setLabelsVisible(bool)));
  d->MoveButton = OptionButton(tr("Move"), tr("Move the points of the sources with the mouse (the points are "
                                             "locked otherwise)."), "SourcesMove", this);
  QObject::connect(d->SlicesButton, SIGNAL(toggled(bool)), this, SLOT(setSlicesVisible(bool)));
  QObject::connect(d->MoveButton, SIGNAL(toggled(bool)), this, SLOT(setPointsMovable(bool)));
  for (QToolButton* button : {d->PointsButton, d->LabelsButton, d->SlicesButton, d->MoveButton})
    {
    toggles->addWidget(button);
    }
  toggles->addStretch(1);
  layout->addLayout(toggles);

  // colours and size of the points, deletion of the selected sources
  d->PointsRow = new QWidget(this);
  QHBoxLayout* pointsRow = new QHBoxLayout(d->PointsRow);
  pointsRow->setContentsMargins(0, 0, 0, 0);
  pointsRow->setSpacing(4);
  d->ColorButton = new QToolButton(d->PointsRow);
  d->ColorButton->setObjectName("SourcesColor");
  d->ColorButton->setProperty("astroRole", "iconButton");
  d->ColorButton->setToolTip(tr("Colour of the points of the sources."));
  d->SelectedColorButton = new QToolButton(d->PointsRow);
  d->SelectedColorButton->setObjectName("SourcesSelectedColor");
  d->SelectedColorButton->setProperty("astroRole", "iconButton");
  d->SelectedColorButton->setToolTip(tr("Colour of the points of the sources selected in the table."));
  QObject::connect(d->ColorButton, &QToolButton::clicked, this, [this, d]()
    {
    vtkMRMLMarkupsDisplayNode* display = d->pointsDisplay();
    if (!display)
      {
      return;
      }
    bool ok = false;
    const QColor color = qSlicerAstroColorPicker::getColor(QColor::fromRgbF(display->GetColor()[0],
      display->GetColor()[1], display->GetColor()[2]), this->window(), tr("Colour of the sources"), &ok);
    if (ok)
      {
      this->setPointColor(color);
      }
    });
  QObject::connect(d->SelectedColorButton, &QToolButton::clicked, this, [this, d]()
    {
    vtkMRMLMarkupsDisplayNode* display = d->pointsDisplay();
    if (!display)
      {
      return;
      }
    bool ok = false;
    const QColor color = qSlicerAstroColorPicker::getColor(QColor::fromRgbF(display->GetSelectedColor()[0],
      display->GetSelectedColor()[1], display->GetSelectedColor()[2]), this->window(),
      tr("Colour of the selected sources"), &ok);
    if (ok)
      {
      this->setSelectedPointColor(color);
      }
    });
  d->SizeField = new qSlicerAstroValueField(d->PointsRow);
  d->SizeField->setObjectName("SourcesSize");
  d->SizeField->setLeadingText(tr("Size"));
  d->SizeField->setDecimals(1);
  d->SizeField->setRange(0.1, 20.);
  d->SizeField->setSingleStep(0.5);
  d->SizeField->setToolTip(tr("Size of the points (relative to the views)."));
  QObject::connect(d->SizeField, SIGNAL(valueChanged(double)), this, SLOT(setPointSize(double)));
  d->DeleteButton = new QPushButton(qSlicerAstroTheme::instance()->icon("trash", "text"), tr("Delete"), d->PointsRow);
  d->DeleteButton->setObjectName("SourcesDeleteSelected");
  d->DeleteButton->setToolTip(tr("Delete the selected sources from the catalogue (rows of the table and points)."));
  QObject::connect(d->DeleteButton, SIGNAL(clicked()), this, SLOT(deleteSelectedSources()));
  pointsRow->addWidget(d->ColorButton);
  pointsRow->addWidget(d->SelectedColorButton);
  pointsRow->addWidget(d->SizeField, 1);
  pointsRow->addWidget(d->DeleteButton);
  layout->addWidget(d->PointsRow);
  d->Status = new QLabel(this);
  d->Status->setObjectName("SourcesStatus");
  d->Status->setProperty("astroRole", "monoLabel");
  d->Status->setWordWrap(true);
  d->Status->setToolTip(tr("The selected source, or the result of the last import."));
  layout->addWidget(d->Status);

  d->UpdateTimer = new QTimer(this);
  d->UpdateTimer->setSingleShot(true);
  d->UpdateTimer->setInterval(0);
  QObject::connect(d->UpdateTimer, SIGNAL(timeout()), this, SLOT(updateFromScene()));
  this->setEnabled(false);
}

//-----------------------------------------------------------------------------
qSlicerAstroSourcesSection::~qSlicerAstroSourcesSection() = default;

//-----------------------------------------------------------------------------
int qSlicerAstroSourcesSection::catalogueCount() const
{
  Q_D(const qSlicerAstroSourcesSection);
  return d->Catalogue->count();
}

//-----------------------------------------------------------------------------
int qSlicerAstroSourcesSection::sourceCount() const
{
  Q_D(const qSlicerAstroSourcesSection);
  vtkMRMLTableNode* table = d->catalogue();
  return table && table->GetTable() ? static_cast<int>(table->GetTable()->GetNumberOfRows()) : 0;
}

//-----------------------------------------------------------------------------
QString qSlicerAstroSourcesSection::status() const
{
  Q_D(const qSlicerAstroSourcesSection);
  return d->Status->text();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroSourcesSection::selectedSegmentID() const
{
  Q_D(const qSlicerAstroSourcesSection);
  return d->SelectedSegmentID;
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::setVolumeNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroSourcesSection);
  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  if (volume && !volume->GetScene())
    {
    // removed from the scene (e.g. while the scene is closed)
    volume = nullptr;
    }
  vtkMRMLScene* scene = volume ? volume->GetScene() : nullptr;
  if (scene != d->Scene.GetPointer())
    {
    this->qvtkReconnect(d->Scene, scene, vtkMRMLScene::NodeAddedEvent, d->UpdateTimer, SLOT(start()));
    this->qvtkReconnect(d->Scene, scene, vtkMRMLScene::NodeRemovedEvent, d->UpdateTimer, SLOT(start()));
    d->Scene = scene;
    d->Table->setMRMLScene(scene);
    }
  if (volume != d->Volume.GetPointer())
    {
    d->Status->clear();
    d->SelectedSegmentID.clear();
    }
  d->Volume = volume;
  this->updateFromScene();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::updateFromScene()
{
  Q_D(qSlicerAstroSourcesSection);
  this->setEnabled(d->Volume != nullptr);
  const QString current = d->Catalogue->currentData().toString();
  vtkNew<vtkCollection> tables;
  vtkSlicerAstroVolumeLogic::GetSourceCatalogues(d->Volume, tables);
    {
    const QSignalBlocker blocker(d->Catalogue);
    d->Catalogue->clear();
    for (int index = 0; index < tables->GetNumberOfItems(); index++)
      {
      vtkMRMLTableNode* table = vtkMRMLTableNode::SafeDownCast(tables->GetItemAsObject(index));
      d->Catalogue->addItem(QString::fromUtf8(table->GetName()), QString::fromUtf8(table->GetID()));
      }
    const int index = d->Catalogue->findData(current);
    d->Catalogue->setCurrentIndex(index >= 0 ? index : 0);
    }
  this->onCatalogueChanged(d->Catalogue->currentIndex());
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::setCatalogueNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroSourcesSection);
  const int index = node ? d->Catalogue->findData(QString::fromUtf8(node->GetID())) : -1;
  if (index >= 0)
    {
    d->Catalogue->setCurrentIndex(index);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::onCatalogueChanged(int index)
{
  Q_UNUSED(index);
  Q_D(qSlicerAstroSourcesSection);
  vtkMRMLTableNode* table = d->catalogue();
  if (d->Table->mrmlTableNode() != table)
    {
    d->Table->setMRMLTableNode(table);
    }
  const bool hasCatalogue = table != nullptr;
  d->Table->setVisible(hasCatalogue);
  d->Catalogue->setVisible(hasCatalogue);
  d->Empty->setVisible(!hasCatalogue);
  vtkMRMLMarkupsDisplayNode* displayNode = d->pointsDisplay();
  const bool states[3] = {displayNode && displayNode->GetVisibility() && displayNode->GetVisibility3D(),
                          displayNode && displayNode->GetPointLabelsVisibility(),
                          displayNode && displayNode->GetVisibility2D()};
  QToolButton* buttons[3] = {d->PointsButton, d->LabelsButton, d->SlicesButton};
  for (int i = 0; i < 3; i++)
    {
    const QSignalBlocker blocker(buttons[i]);
    buttons[i]->setChecked(states[i]);
    buttons[i]->setEnabled(displayNode != nullptr);
    }
  {
  const QSignalBlocker blocker(d->MoveButton);
  d->MoveButton->setChecked(displayNode && !vtkSlicerAstroVolumeLogic::GetSourcePointsLocked(table));
  d->MoveButton->setEnabled(displayNode != nullptr);
  }
  d->RemoveButton->setVisible(hasCatalogue);
  d->PointsRow->setVisible(displayNode != nullptr);
  if (displayNode)
    {
    const double* color = displayNode->GetColor();
    const double* selected = displayNode->GetSelectedColor();
    d->ColorButton->setIcon(qSlicerAstroColorPicker::swatchIcon(QColor::fromRgbF(color[0], color[1], color[2])));
    d->SelectedColorButton->setIcon(qSlicerAstroColorPicker::swatchIcon(
      QColor::fromRgbF(selected[0], selected[1], selected[2])));
    const QSignalBlocker blocker(d->SizeField);
    d->SizeField->setValue(displayNode->GetGlyphScale());
    }
  this->onSelectionChanged();
}

//-----------------------------------------------------------------------------
bool qSlicerAstroSourcesSection::loadCatalogue(const QString& fileName)
{
  Q_D(qSlicerAstroSourcesSection);
  qSlicerCoreApplication* app = qSlicerCoreApplication::application();
  if (!d->Volume || !app || !app->coreIOManager())
    {
    return false;
    }
  QString path = fileName;
  if (path.isEmpty())
    {
    path = QFileDialog::getOpenFileName(this, tr("Import a catalogue of sources"), QString(),
                                        tr("Catalogues (*.txt *.cat *.csv *.xml *.vot *.votable);;All files (*)"));
    if (path.isEmpty())
      {
      return false;
      }
    }
  qSlicerIO::IOProperties properties;
  properties["fileName"] = path;
  properties["referenceNodeID"] = QString::fromUtf8(d->Volume->GetID());
  vtkNew<vtkCollection> loaded;
  vtkNew<vtkMRMLMessageCollection> messages;
  QApplication::setOverrideCursor(Qt::WaitCursor);
  const bool success = app->coreIOManager()->loadNodes("SoFiACatalogFile", properties, loaded, messages);
  QApplication::restoreOverrideCursor();
  vtkMRMLTableNode* table = nullptr;
  for (int index = 0; index < loaded->GetNumberOfItems() && !table; index++)
    {
    table = vtkMRMLTableNode::SafeDownCast(loaded->GetItemAsObject(index));
    }
  if (!success || !table)
    {
    const QString error = QString::fromStdString(messages->GetAllMessagesAsString()).trimmed();
    d->Status->setText(error.isEmpty() ? tr("The catalogue could not be imported") : error);
    return false;
    }
  this->updateFromScene();
  this->setCatalogueNode(table);
  const int rows = table->GetTable() ? static_cast<int>(table->GetTable()->GetNumberOfRows()) : 0;
  d->Status->setText(rows == 1 ? tr("1 source imported") : tr("%1 sources imported").arg(rows));
  return true;
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::onCurrentRowChanged(const QModelIndex& current, const QModelIndex& previous)
{
  Q_UNUSED(previous);
  if (current.isValid())
    {
    this->selectSource(current.row());
    // several sources selected: all their points drawn as selected
    this->onSelectionChanged();
    }
}

//-----------------------------------------------------------------------------
QList<int> qSlicerAstroSourcesSection::selectedRows() const
{
  Q_D(const qSlicerAstroSourcesSection);
  QList<int> rows;
  QItemSelectionModel* selection = d->Table->selectionModel();
  if (!selection || !d->catalogue())
    {
    return rows;
    }
  for (const QModelIndex& index : selection->selectedRows())
    {
    if (!rows.contains(index.row()))
      {
      rows << index.row();
      }
    }
  std::sort(rows.begin(), rows.end());
  return rows;
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::onSelectionChanged()
{
  Q_D(qSlicerAstroSourcesSection);
  const QList<int> rows = this->selectedRows();
  d->DeleteButton->setEnabled(!rows.isEmpty());
  vtkMRMLTableNode* table = d->catalogue();
  if (!table || rows.isEmpty())
    {
    return;
    }
  vtkNew<vtkIntArray> selected;
  for (int row : rows)
    {
    selected->InsertNextValue(row);
    }
  vtkSlicerAstroVolumeLogic::SetSelectedSources(table, selected);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::deleteSelectedSources()
{
  Q_D(qSlicerAstroSourcesSection);
  const QList<int> rows = this->selectedRows();
  vtkMRMLTableNode* table = d->catalogue();
  if (!table || rows.isEmpty())
    {
    return;
    }
  vtkNew<vtkIntArray> deleted;
  for (int row : rows)
    {
    deleted->InsertNextValue(row);
    }
  d->Table->clearSelection();
  vtkSlicerAstroVolumeLogic::DeleteSources(table, deleted);
  d->Status->setText(rows.size() == 1 ? tr("1 source deleted") : tr("%1 sources deleted").arg(rows.size()));
  d->SelectedSegmentID.clear();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::removeCatalogue(bool ask)
{
  Q_D(qSlicerAstroSourcesSection);
  vtkMRMLTableNode* table = d->catalogue();
  if (!table)
    {
    return;
    }
  if (ask)
    {
    // a dialog of the window with the whole theme
    QMessageBox box(QMessageBox::Question, tr("Remove catalogue"),
                    tr("Remove %1 and the points of its sources from the scene?")
                      .arg(QString::fromUtf8(table->GetName())),
                    QMessageBox::Yes | QMessageBox::No, this->window());
    box.setDefaultButton(QMessageBox::No);
    qSlicerAstroTheme::instance()->applyToDialog(&box);
    if (box.exec() != QMessageBox::Yes)
      {
      return;
      }
    }
  d->Table->setMRMLTableNode(static_cast<vtkMRMLTableNode*>(nullptr));
  vtkSlicerAstroVolumeLogic::RemoveSourceCatalogue(table);
  d->Status->clear();
  d->SelectedSegmentID.clear();
  this->updateFromScene();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::setPointsMovable(bool movable)
{
  Q_D(qSlicerAstroSourcesSection);
  vtkSlicerAstroVolumeLogic::SetSourcePointsLocked(d->catalogue(), !movable);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::setPointColor(const QColor& color)
{
  Q_D(qSlicerAstroSourcesSection);
  vtkMRMLMarkupsDisplayNode* displayNode = d->pointsDisplay();
  if (displayNode && color.isValid())
    {
    displayNode->SetColor(color.redF(), color.greenF(), color.blueF());
    d->ColorButton->setIcon(qSlicerAstroColorPicker::swatchIcon(color));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::setSelectedPointColor(const QColor& color)
{
  Q_D(qSlicerAstroSourcesSection);
  vtkMRMLMarkupsDisplayNode* displayNode = d->pointsDisplay();
  if (displayNode && color.isValid())
    {
    displayNode->SetSelectedColor(color.redF(), color.greenF(), color.blueF());
    d->SelectedColorButton->setIcon(qSlicerAstroColorPicker::swatchIcon(color));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::setPointSize(double size)
{
  Q_D(qSlicerAstroSourcesSection);
  vtkMRMLMarkupsDisplayNode* displayNode = d->pointsDisplay();
  if (displayNode && size > 0.)
    {
    displayNode->SetGlyphScale(size);
    displayNode->SetTextScale(std::max(1., size));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::selectSource(int row)
{
  Q_D(qSlicerAstroSourcesSection);
  vtkMRMLTableNode* table = d->catalogue();
  vtkMRMLMarkupsFiducialNode* points = vtkSlicerAstroVolumeLogic::GetSourceCataloguePoints(table);
  if (!points || row < 0 || row >= points->GetNumberOfControlPoints())
    {
    return;
    }
  const std::string sourceID = vtkSlicerAstroVolumeLogic::SelectSource(table, row);
  QStringList parts;
  const QString name = QString::fromStdString(points->GetNthControlPointLabel(row));
  const QString description = QString::fromStdString(points->GetNthControlPointDescription(row));
  for (const QString& part : {name, description})
    {
    if (!part.isEmpty())
      {
      parts << part;
      }
    }
  d->Status->setText(parts.join(QString::fromUtf8(" · ")));

  // the segment of the source in the mask of the data (SoFiA id), selected in
  // the Segment Editor when it edits that mask
  d->SelectedSegmentID.clear();
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  vtkMRMLSegmentationNode* mask = (logic && d->Volume) ? logic->GetMaskSegmentationForVolume(d->Volume, false) : nullptr;
  d->SelectedSegmentID = QString::fromStdString(vtkSlicerAstroVolumeLogic::GetSourceSegmentID(mask, sourceID.c_str()));
  vtkMRMLSegmentEditorNode* editorNode = d->Scene ? vtkMRMLSegmentEditorNode::SafeDownCast(
    d->Scene->GetSingletonNode("SegmentEditor", "vtkMRMLSegmentEditorNode")) : nullptr;
  if (!d->SelectedSegmentID.isEmpty() && editorNode && editorNode->GetSegmentationNode() == mask)
    {
    editorNode->SetSelectedSegmentID(d->SelectedSegmentID.toUtf8().constData());
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::setPointsVisible(bool visible)
{
  Q_D(qSlicerAstroSourcesSection);
  vtkMRMLMarkupsDisplayNode* displayNode = d->pointsDisplay();
  if (displayNode)
    {
    displayNode->SetVisibility(visible || displayNode->GetVisibility2D());
    displayNode->SetVisibility3D(visible);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::setLabelsVisible(bool visible)
{
  Q_D(qSlicerAstroSourcesSection);
  vtkMRMLMarkupsDisplayNode* displayNode = d->pointsDisplay();
  if (displayNode)
    {
    displayNode->SetPointLabelsVisibility(visible);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSourcesSection::setSlicesVisible(bool visible)
{
  Q_D(qSlicerAstroSourcesSection);
  vtkMRMLMarkupsDisplayNode* displayNode = d->pointsDisplay();
  if (displayNode)
    {
    displayNode->SetVisibility2D(visible);
    if (visible)
      {
      displayNode->SetVisibility(true);
      }
    }
}
