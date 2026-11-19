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
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLVolumeNode.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

#include "qSlicerAstroResultCard.h"
#include "qSlicerAstroTheme.h"
#include "qSlicerAstroThumbnails.h"

//-----------------------------------------------------------------------------
qSlicerAstroResultCard::qSlicerAstroResultCard(QWidget* parent)
  : QFrame(parent)
  , ThumbnailLabel(new QLabel(this))
  , TitleLabel(new QLabel(this))
  , SubtitleLabel(new QLabel(this))
  , ShowButton(new QPushButton(tr("Show"), this))
  , MaskButton(new QToolButton(this))
  , DeleteButton(new QToolButton(this))
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
  this->MaskButton->setObjectName("ResultMaskVisible");
  this->MaskButton->setProperty("astroRole", "toolbar");
  this->MaskButton->setCheckable(true);
  this->MaskButton->setAutoRaise(true);
  this->MaskButton->setIconSize(QSize(16, 16));
  this->MaskButton->setIcon(qSlicerAstroTheme::instance()->icon("eye", "muted"));
  this->MaskButton->setToolTip(tr("Show or hide the mask made from this result."));
  this->MaskButton->hide();
  layout->addWidget(this->MaskButton);
  this->DeleteButton->setObjectName("ResultDelete");
  this->DeleteButton->setProperty("astroRole", "toolbar");
  this->DeleteButton->setAutoRaise(true);
  this->DeleteButton->setIconSize(QSize(16, 16));
  this->DeleteButton->setIcon(qSlicerAstroTheme::instance()->icon("trash", "muted"));
  this->DeleteButton->setToolTip(tr("Delete this result (removes it from the scene)."));
  layout->addWidget(this->DeleteButton);
  QObject::connect(this->ShowButton, SIGNAL(clicked()), this, SIGNAL(showRequested()));
  QObject::connect(this->MaskButton, SIGNAL(toggled(bool)), this, SLOT(setMaskVisible(bool)));
  QObject::connect(this->DeleteButton, SIGNAL(clicked()), this, SLOT(onDeleteClicked()));
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
  this->Volume = volume;
  this->Mask = volume ? vtkSlicerAstroVolumeLogic::GetThresholdMaskOf(volume->GetScene(), volume) : nullptr;
  vtkMRMLDisplayNode* maskDisplay = this->Mask ? this->Mask->GetDisplayNode() : nullptr;
  this->MaskButton->setVisible(maskDisplay != nullptr);
  {
  const QSignalBlocker blocker(this->MaskButton);
  const bool maskVisible = maskDisplay && maskDisplay->GetVisibility();
  this->MaskButton->setChecked(maskVisible);
  this->MaskButton->setIcon(qSlicerAstroTheme::instance()->icon(maskVisible ? "eye" : "eyeOff", "muted"));
  }
  this->DeleteButton->setEnabled(volume != nullptr);
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
  this->setSubtitle(unit ? qSlicerAstroTheme::displayUnit(QString::fromUtf8(unit)) : QString());
}

//-----------------------------------------------------------------------------
QPushButton* qSlicerAstroResultCard::showButton() const
{
  return this->ShowButton;
}

//-----------------------------------------------------------------------------
vtkMRMLVolumeNode* qSlicerAstroResultCard::volume() const
{
  return this->Volume;
}

//-----------------------------------------------------------------------------
vtkMRMLSegmentationNode* qSlicerAstroResultCard::maskNode() const
{
  return this->Mask;
}

//-----------------------------------------------------------------------------
QToolButton* qSlicerAstroResultCard::maskButton() const
{
  return this->MaskButton;
}

//-----------------------------------------------------------------------------
QToolButton* qSlicerAstroResultCard::deleteButton() const
{
  return this->DeleteButton;
}

//-----------------------------------------------------------------------------
void qSlicerAstroResultCard::setMaskVisible(bool visible)
{
  vtkMRMLDisplayNode* maskDisplay = this->Mask ? this->Mask->GetDisplayNode() : nullptr;
  if (maskDisplay)
    {
    maskDisplay->SetVisibility(visible);
    }
  const QSignalBlocker blocker(this->MaskButton);
  this->MaskButton->setChecked(visible);
  this->MaskButton->setIcon(qSlicerAstroTheme::instance()->icon(visible ? "eye" : "eyeOff", "muted"));
}

//-----------------------------------------------------------------------------
void qSlicerAstroResultCard::onDeleteClicked()
{
  if (!this->Volume)
    {
    return;
    }
  const QMessageBox::StandardButton answer = QMessageBox::question(this, tr("Delete result"),
    tr("Delete %1? It is removed from the scene.").arg(QString::fromUtf8(this->Volume->GetName())),
    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (answer == QMessageBox::Yes)
    {
    this->deleteResult();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroResultCard::deleteResult()
{
  vtkMRMLVolumeNode* volume = this->Volume;
  vtkMRMLScene* scene = volume ? volume->GetScene() : nullptr;
  this->setVolume(nullptr);
  this->hide();
  if (scene)
    {
    scene->RemoveNode(volume);
    }
  emit this->deleted();
}
