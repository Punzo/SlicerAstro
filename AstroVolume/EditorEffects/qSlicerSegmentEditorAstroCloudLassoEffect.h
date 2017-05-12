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
  and was supported through the European Research Consil grant nr. 291531.

  This file was originally developed by Csaba Pinter, PerkLab, Queen's University
  and was supported through the Applied Cancer Research Unit program of Cancer Care
  Ontario with funds provided by the Ontario Ministry of Health and Long-Term Care

==============================================================================*/

#ifndef __qSlicerSegmentEditorAstroCloudLassoEffect_h
#define __qSlicerSegmentEditorAstroCloudLassoEffect_h

// Segmentations Editor Effects includes
#include "qSlicerAstroVolumeEditorEffectsExport.h"

#include "qSlicerSegmentEditorAbstractLabelEffect.h"

// CTK includes
#include <ctkPimpl.h>
#include <ctkVTKObject.h>

class qSlicerSegmentEditorAstroCloudLassoEffectPrivate;
class vtkPolyData;
class vtkObject;

/// \ingroup SlicerRt_QtModules_Segmentations
class Q_SLICER_ASTROVOLUME_EFFECTS_EXPORT qSlicerSegmentEditorAstroCloudLassoEffect :
  public qSlicerSegmentEditorAbstractLabelEffect
{
public:
  Q_OBJECT
  QVTK_OBJECT

public:
  typedef qSlicerSegmentEditorAbstractLabelEffect Superclass;
  qSlicerSegmentEditorAstroCloudLassoEffect(QObject* parent = NULL);
  virtual ~qSlicerSegmentEditorAstroCloudLassoEffect();

public:
  /// Get icon for effect to be displayed in segment editor
  virtual QIcon icon();

  /// Get help text for effect to be displayed in the help box
  Q_INVOKABLE virtual const QString helpText()const;

  /// Clone editor effect
  virtual qSlicerSegmentEditorAbstractEffect* clone();

  /// Perform actions to deactivate the effect (such as destroy actors, etc.)
  Q_INVOKABLE virtual void deactivate();

  /// Callback function invoked when interaction happens
  /// \param callerInteractor Interactor object that was observed to catch the event
  /// \param eid Event identifier
  /// \param viewWidget Widget of the Slicer layout view. Can be \sa qMRMLSliceWidget or \sa qMRMLThreeDWidget
  virtual bool processInteractionEvents(vtkRenderWindowInteractor* callerInteractor, unsigned long eid, qMRMLWidget* viewWidget);

  /// Callback function invoked when view node is modified
  /// \param callerViewNode View node that was observed to catch the event. Can be either \sa vtkMRMLSliceNode or \sa vtkMRMLViewNode
  /// \param eid Event identifier
  /// \param viewWidget Widget of the Slicer layout view. Can be \sa qMRMLSliceWidget or \sa qMRMLThreeDWidget
  virtual void processViewNodeEvents(vtkMRMLAbstractViewNode* callerViewNode, unsigned long eid, qMRMLWidget* viewWidget);

  /// Create options frame widgets, make connections, and add them to the main options frame using \sa addOptionsWidget
  virtual void setupOptionsFrame();

  /// Set default parameters in the parameter MRML node
  virtual void setMRMLDefaults();

  /// Perform actions needed on master volume change
  virtual void masterVolumeNodeChanged();

  /// Show Segment model
  virtual void CreateSurface(bool on);

public slots:
  /// Update user interface from parameter set node
  virtual void updateGUIFromMRML();

  /// Update parameter set node from user interface
  virtual void updateMRMLFromGUI();

  /// Update automatic Threshold value changed
  virtual void onThresholdValueChanged(double min, double max);

  /// Update automatic Threshold mode on/off
  virtual void onAutomaticThresholdModeChanged(bool mode);

  /// Update Threshold value to optimal one when EraseMode is on/off
  virtual void onEraseModeChanged(bool mode);

protected:
  /// Flag determining whether to paint or erase.
  /// Overridden in the \sa qSlicerSegmentEditorEraseEffect subclass
  bool m_Erase;

protected:
  QScopedPointer<qSlicerSegmentEditorAstroCloudLassoEffectPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerSegmentEditorAstroCloudLassoEffect);
  Q_DISABLE_COPY(qSlicerSegmentEditorAstroCloudLassoEffect);
};

#endif
