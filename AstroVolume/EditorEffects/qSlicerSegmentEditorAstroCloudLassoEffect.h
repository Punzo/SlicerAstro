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
/// Scissors effect (this effect is a Scissors effect: same shapes and drawing).
/// In a slice view the lasso selects the voxels of the current slice, in the
/// 3D view the voxels through the whole cube (no slice cut options). Filling selects only the voxels whose intensity is in
/// the threshold range (by default from 3 times the noise, the DisplayThreshold
/// of the AstroVolume module, to the maximum of the data); erasing removes all
/// the voxels inside the lasso. With the automatic threshold updating, changing
/// the threshold applies the last lasso again.
///
/// Keyboard shortcuts (while the effect is active): n / m decrease / increase
/// the minimum of the threshold range, k / l the maximum, c toggles the
/// automatic threshold updating, x switches between filling and erasing.
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

  /// Drawing of the Scissors effect, with the lasso decoration
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

  /// Keyboard shortcuts (n, m, k, l, c, x)
  void decreaseThresholdMinimum();
  void increaseThresholdMinimum();
  void decreaseThresholdMaximum();
  void increaseThresholdMaximum();
  void toggleAutomaticThresholdMode();
  void toggleFillErase();

protected:
  QScopedPointer<qSlicerSegmentEditorAstroCloudLassoEffectPrivate> d_ptr;

  /// Threshold range used as editable intensity range (source volume intensity mask)
  void updateIntensityMask();
  bool fillOperation();

  /// Style of the lasso drawn by the Scissors effect: amber outline with a soft
  /// glow and a translucent fill (red when erasing).
  void updateLassoDecoration(qMRMLWidget* viewWidget);
  void removeLassoDecorations();

  /// Keyboard shortcuts on the main window while the effect is active: they
  /// work wherever the focus is (e.g. after clicking an option), not only
  /// in the views.
  void installKeyboardShortcuts();
  void removeKeyboardShortcuts();
  void stepThreshold(bool minimum, double direction);

private:
  Q_DECLARE_PRIVATE(qSlicerSegmentEditorAstroCloudLassoEffect);
  Q_DISABLE_COPY(qSlicerSegmentEditorAstroCloudLassoEffect);
};

#endif
