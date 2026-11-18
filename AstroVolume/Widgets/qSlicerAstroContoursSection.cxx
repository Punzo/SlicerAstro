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
#include <QApplication>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

// VTK includes
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkStringArray.h>
#include <vtkWeakPointer.h>

// MRML includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentationNode.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerModuleManager.h>

#include <cmath>
#include <vector>

#include "qSlicerAstroContoursSection.h"
#include "qSlicerAstroSegmentedButton.h"
#include "qSlicerAstroTheme.h"

namespace
{
const double ColumnDensityUnit = 1e19;

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* AstroVolumeLogic()
{
  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerAbstractCoreModule* module = app && app->moduleManager() ? app->moduleManager()->module("AstroVolume") : nullptr;
  return module ? vtkSlicerAstroVolumeLogic::SafeDownCast(module->logic()) : nullptr;
}

//----------------------------------------------------------------------------
QPixmap FamilyStrip(const char* family)
{
  vtkNew<vtkDoubleArray> colors;
  vtkSlicerAstroVolumeLogic::GetContourFamilyColors(family, 5, colors);
  QImage image(50, 12, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  for (int i = 0; i < 5; i++)
    {
    double rgb[3];
    colors->GetTuple(i, rgb);
    const QRgb colour = QColor::fromRgbF(rgb[0], rgb[1], rgb[2]).rgb();
    for (int x = i * 10; x < i * 10 + 9; x++)
      {
      for (int y = 0; y < 12; y++)
        {
        image.setPixel(x, y, colour);
        }
      }
    }
  return QPixmap::fromImage(image);
}

//----------------------------------------------------------------------------
QToolButton* IconButton(const QString& iconName, const QString& toolTip, const QString& objectName, bool checkable,
                        QWidget* parent)
{
  QToolButton* button = new QToolButton(parent);
  button->setProperty("astroRole", "toolbar");
  button->setIcon(qSlicerAstroTheme::instance()->icon(iconName, "muted"));
  button->setIconSize(QSize(16, 16));
  button->setToolTip(toolTip);
  button->setCheckable(checkable);
  button->setAutoRaise(true);
  button->setObjectName(objectName);
  return button;
}

//----------------------------------------------------------------------------
QLabel* RowLabel(const QString& text, const QString& toolTip, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  label->setProperty("astroRole", "label");
  label->setToolTip(toolTip);
  return label;
}

} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroContoursSectionPrivate
{
public:
  vtkMRMLAstroVolumeNode* sourceVolume() const;
  vtkMRMLSegmentationNode* contoursNode(bool create) const;

  vtkWeakPointer<vtkMRMLAstroVolumeNode> Volume;
  vtkWeakPointer<vtkMRMLScene> Scene;
  QComboBox* Source{nullptr};
  QLineEdit* Levels{nullptr};
  qSlicerAstroSegmentedButton* Unit{nullptr};
  QLabel* UnitInfo{nullptr};
  QComboBox* Colours{nullptr};
  QPushButton* DrawButton{nullptr};
  QToolButton* ShowButton{nullptr};
  QToolButton* SmoothButton{nullptr};
  QToolButton* DeleteButton{nullptr};
  QLabel* Status{nullptr};
  QWidget* Sources{nullptr};
  QVBoxLayout* SourcesLayout{nullptr};
};

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroContoursSectionPrivate::sourceVolume() const
{
  const QString nodeID = this->Source->currentData().toString();
  vtkMRMLAstroVolumeNode* source = (this->Scene && !nodeID.isEmpty()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(this->Scene->GetNodeByID(nodeID.toUtf8().constData())) : nullptr;
  return source ? source : this->Volume.GetPointer();
}

//-----------------------------------------------------------------------------
vtkMRMLSegmentationNode* qSlicerAstroContoursSectionPrivate::contoursNode(bool create) const
{
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  return (logic && this->Volume) ? logic->GetContoursSegmentationForVolume(this->Volume, create) : nullptr;
}

//-----------------------------------------------------------------------------
qSlicerAstroContoursSection::qSlicerAstroContoursSection(QWidget* parent)
  : QWidget(parent)
  , d_ptr(new qSlicerAstroContoursSectionPrivate)
{
  Q_D(qSlicerAstroContoursSection);
  this->setObjectName("ContoursSection");
  this->setToolTip(tr("Contour levels drawn over the data in the 2D views."));
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);
  QGridLayout* grid = new QGridLayout();
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(6);

  d->Source = new QComboBox(this);
  d->Source->setObjectName("ContoursSource");
  d->Source->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  d->Source->setMinimumContentsLength(8);
  d->Source->setToolTip(tr("Data of the contours: these data, or other data (e.g. an HI cube or moment-0 map over "
                           "an optical image), reprojected on the sky grid of these data."));
  QObject::connect(d->Source, SIGNAL(currentIndexChanged(int)), this, SLOT(onSourceChanged(int)));
  grid->addWidget(RowLabel(tr("Source"), d->Source->toolTip(), this), 0, 0);
  grid->addWidget(d->Source, 0, 1, 1, 2);

  d->Levels = new QLineEdit("3, 6, 12, 24", this);
  d->Levels->setObjectName("ContoursLevels");
  d->Levels->setToolTip(tr("Contour levels separated by commas, in the unit on the right. A negative level selects "
                           "the values below it."));
  d->Unit = new qSlicerAstroSegmentedButton(this);
  d->Unit->setObjectName("ContoursUnit");
  d->Unit->setToolTip(tr("Unit of the levels."));
  d->Unit->addButton(QString::fromUtf8("σ"), tr("Levels in units of the noise σ of the source."));
  d->Unit->addButton("N_HI", tr("Levels in HI column density, units of 10¹⁹ cm⁻² (HI moment-0 maps with a beam)."));
  d->Unit->addButton(tr("value"), tr("Levels in the units of the source data."));
  QObject::connect(d->Unit, SIGNAL(currentIndexChanged(int)), this, SLOT(onUnitChanged(int)));
  grid->addWidget(RowLabel(tr("Levels"), d->Levels->toolTip(), this), 1, 0);
  grid->addWidget(d->Levels, 1, 1);
  grid->addWidget(d->Unit, 1, 2);
  d->UnitInfo = new QLabel(this);
  d->UnitInfo->setObjectName("ContoursUnitInfo");
  d->UnitInfo->setProperty("astroRole", "monoLabel");
  d->UnitInfo->setToolTip(tr("Noise and unit of the source data."));
  grid->addWidget(d->UnitInfo, 2, 1, 1, 2);

  d->Colours = new QComboBox(this);
  d->Colours->setObjectName("ContoursColours");
  d->Colours->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  d->Colours->setMinimumContentsLength(8);
  d->Colours->setIconSize(QSize(50, 12));
  d->Colours->setToolTip(tr("Colours of the levels, from the lowest to the highest."));
  vtkNew<vtkStringArray> families;
  vtkSlicerAstroVolumeLogic::GetContourColorFamilies(families);
  for (vtkIdType i = 0; i < families->GetNumberOfValues(); i++)
    {
    d->Colours->addItem(QIcon(FamilyStrip(families->GetValue(i).c_str())), QString::fromStdString(families->GetValue(i)));
    }
  grid->addWidget(RowLabel(tr("Colours"), d->Colours->toolTip(), this), 3, 0);
  grid->addWidget(d->Colours, 3, 1, 1, 2);
  grid->setColumnStretch(1, 1);
  layout->addLayout(grid);

  QHBoxLayout* buttons = new QHBoxLayout();
  buttons->setSpacing(4);
  d->DrawButton = new QPushButton(tr("Draw contours"), this);
  d->DrawButton->setObjectName("ContoursDraw");
  d->DrawButton->setProperty("astroRole", "primary");
  d->DrawButton->setIcon(qSlicerAstroTheme::instance()->icon("contour", "on-accent"));
  d->DrawButton->setToolTip(tr("Draw the levels. The contours of the same source are replaced, those of other "
                               "sources are kept."));
  QObject::connect(d->DrawButton, &QPushButton::clicked, this, [this]() { this->drawContours(); });
  buttons->addWidget(d->DrawButton, 1);
  d->ShowButton = IconButton("eye", tr("Show or hide all the contours of the data."), "ContoursShow", true, this);
  d->SmoothButton = IconButton("smoothing", tr("Smooth contour lines (slower) instead of the outlines of the pixels."),
                               "ContoursSmooth", true, this);
  d->DeleteButton = IconButton("trash", tr("Delete all the contours of the data."), "ContoursDelete", false, this);
  QObject::connect(d->ShowButton, SIGNAL(toggled(bool)), this, SLOT(setContoursVisible(bool)));
  QObject::connect(d->SmoothButton, SIGNAL(toggled(bool)), this, SLOT(setContoursSmooth(bool)));
  QObject::connect(d->DeleteButton, SIGNAL(clicked()), this, SLOT(deleteContours()));
  buttons->addWidget(d->ShowButton);
  buttons->addWidget(d->SmoothButton);
  buttons->addWidget(d->DeleteButton);
  layout->addLayout(buttons);

  d->Status = new QLabel(this);
  d->Status->setObjectName("ContoursStatus");
  d->Status->setProperty("astroRole", "monoLabel");
  d->Status->setWordWrap(true);
  d->Status->setToolTip(tr("Result of the last contours."));
  layout->addWidget(d->Status);
  d->Sources = new QWidget(this);
  d->SourcesLayout = new QVBoxLayout(d->Sources);
  d->SourcesLayout->setContentsMargins(0, 0, 0, 0);
  d->SourcesLayout->setSpacing(2);
  layout->addWidget(d->Sources);
  this->setEnabled(false);
}

//-----------------------------------------------------------------------------
qSlicerAstroContoursSection::~qSlicerAstroContoursSection() = default;

//-----------------------------------------------------------------------------
QString qSlicerAstroContoursSection::status() const
{
  Q_D(const qSlicerAstroContoursSection);
  return d->Status->text();
}

//-----------------------------------------------------------------------------
void qSlicerAstroContoursSection::setVolumeNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroContoursSection);
  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  const bool changed = volume != d->Volume.GetPointer();
  vtkMRMLScene* scene = volume ? volume->GetScene() : nullptr;
  if (scene != d->Scene.GetPointer())
    {
    this->qvtkReconnect(d->Scene, scene, vtkMRMLScene::NodeAddedEvent, this, SLOT(onSceneNodeAddedOrRemoved()));
    this->qvtkReconnect(d->Scene, scene, vtkMRMLScene::NodeRemovedEvent, this, SLOT(onSceneNodeAddedOrRemoved()));
    d->Scene = scene;
    }
  d->Volume = volume;
  this->setEnabled(volume != nullptr);
  if (changed)
    {
    d->Status->clear();
    }
  // sources: this dataset, then the other data (its own contours by default
  // for another dataset)
  const QString current = changed ? QString() : d->Source->currentData().toString();
    {
    const QSignalBlocker blocker(d->Source);
    d->Source->clear();
    if (volume)
      {
      d->Source->addItem(tr("This dataset"), QString::fromUtf8(volume->GetID()));
      std::vector<vtkMRMLNode*> volumes;
      scene->GetNodesByClass("vtkMRMLAstroVolumeNode", volumes);
      for (vtkMRMLNode* other : volumes)
        {
        vtkMRMLAstroVolumeNode* otherVolume = vtkMRMLAstroVolumeNode::SafeDownCast(other);
        if (!otherVolume || otherVolume == volume || !otherVolume->GetImageData() || otherVolume->GetHideFromEditors())
          {
          continue;
          }
        const char* naxis = otherVolume->GetAttribute("SlicerAstro.NAXIS");
        const bool hiMap = vtkSlicerAstroVolumeLogic::GetHIColumnDensityFactor(otherVolume) > 0.;
        const QString kind = hiMap ? tr("HI map") : (naxis && QString(naxis).toInt() >= 3 ? tr("cube") : tr("image"));
        d->Source->addItem(QString::fromUtf8("%1 · %2").arg(QString::fromUtf8(otherVolume->GetName()), kind),
                           QString::fromUtf8(otherVolume->GetID()));
        }
      }
    const int index = current.isEmpty() ? 0 : d->Source->findData(current);
    d->Source->setCurrentIndex(std::max(index, 0));
    }
  this->onUnitChanged(d->Unit->currentIndex());
  this->updateSources();
}

