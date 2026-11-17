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
#include <QAction>
#include <QEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>

// VTK includes
#include <vtkCommand.h>
#include <vtkImageData.h>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLVolumeNode.h>

#include <vector>

#include "qSlicerAstroDatasetSelector.h"
#include "qSlicerAstroTheme.h"
#include "qSlicerAstroThumbnails.h"

namespace
{
const int ThumbnailWidth = 40;
const int ThumbnailHeight = 32;
}

//-----------------------------------------------------------------------------
qSlicerAstroDatasetSelector::qSlicerAstroDatasetSelector(QWidget* parent)
  : QAbstractButton(parent)
  , NodeTypes(QStringList() << "vtkMRMLAstroVolumeNode")
  , NoneText(tr("Select data"))
{
  this->setAttribute(Qt::WA_Hover, true);
  this->setCursor(Qt::PointingHandCursor);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  this->setFocusPolicy(Qt::StrongFocus);
  QObject::connect(this, SIGNAL(clicked()), this, SLOT(showMenu()));
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(update()));
}

//-----------------------------------------------------------------------------
qSlicerAstroDatasetSelector::~qSlicerAstroDatasetSelector() = default;

//-----------------------------------------------------------------------------
vtkMRMLScene* qSlicerAstroDatasetSelector::mrmlScene() const
{
  return this->Scene;
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetSelector::setMRMLScene(vtkMRMLScene* scene)
{
  if (scene == this->Scene)
    {
    return;
    }
  this->qvtkReconnect(this->Scene, scene, vtkMRMLScene::NodeRemovedEvent,
                      this, SLOT(onNodeRemoved(vtkObject*, vtkObject*)));
  this->qvtkReconnect(this->Scene, scene, vtkMRMLScene::EndCloseEvent, this, SLOT(onSceneClosed()));
  this->Scene = scene;
  this->setCurrentNode(nullptr);
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroDatasetSelector::nodeTypes() const
{
  return this->NodeTypes;
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetSelector::setNodeTypes(const QStringList& types)
{
  this->NodeTypes = types;
}

//-----------------------------------------------------------------------------
int qSlicerAstroDatasetSelector::requiredDimensions() const
{
  return this->RequiredDimensions;
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetSelector::setRequiredDimensions(int dimensions)
{
  this->RequiredDimensions = dimensions;
}

//-----------------------------------------------------------------------------
QString qSlicerAstroDatasetSelector::subtitle() const
{
  return this->Subtitle;
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetSelector::setSubtitle(const QString& subtitle)
{
  this->Subtitle = subtitle;
  this->update();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroDatasetSelector::noneText() const
{
  return this->NoneText;
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetSelector::setNoneText(const QString& text)
{
  this->NoneText = text;
  this->update();
}

//-----------------------------------------------------------------------------
vtkMRMLNode* qSlicerAstroDatasetSelector::currentNode() const
{
  return this->CurrentNode;
}

//-----------------------------------------------------------------------------
QString qSlicerAstroDatasetSelector::currentNodeID() const
{
  return (this->CurrentNode && this->CurrentNode->GetID()) ?
    QString::fromUtf8(this->CurrentNode->GetID()) : QString();
}

//-----------------------------------------------------------------------------
bool qSlicerAstroDatasetSelector::accepts(vtkMRMLNode* node) const
{
  if (!node || node->GetHideFromEditors())
    {
    return false;
    }
  bool typeAccepted = false;
  for (const QString& type : this->NodeTypes)
    {
    if (node->IsA(type.toUtf8().constData()))
      {
      typeAccepted = true;
      break;
      }
    }
  if (!typeAccepted)
    {
    return false;
    }
  if (this->RequiredDimensions > 0)
    {
    const char* naxis = node->GetAttribute("SlicerAstro.NAXIS");
    if (!naxis || QString::fromUtf8(naxis).toInt() != this->RequiredDimensions)
      {
      return false;
      }
    }
  return true;
}

//-----------------------------------------------------------------------------
QList<vtkMRMLNode*> qSlicerAstroDatasetSelector::nodes() const
{
  QList<vtkMRMLNode*> result;
  if (!this->Scene)
    {
    return result;
    }
  std::vector<vtkMRMLNode*> nodes;
  this->Scene->GetNodesByClass("vtkMRMLVolumeNode", nodes);
  for (vtkMRMLNode* node : nodes)
    {
    if (this->accepts(node))
      {
      result << node;
      }
    }
  return result;
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetSelector::setCurrentNode(vtkMRMLNode* node)
{
  if (node && !this->accepts(node))
    {
    node = nullptr;
    }
  if (node == this->CurrentNode)
    {
    return;
    }
  this->qvtkReconnect(this->CurrentNode, node, vtkCommand::ModifiedEvent, this, SLOT(onCurrentNodeModified()));
  this->CurrentNode = node;
  this->update();
  emit this->currentNodeChanged(node);
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetSelector::setCurrentNodeID(const QString& nodeID)
{
  this->setCurrentNode(this->Scene && !nodeID.isEmpty() ?
                       this->Scene->GetNodeByID(nodeID.toUtf8().constData()) : nullptr);
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetSelector::onNodeRemoved(vtkObject* scene, vtkObject* node)
{
  Q_UNUSED(scene);
  if (node && node == this->CurrentNode)
    {
    this->setCurrentNode(nullptr);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetSelector::onSceneClosed()
{
  this->setCurrentNode(nullptr);
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetSelector::onCurrentNodeModified()
{
  this->update();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroDatasetSelector::defaultSubtitle(vtkMRMLNode* node) const
{
  vtkMRMLVolumeNode* volume = vtkMRMLVolumeNode::SafeDownCast(node);
  if (!volume || !volume->GetImageData())
    {
    return QString();
    }
  int dims[3];
  volume->GetImageData()->GetDimensions(dims);
  QString text = dims[2] > 1 ? QString("%1 × %2 × %3").arg(dims[0]).arg(dims[1]).arg(dims[2]) :
                               QString("%1 × %2").arg(dims[0]).arg(dims[1]);
  const char* unit = volume->GetAttribute("SlicerAstro.BUNIT");
  if (unit && *unit)
    {
    text += QString(" · %1").arg(QString::fromUtf8(unit));
    }
  return text;
}

//-----------------------------------------------------------------------------
QSize qSlicerAstroDatasetSelector::sizeHint() const
{
  return QSize(240, ThumbnailHeight + 14);
}

//-----------------------------------------------------------------------------
QSize qSlicerAstroDatasetSelector::minimumSizeHint() const
{
  return QSize(140, ThumbnailHeight + 14);
}

//-----------------------------------------------------------------------------
bool qSlicerAstroDatasetSelector::event(QEvent* event)
{
  if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave)
    {
    this->update();
    }
  return QAbstractButton::event(event);
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetSelector::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  QPainterPath path;
  path.addRoundedRect(box, 10., 10.);
  painter.fillPath(path, this->underMouse() && this->isEnabled() ? theme->color("hover") : theme->color("raised"));
  painter.setPen(QPen(this->hasFocus() ? theme->color("data") : theme->color("line"), 1.));
  painter.drawPath(path);

  const int top = (this->height() - ThumbnailHeight) / 2;
  int x = 6;
  vtkMRMLVolumeNode* volume = vtkMRMLVolumeNode::SafeDownCast(this->CurrentNode);
  if (volume)
    {
    const QPixmap thumbnail = qSlicerAstroThumbnails::instance()->roundedPixmap(
      volume, ThumbnailWidth, ThumbnailHeight, 6., this->devicePixelRatioF());
    painter.drawPixmap(x, top, thumbnail);
    x += ThumbnailWidth + 9;
    }
  else
    {
    x += 6;
    }
  const int chevron = 15;
  const int textWidth = this->width() - x - chevron - 16;
  QFont titleFont = theme->font("body");
  titleFont.setWeight(QFont::Bold);
  const QFontMetrics titleMetrics(titleFont);
  const QFont subtitleFont = theme->font("monoSmall");
  const QFontMetrics subtitleMetrics(subtitleFont);
  painter.setPen(this->isEnabled() ? theme->color("text") : theme->color("subtle"));
  if (volume)
    {
    const QString subtitle = this->Subtitle.isEmpty() ? this->defaultSubtitle(volume) : this->Subtitle;
    const int textHeight = titleMetrics.height() + subtitleMetrics.height();
    const int y = (this->height() - textHeight) / 2;
    painter.setFont(titleFont);
    painter.drawText(QRect(x, y, textWidth, titleMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                     titleMetrics.elidedText(QString::fromUtf8(volume->GetName()), Qt::ElideMiddle, textWidth));
    painter.setFont(subtitleFont);
    painter.setPen(theme->color("muted"));
    painter.drawText(QRect(x, y + titleMetrics.height(), textWidth, subtitleMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     subtitleMetrics.elidedText(subtitle, Qt::ElideRight, textWidth));
    }
  else
    {
    painter.setFont(theme->font("body"));
    painter.setPen(theme->color("muted"));
    painter.drawText(QRect(x, 0, textWidth, this->height()), Qt::AlignLeft | Qt::AlignVCenter, this->NoneText);
    }
  const QIcon icon = theme->icon("chevronDown", "subtle");
  icon.paint(&painter, QRect(this->width() - chevron - 10, (this->height() - chevron) / 2, chevron, chevron),
             Qt::AlignCenter, this->isEnabled() ? QIcon::Normal : QIcon::Disabled);
}

//-----------------------------------------------------------------------------
void qSlicerAstroDatasetSelector::showMenu()
{
  QMenu menu(this);
  qSlicerAstroTheme::instance()->applyTo(&menu);
  menu.setMinimumWidth(this->width());
  const QList<vtkMRMLNode*> nodes = this->nodes();
  if (nodes.isEmpty())
    {
    QAction* action = menu.addAction(this->RequiredDimensions == 3 ? tr("No data cube loaded") : tr("No data loaded"));
    action->setEnabled(false);
    }
  for (vtkMRMLNode* node : nodes)
    {
    vtkMRMLVolumeNode* volume = vtkMRMLVolumeNode::SafeDownCast(node);
    QAction* action = menu.addAction(QString::fromUtf8(node->GetName()));
    action->setIcon(QIcon(qSlicerAstroThumbnails::instance()->roundedPixmap(
      volume, 32, 24, 4., this->devicePixelRatioF())));
    action->setCheckable(true);
    action->setChecked(node == this->CurrentNode);
    action->setToolTip(this->defaultSubtitle(node));
    action->setData(QString::fromUtf8(node->GetID()));
    }
  menu.setStyleSheet(menu.styleSheet() + "QMenu { icon-size: 32px; } QMenu::item { padding: 6px 12px 6px 6px; }");
  QAction* chosen = menu.exec(this->mapToGlobal(QPoint(0, this->height() + 2)));
  if (chosen && chosen->data().isValid())
    {
    this->setCurrentNodeID(chosen->data().toString());
    }
}
