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

  This file was developed by Davide Punzo, Kapteyn Astronomical Institute,
  and was supported through the European Research Council grant nr. 291531.

==============================================================================*/

// Segmentations includes
#include "qSlicerSegmentEditorAstroCloudLassoEffect.h"
#include "vtkSlicerSegmentationsModuleLogic.h"

// Slicer includes
#include <qSlicerCoreApplication.h>
#include <vtkSlicerApplicationLogic.h>

// MRML includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLScalarVolumeNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLUnitNode.h>

// SegmentationCore includes
#include <vtkOrientedImageData.h>
#include <vtkSegmentation.h>

// Qt includes
#include <QCheckBox>
#include <QDebug>
#include <QLabel>

// CTK includes
#include <ctkRangeWidget.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>

// STD includes
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
//----------------------------------------------------------------------------
double AttributeToDouble(vtkMRMLNode* node, const char* name, double defaultValue)
{
  const char* value = node ? node->GetAttribute(name) : nullptr;
  return value && *value ? std::atof(value) : defaultValue;
}
}

//-----------------------------------------------------------------------------
class qSlicerSegmentEditorAstroCloudLassoEffectPrivate
{
public:
  QIcon CloudLassoIcon{ QIcon(":Icons/AstroCloudLasso.png") };
  QLabel* ThresholdRangeLabel{ nullptr };
  ctkRangeWidget* ThresholdRangeWidget{ nullptr };
  QCheckBox* AutomaticThresholdCheckBox{ nullptr };

  /// Last lasso (modifier labelmap), its segment, and the segment before it:
  /// used to apply the lasso again when the threshold range changes.
  vtkSmartPointer<vtkOrientedImageData> LastLasso;
  vtkSmartPointer<vtkOrientedImageData> SegmentBeforeLastLasso;
  std::string LastSegmentID;
  qSlicerSegmentEditorAbstractEffect::ModificationMode LastModificationMode{
    qSlicerSegmentEditorAbstractEffect::ModificationModeAdd };
  bool Reapplying{ false };

  void clearLastLasso()
  {
    this->LastLasso = nullptr;
    this->SegmentBeforeLastLasso = nullptr;
    this->LastSegmentID.clear();
  }
};

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAstroCloudLassoEffect::qSlicerSegmentEditorAstroCloudLassoEffect(QObject* parent)
  : Superclass(parent)
  , d_ptr(new qSlicerSegmentEditorAstroCloudLassoEffectPrivate)
{
  this->m_Name = QString("AstroCloudLasso");
  this->m_Title = QString("Cloud Lasso");
  this->m_ShowEffectCursorInThreeDView = true;
}

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAstroCloudLassoEffect::~qSlicerSegmentEditorAstroCloudLassoEffect() = default;

//-----------------------------------------------------------------------------
QIcon qSlicerSegmentEditorAstroCloudLassoEffect::icon()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  return d->CloudLassoIcon;
}