//-----------------------------------------------------------------------------
void qSlicerAstroContoursSection::onSceneNodeAddedOrRemoved()
{
  Q_D(qSlicerAstroContoursSection);
  if (d->Scene && !d->Scene->IsBatchProcessing())
    {
    this->setVolumeNode(d->Volume);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroContoursSection::setSourceNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroContoursSection);
  const int index = node ? d->Source->findData(QString::fromUtf8(node->GetID())) : 0;
  d->Source->setCurrentIndex(std::max(index, 0));
}

//-----------------------------------------------------------------------------
void qSlicerAstroContoursSection::setLevels(const QString& levels, int unit)
{
  Q_D(qSlicerAstroContoursSection);
  d->Levels->setText(levels);
  d->Unit->setCurrentIndex(unit);
}

//-----------------------------------------------------------------------------
void qSlicerAstroContoursSection::onSourceChanged(int index)
{
  Q_UNUSED(index);
  Q_D(qSlicerAstroContoursSection);
  this->onUnitChanged(d->Unit->currentIndex());
}

//-----------------------------------------------------------------------------
void qSlicerAstroContoursSection::onUnitChanged(int index)
{
  Q_D(qSlicerAstroContoursSection);
  vtkMRMLAstroVolumeNode* source = d->sourceVolume();
  if (!source)
    {
    d->UnitInfo->clear();
    return;
    }
  const double factor = vtkSlicerAstroVolumeLogic::GetHIColumnDensityFactor(source);
  d->Unit->button(ColumnDensity)->setEnabled(factor > 0.);
  if (index == ColumnDensity && factor <= 0.)
    {
    d->Unit->setCurrentIndex(Sigma);
    return;
    }
  const QString unit = QString::fromUtf8(source->GetAttribute("SlicerAstro.BUNIT"));
  const double noise = source->GetDisplayThreshold();
  if (index == Sigma)
    {
    d->UnitInfo->setText(noise > 0. ? QString::fromUtf8("σ = %1").arg(qSlicerAstroTheme::formatFlux(noise, unit)) : tr("σ unknown"));
    }
  else if (index == ColumnDensity)
    {
    d->UnitInfo->setText(QString("1 = %1 %2").arg(ColumnDensityUnit / factor, 0, 'g', 3)
                           .arg(qSlicerAstroTheme::displayUnit(unit)));
    }
  else
    {
    d->UnitInfo->setText(qSlicerAstroTheme::displayUnit(unit));
    }
}

