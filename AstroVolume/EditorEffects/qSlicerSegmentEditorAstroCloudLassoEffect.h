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

#ifndef __qSlicerSegmentEditorAstroCloudLassoEffect_h
#define __qSlicerSegmentEditorAstroCloudLassoEffect_h

// Segmentations Editor Effects includes
#include "qSlicerAstroVolumeEditorEffectsExport.h"
#include "qSlicerSegmentEditorScissorsEffect.h"

class qSlicerSegmentEditorAstroCloudLassoEffectPrivate;

/// \brief Cloud Lasso: select the emission (the "cloud") inside a lasso.
///
/// The lasso is drawn in 2D on a slice view or on the 3D view, as with the
/// Scissors effect (this effect is a Scissors effect: same shapes, slice cut
/// modes, and drawing). Filling selects only the voxels whose intensity is in
/// the threshold range (by default from 3 times the noise, the DisplayThreshold
/// of the AstroVolume module, to the maximum of the data); erasing removes all
/// the voxels inside the lasso. With the automatic threshold updating, changing
/// the threshold applies the last lasso again.
///
/// Keyboard shortcuts in the views: n / m decrease / increase the minimum of
/// the threshold range, k / l the maximum, c toggles the automatic threshold
/// updating, x switches between filling and erasing.
/// \ingroup SlicerAstro_QtModules_AstroVolume
class Q_SLICERASTRO_ASTROVOLUME_EFFECTS_EXPORT qSlicerSegmentEditorAstroCloudLassoEffect :
  public qSlicerSegmentEditorScissorsEffect
{
public:
  Q_OBJECT

public:
  typedef qSlicerSegmentEditorScissorsEffect Superclass;
  qSlicerSegmentEditorAstroCloudLassoEffect(QObject* parent = nullptr);
  ~qSlicerSegmentEditorAstroCloudLassoEffect() override;

  QIcon icon() override;
  Q_INVOKABLE const QString helpText() const override;
  qSlicerSegmentEditorAbstractEffect* clone() override;

  Q_INVOKABLE void activate() override;
  Q_INVOKABLE void deactivate() override;

  void setupOptionsFrame() override;
  void setMRMLDefaults() override;
  void sourceVolumeNodeChanged() override;

  /// Keyboard shortcuts, then the drawing of the Scissors effect
  bool processInteractionEvents(vtkRenderWindowInteractor* callerInteractor, unsigned long eid,
                                qMRMLWidget* viewWidget) override;

  /// Keeps the lasso (for the automatic threshold updating); erasing is not
  /// limited to the threshold range.
  using Superclass::modifySelectedSegmentByLabelmap;
  void modifySelectedSegmentByLabelmap(vtkOrientedImageData* modifierLabelmap, ModificationMode modificationMode,
                                       bool bypassMasking = false) override;

  /// Apply the last lasso again with the current threshold range.
  Q_INVOKABLE void reapplyLastLasso();

public slots:
  void updateGUIFromMRML() override;

  void onThresholdValuesChanged(double minimum, double maximum);
  void onAutomaticThresholdModeChanged(bool automatic);

protected:
  QScopedPointer<qSlicerSegmentEditorAstroCloudLassoEffectPrivate> d_ptr;

  /// Threshold range used as editable intensity range (source volume intensity mask)
  void updateIntensityMask();
  bool fillOperation();

private:
  Q_DECLARE_PRIVATE(qSlicerSegmentEditorAstroCloudLassoEffect);
  Q_DISABLE_COPY(qSlicerSegmentEditorAstroCloudLassoEffect);
};

#endif
