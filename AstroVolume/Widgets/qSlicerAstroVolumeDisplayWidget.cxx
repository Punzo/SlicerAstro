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

  This file was originally developed by Davide Punzo, Kapteyn Astronomical Institute,
  and was supported through the European Research Council grant nr. 291531.

==============================================================================*/

// SlicerQT includes
#include <qSlicerAstroVolumeDisplayWidget.h>
#include <qSlicerAstroScalarVolumeDisplayWidget.h>
#include <qSlicerAstroLabelMapVolumeDisplayWidget.h>

// MRML includes
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLVolumeNode.h>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroVolume
class qSlicerAstroVolumeDisplayWidgetPrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroVolumeDisplayWidget);

protected:
  qSlicerAstroVolumeDisplayWidget* const q_ptr;

public:
  qSlicerAstroVolumeDisplayWidgetPrivate(qSlicerAstroVolumeDisplayWidget& object);
  void init();
  void setCurrentDisplayWidget(qSlicerWidget* displayWidget);

  qSlicerAstroScalarVolumeDisplayWidget*            ScalarVolumeDisplayWidget;
  qSlicerAstroLabelMapVolumeDisplayWidget*          LabelMapVolumeDisplayWidget;
};

// --------------------------------------------------------------------------
qSlicerAstroVolumeDisplayWidgetPrivate::qSlicerAstroVolumeDisplayWidgetPrivate(
  qSlicerAstroVolumeDisplayWidget& object)
  : q_ptr(&object)
{
  this->ScalarVolumeDisplayWidget = 0;
  this->LabelMapVolumeDisplayWidget = 0;
}

// --------------------------------------------------------------------------
void qSlicerAstroVolumeDisplayWidgetPrivate::init()
{
  Q_Q(qSlicerAstroVolumeDisplayWidget);
  this->ScalarVolumeDisplayWidget = new qSlicerAstroScalarVolumeDisplayWidget(q);
  q->addWidget(this->ScalarVolumeDisplayWidget);

  this->LabelMapVolumeDisplayWidget = new qSlicerAstroLabelMapVolumeDisplayWidget(q);
  q->addWidget(this->LabelMapVolumeDisplayWidget);
}

// --------------------------------------------------------------------------
void qSlicerAstroVolumeDisplayWidgetPrivate::setCurrentDisplayWidget(
  qSlicerWidget* displayWidget)
{
  Q_Q(qSlicerAstroVolumeDisplayWidget);
  qSlicerWidget* activeWidget = qobject_cast<qSlicerWidget*>(q->currentWidget());
  if (activeWidget == displayWidget)
    {
    return;
    }
  if (activeWidget)
    {
    // We must remove the node "before" the setting the scene to 0.
    // Because removing the scene could modify the observed node (e.g setting
    // the scene to 0 on a colortable combobox will set the color node of the
    // observed node to 0.
    vtkMRMLNode* emptyVolumeNode = 0;
    if (activeWidget == this->ScalarVolumeDisplayWidget)
      {
      this->ScalarVolumeDisplayWidget->setMRMLVolumeNode(emptyVolumeNode);
      }

    if (activeWidget == this->LabelMapVolumeDisplayWidget)
      {
      this->LabelMapVolumeDisplayWidget->setMRMLVolumeNode(emptyVolumeNode);
      }
    activeWidget->setMRMLScene(0);
  }
  // QStackWidget::setCurrentWidget(0) is not supported
  if (displayWidget)
    {
    q->setCurrentWidget(displayWidget);
    }
}

// --------------------------------------------------------------------------
// qSlicerAstroVolumeDisplayWidget
// --------------------------------------------------------------------------
qSlicerAstroVolumeDisplayWidget::qSlicerAstroVolumeDisplayWidget(QWidget* parentWidget)
  : Superclass(parentWidget)
  , d_ptr(new qSlicerAstroVolumeDisplayWidgetPrivate(*this))
{
  Q_D(qSlicerAstroVolumeDisplayWidget);
  d->init();
}

// --------------------------------------------------------------------------
qSlicerAstroVolumeDisplayWidget::~qSlicerAstroVolumeDisplayWidget()
{
}

// --------------------------------------------------------------------------
void qSlicerAstroVolumeDisplayWidget::setMRMLVolumeNode(vtkMRMLNode* VolumeNode)
{
   Q_D(qSlicerAstroVolumeDisplayWidget);

   if (!VolumeNode)
     {
     return;
     }

  vtkMRMLScene* scene = VolumeNode->GetScene();
  vtkMRMLAstroLabelMapVolumeNode* labelMapVolumeNode =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(VolumeNode);
  vtkMRMLAstroVolumeNode* astroVolumeNode =
    vtkMRMLAstroVolumeNode::SafeDownCast(VolumeNode);
  if (astroVolumeNode)
    {
    qvtkReconnect(VolumeNode, vtkCommand::ModifiedEvent,
                  this, SLOT(updateFromMRML(vtkObject*)));
    d->ScalarVolumeDisplayWidget->setMRMLScene(scene);
    d->ScalarVolumeDisplayWidget->setMRMLVolumeNode(VolumeNode);
    d->setCurrentDisplayWidget(d->ScalarVolumeDisplayWidget);
    }
  else if (labelMapVolumeNode)
   {
   qvtkReconnect(VolumeNode, vtkCommand::ModifiedEvent,
                 this, SLOT(updateFromMRML(vtkObject*)));
   d->LabelMapVolumeDisplayWidget->setMRMLScene(scene);
   d->LabelMapVolumeDisplayWidget->setMRMLVolumeNode(VolumeNode);
   d->setCurrentDisplayWidget(d->LabelMapVolumeDisplayWidget);
  }
}

// --------------------------------------------------------------------------
void qSlicerAstroVolumeDisplayWidget::updateFromMRML(vtkObject* Volume)
{
  vtkMRMLVolumeNode* VolumeNode = vtkMRMLVolumeNode::SafeDownCast(Volume);
  this->setMRMLVolumeNode(VolumeNode);
}

