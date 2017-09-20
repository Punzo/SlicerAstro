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

#include "qSlicerAstroScalarVolumeDisplayWidget.h"
#include "ui_qSlicerAstroScalarVolumeDisplayWidget.h"

// MRML includes
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLColorNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLUnitNode.h>

// VTK includes
#include <vtkAlgorithm.h>
#include <vtkAlgorithmOutput.h>
#include <vtkColorTransferFunction.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

// STD includes
#include <limits>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_Volumes
class qSlicerAstroScalarVolumeDisplayWidgetPrivate
  : public Ui_qSlicerAstroScalarVolumeDisplayWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroScalarVolumeDisplayWidget);
protected:
  qSlicerAstroScalarVolumeDisplayWidget* const q_ptr;
public:
  qSlicerAstroScalarVolumeDisplayWidgetPrivate(qSlicerAstroScalarVolumeDisplayWidget& object);
  ~qSlicerAstroScalarVolumeDisplayWidgetPrivate();
  void init();

  vtkSmartPointer<vtkColorTransferFunction> ColorTransferFunction;
};

//-----------------------------------------------------------------------------
qSlicerAstroScalarVolumeDisplayWidgetPrivate::qSlicerAstroScalarVolumeDisplayWidgetPrivate(
  qSlicerAstroScalarVolumeDisplayWidget& object)
  : q_ptr(&object)
{
  this->ColorTransferFunction = vtkSmartPointer<vtkColorTransferFunction>::New();
}

//-----------------------------------------------------------------------------
qSlicerAstroScalarVolumeDisplayWidgetPrivate::~qSlicerAstroScalarVolumeDisplayWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidgetPrivate::init()
{
  Q_Q(qSlicerAstroScalarVolumeDisplayWidget);

  this->setupUi(q);

  QObject::connect(this->InterpolateCheckbox, SIGNAL(toggled(bool)),
                   q, SLOT(setInterpolate(bool)));
  QObject::connect(this->ColorTableComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setColorNode(vtkMRMLNode*)));
}

// --------------------------------------------------------------------------
qSlicerAstroScalarVolumeDisplayWidget::qSlicerAstroScalarVolumeDisplayWidget(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroScalarVolumeDisplayWidgetPrivate(*this))
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);
  d->init();

  // disable as there is not MRML Node associated with the widget
  this->setEnabled(false);
}

// --------------------------------------------------------------------------
qSlicerAstroScalarVolumeDisplayWidget::~qSlicerAstroScalarVolumeDisplayWidget()
{
}

// --------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroScalarVolumeDisplayWidget::volumeNode()const
{
  Q_D(const qSlicerAstroScalarVolumeDisplayWidget);
  return vtkMRMLAstroVolumeNode::SafeDownCast(
    d->MRMLWindowLevelWidget->mrmlVolumeNode());
}

// --------------------------------------------------------------------------
bool qSlicerAstroScalarVolumeDisplayWidget::isColorTableComboBoxEnabled()const
{
  Q_D(const qSlicerAstroScalarVolumeDisplayWidget);
  return d->ColorTableComboBox->isEnabled();
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setColorTableComboBoxEnabled(bool enable)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);
  d->ColorTableComboBox->setEnabled(enable);
}

// --------------------------------------------------------------------------
bool qSlicerAstroScalarVolumeDisplayWidget::isMRMLWindowLevelWidgetEnabled()const
{
  Q_D(const qSlicerAstroScalarVolumeDisplayWidget);
  return d->MRMLWindowLevelWidget->isEnabled();
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setMRMLWindowLevelWidgetEnabled(bool enable)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);
  d->MRMLWindowLevelWidget->setEnabled(enable);
}

// --------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode* qSlicerAstroScalarVolumeDisplayWidget::volumeDisplayNode()const
{
  vtkMRMLVolumeNode* volumeNode = this->volumeNode();
  return volumeNode ? vtkMRMLAstroVolumeDisplayNode::SafeDownCast(
    volumeNode->GetDisplayNode()) : 0;
}

