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
#include <QAbstractButton>
#include <QButtonGroup>
#include <QSignalBlocker>
#include <QHBoxLayout>
#include <QToolButton>
#include <QVariant>

#include "qSlicerAstroSegmentedButton.h"

//-----------------------------------------------------------------------------
qSlicerAstroSegmentedButton::qSlicerAstroSegmentedButton(QWidget* parent)
  : QFrame(parent)
  , Group(new QButtonGroup(this))
{
  this->setProperty("astroRole", "segmented");
  this->Group->setExclusive(true);
  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setContentsMargins(2, 2, 2, 2);
  layout->setSpacing(2);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  QObject::connect(this->Group, &QButtonGroup::idToggled, this, [this](int id, bool checked)
#else
  QObject::connect(this->Group, QOverload<int, bool>::of(&QButtonGroup::buttonToggled), this, [this](int id, bool checked)
#endif
    {
    if (checked)
      {
      emit this->currentIndexChanged(id);
      }
    });
}

//-----------------------------------------------------------------------------
qSlicerAstroSegmentedButton::~qSlicerAstroSegmentedButton() = default;

//-----------------------------------------------------------------------------
int qSlicerAstroSegmentedButton::addButton(const QString& text, const QString& toolTip)
{
  QToolButton* button = new QToolButton(this);
  button->setText(text);
  button->setToolTip(toolTip);
  button->setCheckable(true);
  button->setAutoRaise(false);
  // text, icon or both
  button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  const int index = this->Group->buttons().size();
  this->Group->addButton(button, index);
  this->layout()->addWidget(button);
  if (index == 0)
    {
    button->setChecked(true);
    }
  return index;
}

//-----------------------------------------------------------------------------
QToolButton* qSlicerAstroSegmentedButton::button(int index) const
{
  return qobject_cast<QToolButton*>(this->Group->button(index));
}

//-----------------------------------------------------------------------------
int qSlicerAstroSegmentedButton::count() const
{
  return this->Group->buttons().size();
}

//-----------------------------------------------------------------------------
int qSlicerAstroSegmentedButton::currentIndex() const
{
  return this->Group->checkedId();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSegmentedButton::setCurrentIndex(int index)
{
  QAbstractButton* button = this->Group->button(index);
  if (button && !button->isChecked())
    {
    button->setChecked(true);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSegmentedButton::clearSelection()
{
  // an exclusive group keeps one button checked: not exclusive meanwhile
  QAbstractButton* checked = this->Group->checkedButton();
  if (!checked)
    {
    return;
    }
  const QSignalBlocker blocker(this->Group);
  this->Group->setExclusive(false);
  checked->setChecked(false);
  this->Group->setExclusive(true);
}