//-----------------------------------------------------------------------------
int qSlicerAstroContoursSection::drawContours()
{
  Q_D(qSlicerAstroContoursSection);
  vtkMRMLAstroVolumeNode* source = d->sourceVolume();
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (!d->Volume || !source || !logic)
    {
    return 0;
    }
  std::vector<double> values;
  for (QString text : d->Levels->text().replace(';', ',').split(',', Qt::SkipEmptyParts))
    {
    bool ok = false;
    const double value = text.trimmed().replace(QChar(0x2212), '-').toDouble(&ok);
    if (!ok)
      {
      d->Status->setText(tr("Levels: numbers separated by commas"));
      return 0;
      }
    values.push_back(value);
    }
  if (values.empty())
    {
    d->Status->setText(tr("No levels"));
    return 0;
    }
  double scale = 1.;
  if (d->Unit->currentIndex() == Sigma)
    {
    scale = source->GetDisplayThreshold();
    if (!(scale > 0.))
      {
      d->Status->setText(tr("The noise of the source is unknown: use values"));
      return 0;
      }
    }
  else if (d->Unit->currentIndex() == ColumnDensity)
    {
    scale = ColumnDensityUnit / vtkSlicerAstroVolumeLogic::GetHIColumnDensityFactor(source);
    }
  vtkNew<vtkDoubleArray> levels;
  for (double value : values)
    {
    levels->InsertNextValue(value * scale);
    }
  vtkNew<vtkDoubleArray> colours;
  vtkSlicerAstroVolumeLogic::GetContourFamilyColors(d->Colours->currentText().toUtf8().constData(),
                                                    static_cast<int>(values.size()), colours);
  QApplication::setOverrideCursor(Qt::WaitCursor);
  const int created = logic->CreateContours(d->Volume, source, levels, false, true, colours, false);
  QApplication::restoreOverrideCursor();
  const QString message = QString::fromStdString(logic->GetLastContoursMessage());
  if (created > 0)
    {
    d->Status->setText((created == 1 ? tr("1 level drawn") : tr("%1 levels drawn").arg(created)) +
                       (message.isEmpty() ? QString() : QString::fromUtf8(" · ") + message));
    }
  else
    {
    d->Status->setText(message.isEmpty() ? tr("No contours") : message);
    }
  this->updateSources();
  return created;
}