// --------------------------------------------------------------------------
vtkImageData* qSlicerAstroScalarVolumeDisplayWidget::volumeImageData()const
{
  vtkMRMLVolumeNode* volumeNode = this->volumeNode();
  return volumeNode ? vtkImageData::SafeDownCast(
    volumeNode->GetImageData()) : 0;
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setMRMLVolumeNode(vtkMRMLNode* node)
{
  this->setMRMLVolumeNode(vtkMRMLAstroVolumeNode::SafeDownCast(node));
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setMRMLVolumeNode(vtkMRMLAstroVolumeNode* volumeNode)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  if(!volumeNode)
  {
    return;
  }

  vtkMRMLAstroVolumeDisplayNode* oldVolumeDisplayNode = this->volumeDisplayNode();
  d->MRMLWindowLevelWidget->setMRMLVolumeNode(volumeNode);
  d->MRMLVolumeThresholdWidget->setMRMLVolumeNode(volumeNode);
  qvtkReconnect(oldVolumeDisplayNode, volumeNode ? volumeNode->GetDisplayNode() :0,
                vtkCommand::ModifiedEvent,
                this, SLOT(updateWidgetFromMRML()));
  this->setEnabled(volumeNode != 0);
  this->updateWidgetFromMRML();
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::updateWidgetFromMRML()
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);
  vtkMRMLAstroVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (displayNode)
    {
    d->ColorTableComboBox->setCurrentNode(displayNode->GetColorNode());
    d->InterpolateCheckbox->setChecked(displayNode->GetInterpolate());
    }
  if (this->isVisible())
    {
    this->updateTransferFunction();
    }
}

//----------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::updateTransferFunction()
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);
  // from vtkKWWindowLevelThresholdEditor::UpdateTransferFunction
  vtkMRMLVolumeNode* volumeNode = d->MRMLWindowLevelWidget->mrmlVolumeNode();
  Q_ASSERT(volumeNode == d->MRMLVolumeThresholdWidget->mrmlVolumeNode());
  vtkImageData* imageData = volumeNode ? volumeNode->GetImageData() : 0;
  if (imageData == 0)
    {
    d->ColorTransferFunction->RemoveAllPoints();
    return;
    }
  double range[2] = {0,255};
  vtkMRMLAstroVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (displayNode)
    {
    displayNode->GetDisplayScalarRange(range);
    }
  else
    {
    imageData->GetScalarRange(range);
    }
  // AdjustRange call will take out points that are outside of the new
  // range, but it needs the points to be there in order to work, so call
  // RemoveAllPoints after it's done

  d->ColorTransferFunction->AdjustRange(range);
  d->ColorTransferFunction->RemoveAllPoints();

  double min = d->MRMLWindowLevelWidget->level() - 0.5 * d->MRMLWindowLevelWidget->window();
  double max = d->MRMLWindowLevelWidget->level() + 0.5 * d->MRMLWindowLevelWidget->window();
  double minVal = 0;
  double maxVal = 1;
  double low   = d->MRMLVolumeThresholdWidget->isOff() ? range[0] : d->MRMLVolumeThresholdWidget->lowerThreshold();
  double upper = d->MRMLVolumeThresholdWidget->isOff() ? range[1] : d->MRMLVolumeThresholdWidget->upperThreshold();

  d->ColorTransferFunction->SetColorSpaceToRGB();

  if (low >= max || upper <= min)
    {
    d->ColorTransferFunction->AddRGBPoint(range[0], 0, 0, 0);
    d->ColorTransferFunction->AddRGBPoint(range[1], 0, 0, 0);
    }
  else
    {
    max = qMax(min+0.001, max);
    low = qMax(range[0] + 0.001, low);
    min = qMax(range[0] + 0.001, min);
    upper = qMin(range[1] - 0.001, upper);

    if (min <= low)
      {
      minVal = (low - min)/(max - min);
      min = low + 0.001;
      }

    if (max >= upper)
      {
      maxVal = (upper - min)/(max-min);
      max = upper - 0.001;
      }

    d->ColorTransferFunction->AddRGBPoint(range[0], 0, 0, 0);
    d->ColorTransferFunction->AddRGBPoint(low, 0, 0, 0);
    d->ColorTransferFunction->AddRGBPoint(min, minVal, minVal, minVal);
    d->ColorTransferFunction->AddRGBPoint(max, maxVal, maxVal, maxVal);
    d->ColorTransferFunction->AddRGBPoint(upper, maxVal, maxVal, maxVal);
    if (upper+0.001 < range[1])
      {
      d->ColorTransferFunction->AddRGBPoint(upper+0.001, 0, 0, 0);
      d->ColorTransferFunction->AddRGBPoint(range[1], 0, 0, 0);
      }
    }

  d->ColorTransferFunction->SetAlpha(1.0);
  d->ColorTransferFunction->Build();
}

// -----------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::showEvent( QShowEvent * event )
{
  this->updateTransferFunction();
  this->Superclass::showEvent(event);
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setInterpolate(bool interpolate)
{
  vtkMRMLAstroVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (!displayNode)
    {
    return;
    }
  displayNode->SetInterpolate(interpolate);
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setColorNode(vtkMRMLNode* colorNode)
{
  vtkMRMLAstroVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (!displayNode || !colorNode)
    {
    return;
    }
  Q_ASSERT(vtkMRMLColorNode::SafeDownCast(colorNode));
  displayNode->SetAndObserveColorNodeID(colorNode->GetID());
}
