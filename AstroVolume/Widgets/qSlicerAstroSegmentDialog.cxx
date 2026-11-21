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
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

// VTK includes
#include <vtkWeakPointer.h>

// MRML includes
#include <vtkCodedEntry.h>
#include <vtkMRMLColorTableNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkSegment.h>
#include <vtkSegmentation.h>

#include "qSlicerAstroColorPicker.h"
#include "qSlicerAstroSegmentDialog.h"
#include "qSlicerAstroTheme.h"

namespace
{
const int IndexRole = Qt::UserRole + 1;
}

//-----------------------------------------------------------------------------
class qSlicerAstroSegmentDialogPrivate
{
public:
  vtkWeakPointer<vtkMRMLSegmentationNode> SegmentationNode;
  vtkWeakPointer<vtkMRMLColorTableNode> Types;
  QString SegmentID;
  QColor Color;
  QLineEdit* Name{nullptr};
  QListWidget* TypeList{nullptr};
  QToolButton* ColorButton{nullptr};
  QLabel* ColorCode{nullptr};
};

//-----------------------------------------------------------------------------
qSlicerAstroSegmentDialog::qSlicerAstroSegmentDialog(QWidget* parent)
  : QDialog(parent)
  , d_ptr(new qSlicerAstroSegmentDialogPrivate)
{
  Q_D(qSlicerAstroSegmentDialog);
  this->setObjectName("AstroSegmentDialog");
  this->setWindowTitle(tr("Segment"));
  this->setMinimumWidth(340);
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(10);

  QLabel* nameTitle = new QLabel(tr("NAME"), this);
  qSlicerAstroTheme::setLabelRole(nameTitle, "overline");
  layout->addWidget(nameTitle);
  d->Name = new QLineEdit(this);
  d->Name->setObjectName("SegmentName");
  d->Name->setToolTip(tr("Name of the segment (e.g. the name of the source)"));
  layout->addWidget(d->Name);

  QLabel* typeTitle = new QLabel(tr("TYPE"), this);
  qSlicerAstroTheme::setLabelRole(typeTitle, "overline");
  layout->addWidget(typeTitle);
  d->TypeList = new QListWidget(this);
  d->TypeList->setObjectName("SegmentType");
  d->TypeList->setToolTip(tr("Astronomical type of the segment (SlicerAstro terminology); its colour becomes the "
                             "colour of the segment"));
  d->TypeList->setIconSize(QSize(14, 14));
  d->TypeList->setMinimumHeight(240);
  d->TypeList->setProperty("astroRole", "segments");
  QObject::connect(d->TypeList, SIGNAL(itemSelectionChanged()), this, SLOT(onTypeChanged()));
  layout->addWidget(d->TypeList, 1);

  QHBoxLayout* colorRow = new QHBoxLayout();
  colorRow->setSpacing(8);
  QLabel* colorTitle = new QLabel(tr("COLOUR"), this);
  qSlicerAstroTheme::setLabelRole(colorTitle, "overline");
  d->ColorButton = new QToolButton(this);
  d->ColorButton->setObjectName("SegmentColor");
  d->ColorButton->setToolTip(tr("Colour of the segment: click to choose another"));
  d->ColorButton->setIconSize(QSize(20, 20));
  d->ColorButton->setFixedSize(32, 30);
  QObject::connect(d->ColorButton, SIGNAL(clicked()), this, SLOT(chooseColor()));
  d->ColorCode = new QLabel(this);
  d->ColorCode->setProperty("astroRole", "monoLabel");
  colorRow->addWidget(colorTitle);
  colorRow->addStretch(1);
  colorRow->addWidget(d->ColorCode);
  colorRow->addWidget(d->ColorButton);
  layout->addLayout(colorRow);

  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  for (QAbstractButton* button : buttons->buttons())
    {
    button->setIcon(QIcon());
    }
  QObject::connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
  QObject::connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
  layout->addWidget(buttons);
  this->setColor(QColor("#F2B24C"));
  qSlicerAstroTheme::instance()->applyToDialog(this);
}

