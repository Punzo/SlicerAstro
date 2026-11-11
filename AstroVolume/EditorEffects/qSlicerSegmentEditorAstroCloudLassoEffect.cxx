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
#include <QRadioButton>
#include <QStringList>

// CTK includes
#include <ctkRangeWidget.h>

// VTK includes
#include <vtkActor2D.h>
#include <vtkActor2DCollection.h>
#include <vtkCellArray.h>
#include <vtkCommand.h>
#include <vtkContourTriangulator.h>
#include <vtkIdList.h>
#include <vtkImageData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

// STD includes
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
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
/// Decoration of the lasso of the Scissors effect in a view
struct LassoDecoration
{
  vtkWeakPointer<vtkRenderer> Renderer;
  vtkWeakPointer<vtkActor2D> Outline;  // drawn path (Scissors)
  vtkWeakPointer<vtkActor2D> Closing;  // segment back to the first point (Scissors)
  vtkSmartPointer<vtkActor2D> Glow;
  vtkSmartPointer<vtkActor2D> Fill;
  vtkSmartPointer<vtkPolyData> FillPolyData;
};

//-----------------------------------------------------------------------------
class qSlicerSegmentEditorAstroCloudLassoEffectPrivate
{
public:
  std::map<vtkRenderer*, LassoDecoration> LassoDecorations;

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
                 "Left-click and drag in a slice view or in the 3D view to draw the lasso: "
                 "in a slice view it selects the voxels of the current slice, in the 3D view "
                 "the voxels through the whole cube. "
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
  // in a slice view: only the current slice (the slice cut options are hidden)
  this->setParameter("SliceCutMode", "Symmetric");
  this->setParameter("SliceCutDepthMm", 0.);
  this->sourceVolumeNodeChanged();
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::deactivate()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  this->removeLassoDecorations();
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

  // operation and shape options of the Scissors effect
  Superclass::setupOptionsFrame();

