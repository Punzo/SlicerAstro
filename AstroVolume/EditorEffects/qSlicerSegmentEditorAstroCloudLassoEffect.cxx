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
#include <qSlicerApplication.h>
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
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QShortcut>
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
#include <vtkPointData.h>
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
  QList<QPointer<QShortcut>> KeyboardShortcuts;

  QIcon CloudLassoIcon{ QIcon(":Icons/AstroCloudLasso.png") };
  QCheckBox* AutomaticThresholdCheckBox{ nullptr };

  /// Editable intensity range of the Segment Editor when the effect was activated
  bool IntensityMaskEnabledBeforeActivation{ false };
  /// Last intensity range seen (automatic threshold updating)
  double LastIntensityRange[2]{ 0., 0. };
  bool LastIntensityRangeValid{ false };
  bool UpdatingIntensityRange{ false };
  vtkWeakPointer<vtkMRMLSegmentEditorNode> ObservedSegmentEditorNode;

  /// Last lasso (modifier labelmap), its segment, and the segment before it:
  /// used to apply the lasso again when the intensity range changes.
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
                 "Drag in a slice view (current slice) or in the 3D view (whole cube). "
                 "<b>Fill</b> keeps only the voxels in the <i>Editable intensity range</i> "
                 "(Masking; default: 3 times the noise to the maximum). <b>Erase</b> clears the lasso.<p>"
                 "Keys: <b>n</b>/<b>m</b> minimum -/+, <b>k</b>/<b>l</b> maximum -/+, "
                 "<b>c</b> update the last lasso with the range, <b>x</b> fill/erase.</html>");
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
  this->installKeyboardShortcuts();
  d->clearLastLasso();
  // in a slice view: only the current slice (the slice cut options are hidden)
  this->setParameter("SliceCutMode", "Symmetric");
  this->setParameter("SliceCutDepthMm", 0.);
  vtkMRMLSegmentEditorNode* parameterSetNode = this->parameterSetNode();
  d->IntensityMaskEnabledBeforeActivation = parameterSetNode && parameterSetNode->GetSourceVolumeIntensityMask();
  this->sourceVolumeNodeChanged();
  // the Segment Editor notifies the effects only of changes of their parameters
  d->ObservedSegmentEditorNode = parameterSetNode;
  this->qvtkConnect(parameterSetNode, vtkCommand::ModifiedEvent, this, SLOT(onSegmentEditorNodeModified()));
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::deactivate()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  this->removeKeyboardShortcuts();
  this->removeLassoDecorations();
  this->qvtkDisconnect(d->ObservedSegmentEditorNode, vtkCommand::ModifiedEvent, this, SLOT(onSegmentEditorNodeModified()));
  d->ObservedSegmentEditorNode = nullptr;
  Superclass::deactivate();
  d->clearLastLasso();
  d->LastIntensityRangeValid = false;
  // the editable intensity range is enabled as it was (its values are kept)
  if (this->parameterSetNode())
    {
    this->parameterSetNode()->SetSourceVolumeIntensityMask(d->IntensityMaskEnabledBeforeActivation);
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

  // The intensity range of the lasso is the editable intensity range of the
  // Masking section of the Segment Editor (no duplicated threshold widget)
  d->AutomaticThresholdCheckBox = new QCheckBox("Update the last lasso with the intensity range");
  d->AutomaticThresholdCheckBox->setToolTip(
    "Draw a lasso, then tune the selection: when the Editable intensity range of the Masking "
    "section changes (slider or keys n/m/k/l), the last lasso is applied again with the new "
    "range, replacing its previous result. Only for Fill. Shortcut key: c.");
  this->addOptionsWidget(d->AutomaticThresholdCheckBox);
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
  this->setParameterDefault("AutomaticThresholdUpdating", 0);
}