//-----------------------------------------------------------------------------
const QString qSlicerSegmentEditorAstroCloudLassoEffect::helpText() const
{
  return QString("<html>Select the emission inside a lasso.<p>"
                 "Left-click and drag in a slice view or in the 3D view to draw the lasso "
                 "(as with Scissors: shape and slice cut options below). "
                 "<b>Fill</b> selects only the voxels in the threshold range (by default from 3 times the "
                 "noise, the DisplayThreshold of the AstroVolume module, to the maximum of the data); "
                 "<b>Erase</b> removes all the voxels inside the lasso.<p>"
                 "Shortcut keys: <b>n</b> / <b>m</b> decrease / increase the minimum threshold, "
                 "<b>k</b> / <b>l</b> the maximum threshold, <b>c</b> toggles the automatic threshold "
                 "updating, <b>x</b> switches between fill and erase.</html>");
}

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAbstractEffect* qSlicerSegmentEditorAstroCloudLassoEffect::clone()
{
  return new qSlicerSegmentEditorAstroCloudLassoEffect();
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::activate()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  Superclass::activate();
  d->clearLastLasso();
  this->sourceVolumeNodeChanged();
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::deactivate()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  Superclass::deactivate();
  d->clearLastLasso();
  if (this->parameterSetNode())
    {
    this->parameterSetNode()->SetSourceVolumeIntensityMask(false);
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::setupOptionsFrame()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);

  // operation, shape and slice cut options of the Scissors effect
  Superclass::setupOptionsFrame();

  d->ThresholdRangeLabel = new QLabel("Threshold range:");
  this->addOptionsWidget(d->ThresholdRangeLabel);

  d->ThresholdRangeWidget = new ctkRangeWidget();
  d->ThresholdRangeWidget->setToolTip("Intensity range of the voxels selected by the lasso (fill).\n"
                                      "Shortcut keys: 'n' / 'm' decrease / increase the minimum,\n"
                                      "'k' / 'l' decrease / increase the maximum.");
  this->addOptionsWidget(d->ThresholdRangeWidget);

  d->AutomaticThresholdCheckBox = new QCheckBox("Automatic threshold updating");
  d->AutomaticThresholdCheckBox->setToolTip("Apply the last lasso again when the threshold range changes. "
                                            "Shortcut key: 'c'.");
  this->addOptionsWidget(d->AutomaticThresholdCheckBox);

  QObject::connect(d->ThresholdRangeWidget, SIGNAL(valuesChanged(double, double)),
                   this, SLOT(onThresholdValuesChanged(double, double)));
  QObject::connect(d->AutomaticThresholdCheckBox, SIGNAL(toggled(bool)),
                   this, SLOT(onAutomaticThresholdModeChanged(bool)));
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::setMRMLDefaults()
{
  // before the defaults of Scissors (erase inside): the lasso fills
  this->setParameterDefault("Operation", "FillInside");
  Superclass::setMRMLDefaults();
  this->setParameterDefault("ThresholdMinimum", 0.);
  this->setParameterDefault("ThresholdMaximum", 0.);
  this->setParameterDefault("AutomaticThresholdUpdating", 0);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::sourceVolumeNodeChanged()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  Superclass::sourceVolumeNodeChanged();

  vtkMRMLSegmentEditorNode* parameterSetNode = this->parameterSetNode();
  vtkMRMLScalarVolumeNode* sourceVolume = parameterSetNode ? parameterSetNode->GetSourceVolumeNode() : nullptr;
  if (!sourceVolume || !sourceVolume->GetImageData() || !d->ThresholdRangeWidget)
    {
    return;
    }

  double range[2] = { 0., 1. };
  sourceVolume->GetImageData()->GetScalarRange(range);
  const double minimum = AttributeToDouble(sourceVolume, "SlicerAstro.DATAMIN", range[0]);
  const double maximum = AttributeToDouble(sourceVolume, "SlicerAstro.DATAMAX", range[1]);
  // the emission: from 3 times the noise
  const double noise3 = 3. * AttributeToDouble(sourceVolume, "SlicerAstro.DisplayThreshold", 0.);

  int decimals = 6;
  vtkSlicerApplicationLogic* appLogic = qSlicerCoreApplication::application() ?
    qSlicerCoreApplication::application()->applicationLogic() : nullptr;
  vtkMRMLSelectionNode* selectionNode = appLogic ? appLogic->GetSelectionNode() : nullptr;
  vtkMRMLUnitNode* intensityUnit = selectionNode ? selectionNode->GetUnitNode("intensity") : nullptr;
  if (intensityUnit && sourceVolume->IsA("vtkMRMLAstroVolumeNode"))
    {
    decimals = intensityUnit->GetPrecision();
    }

  const bool wasBlocked = d->ThresholdRangeWidget->blockSignals(true);
  d->ThresholdRangeWidget->setDecimals(decimals);
  d->ThresholdRangeWidget->setRange(minimum, maximum);
  d->ThresholdRangeWidget->setSingleStep((maximum - minimum) / 100.);
  d->ThresholdRangeWidget->blockSignals(wasBlocked);

  const bool wasModifying = parameterSetNode->StartModify();
  this->setParameter("ThresholdMinimum", noise3 > minimum && noise3 < maximum ? noise3 : minimum);
  this->setParameter("ThresholdMaximum", maximum);
  parameterSetNode->EndModify(wasModifying);
  this->updateIntensityMask();
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::updateGUIFromMRML()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);

  Superclass::updateGUIFromMRML();
  if (!this->active() || !this->parameterSetNode() || !d->ThresholdRangeWidget)
    {
    return;
    }

  const bool fill = this->fillOperation();
  bool wasBlocked = d->ThresholdRangeWidget->blockSignals(true);
  d->ThresholdRangeWidget->setValues(this->doubleParameter("ThresholdMinimum"),
                                     this->doubleParameter("ThresholdMaximum"));
  d->ThresholdRangeWidget->setEnabled(fill);
  d->ThresholdRangeWidget->blockSignals(wasBlocked);

  wasBlocked = d->AutomaticThresholdCheckBox->blockSignals(true);
  d->AutomaticThresholdCheckBox->setChecked(this->integerParameter("AutomaticThresholdUpdating") != 0);
  d->AutomaticThresholdCheckBox->setEnabled(fill);
  d->AutomaticThresholdCheckBox->blockSignals(wasBlocked);
  d->ThresholdRangeLabel->setEnabled(fill);

  this->updateIntensityMask();
}