//-----------------------------------------------------------------------------
qSlicerAstroSegmentDialog::~qSlicerAstroSegmentDialog() = default;

//-----------------------------------------------------------------------------
bool qSlicerAstroSegmentDialog::setSegment(vtkMRMLNode* node, const QString& segmentID)
{
  Q_D(qSlicerAstroSegmentDialog);
  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node);
  vtkSegment* segment = segmentationNode && segmentationNode->GetSegmentation() ?
    segmentationNode->GetSegmentation()->GetSegment(segmentID.toStdString()) : nullptr;
  if (!segment)
    {
    return false;
    }
  d->SegmentationNode = segmentationNode;
  d->SegmentID = segmentID;
  d->Name->setText(QString::fromUtf8(segment->GetName()));
  double rgb[3] = {0., 0., 0.};
  segment->GetColor(rgb);
  const QColor segmentColor = QColor::fromRgbF(rgb[0], rgb[1], rgb[2]);

  // the types of the SlicerAstro terminology (its colour table), by category
  vtkMRMLScene* scene = segmentationNode->GetScene();
  d->Types = scene ? vtkMRMLColorTableNode::SafeDownCast(
    scene->GetSingletonNode("SlicerAstro.Terminology", "vtkMRMLColorTableNode")) : nullptr;
  const QSignalBlocker blocker(d->TypeList);
  d->TypeList->clear();
  const std::string terminology = segment->GetTerminology();
  QListWidgetItem* current = nullptr;
  QString category;
  for (int index = 1; d->Types && index < d->Types->GetNumberOfColors(); index++)
    {
    vtkCodedEntry* type = d->Types->GetTerminologyType(index);
    if (!type || !type->GetCodeValue())
      {
      continue;
      }
    vtkCodedEntry* typeCategory = d->Types->GetTerminologyCategory(index);
    const QString categoryName = typeCategory && typeCategory->GetCodeMeaning() ?
      QString::fromUtf8(typeCategory->GetCodeMeaning()) : QString();
    if (categoryName != category)
      {
      // a header of the category (not selectable)
      category = categoryName;
      QListWidgetItem* header = new QListWidgetItem(category.toUpper(), d->TypeList);
      header->setFlags(Qt::NoItemFlags);
      QFont font = header->font();
      font.setBold(true);
      font.setPointSizeF(font.pointSizeF() * 0.85);
      header->setFont(font);
      }
    double color[4] = {0., 0., 0., 1.};
    d->Types->GetColor(index, color);
    QListWidgetItem* item = new QListWidgetItem(
      qSlicerAstroColorPicker::swatchIcon(QColor::fromRgbF(color[0], color[1], color[2]), 14),
      QString::fromUtf8(d->Types->GetColorName(index)), d->TypeList);
    item->setData(IndexRole, index);
    if (!current && !terminology.empty() && terminology.find(type->GetCodeValue()) != std::string::npos)
      {
      current = item;
      }
    }
  if (current)
    {
    d->TypeList->setCurrentItem(current);
    d->TypeList->scrollToItem(current);
    }
  this->setColor(segmentColor);
  return true;
}