//-----------------------------------------------------------------------------
ctkRangeWidget* qSlicerSegmentEditorAstroCloudLassoEffect::intensityRangeWidget()
{
  // editable intensity range widget of the Segment Editor that contains the options of the effect
  for (QWidget* widget = this->optionsFrame(); widget; widget = widget->parentWidget())
    {
    if (widget->inherits("qMRMLSegmentEditorWidget"))
      {
      return widget->findChild<ctkRangeWidget*>("SourceVolumeIntensityMaskRangeWidget");
      }
    }
  return nullptr;
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::sourceVolumeNodeChanged()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  Superclass::sourceVolumeNodeChanged();

  vtkMRMLSegmentEditorNode* parameterSetNode = this->parameterSetNode();
  vtkMRMLScalarVolumeNode* sourceVolume = parameterSetNode ? parameterSetNode->GetSourceVolumeNode() : nullptr;
  if (!this->active() || !sourceVolume || !sourceVolume->GetImageData())
    {
    return;
    }

  double range[2] = { 0., 1. };
  sourceVolume->GetImageData()->GetScalarRange(range);
  const double minimum = AttributeToDouble(sourceVolume, "SlicerAstro.DATAMIN", range[0]);
  const double maximum = AttributeToDouble(sourceVolume, "SlicerAstro.DATAMAX", range[1]);
  // the emission: from 3 times the noise
  const double noise3 = 3. * AttributeToDouble(sourceVolume, "SlicerAstro.DisplayThreshold", 0.);
  const double step = (maximum - minimum) / 100.;

  // Editable intensity range of the Masking section: decimals, unit and step
  // suited to the data (astronomical intensities are small, e.g. 0.001 Jy/beam)
  ctkRangeWidget* rangeWidget = this->intensityRangeWidget();
  if (rangeWidget && sourceVolume->IsA("vtkMRMLAstroVolumeNode") && step > 0.)
    {
    int decimals = 0;
    vtkSlicerApplicationLogic* appLogic = qSlicerCoreApplication::application() ?
      qSlicerCoreApplication::application()->applicationLogic() : nullptr;
    vtkMRMLSelectionNode* selectionNode = appLogic ? appLogic->GetSelectionNode() : nullptr;
    vtkMRMLUnitNode* intensityUnit = selectionNode ? selectionNode->GetUnitNode("intensity") : nullptr;
    if (intensityUnit)
      {
      decimals = intensityUnit->GetPrecision();
      }
    // at least the precision of the step
    decimals = std::max(decimals, static_cast<int>(std::ceil(-std::log10(step))) + 1);
    const char* unit = sourceVolume->GetAttribute("SlicerAstro.BUNIT");
    const bool wasBlocked = rangeWidget->blockSignals(true);
    rangeWidget->setDecimals(std::min(decimals, 10));
    rangeWidget->setSingleStep(step);
    rangeWidget->setSuffix(unit && *unit ? QString(" ") + unit : QString());
    rangeWidget->blockSignals(wasBlocked);
    }

  // the lasso selects the emission: editable intensity range from 3 times the noise
  d->UpdatingIntensityRange = true;
  const bool wasModifying = parameterSetNode->StartModify();
  parameterSetNode->SetSourceVolumeIntensityMask(true);
  parameterSetNode->SetSourceVolumeIntensityMaskRange(noise3 > minimum && noise3 < maximum ? noise3 : minimum,
                                                      maximum);
  parameterSetNode->EndModify(wasModifying);
  d->UpdatingIntensityRange = false;
  // reference for the automatic threshold updating
  d->LastIntensityRange[0] = parameterSetNode->GetSourceVolumeIntensityMaskRange()[0];
  d->LastIntensityRange[1] = parameterSetNode->GetSourceVolumeIntensityMaskRange()[1];
  d->LastIntensityRangeValid = true;
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::updateGUIFromMRML()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);

  Superclass::updateGUIFromMRML();
  vtkMRMLSegmentEditorNode* parameterSetNode = this->parameterSetNode();
  if (!this->active() || !parameterSetNode || !d->AutomaticThresholdCheckBox)
    {
    return;
    }

  const bool fill = this->fillOperation();
  const bool wasBlocked = d->AutomaticThresholdCheckBox->blockSignals(true);
  d->AutomaticThresholdCheckBox->setChecked(this->integerParameter("AutomaticThresholdUpdating") != 0);
  d->AutomaticThresholdCheckBox->setEnabled(fill);
  d->AutomaticThresholdCheckBox->blockSignals(wasBlocked);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::onSegmentEditorNodeModified()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  vtkMRMLSegmentEditorNode* parameterSetNode = this->parameterSetNode();
  if (!this->active() || !parameterSetNode)
    {
    return;
    }
  const bool fill = this->fillOperation();
  // automatic threshold updating: the last lasso again when the range changes
  const double* range = parameterSetNode->GetSourceVolumeIntensityMaskRange();
  const bool rangeChanged = d->LastIntensityRangeValid &&
    (range[0] != d->LastIntensityRange[0] || range[1] != d->LastIntensityRange[1]);
  d->LastIntensityRange[0] = range[0];
  d->LastIntensityRange[1] = range[1];
  d->LastIntensityRangeValid = true;
  if (rangeChanged && !d->UpdatingIntensityRange && !d->Reapplying && fill &&
      parameterSetNode->GetSourceVolumeIntensityMask() &&
      this->integerParameter("AutomaticThresholdUpdating"))
    {
    this->reapplyLastLasso();
    }
}