  // No slice cut options: in a slice view the lasso selects the voxels of the
  // current slice, in the 3D view through the whole cube (the widgets of the
  // Scissors effect have no object names: they are found by their text).
  const QStringList sliceCutTexts = { Superclass::tr("Slice cut:"), Superclass::tr("Unlimited"),
                                      Superclass::tr("Positive"), Superclass::tr("Negative"),
                                      Superclass::tr("Symmetric") };
  for (QRadioButton* button : this->optionsFrame()->findChildren<QRadioButton*>())
    {
    if (sliceCutTexts.contains(button->text()))
      {
      button->hide();
      }
    }
  for (QLabel* label : this->optionsFrame()->findChildren<QLabel*>())
    {
    if (sliceCutTexts.contains(label->text()))
      {
      label->hide();
      }
    else if (label->text() == Superclass::tr("Apply to visible segments:"))
      {
      label->setText(tr("Apply to all visible segments:"));
      }
    }
  for (QWidget* widget : this->optionsFrame()->findChildren<QWidget*>())
    {
    // depth of the slice cut: the only length spin box of the Scissors options
    if (widget->inherits("qMRMLSpinBox") && widget->property("quantity").toString() == "length")
      {
      widget->hide();
      }
    }

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
  // before the defaults of Scissors (erase inside, unlimited slice cut):
  // the lasso fills, in a slice view only the current slice
  this->setParameterDefault("Operation", "FillInside");
  this->setParameterDefault("SliceCutMode", "Symmetric");
  this->setParameterDefault("SliceCutDepthMm", 0.);
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
  const bool abortEvent = Superclass::processInteractionEvents(callerInteractor, eid, viewWidget);
  this->updateLassoDecoration(viewWidget);
  return abortEvent;
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::updateLassoDecoration(qMRMLWidget* viewWidget)
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);

  vtkRenderer* renderer = qSlicerSegmentEditorAbstractEffect::renderer(viewWidget);
  if (!renderer)
    {
    return;
    }
  LassoDecoration& decoration = d->LassoDecorations[renderer];
  decoration.Renderer = renderer;

  if (!decoration.Outline)
    {
    // the actors of the Scissors effect, recognized by their default style
    vtkActor2DCollection* actors = renderer->GetActors2D();
    actors->InitTraversal();
    for (vtkActor2D* actor = actors->GetNextActor2D(); actor; actor = actors->GetNextActor2D())
      {
      if (!vtkPolyDataMapper2D::SafeDownCast(actor->GetMapper()) || actor == decoration.Glow || actor == decoration.Fill)
        {
        continue;
        }
      const double* color = actor->GetProperty()->GetColor();
      const double width = actor->GetProperty()->GetLineWidth();
      if (color[0] == 1. && color[1] == 1. && color[2] == 0. && width == 2.)
        {
        decoration.Outline = actor;
        }
      else if (color[0] == 0.7 && color[1] == 0.7 && color[2] == 0. && width == 1.)
        {
        decoration.Closing = actor;
        }
      }
    if (!decoration.Outline)
      {
      return;
      }
    vtkPolyDataMapper2D* outlineMapper = vtkPolyDataMapper2D::SafeDownCast(decoration.Outline->GetMapper());

    // soft glow under the outline (same path)
    decoration.Glow = vtkSmartPointer<vtkActor2D>::New();
    vtkNew<vtkPolyDataMapper2D> glowMapper;
    glowMapper->SetInputConnection(outlineMapper->GetInputConnection(0, 0));
    if (!outlineMapper->GetInputConnection(0, 0))
      {
      glowMapper->SetInputData(vtkPolyData::SafeDownCast(outlineMapper->GetInputDataObject(0, 0)));
      }
    glowMapper->SetTransformCoordinate(outlineMapper->GetTransformCoordinate());
    decoration.Glow->SetMapper(glowMapper);
    decoration.Glow->GetProperty()->SetLineWidth(9.);
    decoration.Glow->GetProperty()->SetOpacity(0.22);

    // translucent fill of the area of the lasso
    decoration.FillPolyData = vtkSmartPointer<vtkPolyData>::New();
    decoration.Fill = vtkSmartPointer<vtkActor2D>::New();
    vtkNew<vtkPolyDataMapper2D> fillMapper;
    fillMapper->SetInputData(decoration.FillPolyData);
    fillMapper->SetTransformCoordinate(outlineMapper->GetTransformCoordinate());
    decoration.Fill->SetMapper(fillMapper);
    decoration.Fill->GetProperty()->SetOpacity(0.15);

    // drawing order: fill, glow, outline, closing segment
    renderer->RemoveViewProp(decoration.Outline);
    if (decoration.Closing)
      {
      renderer->RemoveViewProp(decoration.Closing);
      }
    renderer->AddViewProp(decoration.Fill);
    renderer->AddViewProp(decoration.Glow);
    renderer->AddViewProp(decoration.Outline);
    if (decoration.Closing)
      {
      renderer->AddViewProp(decoration.Closing);
      }
    }

  // colors: amber when filling, red when erasing
  const bool fill = this->fillOperation();
  const double amber[3] = { 1.0, 0.73, 0.08 };
  const double red[3] = { 1.0, 0.32, 0.32 };
  const double* color = fill ? amber : red;
  decoration.Outline->GetProperty()->SetColor(color[0], color[1], color[2]);
  decoration.Outline->GetProperty()->SetLineWidth(2.5);
  decoration.Glow->GetProperty()->SetColor(color[0], color[1], color[2]);
  decoration.Fill->GetProperty()->SetColor(color[0], color[1], color[2]);
  if (decoration.Closing)
    {
    decoration.Closing->GetProperty()->SetColor(color[0], color[1], color[2]);
    decoration.Closing->GetProperty()->SetOpacity(0.55);
    decoration.Closing->GetProperty()->SetLineWidth(1.5);
    }

  // the decoration follows the lasso
  const bool visible = decoration.Outline->GetVisibility() != 0;
  decoration.Glow->SetVisibility(visible);
  decoration.Fill->SetVisibility(visible);
  vtkPolyData* path = vtkPolyData::SafeDownCast(decoration.Outline->GetMapper()->GetInputDataObject(0, 0));
  vtkPoints* points = path ? path->GetPoints() : nullptr;
  vtkNew<vtkCellArray> triangles;
  if (visible && points && points->GetNumberOfPoints() >= 3)
    {
    vtkNew<vtkIdList> polygon;
    for (vtkIdType pointIndex = 0; pointIndex < points->GetNumberOfPoints(); ++pointIndex)
      {
      polygon->InsertNextId(pointIndex);
      }
    // concave polygons are triangulated correctly; self-intersecting lassos are partly filled
    vtkContourTriangulator::TriangulatePolygon(polygon, points, triangles);
    }
  decoration.FillPolyData->SetPoints(points);
  decoration.FillPolyData->SetPolys(triangles);
  decoration.FillPolyData->Modified();
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::removeLassoDecorations()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  for (std::pair<vtkRenderer* const, LassoDecoration>& rendererDecoration : d->LassoDecorations)
    {
    LassoDecoration& decoration = rendererDecoration.second;
    if (decoration.Renderer)
      {
      decoration.Renderer->RemoveViewProp(decoration.Glow);
      decoration.Renderer->RemoveViewProp(decoration.Fill);
      }
    }
  d->LassoDecorations.clear();
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
