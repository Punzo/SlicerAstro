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

==============================================================================*/

#ifndef __qSlicerAstroVolumeModuleWidget_p_h
#define __qSlicerAstroVolumeModuleWidget_p_h

//
//  W A R N I N G
//  -------------
//
// This file is not part of the SlicerAstro API: private members of
// qSlicerAstroVolumeModuleWidget, shared by its source files (panel and
// comparison views).
//

// VTK includes
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

// MRML includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSelectionNode.h>

#include "qSlicerAstroVolumeModuleWidget.h"

class QStackedWidget;
class qSlicerAstroDatasetSelector;
class qSlicerAstroPanelHeader;
class qSlicerAstroSegmentedButton;
class QToolButton;
class vtkSlicerAstroVolumeLogic;

//-----------------------------------------------------------------------------
class qSlicerAstroVolumeModuleWidgetPrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroVolumeModuleWidget);

protected:
  qSlicerAstroVolumeModuleWidget* const q_ptr;

public:
  qSlicerAstroVolumeModuleWidgetPrivate(qSlicerAstroVolumeModuleWidget& object);

  void setupUi();
  vtkSlicerAstroVolumeLogic* logic() const;
  /// The Segment Editor node of the scene (singleton "SegmentEditor")
  vtkMRMLSegmentEditorNode* ensureSegmentEditorNode();

  qSlicerAstroPanelHeader* Header{nullptr};
  qSlicerAstroDatasetSelector* DatasetSelector{nullptr};
  QToolButton* AutoShowButton{nullptr};
  qSlicerAstroSegmentedButton* Tabs{nullptr};
  QStackedWidget* Pages{nullptr};
  qSlicerAstroDisplaySection* DisplaySection{nullptr};
  qSlicerAstroContoursSection* ContoursSection{nullptr};
  qSlicerAstroRenderingSection* RenderingSection{nullptr};
  qSlicerAstroMasksSection* MasksSection{nullptr};
  qSlicerAstroSourcesSection* SourcesSection{nullptr};
  qSlicerAstroInfoSection* InfoSection{nullptr};

  vtkWeakPointer<vtkMRMLAstroVolumeNode> astroVolumeNode;
  vtkWeakPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;
};

#endif