//-----------------------------------------------------------------------------
bool qSlicerSegmentEditorAstroCloudLassoEffect::fillOperation()
{
  return this->parameter("Operation").startsWith("Fill");
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
void qSlicerSegmentEditorAstroCloudLassoEffect::installKeyboardShortcuts()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  this->removeKeyboardShortcuts();

  QWidget* parent = qSlicerApplication::application() ? qSlicerApplication::application()->mainWindow() : nullptr;
  if (!parent && this->optionsFrame())
    {
    parent = this->optionsFrame()->window();
    }
  if (!parent)
    {
    return;
    }
  const char* keys[] = { "n", "m", "k", "l", "c", "x" };
  const char* slotSignatures[] = { SLOT(decreaseThresholdMinimum()), SLOT(increaseThresholdMinimum()),
                          SLOT(decreaseThresholdMaximum()), SLOT(increaseThresholdMaximum()),
                          SLOT(toggleAutomaticThresholdMode()), SLOT(toggleFillErase()) };
  for (int index = 0; index < 6; ++index)
    {
    // the text fields (e.g. renaming a segment) keep the keys typed in them
    QShortcut* shortcut = new QShortcut(QKeySequence(keys[index]), parent);
    shortcut->setObjectName(QString("AstroCloudLassoShortcut_") + keys[index]);
    QObject::connect(shortcut, SIGNAL(activated()), this, slotSignatures[index]);
    d->KeyboardShortcuts.push_back(shortcut);
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::removeKeyboardShortcuts()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  for (QPointer<QShortcut>& shortcut : d->KeyboardShortcuts)
    {
    if (shortcut)
      {
      shortcut->setEnabled(false);
      shortcut->deleteLater();
      }
    }
  d->KeyboardShortcuts.clear();
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::stepThreshold(bool minimum, double direction)
{
  vtkMRMLSegmentEditorNode* parameterSetNode = this->parameterSetNode();
  vtkMRMLScalarVolumeNode* sourceVolume = parameterSetNode ? parameterSetNode->GetSourceVolumeNode() : nullptr;
  if (!this->active() || !sourceVolume || !sourceVolume->GetImageData() || !this->fillOperation())
    {
    return;
    }
  double dataRange[2] = { 0., 1. };
  sourceVolume->GetImageData()->GetScalarRange(dataRange);
  const double step = (AttributeToDouble(sourceVolume, "SlicerAstro.DATAMAX", dataRange[1]) -
                       AttributeToDouble(sourceVolume, "SlicerAstro.DATAMIN", dataRange[0])) / 100.;
  double lower = parameterSetNode->GetSourceVolumeIntensityMaskRange()[0];
  double upper = parameterSetNode->GetSourceVolumeIntensityMaskRange()[1];
  if (minimum)
    {
    lower = std::min(lower + direction * step, upper);
    }
  else
    {
    upper = std::max(upper + direction * step, lower);
    }
  // the Masking section shows the range; the automatic updating follows it
  parameterSetNode->SetSourceVolumeIntensityMask(true);
  parameterSetNode->SetSourceVolumeIntensityMaskRange(lower, upper);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::decreaseThresholdMinimum()
{
  this->stepThreshold(true, -1.);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::increaseThresholdMinimum()
{
  this->stepThreshold(true, 1.);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::decreaseThresholdMaximum()
{
  this->stepThreshold(false, -1.);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::increaseThresholdMaximum()
{
  this->stepThreshold(false, 1.);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::toggleAutomaticThresholdMode()
{
  if (!this->active() || !this->parameterSetNode())
    {
    return;
    }
  this->onAutomaticThresholdModeChanged(this->integerParameter("AutomaticThresholdUpdating") == 0);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::toggleFillErase()
{
  if (!this->active() || !this->parameterSetNode())
    {
    return;
    }
  this->setParameter("Operation", this->fillOperation() ? "EraseInside" : "FillInside");
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

  // erasing removes all the voxels inside the lasso (not only the ones in the intensity range)
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

  // the segment as it was before the lasso (empty if it was empty), then the
  // lasso with the current range
  vtkSlicerSegmentationsModuleLogic::ClearSegment(segmentationNode, d->LastSegmentID);
  if (d->SegmentBeforeLastLasso && d->SegmentBeforeLastLasso->GetNumberOfPoints() > 0 &&
      d->SegmentBeforeLastLasso->GetPointData()->GetScalars())
    {
    vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
      d->SegmentBeforeLastLasso, segmentationNode, d->LastSegmentID, vtkSlicerSegmentationsModuleLogic::MODE_REPLACE);
    }
  d->Reapplying = true;
  this->modifySelectedSegmentByLabelmap(d->LastLasso, d->LastModificationMode);
  d->Reapplying = false;

  parameterSetNode->SetSelectedSegmentID(selectedSegmentID.empty() ? nullptr : selectedSegmentID.c_str());
}
