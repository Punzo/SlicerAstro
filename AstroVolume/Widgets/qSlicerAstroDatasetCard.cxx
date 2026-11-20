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
#include <QDrag>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

// VTK includes
#include <vtkCommand.h>
#include <vtkImageData.h>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLVolumeNode.h>

#include <algorithm>
#include <cmath>

#include "qSlicerAstroDatasetCard.h"
#include "qSlicerAstroTheme.h"
#include "qSlicerAstroThumbnails.h"
#include "qSlicerAstroViewFrame.h"

//-----------------------------------------------------------------------------
/// Preview, name, "active" chip, dimensions and the integrated spectrum
class qSlicerAstroDatasetCardHeader : public QWidget
{
public:
  explicit qSlicerAstroDatasetCardHeader(QWidget* parent)
    : QWidget(parent)
    {
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

  QSize sizeHint() const override
    {
    return QSize(260, 58);
    }

  vtkMRMLVolumeNode* Volume{nullptr};
  bool Active{false};

protected:
  void paintEvent(QPaintEvent* event) override
    {
    Q_UNUSED(event);
    qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const int thumbWidth = 76;
    const int thumbHeight = 58;
    if (this->Volume)
      {
      painter.drawPixmap(0, 0, qSlicerAstroThumbnails::instance()->roundedPixmap(
        this->Volume, thumbWidth, thumbHeight, 7., this->devicePixelRatioF()));
      }
    const int x = thumbWidth + 10;
    const int width = this->width() - x;
    QFont titleFont = theme->font("body");
    titleFont.setWeight(QFont::Bold);
    const QFontMetrics titleMetrics(titleFont);
    painter.setFont(titleFont);
    painter.setPen(theme->color("text"));
    // the name on the whole width (the "active" chip is on the line below)
    QString name = this->Volume ? QString::fromUtf8(this->Volume->GetName()) : QString();
    name = titleMetrics.elidedText(name, Qt::ElideMiddle, width);
    painter.drawText(QRect(x, 0, width, titleMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter, name);
    // dimensions and unit
    const QFont metaFont = theme->font("monoSmall");
    const QFontMetrics metaMetrics(metaFont);
    painter.setFont(metaFont);
    painter.setPen(theme->color("muted"));
    QString meta;
    vtkImageData* imageData = this->Volume ? this->Volume->GetImageData() : nullptr;
    int dims[3] = {0, 0, 0};
    if (imageData)
      {
      imageData->GetDimensions(dims);
      meta = dims[2] > 1 ? QString::fromUtf8("%1×%2×%3").arg(dims[0]).arg(dims[1]).arg(dims[2]) :
                           QString::fromUtf8("%1×%2").arg(dims[0]).arg(dims[1]);
      const QString unit = qSlicerAstroTheme::displayUnit(
        QString::fromUtf8(this->Volume->GetAttribute("SlicerAstro.BUNIT")));
      if (!unit.isEmpty())
        {
        meta += QString::fromUtf8(" · %1").arg(unit);
        }
      }
    const int metaY = titleMetrics.height() + 2;
    // "active" chip at the right end of the dimensions line
    int chipWidth = 0;
    const QString chipText = qSlicerAstroDatasetCard::tr("active");
    if (this->Active)
      {
      chipWidth = metaMetrics.horizontalAdvance(chipText) + 14;
      const QRectF chip(x + width - chipWidth + 0.5, metaY + (metaMetrics.height() - 16) / 2. + 0.5,
                        chipWidth - 1, 16);
      QPainterPath path;
      path.addRoundedRect(chip, 8., 8.);
      painter.fillPath(path, theme->color("accent-soft"));
      painter.setPen(QPen(theme->color("accent"), 1.));
      painter.drawPath(path);
      painter.setPen(theme->color("accent-ink"));
      painter.drawText(chip, Qt::AlignCenter, chipText);
      painter.setPen(theme->color("muted"));
      }
    const int metaWidth = width - (chipWidth > 0 ? chipWidth + 6 : 0);
    painter.drawText(QRect(x, metaY, metaWidth, metaMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                     metaMetrics.elidedText(meta, Qt::ElideRight, metaWidth));
    // integrated spectrum (cubes)
    const QVector<double> spectrum = this->Volume ?
      qSlicerAstroThumbnails::instance()->integratedSpectrum(this->Volume) : QVector<double>();
    const int sparkTop = metaY + metaMetrics.height() + 3;
    const int sparkHeight = this->height() - sparkTop - 1;
    if (spectrum.size() > 1 && sparkHeight > 4)
      {
      double minimum = 0., maximum = 0.;
      bool first = true;
      for (double value : spectrum)
        {
        if (std::isfinite(value))
          {
          minimum = first ? value : std::min(minimum, value);
          maximum = first ? value : std::max(maximum, value);
          first = false;
          }
        }
      if (maximum > minimum)
        {
        QPainterPath line;
        bool drawing = false;
        for (int k = 0; k < spectrum.size(); k++)
          {
          if (!std::isfinite(spectrum[k]))
            {
            drawing = false;
            continue;
            }
          const QPointF point(x + width * k / double(spectrum.size() - 1),
                              sparkTop + sparkHeight * (1. - (spectrum[k] - minimum) / (maximum - minimum)));
          if (drawing)
            {
            line.lineTo(point);
            }
          else
            {
            line.moveTo(point);
            drawing = true;
            }
          }
        painter.setPen(QPen(theme->color("data"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(line);
        }
      }
    }
};

//-----------------------------------------------------------------------------
qSlicerAstroDatasetCard::qSlicerAstroDatasetCard(QWidget* parent)
  : QFrame(parent)
  , Header(new qSlicerAstroDatasetCardHeader(this))
  , ChipsRow(new QWidget(this))
  , ActionChipsRow(new QWidget(this))
  , Products(new QWidget(this))
  , ProductsLayout(new QVBoxLayout(this->Products))
  , Kind("cube")
{
  this->setProperty("astroRole", "datasetCard");
  this->setProperty("activeCard", false);
  this->setCursor(Qt::PointingHandCursor);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  // hover highlight (style sheet :hover)
  this->setAttribute(Qt::WA_Hover, true);
  this->setToolTip(tr("Click: open in the views. Drag: show in one view."));
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(9);
  QHBoxLayout* headerRow = new QHBoxLayout();
  headerRow->setContentsMargins(0, 0, 0, 0);
  headerRow->setSpacing(4);
  headerRow->addWidget(this->Header, 1);
  this->DeleteButton = new QToolButton(this);
  this->DeleteButton->setObjectName("DatasetDelete");
  this->DeleteButton->setProperty("astroRole", "toolbar");
  this->DeleteButton->setAutoRaise(true);
  this->DeleteButton->setIconSize(QSize(15, 15));
  this->DeleteButton->setIcon(qSlicerAstroTheme::instance()->icon("trash", "muted"));
  this->DeleteButton->setToolTip(tr("Delete this dataset (and, if chosen, its products) from the scene."));
  this->DeleteButton->setCursor(Qt::ArrowCursor);
  QObject::connect(this->DeleteButton, &QToolButton::clicked, this, &qSlicerAstroDatasetCard::deleteRequested);
  headerRow->addWidget(this->DeleteButton, 0, Qt::AlignTop);
  layout->addLayout(headerRow);
  QHBoxLayout* chipsLayout = new QHBoxLayout(this->ChipsRow);
  chipsLayout->setContentsMargins(0, 0, 0, 0);
  chipsLayout->setSpacing(5);
  chipsLayout->addStretch(1);
  this->ChipsRow->hide();
  layout->addWidget(this->ChipsRow);
  QHBoxLayout* actionChipsLayout = new QHBoxLayout(this->ActionChipsRow);
  actionChipsLayout->setContentsMargins(0, 0, 0, 0);
  actionChipsLayout->setSpacing(5);
  actionChipsLayout->addStretch(1);
  this->ActionChipsRow->hide();
  layout->addWidget(this->ActionChipsRow);
  this->Products->setProperty("astroRole", "lineage");
  this->Products->setAttribute(Qt::WA_StyledBackground, true);
  this->ProductsLayout->setContentsMargins(8, 0, 0, 0);
  this->ProductsLayout->setSpacing(2);
  this->Products->hide();
  layout->addWidget(this->Products);
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this->Header, SLOT(update()));
}

//-----------------------------------------------------------------------------
qSlicerAstroDatasetCard::~qSlicerAstroDatasetCard() = default;

//-----------------------------------------------------------------------------
vtkMRMLVolumeNode* qSlicerAstroDatasetCard::volume() const
{
  return this->Volume;
}

//-----------------------------------------------------------------------------
QString qSlicerAstroDatasetCard::volumeID() const
{
  return (this->Volume && this->Volume->GetID()) ? QString::fromUtf8(this->Volume->GetID()) : QString();
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::setVolume(vtkMRMLVolumeNode* volume)
{
  this->qvtkReconnect(this->Volume, volume, vtkCommand::ModifiedEvent, this, SLOT(onVolumeModified()));
  this->Volume = volume;
  this->Header->Volume = volume;
  this->Header->update();
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::onVolumeModified()
{
  this->Header->update();
}

//-----------------------------------------------------------------------------
bool qSlicerAstroDatasetCard::isActive() const
{
  return this->Header->Active;
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::setActive(bool active)
{
  this->Header->Active = active;
  this->setProperty("activeCard", active);
  this->style()->unpolish(this);
  this->style()->polish(this);
  this->Products->setVisible(active && this->productCount() > 0);
  this->Header->update();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroDatasetCard::kind() const
{
  return this->Kind;
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::setKind(const QString& kind)
{
  this->Kind = kind;
  this->Header->update();
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroDatasetCard::chips() const
{
  return this->Chips;
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::setChips(const QStringList& chips)
{
  this->Chips = chips;
  QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(this->ChipsRow->layout());
  while (layout->count() > 1)
    {
    QLayoutItem* item = layout->takeAt(0);
    if (item->widget())
      {
      item->widget()->deleteLater();
      }
    delete item;
    }
  for (const QString& text : chips)
    {
    QLabel* chip = new QLabel(text, this->ChipsRow);
    chip->setProperty("astroRole", "chip");
    chip->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->insertWidget(layout->count() - 1, chip);
    }
  this->ChipsRow->setVisible(!chips.isEmpty());
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::clearActionChips()
{
  QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(this->ActionChipsRow->layout());
  while (layout->count() > 1)
    {
    QLayoutItem* item = layout->takeAt(0);
    if (item->widget())
      {
      item->widget()->deleteLater();
      }
    delete item;
    }
  this->ActionChipsRow->hide();
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::addActionChip(const QString& id, const QString& text, bool checked,
                                           const QString& iconName, const QString& toolTip)
{
  QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(this->ActionChipsRow->layout());
  QToolButton* chip = new QToolButton(this->ActionChipsRow);
  chip->setObjectName(QString("ActionChip_%1").arg(id));
  chip->setProperty("astroRole", "actionChip");
  chip->setProperty("chipID", id);
  chip->setCheckable(true);
  chip->setChecked(checked);
  chip->setText(text);
  chip->setToolTip(toolTip);
  chip->setCursor(Qt::PointingHandCursor);
  chip->setToolButtonStyle(iconName.isEmpty() ? Qt::ToolButtonTextOnly : Qt::ToolButtonTextBesideIcon);
  if (!iconName.isEmpty())
    {
    chip->setIcon(qSlicerAstroTheme::instance()->icon(iconName, "muted"));
    chip->setIconSize(QSize(13, 13));
    }
  QObject::connect(chip, &QToolButton::toggled, this, [this, id](bool on) { emit this->actionChipToggled(id, on); });
  layout->insertWidget(layout->count() - 1, chip);
  this->ActionChipsRow->show();
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::setActionChipChecked(const QString& id, bool checked)
{
  QToolButton* chip = this->ActionChipsRow->findChild<QToolButton*>(QString("ActionChip_%1").arg(id));
  if (chip)
    {
    const QSignalBlocker blocker(chip);
    chip->setChecked(checked);
    }
}

//-----------------------------------------------------------------------------
int qSlicerAstroDatasetCard::actionChipCount() const
{
  return qobject_cast<QHBoxLayout*>(this->ActionChipsRow->layout())->count() - 1;
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::clearProducts()
{
  while (QLayoutItem* item = this->ProductsLayout->takeAt(0))
    {
    if (item->widget())
      {
      item->widget()->deleteLater();
      }
    delete item;
    }
  this->Products->hide();
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::addProduct(const QString& id, const QString& name, const QString& detail,
                                        const QString& iconName)
{
  QToolButton* button = new QToolButton(this->Products);
  button->setProperty("astroRole", "product");
  button->setObjectName(QString("Product_%1").arg(id));
  button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  button->setIcon(qSlicerAstroTheme::instance()->icon(iconName, "subtle"));
  button->setIconSize(QSize(14, 14));
  // long names are elided (middle) to the width of the card; the tooltip
  // has the full name
  button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  button->setMinimumWidth(40);
  button->setProperty("productName", name);
  button->setProperty("productDetail", detail);
  button->setText(detail.isEmpty() ? name : QString("%1   %2").arg(name, detail));
  button->setToolTip(tr("%1\nClick: show. Drag: show in one view.").arg(
    detail.isEmpty() ? name : QString("%1 (%2)").arg(name, detail)));
  button->setProperty("nodeID", id);
  button->installEventFilter(this);
  QObject::connect(button, &QToolButton::clicked, this, [this, id]() { emit this->productClicked(id); });
  this->ProductsLayout->addWidget(button);
  this->Products->setVisible(this->isActive());
}

//-----------------------------------------------------------------------------
int qSlicerAstroDatasetCard::productCount() const
{
  return this->ProductsLayout->count();
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
    {
    this->PressPosition = event->pos();
    this->Pressed = true;
    this->Dragged = false;
    }
  QFrame::mousePressEvent(event);
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::mouseMoveEvent(QMouseEvent* event)
{
  if (this->Pressed && !this->Dragged && (event->buttons() & Qt::LeftButton) &&
      (event->pos() - this->PressPosition).manhattanLength() >= QApplication::startDragDistance())
    {
    this->Dragged = true;
    this->startDrag(this->volumeID());
    }
  QFrame::mouseMoveEvent(event);
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::mouseReleaseEvent(QMouseEvent* event)
{
  const bool click = this->Pressed && !this->Dragged;
  this->Pressed = false;
  if (click && event->button() == Qt::LeftButton && this->rect().contains(event->pos()))
    {
    emit this->clicked();
    }
  QFrame::mouseReleaseEvent(event);
}

//-----------------------------------------------------------------------------
bool qSlicerAstroDatasetCard::eventFilter(QObject* watched, QEvent* event)
{
  // products: dragged to a view
  QToolButton* button = qobject_cast<QToolButton*>(watched);
  if (button && event->type() == QEvent::Resize && button->property("productName").isValid())
    {
    qSlicerAstroDatasetCard::elideProductText(button);
    }
  if (button && event->type() == QEvent::MouseButtonPress)
    {
    button->setProperty("pressPosition", static_cast<QMouseEvent*>(event)->pos());
    }
  else if (button && event->type() == QEvent::MouseMove &&
           (static_cast<QMouseEvent*>(event)->buttons() & Qt::LeftButton))
    {
    const QPoint start = button->property("pressPosition").toPoint();
    if ((static_cast<QMouseEvent*>(event)->pos() - start).manhattanLength() >= QApplication::startDragDistance())
      {
      button->setDown(false);
      this->startDrag(button->property("nodeID").toString());
      return true;
      }
    }
  return QFrame::eventFilter(watched, event);
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::startDrag(const QString& nodeID)
{
  if (nodeID.isEmpty())
    {
    return;
    }
  QMimeData* mimeData = new QMimeData;
  mimeData->setData(qSlicerAstroViewFrame::mimeType(), nodeID.toUtf8());
  mimeData->setText(nodeID);
  QDrag* drag = new QDrag(this);
  drag->setMimeData(mimeData);
  vtkMRMLVolumeNode* node = nullptr;
  if (this->Volume && this->Volume->GetScene())
    {
    node = vtkMRMLVolumeNode::SafeDownCast(this->Volume->GetScene()->GetNodeByID(nodeID.toUtf8().constData()));
    }
  if (node)
    {
    QPixmap pixmap = qSlicerAstroThumbnails::instance()->roundedPixmap(node, 76, 58, 7., this->devicePixelRatioF());
    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(38, 29));
    }
  drag->exec(Qt::CopyAction);
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetCard::elideProductText(QToolButton* button)
{
  const QString name = button->property("productName").toString();
  const QString detail = button->property("productDetail").toString();
  const QFontMetrics metrics(button->font());
  const QString separator("   ");
  // text room: width minus the icon and the padding of the style
  const int room = button->width() - button->iconSize().width() - 22;
  const int detailWidth = detail.isEmpty() ? 0 : metrics.horizontalAdvance(separator + detail);
  QString elidedName = metrics.elidedText(name, Qt::ElideMiddle, std::max(24, room - detailWidth));
  const QString text = detail.isEmpty() ? elidedName : elidedName + separator + detail;
  if (button->text() != text)
    {
    button->setText(text);
    }
}

//-----------------------------------------------------------------------------
QToolButton* qSlicerAstroDatasetCard::deleteButton() const
{
  return this->DeleteButton;
}
