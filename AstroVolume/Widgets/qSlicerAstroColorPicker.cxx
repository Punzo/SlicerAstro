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
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QToolButton>
#include <QVBoxLayout>

#include "qSlicerAstroColorPicker.h"
#include "qSlicerAstroTheme.h"

//-----------------------------------------------------------------------------
class qSlicerAstroColorPickerPrivate
{
public:
  QColor Color;
  QList<QColor> Palette;
  QWidget* Swatches{nullptr};
  QGridLayout* SwatchLayout{nullptr};
  QList<QToolButton*> SwatchButtons;
  QLabel* Preview{nullptr};
  QLineEdit* Hex{nullptr};
};

//-----------------------------------------------------------------------------
qSlicerAstroColorPicker::qSlicerAstroColorPicker(QWidget* parent)
  : QDialog(parent)
  , d_ptr(new qSlicerAstroColorPickerPrivate)
{
  Q_D(qSlicerAstroColorPicker);
  this->setObjectName("AstroColorPicker");
  this->setWindowTitle(tr("Colour"));
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  QLabel* paletteTitle = new QLabel(tr("PALETTE"), this);
  qSlicerAstroTheme::setLabelRole(paletteTitle, "overline");
  layout->addWidget(paletteTitle);
  d->Swatches = new QWidget(this);
  d->SwatchLayout = new QGridLayout(d->Swatches);
  d->SwatchLayout->setContentsMargins(0, 0, 0, 0);
  d->SwatchLayout->setSpacing(6);
  layout->addWidget(d->Swatches);

  QHBoxLayout* hexRow = new QHBoxLayout();
  hexRow->setSpacing(8);
  d->Preview = new QLabel(this);
  d->Preview->setObjectName("ColorPreview");
  d->Preview->setFixedSize(44, 28);
  d->Preview->setToolTip(tr("The chosen colour"));
  d->Hex = new QLineEdit(this);
  d->Hex->setObjectName("ColorHex");
  d->Hex->setToolTip(tr("Hexadecimal code of the colour (#RRGGBB)"));
  d->Hex->setValidator(new QRegularExpressionValidator(QRegularExpression("#?[0-9A-Fa-f]{0,6}"), d->Hex));
  d->Hex->setMaxLength(7);
  QObject::connect(d->Hex, SIGNAL(editingFinished()), this, SLOT(onHexEdited()));
  QPushButton* more = new QPushButton(tr("More colours…"), this);
  more->setObjectName("ColorMore");
  more->setToolTip(tr("Any colour (hue, saturation, value; red, green, blue)"));
  QObject::connect(more, SIGNAL(clicked()), this, SLOT(showMoreColors()));
  hexRow->addWidget(d->Preview);
  hexRow->addWidget(d->Hex, 1);
  hexRow->addWidget(more);
  layout->addLayout(hexRow);

  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  for (QAbstractButton* button : buttons->buttons())
    {
    // no icons of the Slicer style on the buttons
    button->setIcon(QIcon());
    }
  QObject::connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
  QObject::connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
  layout->addWidget(buttons);

  this->setPalette(qSlicerAstroColorPicker::defaultPalette());
  this->setColor(QColor("#F2B24C"));
  qSlicerAstroTheme::instance()->applyToDialog(this);
}

//-----------------------------------------------------------------------------
qSlicerAstroColorPicker::~qSlicerAstroColorPicker() = default;