//-----------------------------------------------------------------------------
QString qSlicerAstroSegmentDialog::name() const
{
  Q_D(const qSlicerAstroSegmentDialog);
  return d->Name->text().trimmed();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSegmentDialog::setName(const QString& name)
{
  Q_D(qSlicerAstroSegmentDialog);
  d->Name->setText(name);
}

//-----------------------------------------------------------------------------
QColor qSlicerAstroSegmentDialog::color() const
{
  Q_D(const qSlicerAstroSegmentDialog);
  return d->Color;
}

//-----------------------------------------------------------------------------
void qSlicerAstroSegmentDialog::setColor(const QColor& color)
{
  Q_D(qSlicerAstroSegmentDialog);
  if (!color.isValid())
    {
    return;
    }
  d->Color = color;
  d->ColorButton->setIcon(qSlicerAstroColorPicker::swatchIcon(color, 20));
  d->ColorCode->setText(color.name().toUpper());
}

//-----------------------------------------------------------------------------
void qSlicerAstroSegmentDialog::chooseColor()
{
  Q_D(qSlicerAstroSegmentDialog);
  qSlicerAstroColorPicker picker(this);
  picker.setWindowTitle(tr("Colour of %1").arg(this->name()));
  // the palette of the terminology types first
  QList<QColor> palette;
  for (int index = 1; d->Types && index < d->Types->GetNumberOfColors(); index++)
    {
    double color[4] = {0., 0., 0., 1.};
    d->Types->GetColor(index, color);
    const QColor typeColor = QColor::fromRgbF(color[0], color[1], color[2]);
    if (!palette.contains(typeColor))
      {
      palette << typeColor;
      }
    }
  for (const QColor& color : qSlicerAstroColorPicker::defaultPalette())
    {
    if (!palette.contains(color))
      {
      palette << color;
      }
    }
  picker.setPalette(palette);
  picker.setColor(d->Color);
  if (picker.exec() == QDialog::Accepted)
    {
    this->setColor(picker.color());
    }
}

//-----------------------------------------------------------------------------
int qSlicerAstroSegmentDialog::typeIndex() const
{
  Q_D(const qSlicerAstroSegmentDialog);
  QListWidgetItem* item = d->TypeList->currentItem();
  return item && item->data(IndexRole).isValid() ? item->data(IndexRole).toInt() : -1;
}

//-----------------------------------------------------------------------------
void qSlicerAstroSegmentDialog::setTypeIndex(int index)
{
  Q_D(qSlicerAstroSegmentDialog);
  for (int row = 0; row < d->TypeList->count(); row++)
    {
    QListWidgetItem* item = d->TypeList->item(row);
    if (item->data(IndexRole).isValid() && item->data(IndexRole).toInt() == index)
      {
      d->TypeList->setCurrentItem(item);
      return;
      }
    }
}

//-----------------------------------------------------------------------------
QString qSlicerAstroSegmentDialog::terminology() const
{
  Q_D(const qSlicerAstroSegmentDialog);
  const int index = this->typeIndex();
  return (d->Types && index > 0) ? QString::fromStdString(d->Types->GetTerminologyAsString(index)) : QString();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSegmentDialog::onTypeChanged()
{
  Q_D(qSlicerAstroSegmentDialog);
  const int index = this->typeIndex();
  if (d->Types && index > 0)
    {
    // the recommended colour of the type
    double color[4] = {0., 0., 0., 1.};
    d->Types->GetColor(index, color);
    this->setColor(QColor::fromRgbF(color[0], color[1], color[2]));
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroSegmentDialog::apply()
{
  Q_D(qSlicerAstroSegmentDialog);
  vtkSegment* segment = d->SegmentationNode && d->SegmentationNode->GetSegmentation() ?
    d->SegmentationNode->GetSegmentation()->GetSegment(d->SegmentID.toStdString()) : nullptr;
  if (!segment)
    {
    return false;
    }
  MRMLNodeModifyBlocker blocker(d->SegmentationNode);
  if (!this->name().isEmpty())
    {
    segment->SetName(this->name().toUtf8().constData());
    segment->SetNameAutoGenerated(false);
    }
  segment->SetColor(d->Color.redF(), d->Color.greenF(), d->Color.blueF());
  segment->SetColorAutoGenerated(false);
  const QString terminology = this->terminology();
  if (!terminology.isEmpty())
    {
    segment->SetTerminology(terminology.toStdString());
    }
  return true;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroSegmentDialog::editSegment(vtkMRMLNode* segmentationNode, const QString& segmentID, QWidget* parent)
{
  qSlicerAstroSegmentDialog dialog(parent);
  if (!dialog.setSegment(segmentationNode, segmentID))
    {
    return false;
    }
  return dialog.exec() == QDialog::Accepted && dialog.apply();
}
