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

#include "qSlicerAstroPanelHeader.h"
#include "qSlicerAstroTheme.h"

//-----------------------------------------------------------------------------
qSlicerAstroPanelHeader::qSlicerAstroPanelHeader(QWidget* parent)
  : QFrame(parent)
  , IconLabel(new QLabel(this))
  , TitleLabel(new QLabel(this))
  , Layout(new QHBoxLayout(this))
{
  this->setProperty("astroRole", "panelHeader");
  this->Layout->setContentsMargins(12, 12, 12, 10);
  this->Layout->setSpacing(10);
  this->IconLabel->setFixedSize(22, 22);
  this->TitleLabel->setProperty("astroRole", "title");
  this->Layout->addWidget(this->IconLabel);
  this->Layout->addWidget(this->TitleLabel, 1);
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(updateIcon()));
}

//-----------------------------------------------------------------------------
qSlicerAstroPanelHeader::~qSlicerAstroPanelHeader() = default;

//-----------------------------------------------------------------------------
QString qSlicerAstroPanelHeader::title() const
{
  return this->TitleLabel->text();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPanelHeader::setTitle(const QString& title)
{
  this->TitleLabel->setText(title);
}

//-----------------------------------------------------------------------------
QString qSlicerAstroPanelHeader::iconName() const
{
  return this->IconName;
}

//-----------------------------------------------------------------------------
void qSlicerAstroPanelHeader::setIconName(const QString& name)
{
  this->IconName = name;
  this->updateIcon();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPanelHeader::addWidget(QWidget* widget)
{
  this->Layout->addWidget(widget);
}

//-----------------------------------------------------------------------------
void qSlicerAstroPanelHeader::updateIcon()
{
  if (this->IconName.isEmpty())
    {
    this->IconLabel->clear();
    return;
    }
  const qreal ratio = this->devicePixelRatioF();
  QPixmap pixmap = qSlicerAstroTheme::instance()->icon(this->IconName, "text").pixmap(QSize(22 * ratio, 22 * ratio));
  pixmap.setDevicePixelRatio(ratio);
  this->IconLabel->setPixmap(pixmap);
}