//-----------------------------------------------------------------------------
bool qSlicerSegmentEditorAstroCloudLassoEffect::fillOperation()
{
  return this->parameter("Operation").startsWith("Fill");
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::updateIntensityMask()
{
  vtkMRMLSegmentEditorNode* parameterSetNode = this->parameterSetNode();
  if (!parameterSetNode || !this->active())
    {
    return;
    }
  // the threshold range is the editable intensity range of the segment editor
  parameterSetNode->SetSourceVolumeIntensityMask(true);
  parameterSetNode->SetSourceVolumeIntensityMaskRange(this->doubleParameter("ThresholdMinimum"),
                                                      this->doubleParameter("ThresholdMaximum"));
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::onThresholdValuesChanged(double minimum, double maximum)
{
  if (!this->parameterSetNode())
    {
    return;
    }
  const bool wasModifying = this->parameterSetNode()->StartModify();
  this->setParameter("ThresholdMinimum", minimum);
  this->setParameter("ThresholdMaximum", maximum);
  this->parameterSetNode()->EndModify(wasModifying);
  this->updateIntensityMask();
  if (this->integerParameter("AutomaticThresholdUpdating"))
    {
    this->reapplyLastLasso();
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::onAutomaticThresholdModeChanged(bool automatic)
{
  this->setParameter("AutomaticThresholdUpdating", automatic ? 1 : 0);
}

//-----------------------------------------------------------------------------
bool qSlicerSegmentEditorAstroCloudLassoEffect::processInteractionEvents(
  vtkRenderWindowInteractor* callerInteractor, unsigned long eid, qMRMLWidget* viewWidget)
{
  if (eid == vtkCommand::KeyPressEvent && callerInteractor && callerInteractor->GetKeySym() &&
      this->parameterSetNode())
    {
    const std::string key(callerInteractor->GetKeySym());
    const bool fill = this->fillOperation();
    double minimum = this->doubleParameter("ThresholdMinimum");
    double maximum = this->doubleParameter("ThresholdMaximum");
    const double step = this->d_ptr->ThresholdRangeWidget ? this->d_ptr->ThresholdRangeWidget->singleStep() : 0.;
    if (fill && (key == "n" || key == "m" || key == "k" || key == "l"))
      {
      if (key == "n") { minimum -= step; }
      if (key == "m") { minimum += step; }
      if (key == "k") { maximum -= step; }
      if (key == "l") { maximum += step; }
      this->onThresholdValuesChanged(minimum, std::max(minimum, maximum));
      return true;
      }
    if (key == "c")
      {
      this->onAutomaticThresholdModeChanged(this->integerParameter("AutomaticThresholdUpdating") == 0);
      return true;
      }
    if (key == "x")
      {
      this->setParameter("Operation", fill ? "EraseInside" : "FillInside");
      return true;
      }
    }
  return Superclass::processInteractionEvents(callerInteractor, eid, viewWidget);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::modifySelectedSegmentByLabelmap(
  vtkOrientedImageData* modifierLabelmap, ModificationMode modificationMode, bool bypassMasking)
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);

  vtkMRMLSegmentEditorNode* parameterSetNode = this->parameterSetNode();
  vtkMRMLSegmentationNode* segmentationNode = parameterSetNode ? parameterSetNode->GetSegmentationNode() : nullptr;
  if (!d->Reapplying && modifierLabelmap && segmentationNode && parameterSetNode->GetSelectedSegmentID())
    {
    // keep the lasso and the segment before it (automatic threshold updating)
    d->LastLasso = vtkSmartPointer<vtkOrientedImageData>::New();
    d->LastLasso->DeepCopy(modifierLabelmap);
    d->LastModificationMode = modificationMode;
    d->LastSegmentID = parameterSetNode->GetSelectedSegmentID();
    d->SegmentBeforeLastLasso = vtkSmartPointer<vtkOrientedImageData>::New();
    if (!vtkSlicerSegmentationsModuleLogic::GetSegmentBinaryLabelmapRepresentation(
          segmentationNode, d->LastSegmentID, d->SegmentBeforeLastLasso, false))
      {
      d->SegmentBeforeLastLasso = nullptr;
      }
    }

  // erasing removes all the voxels inside the lasso (not only the ones in the threshold range)
  const bool restoreIntensityMask = modificationMode == ModificationModeRemove && parameterSetNode &&
                                    parameterSetNode->GetSourceVolumeIntensityMask();
  if (restoreIntensityMask)
    {
    parameterSetNode->SetSourceVolumeIntensityMask(false);
    }
  Superclass::modifySelectedSegmentByLabelmap(modifierLabelmap, modificationMode, bypassMasking);
  if (restoreIntensityMask)
    {
    parameterSetNode->SetSourceVolumeIntensityMask(true);
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::reapplyLastLasso()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);

  vtkMRMLSegmentEditorNode* parameterSetNode = this->parameterSetNode();
  vtkMRMLSegmentationNode* segmentationNode = parameterSetNode ? parameterSetNode->GetSegmentationNode() : nullptr;
  if (!d->LastLasso || !segmentationNode || d->LastModificationMode != ModificationModeAdd ||
      !segmentationNode->GetSegmentation()->GetSegment(d->LastSegmentID))
    {
    return;
    }
  const std::string selectedSegmentID = parameterSetNode->GetSelectedSegmentID() ?
    parameterSetNode->GetSelectedSegmentID() : "";
  parameterSetNode->SetSelectedSegmentID(d->LastSegmentID.c_str());

  // the segment as it was before the lasso, then the lasso with the current range
  if (d->SegmentBeforeLastLasso)
    {
    vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
      d->SegmentBeforeLastLasso, segmentationNode, d->LastSegmentID, vtkSlicerSegmentationsModuleLogic::MODE_REPLACE);
    }
  d->Reapplying = true;
  this->modifySelectedSegmentByLabelmap(d->LastLasso, d->LastModificationMode);
  d->Reapplying = false;

  parameterSetNode->SetSelectedSegmentID(selectedSegmentID.empty() ? nullptr : selectedSegmentID.c_str());
}
