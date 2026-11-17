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
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

// VTK includes
#include <vtkCommand.h>
#include <vtkImageData.h>

// MRML includes
#include <vtkMRMLVolumeNode.h>

#include <algorithm>
#include <cmath>

#include "qSlicerAstroDatasetCard.h"
#include "qSlicerAstroTheme.h"
#include "qSlicerAstroThumbnails.h"

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
    QString name = this->Volume ? QString::fromUtf8(this->Volume->GetName()) : QString();
    int chipWidth = 0;
    const QFont chipFont = theme->font("monoSmall");
    const QFontMetrics chipMetrics(chipFont);
    const QString chipText = qSlicerAstroDatasetCard::tr("active");
    if (this->Active)
      {
      chipWidth = chipMetrics.horizontalAdvance(chipText) + 14 + 6;
      }
    name = titleMetrics.elidedText(name, Qt::ElideMiddle, width - chipWidth);
    painter.drawText(QRect(x, 0, width, titleMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter, name);
    if (this->Active)
      {
      const int chipX = x + titleMetrics.horizontalAdvance(name) + 6;
      const QRectF chip(chipX + 0.5, (titleMetrics.height() - 18) / 2. + 0.5, chipWidth - 6, 18);
      QPainterPath path;
      path.addRoundedRect(chip, 9., 9.);
      painter.fillPath(path, theme->color("accent-soft"));
      painter.setPen(QPen(theme->color("accent"), 1.));
      painter.drawPath(path);
      painter.setFont(chipFont);
      painter.setPen(theme->color("accent-ink"));
      painter.drawText(chip, Qt::AlignCenter, chipText);
      }
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
      const char* unit = this->Volume->GetAttribute("SlicerAstro.BUNIT");
      if (unit && *unit)
        {
        meta += QString::fromUtf8(" · %1").arg(QString::fromUtf8(unit));
        }
      }
    const int metaY = titleMetrics.height() + 2;
    painter.drawText(QRect(x, metaY, width, metaMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                     metaMetrics.elidedText(meta, Qt::ElideRight, width));
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
  , Products(new QWidget(this))
  , ProductsLayout(new QVBoxLayout(this->Products))
  , Kind("cube")
{
  this->setProperty("astroRole", "datasetCard");
  this->setProperty("activeCard", false);
  this->setCursor(Qt::PointingHandCursor);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(9);
  layout->addWidget(this->Header);
  QHBoxLayout* chipsLayout = new QHBoxLayout(this->ChipsRow);
  chipsLayout->setContentsMargins(0, 0, 0, 0);
  chipsLayout->setSpacing(5);
  chipsLayout->addStretch(1);
  this->ChipsRow->hide();
  layout->addWidget(this->ChipsRow);
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
  button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  button->setText(detail.isEmpty() ? name : QString("%1   %2").arg(name, detail));
  button->setToolTip(name);
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
void qSlicerAstroDatasetCard::mouseReleaseEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton && this->rect().contains(event->pos()))
    {
    emit this->clicked();
    }
  QFrame::mouseReleaseEvent(event);
}
