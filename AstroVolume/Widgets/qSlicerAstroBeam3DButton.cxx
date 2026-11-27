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
#include <QEvent>
#include <QSignalBlocker>

// SlicerAstro includes
#include "qSlicerAstroBeam3DButton.h"
#include "qSlicerAstroTheme.h"

// MRML includes
#include <vtkMRMLAbstractViewNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkCollection.h>
#include <vtkCommand.h>
#include <vtkSmartPointer.h>

// STD includes
#include <cstring>

namespace
{
const char* VisibilityAttribute = "SlicerAstro.Beam3D";
}

//-----------------------------------------------------------------------------
qSlicerAstroBeam3DButton::qSlicerAstroBeam3DButton(QWidget* parent)
  : QToolButton(parent)
{
  this->setObjectName("Beam3DButton");
  this->setAutoRaise(true);
  this->setCheckable(true);
  this->updateIcon();
  QObject::connect(this, SIGNAL(toggled(bool)), this, SLOT(onToggled(bool)));
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(updateIcon()));
  this->updateFromMRML();
}

//-----------------------------------------------------------------------------
qSlicerAstroBeam3DButton::~qSlicerAstroBeam3DButton() = default;

//-----------------------------------------------------------------------------
vtkMRMLAbstractViewNode* qSlicerAstroBeam3DButton::mrmlViewNode() const
{
  return this->ViewNode;
}

//-----------------------------------------------------------------------------
QString qSlicerAstroBeam3DButton::iconColorToken() const
{
  return this->IconColorToken;
}

//-----------------------------------------------------------------------------
void qSlicerAstroBeam3DButton::setIconColorToken(const QString& token)
{
  this->IconColorToken = token;
  this->updateIcon();
}

//-----------------------------------------------------------------------------
void qSlicerAstroBeam3DButton::setMRMLViewNode(vtkMRMLNode* viewNode)
{
  vtkMRMLAbstractViewNode* node = vtkMRMLAbstractViewNode::SafeDownCast(viewNode);
  this->qvtkReconnect(this->ViewNode, node, vtkCommand::ModifiedEvent, this, SLOT(updateFromMRML()));
  this->ViewNode = node;
  this->updateObservations();
  this->updateFromMRML();
}

//-----------------------------------------------------------------------------
void qSlicerAstroBeam3DButton::updateObservations()
{
  vtkMRMLScene* scene = this->ViewNode ? this->ViewNode->GetScene() : nullptr;
  if (this->ObservedScene != scene)
    {
    for (unsigned long event : {vtkMRMLScene::NodeAddedEvent, vtkMRMLScene::NodeRemovedEvent,
                                vtkMRMLScene::EndBatchProcessEvent})
      {
      this->qvtkReconnect(this->ObservedScene, scene, event, this, SLOT(onSceneChanged()));
      }
    this->ObservedScene = scene;
    }

  // the cubes: their beam keywords (node modified) and their volume
  // renderings (display nodes added, removed or modified)
  for (vtkMRMLAstroVolumeNode* volume : this->ObservedVolumes)
    {
    if (volume)
      {
      this->qvtkDisconnect(volume, vtkCommand::ModifiedEvent, this, SLOT(updateFromMRML()));
      this->qvtkDisconnect(volume, vtkMRMLDisplayableNode::DisplayModifiedEvent, this, SLOT(updateFromMRML()));
      }
    }
  this->ObservedVolumes.clear();
  if (!scene)
    {
    return;
    }
  vtkSmartPointer<vtkCollection> volumes =
    vtkSmartPointer<vtkCollection>::Take(scene->GetNodesByClass("vtkMRMLAstroVolumeNode"));
  for (int index = 0; index < volumes->GetNumberOfItems(); index++)
    {
    vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(volumes->GetItemAsObject(index));
    if (volume)
      {
      this->qvtkConnect(volume, vtkCommand::ModifiedEvent, this, SLOT(updateFromMRML()));
      this->qvtkConnect(volume, vtkMRMLDisplayableNode::DisplayModifiedEvent, this, SLOT(updateFromMRML()));
      this->ObservedVolumes.emplace_back(volume);
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroBeam3DButton::onSceneChanged()
{
  // a scene loaded or closed: once at the end
  if (this->ObservedScene && this->ObservedScene->IsBatchProcessing())
    {
    return;
    }
  this->updateObservations();
  this->updateFromMRML();
}

//-----------------------------------------------------------------------------
void qSlicerAstroBeam3DButton::updateFromMRML()
{
  const char* shown = this->ViewNode ? this->ViewNode->GetAttribute(VisibilityAttribute) : nullptr;
  const QSignalBlocker blocker(this);
  const bool checked = shown && !strcmp(shown, "on");
  this->setChecked(checked);
  const QString reason = QString::fromStdString(vtkMRMLAstroVolumeNode::GetBeam3DUnavailableReason(this->ViewNode));
  const bool available = this->ViewNode && reason.isEmpty();
  this->setEnabled(available);
  if (available != this->Available)
    {
    this->Available = available;
    if (checked)
      {
      // the resolution element shown or hidden (e.g. a beam entered in the
      // Info section of the cube rendered)
      this->ViewNode->Modified();
      }
    }
  const QString help = tr("Show or hide the 3D resolution element: an ellipsoid with the beam (FWHM) on the "
                          "sky and the spectral resolution (FWHM) along the spectral axis, in the corner "
                          "of the first pixel and channel of the cube");
  this->setToolTip(reason.isEmpty() ? help : help + "\n\n" + reason);
}

//-----------------------------------------------------------------------------
void qSlicerAstroBeam3DButton::onToggled(bool checked)
{
  if (!this->ViewNode)
    {
    return;
    }
  this->ViewNode->SetAttribute(VisibilityAttribute, checked ? "on" : "off");
  // the attribute does not modify the node: the view is rendered again
  this->ViewNode->Modified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroBeam3DButton::updateIcon()
{
  this->setIcon(qSlicerAstroTheme::instance()->icon("beam", this->IconColorToken));
}

//-----------------------------------------------------------------------------
bool qSlicerAstroBeam3DButton::event(QEvent* event)
{
  if (event->type() == QEvent::ToolTip)
    {
    this->updateFromMRML();
    }
  return QToolButton::event(event);
}
