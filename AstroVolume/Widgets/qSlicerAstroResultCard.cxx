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
#include <QPushButton>
#include <QVBoxLayout>

// MRML includes
#include <vtkMRMLVolumeNode.h>

#include "qSlicerAstroResultCard.h"
#include "qSlicerAstroThumbnails.h"

//-----------------------------------------------------------------------------
qSlicerAstroResultCard::qSlicerAstroResultCard(QWidget* parent)
  : QFrame(parent)
  , ThumbnailLabel(new QLabel(this))
  , TitleLabel(new QLabel(this))
  , SubtitleLabel(new QLabel(this))
  , ShowButton(new QPushButton(tr("Show"), this))
{
  this->setProperty("astroRole", "result");
  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setSpacing(9);
  this->ThumbnailLabel->setFixedSize(44, 34);
  QWidget* texts = new QWidget(this);
  QVBoxLayout* textLayout = new QVBoxLayout(texts);
  textLayout->setContentsMargins(0, 0, 0, 0);
  textLayout->setSpacing(0);
  this->TitleLabel->setStyleSheet("font-weight: 600;");
  this->SubtitleLabel->setProperty("astroRole", "monoLabel");
  textLayout->addWidget(this->TitleLabel);
  textLayout->addWidget(this->SubtitleLabel);
  this->ShowButton->setProperty("astroRole", "ghost");
  this->ShowButton->setMinimumHeight(26);
  layout->addWidget(this->ThumbnailLabel);
  layout->addWidget(texts, 1);
  layout->addWidget(this->ShowButton);
  QObject::connect(this->ShowButton, SIGNAL(clicked()), this, SIGNAL(showRequested()));
}

//-----------------------------------------------------------------------------
qSlicerAstroResultCard::~qSlicerAstroResultCard() = default;

//-----------------------------------------------------------------------------
QString qSlicerAstroResultCard::title() const
{
  return this->TitleLabel->text();
}

//-----------------------------------------------------------------------------
void qSlicerAstroResultCard::setTitle(const QString& title)
{
  this->TitleLabel->setText(title);
}

//-----------------------------------------------------------------------------
QString qSlicerAstroResultCard::subtitle() const
{
  return this->SubtitleLabel->text();
}

//-----------------------------------------------------------------------------
void qSlicerAstroResultCard::setSubtitle(const QString& subtitle)
{
  this->SubtitleLabel->setText(subtitle);
}

//-----------------------------------------------------------------------------
void qSlicerAstroResultCard::setVolume(vtkMRMLVolumeNode* volume)
{
  if (!volume)
    {
    this->ThumbnailLabel->clear();
    this->setTitle(QString());
    this->setSubtitle(QString());
    return;
    }
  this->ThumbnailLabel->setPixmap(qSlicerAstroThumbnails::instance()->roundedPixmap(
    volume, 44, 34, 6., this->devicePixelRatioF()));
  this->setTitle(QString::fromUtf8(volume->GetName()));
  const char* unit = volume->GetAttribute("SlicerAstro.BUNIT");
  this->setSubtitle(unit ? QString::fromUtf8(unit) : QString());
}

//-----------------------------------------------------------------------------
QPushButton* qSlicerAstroResultCard::showButton() const
{
  return this->ShowButton;
}
