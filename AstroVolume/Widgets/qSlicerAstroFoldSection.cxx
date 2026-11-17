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
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

#include "qSlicerAstroFoldSection.h"
#include "qSlicerAstroTheme.h"

//-----------------------------------------------------------------------------
qSlicerAstroFoldSection::qSlicerAstroFoldSection(QWidget* parent)
  : QWidget(parent)
  , Header(new QToolButton(this))
  , SummaryLabel(new QLabel(this))
  , Content(new QWidget(this))
  , ContentLayout(new QVBoxLayout(this->Content))
{
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);
  // header: chevron, title (overline) and summary, all in the button
  this->Header->setProperty("astroRole", "fold");
  this->Header->setCursor(Qt::PointingHandCursor);
  this->Header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  this->Header->setMinimumHeight(28);
  QHBoxLayout* headerLayout = new QHBoxLayout(this->Header);
  headerLayout->setContentsMargins(0, 6, 0, 0);
  headerLayout->setSpacing(6);
  this->IconLabel = new QLabel(this->Header);
  this->IconLabel->setFixedSize(14, 14);
  this->TitleLabel = new QLabel(this->Header);
  qSlicerAstroTheme::setLabelRole(this->TitleLabel, "overline");
  this->SummaryLabel->setParent(this->Header);
  this->SummaryLabel->setProperty("astroRole", "label");
  for (QLabel* label : {this->IconLabel, this->TitleLabel, this->SummaryLabel})
    {
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
  headerLayout->addWidget(this->IconLabel);
  headerLayout->addWidget(this->TitleLabel);
  headerLayout->addSpacing(4);
  headerLayout->addWidget(this->SummaryLabel, 1);
  layout->addWidget(this->Header);
  this->ContentLayout->setContentsMargins(0, 2, 0, 0);
  this->ContentLayout->setSpacing(10);
  layout->addWidget(this->Content);
  this->Content->hide();
  this->setTitle(tr("Advanced"));
  QObject::connect(this->Header, SIGNAL(clicked()), this, SLOT(toggle()));
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(updateIcon()));
  this->updateIcon();
}

//-----------------------------------------------------------------------------
qSlicerAstroFoldSection::~qSlicerAstroFoldSection() = default;

//-----------------------------------------------------------------------------
QString qSlicerAstroFoldSection::title() const
{
  return this->Title;
}

//-----------------------------------------------------------------------------
void qSlicerAstroFoldSection::setTitle(const QString& title)
{
  this->Title = title;
  this->TitleLabel->setText(title.toUpper());
}

//-----------------------------------------------------------------------------
QString qSlicerAstroFoldSection::summary() const
{
  return this->SummaryLabel->text();
}

//-----------------------------------------------------------------------------
void qSlicerAstroFoldSection::setSummary(const QString& summary)
{
  this->SummaryLabel->setText(summary);
}

//-----------------------------------------------------------------------------
bool qSlicerAstroFoldSection::isCollapsed() const
{
  return this->Content->isHidden();
}

//-----------------------------------------------------------------------------
void qSlicerAstroFoldSection::setCollapsed(bool collapsed)
{
  if (collapsed == this->isCollapsed())
    {
    return;
    }
  this->Content->setVisible(!collapsed);
  this->updateIcon();
  if (!this->SettingsKey.isEmpty())
    {
    QSettings settings;
    settings.setValue(this->SettingsKey, collapsed);
    }
  emit this->collapsedChanged(collapsed);
}

//-----------------------------------------------------------------------------
void qSlicerAstroFoldSection::toggle()
{
  this->setCollapsed(!this->isCollapsed());
}

//-----------------------------------------------------------------------------
QString qSlicerAstroFoldSection::settingsKey() const
{
  return this->SettingsKey;
}

//-----------------------------------------------------------------------------
void qSlicerAstroFoldSection::setSettingsKey(const QString& key)
{
  this->SettingsKey = key;
  if (!key.isEmpty())
    {
    QSettings settings;
    this->setCollapsed(settings.value(key, true).toBool());
    }
}

//-----------------------------------------------------------------------------
QWidget* qSlicerAstroFoldSection::contentWidget() const
{
  return this->Content;
}

//-----------------------------------------------------------------------------
QVBoxLayout* qSlicerAstroFoldSection::contentLayout() const
{
  return this->ContentLayout;
}

//-----------------------------------------------------------------------------
void qSlicerAstroFoldSection::updateIcon()
{
  const qreal ratio = this->devicePixelRatioF();
  QPixmap pixmap = qSlicerAstroTheme::instance()->icon(this->isCollapsed() ? "chevronRight" : "chevronDown",
                                                       "subtle").pixmap(QSize(14 * ratio, 14 * ratio));
  pixmap.setDevicePixelRatio(ratio);
  this->IconLabel->setPixmap(pixmap);
}