//-----------------------------------------------------------------------------
QList<QColor> qSlicerAstroColorPicker::defaultPalette()
{
  // Okabe-Ito (colour-blind safe) and the colours of the SlicerAstro theme
  const char* codes[16] = {"#E69F00", "#56B4E9", "#009E73", "#F0E442", "#0072B2", "#D55E00", "#CC79A7", "#999999",
                           "#F2B24C", "#4FD1E0", "#5BD08A", "#FF6B6B", "#9B8CFF", "#2F6BFF", "#E5672E", "#E7EBF3"};
  QList<QColor> colors;
  for (const char* code : codes)
    {
    colors << QColor(code);
    }
  return colors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerAstroColorPicker::swatchIcon(const QColor& color, int size)
{
  QPixmap pixmap(size * 2, size * 2);
  pixmap.setDevicePixelRatio(2.);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  QPainterPath path;
  path.addRoundedRect(QRectF(0.5, 0.5, size - 1., size - 1.), 4., 4.);
  painter.fillPath(path, color);
  painter.setPen(QPen(QColor(0, 0, 0, 60), 1.));
  painter.drawPath(path);
  painter.end();
  return QIcon(pixmap);
}

//-----------------------------------------------------------------------------
void qSlicerAstroColorPicker::setPalette(const QList<QColor>& colors)
{
  Q_D(qSlicerAstroColorPicker);
  d->Palette = colors;
  qDeleteAll(d->SwatchButtons);
  d->SwatchButtons.clear();
  const int columns = 8;
  for (int index = 0; index < colors.size(); index++)
    {
    QToolButton* swatch = new QToolButton(d->Swatches);
    swatch->setObjectName(QString("ColorSwatch%1").arg(index));
    swatch->setProperty("astroRole", "swatch");
    swatch->setCheckable(true);
    swatch->setAutoRaise(true);
    swatch->setFixedSize(30, 30);
    swatch->setIconSize(QSize(20, 20));
    swatch->setIcon(qSlicerAstroColorPicker::swatchIcon(colors[index], 20));
    swatch->setToolTip(colors[index].name().toUpper());
    const QColor color = colors[index];
    QObject::connect(swatch, &QToolButton::clicked, this, [this, color]() { this->setColor(color); });
    d->SwatchLayout->addWidget(swatch, index / columns, index % columns);
    d->SwatchButtons << swatch;
    }
  this->setColor(d->Color);
}

//-----------------------------------------------------------------------------
QList<QColor> qSlicerAstroColorPicker::paletteColors() const
{
  Q_D(const qSlicerAstroColorPicker);
  return d->Palette;
}

//-----------------------------------------------------------------------------
QColor qSlicerAstroColorPicker::color() const
{
  Q_D(const qSlicerAstroColorPicker);
  return d->Color;
}

//-----------------------------------------------------------------------------
void qSlicerAstroColorPicker::setColor(const QColor& color)
{
  Q_D(qSlicerAstroColorPicker);
  if (!color.isValid())
    {
    return;
    }
  d->Color = color;
  d->Preview->setPixmap(qSlicerAstroColorPicker::swatchIcon(color, 28).pixmap(44, 28));
  const QString name = color.name().toUpper();
  if (d->Hex->text().toUpper() != name)
    {
    d->Hex->setText(name);
    }
  for (int index = 0; index < d->SwatchButtons.size(); index++)
    {
    d->SwatchButtons[index]->setChecked(d->Palette.value(index).rgb() == color.rgb());
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroColorPicker::onHexEdited()
{
  Q_D(qSlicerAstroColorPicker);
  QString text = d->Hex->text().trimmed();
  if (!text.startsWith('#'))
    {
    text.prepend('#');
    }
  const QColor color(text);
  if (color.isValid() && text.size() == 7)
    {
    this->setColor(color);
    }
  else
    {
    d->Hex->setText(d->Color.name().toUpper());
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroColorPicker::showMoreColors()
{
  Q_D(qSlicerAstroColorPicker);
  QColorDialog dialog(d->Color, this);
  dialog.setOption(QColorDialog::DontUseNativeDialog, true);
  dialog.setWindowTitle(tr("More colours"));
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  theme->applyToDialog(&dialog);
  // readable labels and buttons without the icons of the Slicer style
  for (QLabel* label : dialog.findChildren<QLabel*>())
    {
    label->setStyleSheet(QString("color: %1; background: transparent;").arg(theme->color("text").name()));
    }
  for (QDialogButtonBox* box : dialog.findChildren<QDialogButtonBox*>())
    {
    for (QAbstractButton* button : box->buttons())
      {
      button->setIcon(QIcon());
      }
    }
  if (dialog.exec() == QDialog::Accepted)
    {
    this->setColor(dialog.selectedColor());
    }
}

//-----------------------------------------------------------------------------
QColor qSlicerAstroColorPicker::getColor(const QColor& initial, QWidget* parent, const QString& title, bool* ok)
{
  qSlicerAstroColorPicker picker(parent);
  if (!title.isEmpty())
    {
    picker.setWindowTitle(title);
    }
  picker.setColor(initial);
  const bool accepted = picker.exec() == QDialog::Accepted;
  if (ok)
    {
    *ok = accepted;
    }
  return accepted ? picker.color() : initial;
}
