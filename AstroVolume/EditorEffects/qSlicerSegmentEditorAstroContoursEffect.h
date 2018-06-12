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

#ifndef __qSlicerSegmentEditorAstroContoursEffect_h
#define __qSlicerSegmentEditorAstroContoursEffect_h

// Segmentations Editor Effects includes
#include "qSlicerAstroVolumeEditorEffectsExport.h"

#include "qSlicerSegmentEditorAbstractLabelEffect.h"

class qSlicerSegmentEditorAstroContoursEffectPrivate;
class QString;

/// \ingroup SlicerRt_QtModules_Segmentations
class Q_SLICERASTRO_ASTROVOLUME_EFFECTS_EXPORT qSlicerSegmentEditorAstroContoursEffect :
  public qSlicerSegmentEditorAbstractLabelEffect
{
public:
  Q_OBJECT

public:
  typedef qSlicerSegmentEditorAbstractLabelEffect Superclass;
  qSlicerSegmentEditorAstroContoursEffect(QObject* parent = NULL);
  virtual ~qSlicerSegmentEditorAstroContoursEffect();

public:
  /// Get icon for effect to be displayed in segment editor
  virtual QIcon icon();

  /// Get help text for effect to be displayed in the help box
  Q_INVOKABLE virtual const QString helpText()const;

  /// Create options frame widgets, make connections, and add them to the main options frame using \sa addOptionsWidget
  virtual void setupOptionsFrame();

  /// Set default parameters in the parameter MRML node
  virtual void setMRMLDefaults();

  /// Clone editor effect
  virtual qSlicerSegmentEditorAbstractEffect* clone();

  /// Perform actions to activate the effect (show options frame, etc.)
  Q_INVOKABLE virtual void activate();

  /// Perform actions to deactivate the effect (such as destroy actors, etc.)
  Q_INVOKABLE virtual void deactivate();

  /// Perform actions needed on master volume change
  virtual void masterVolumeNodeChanged();

public slots:
  /// Update user interface from parameter set node
  virtual void updateGUIFromMRML();

  /// Update parameter set node from user interface
  virtual void updateMRMLFromGUI();

  /// Apply the Contours
  virtual void CreateContours();

protected:
  QScopedPointer<qSlicerSegmentEditorAstroContoursEffectPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerSegmentEditorAstroContoursEffect);
  Q_DISABLE_COPY(qSlicerSegmentEditorAstroContoursEffect);
};

#endif
