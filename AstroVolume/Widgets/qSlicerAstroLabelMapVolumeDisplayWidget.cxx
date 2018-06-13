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

// Qt includes

#include <qSlicerAstroLabelMapVolumeDisplayWidget.h>
#include <ui_qSlicerAstroLabelMapVolumeDisplayWidget.h>

// MRML includes
#include <vtkMRMLColorNode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>

// VTK includes

//-----------------------------------------------------------------------------
/// \ingroup SlicerAstro_QtModules_Volumes
class qSlicerAstroLabelMapVolumeDisplayWidgetPrivate:
  public Ui_qSlicerAstroLabelMapVolumeDisplayWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroLabelMapVolumeDisplayWidget);
protected:
  qSlicerAstroLabelMapVolumeDisplayWidget* const q_ptr;
public:
  qSlicerAstroLabelMapVolumeDisplayWidgetPrivate(qSlicerAstroLabelMapVolumeDisplayWidget& object);
  ~qSlicerAstroLabelMapVolumeDisplayWidgetPrivate();
  void init();

  vtkWeakPointer<vtkMRMLAstroLabelMapVolumeNode> VolumeNode;
};

//-----------------------------------------------------------------------------
qSlicerAstroLabelMapVolumeDisplayWidgetPrivate::qSlicerAstroLabelMapVolumeDisplayWidgetPrivate(qSlicerAstroLabelMapVolumeDisplayWidget& object)
  : q_ptr(&object)
{
  this->VolumeNode = 0;
}

//-----------------------------------------------------------------------------
qSlicerAstroLabelMapVolumeDisplayWidgetPrivate::~qSlicerAstroLabelMapVolumeDisplayWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroLabelMapVolumeDisplayWidgetPrivate::init()
{
  Q_Q(qSlicerAstroLabelMapVolumeDisplayWidget);

  this->setupUi(q);
  QObject::connect(this->ColorTableComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setColorNode(vtkMRMLNode*)));
  QObject::connect(this->SliceIntersectionThicknessSpinBox, SIGNAL(valueChanged(int)),
                   q, SLOT(setSliceIntersectionThickness(int)));
  // disable as there is not MRML Node associated with the widget
  q->setEnabled(false);
}

// --------------------------------------------------------------------------
qSlicerAstroLabelMapVolumeDisplayWidget::qSlicerAstroLabelMapVolumeDisplayWidget(QWidget* _parent) : Superclass(_parent)
  , d_ptr(new qSlicerAstroLabelMapVolumeDisplayWidgetPrivate(*this))
{
  Q_D(qSlicerAstroLabelMapVolumeDisplayWidget);
  d->init();
}

// --------------------------------------------------------------------------
qSlicerAstroLabelMapVolumeDisplayWidget::~qSlicerAstroLabelMapVolumeDisplayWidget()
{
}

// --------------------------------------------------------------------------
vtkMRMLAstroLabelMapVolumeNode* qSlicerAstroLabelMapVolumeDisplayWidget::volumeNode()const
{
  Q_D(const qSlicerAstroLabelMapVolumeDisplayWidget);
  return d->VolumeNode;
}

// --------------------------------------------------------------------------
vtkMRMLAstroLabelMapVolumeDisplayNode* qSlicerAstroLabelMapVolumeDisplayWidget::volumeDisplayNode()const
{
  Q_D(const qSlicerAstroLabelMapVolumeDisplayWidget);
  return d->VolumeNode ? vtkMRMLAstroLabelMapVolumeDisplayNode::SafeDownCast(
    d->VolumeNode->GetAstroLabelMapVolumeDisplayNode()) : 0;
}

// --------------------------------------------------------------------------
void qSlicerAstroLabelMapVolumeDisplayWidget::setMRMLVolumeNode(vtkMRMLNode* node)
{
  this->setMRMLVolumeNode(vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(node));
}

// --------------------------------------------------------------------------
void qSlicerAstroLabelMapVolumeDisplayWidget::setMRMLVolumeNode(vtkMRMLAstroLabelMapVolumeNode* volumeNode)
{
  Q_D(qSlicerAstroLabelMapVolumeDisplayWidget);
  vtkMRMLAstroLabelMapVolumeDisplayNode* oldVolumeDisplayNode = this->volumeDisplayNode();

  qvtkReconnect(oldVolumeDisplayNode, volumeNode ? volumeNode->GetDisplayNode() : 0,
                vtkCommand::ModifiedEvent,
                this, SLOT(updateWidgetFromMRML()));
  d->VolumeNode = volumeNode;
  this->setEnabled(volumeNode != 0);
  this->updateWidgetFromMRML();
}

// --------------------------------------------------------------------------
void qSlicerAstroLabelMapVolumeDisplayWidget::updateWidgetFromMRML()
{
  Q_D(qSlicerAstroLabelMapVolumeDisplayWidget);
  vtkMRMLAstroLabelMapVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (displayNode)
    {
    d->ColorTableComboBox->setCurrentNode(displayNode->GetColorNode());
    d->SliceIntersectionThicknessSpinBox->setValue(
       displayNode->GetSliceIntersectionThickness());
    }
}

// --------------------------------------------------------------------------
void qSlicerAstroLabelMapVolumeDisplayWidget::setColorNode(vtkMRMLNode* colorNode)
{
  vtkMRMLAstroLabelMapVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (!displayNode || !colorNode)
    {
    return;
    }
  Q_ASSERT(vtkMRMLColorNode::SafeDownCast(colorNode));
  displayNode->SetAndObserveColorNodeID(colorNode->GetID());

}

// --------------------------------------------------------------------------
void qSlicerAstroLabelMapVolumeDisplayWidget::setSliceIntersectionThickness(int thickness)
{
  vtkMRMLAstroLabelMapVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (!displayNode)
    {
    return;
    }
  displayNode->SetSliceIntersectionThickness(thickness);
}

//------------------------------------------------------------------------------
int qSlicerAstroLabelMapVolumeDisplayWidget::sliceIntersectionThickness()const
{
  Q_D(const qSlicerAstroLabelMapVolumeDisplayWidget);
  return d->SliceIntersectionThicknessSpinBox->value();
}

