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
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

#include <limits>

#include "qSlicerAstroValueField.h"
#include "qSlicerAstroTheme.h"

//-----------------------------------------------------------------------------
qSlicerAstroValueField::qSlicerAstroValueField(QWidget* parent)
  : QFrame(parent)
  , SpinBox(new QDoubleSpinBox(this))
  , LeadingLabel(new QLabel(this))
  , UnitLabel(new QLabel(this))
{
  this->setProperty("astroRole", "field");
  this->setCursor(Qt::IBeamCursor);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setContentsMargins(9, 1, 9, 1);
  layout->setSpacing(6);
  this->LeadingLabel->setProperty("astroRole", "unit");
  this->LeadingLabel->hide();
  this->UnitLabel->setProperty("astroRole", "unit");
  this->UnitLabel->hide();
  // no steppers: the buttons have no width in the style sheet (a spin box
  // without buttons and with a styled box loses its edit area in Qt)
  this->SpinBox->setFrame(false);
  this->SpinBox->setKeyboardTracking(false);
  this->SpinBox->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  this->SpinBox->setRange(-std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
  this->SpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  this->SpinBox->setMinimumWidth(30);
  this->SpinBox->installEventFilter(this);
  layout->addWidget(this->LeadingLabel);
  layout->addWidget(this->SpinBox, 1);
  layout->addWidget(this->UnitLabel);
  this->setFocusProxy(this->SpinBox);
  QObject::connect(this->SpinBox, SIGNAL(valueChanged(double)), this, SIGNAL(valueChanged(double)));
  QObject::connect(this->SpinBox, SIGNAL(editingFinished()), this, SIGNAL(editingFinished()));
}

//-----------------------------------------------------------------------------
qSlicerAstroValueField::~qSlicerAstroValueField() = default;

//-----------------------------------------------------------------------------
double qSlicerAstroValueField::value() const
{
  return this->SpinBox->value();
}

//-----------------------------------------------------------------------------
void qSlicerAstroValueField::setValue(double value)
{
  this->SpinBox->setValue(value);
}

//-----------------------------------------------------------------------------
QString qSlicerAstroValueField::unit() const
{
  return this->UnitLabel->text();
}

//-----------------------------------------------------------------------------
void qSlicerAstroValueField::setUnit(const QString& unit)
{
  this->UnitLabel->setText(qSlicerAstroTheme::displayUnit(unit));
  this->UnitLabel->setVisible(!unit.isEmpty());
}

//-----------------------------------------------------------------------------
QString qSlicerAstroValueField::leadingText() const
{
  return this->LeadingLabel->text();
}

//-----------------------------------------------------------------------------
void qSlicerAstroValueField::setLeadingText(const QString& text)
{
  this->LeadingLabel->setText(text);
  this->LeadingLabel->setVisible(!text.isEmpty());
}

//-----------------------------------------------------------------------------
int qSlicerAstroValueField::decimals() const
{
  return this->SpinBox->decimals();
}

//-----------------------------------------------------------------------------
void qSlicerAstroValueField::setDecimals(int decimals)
{
  this->SpinBox->setDecimals(decimals);
}

//-----------------------------------------------------------------------------
void qSlicerAstroValueField::setRange(double minimum, double maximum)
{
  this->SpinBox->setRange(minimum, maximum);
}

//-----------------------------------------------------------------------------
void qSlicerAstroValueField::setSingleStep(double step)
{
  this->SpinBox->setSingleStep(step);
}

//-----------------------------------------------------------------------------
QDoubleSpinBox* qSlicerAstroValueField::spinBox() const
{
  return this->SpinBox;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroValueField::eventFilter(QObject* watched, QEvent* event)
{
  if (watched == this->SpinBox && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut))
    {
    // the field shows the focus of its spin box (no :focus-within in style sheets)
    this->setProperty("focused", event->type() == QEvent::FocusIn);
    this->style()->unpolish(this);
    this->style()->polish(this);
    this->update();
    }
  return QFrame::eventFilter(watched, event);
}

//-----------------------------------------------------------------------------
void qSlicerAstroValueField::mousePressEvent(QMouseEvent* event)
{
  this->SpinBox->setFocus(Qt::MouseFocusReason);
  QFrame::mousePressEvent(event);
}