//-----------------------------------------------------------------------------
void qSlicerAstroContoursSection::setContoursVisible(bool visible)
{
  Q_D(qSlicerAstroContoursSection);
  vtkMRMLSegmentationNode* contours = d->contoursNode(false);
  if (contours && contours->GetDisplayNode())
    {
    contours->GetDisplayNode()->SetVisibility(visible);
    }
  this->updateSources();
}

//-----------------------------------------------------------------------------
void qSlicerAstroContoursSection::setContoursSmooth(bool smooth)
{
  Q_D(qSlicerAstroContoursSection);
  vtkMRMLSegmentationNode* contours = d->contoursNode(false);
  if (contours)
    {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    vtkSlicerAstroVolumeLogic::SetContoursSmooth2D(contours, smooth);
    QApplication::restoreOverrideCursor();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroContoursSection::deleteContours()
{
  Q_D(qSlicerAstroContoursSection);
  vtkMRMLSegmentationNode* contours = d->contoursNode(false);
  if (contours && contours->GetSegmentation())
    {
    contours->GetSegmentation()->RemoveAllSegments();
    }
  d->Status->clear();
  this->updateSources();
}

//-----------------------------------------------------------------------------
void qSlicerAstroContoursSection::updateSources()
{
  Q_D(qSlicerAstroContoursSection);
  while (QLayoutItem* item = d->SourcesLayout->takeAt(0))
    {
    delete item->widget();
    delete item;
    }
  vtkMRMLSegmentationNode* contours = d->contoursNode(false);
  vtkNew<vtkStringArray> sourceIDs;
  vtkNew<vtkIntArray> levels;
  vtkSlicerAstroVolumeLogic::GetContourSources(contours, sourceIDs, levels);
  const bool hasContours = sourceIDs->GetNumberOfValues() > 0;
  for (QToolButton* button : {d->ShowButton, d->SmoothButton, d->DeleteButton})
    {
    button->setEnabled(hasContours);
    }
    {
    const QSignalBlocker blocker(d->ShowButton);
    d->ShowButton->setChecked(hasContours && contours->GetDisplayNode() && contours->GetDisplayNode()->GetVisibility());
    }
  for (vtkIdType i = 0; i < sourceIDs->GetNumberOfValues(); i++)
    {
    const std::string sourceID = sourceIDs->GetValue(i);
    vtkMRMLNode* source = d->Scene ? d->Scene->GetNodeByID(sourceID) : nullptr;
    const QString name = source && source->GetName() ? QString::fromUtf8(source->GetName()) : QString::fromStdString(sourceID);
    QWidget* row = new QWidget(d->Sources);
    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(6);
    QToolButton* eye = IconButton("eye", tr("Show or hide the contours of %1.").arg(name),
                                  QString("ContoursSourceShow_%1").arg(QString::fromStdString(sourceID)), true, row);
    eye->setChecked(vtkSlicerAstroVolumeLogic::GetContourSourceVisibility(contours, sourceID.c_str()));
    eye->setProperty("sourceID", QString::fromStdString(sourceID));
    QObject::connect(eye, SIGNAL(toggled(bool)), this, SLOT(onSourceVisibilityToggled(bool)));
    const int count = levels->GetValue(i);
    QLabel* text = new QLabel(QString::fromUtf8("%1 · %2").arg(name,
      count == 1 ? tr("1 level") : tr("%1 levels").arg(count)), row);
    text->setProperty("astroRole", "mono");
    text->setToolTip(tr("Contours of %1 drawn over %2.").arg(name, QString::fromUtf8(d->Volume->GetName())));
    rowLayout->addWidget(eye);
    rowLayout->addWidget(text, 1);
    d->SourcesLayout->addWidget(row);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroContoursSection::onSourceVisibilityToggled(bool visible)
{
  Q_D(qSlicerAstroContoursSection);
  QObject* button = this->sender();
  if (button)
    {
    vtkSlicerAstroVolumeLogic::SetContourSourceVisibility(d->contoursNode(false),
      button->property("sourceID").toString().toUtf8().constData(), visible);
    }
}
