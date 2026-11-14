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

// STD includes
#include <algorithm>
#include <functional>
#include <array>
#include <regex>
#include <type_traits>
#include <vector>

// Slicer includes
#include <vtkSlicerVolumesLogic.h>
#include <cmath>
#include <limits>
#include <vtkMath.h>
#include <vtkMRMLViewLogic.h>

// AstroVolume includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroConfigure.h>
#include <vtkSlicerAstroSMPTools.h>

// MRML nodes includes
#include <vtkMRMLAstroMeasurement.h>
#include <vtkMRMLMarkupsLineNode.h>
#include <vtkMRMLMarkupsROINode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroReprojectParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroSegmentationStorageNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLCameraNode.h>
#include <vtkMRMLColorTableNode.h>
#include <vtkMRMLDisplayNode.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLNode.h>
#include <vtkMRMLPlotChartNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSliceViewDisplayableManagerFactory.h>
#include <vtkMRMLThreeDViewDisplayableManagerFactory.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLViewNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLVolumePropertyNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

//VTK includes
#include <vtkAddonMathUtilities.h>
#include <vtkBoundingBox.h>
#include <vtkCacheManager.h>
#include <vtkCollection.h>
#include <vtkColorTransferFunction.h>
#include <vtkGeneralTransform.h>
#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSegmentationConverter.h>
#include <vtkPolyData.h>
#include <vtkOrientedImageData.h>
#include <vtkVolumeProperty.h>
#include <vtkPointData.h>
#include <vtkSegment.h>
#include <vtkSmartPointer.h>

// WCS includes
#include "wcslib.h"
#include <vtkAstroWCSErrors.h>
#include <sstream>
#include <vtkSMPThreadLocal.h>
#include <memory>


#include <iostream>
#include <chrono>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroVolumeLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic::vtkSlicerAstroVolumeLogic()
{
  this->PresetsScene = nullptr;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic::~vtkSlicerAstroVolumeLogic()
{
  if (this->PresetsScene)
    {
    this->PresetsScene->Delete();
    }
}

namespace
{
//----------------------------------------------------------------------------
template <typename T> T StringToNumber(const char* num)
{
  std::stringstream ss;
  ss << num;
  T result;
  return ss >> result ? result : 0;
}

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

//----------------------------------------------------------------------------
template <typename T> bool isNaN(T value)
{
  return value != value;
}

//----------------------------------------------------------------------------
bool DoubleIsNaN(double Value)
{
  return isNaN<double>(Value);
}

}// end namespace

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::UpdateFromMRMLScene()
{
  assert(this->GetMRMLScene() != 0);
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SetMRMLSceneInternal(vtkMRMLScene * newScene)
{
  // List of events the slice logics should listen
  vtkNew<vtkIntArray> events;

  // Events that use the default priority.
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::EndImportEvent);
  events->InsertNextValue(vtkMRMLScene::EndCloseEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());
  this->ObserveSelectionNode();
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::ObserveSelectionNode()
{
  vtkMRMLSelectionNode* selectionNode = this->GetMRMLScene() ? vtkMRMLSelectionNode::SafeDownCast(
    this->GetMRMLScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton")) : nullptr;
  if (selectionNode)
    {
    vtkNew<vtkIntArray> events;
    events->InsertNextValue(vtkCommand::ModifiedEvent);
    vtkObserveMRMLNodeEventsMacro(selectionNode, events.GetPointer());
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::UpdateAstroMeasurements(vtkMRMLMarkupsNode* markupsNode)
{
  if (!markupsNode)
    {
    return;
    }
  // astronomical quantities and the geometric measurements they replace
  std::vector<int> quantities;
  std::vector<std::string> geometric;
  if (markupsNode->IsA("vtkMRMLMarkupsLineNode"))
    {
    quantities = {vtkMRMLAstroMeasurement::Separation, vtkMRMLAstroMeasurement::VelocityDifference};
    geometric = {"length"};
    }
  else if (markupsNode->IsA("vtkMRMLMarkupsClosedCurveNode"))
    {
    quantities = {vtkMRMLAstroMeasurement::SkyLength, vtkMRMLAstroMeasurement::SkyArea};
    geometric = {"length", "area"};
    }
  else if (markupsNode->IsA("vtkMRMLMarkupsCurveNode"))
    {
    quantities = {vtkMRMLAstroMeasurement::SkyLength, vtkMRMLAstroMeasurement::VelocityDifference};
    geometric = {"length"};
    }
  else if (markupsNode->IsA("vtkMRMLMarkupsAngleNode"))
    {
    quantities = {vtkMRMLAstroMeasurement::SkyAngle};
    geometric = {"angle"};
    }
  else
    {
    return;
    }
  std::vector<bool> found(quantities.size(), false);
  for (int index = markupsNode->GetNumberOfMeasurements() - 1; index >= 0; index--)
    {
    vtkMRMLMeasurement* measurement = markupsNode->GetNthMeasurement(index);
    if (!measurement)
      {
      continue;
      }
    for (std::size_t quantity = 0; quantity < quantities.size(); quantity++)
      {
      if (measurement->GetName() != vtkMRMLAstroMeasurement::GetQuantityName(quantities[quantity]))
        {
        continue;
        }
      if (vtkMRMLAstroMeasurement::SafeDownCast(measurement))
        {
        found[quantity] = true;
        }
      else
        {
        // static copy of a loaded scene: replaced by the computed measurement
        markupsNode->RemoveNthMeasurement(index);
        }
      }
    }
  for (std::size_t quantity = 0; quantity < quantities.size(); quantity++)
    {
    if (!found[quantity])
      {
      vtkNew<vtkMRMLAstroMeasurement> measurement;
      measurement->SetQuantity(quantities[quantity]);
      measurement->SetInputMRMLNode(markupsNode);
      markupsNode->AddMeasurement(measurement);
      }
    }
  // the geometric measurements only for data without WCS
  const bool astronomical = vtkMRMLAstroMeasurement::GetMeasurementVolume(markupsNode) != nullptr;
  for (const std::string& name : geometric)
    {
    vtkMRMLMeasurement* measurement = markupsNode->GetMeasurement(name.c_str());
    if (measurement)
      {
      measurement->SetEnabled(!astronomical);
      }
    }
  markupsNode->UpdateAllMeasurements();
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::UpdateAllAstroMeasurements()
{
  if (!this->GetMRMLScene())
    {
    return;
    }
  std::vector<vtkMRMLNode*> markupsNodes;
  this->GetMRMLScene()->GetNodesByClass("vtkMRMLMarkupsNode", markupsNodes);
  for (vtkMRMLNode* markupsNode : markupsNodes)
    {
    vtkSlicerAstroVolumeLogic::UpdateAstroMeasurements(vtkMRMLMarkupsNode::SafeDownCast(markupsNode));
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SetAstroSliceOrientationPresets(vtkMRMLSliceNode* sliceNode)
{
  if (!sliceNode)
    {
    return;
    }
  // anatomical preset -> astronomical preset (same slice to RAS matrices)
  static const char* const names[3][2] = {{"Axial", "XZ"}, {"Sagittal", "ZY"}, {"Coronal", "XY"}};
  // orientation of the views: the views that are axial, sagittal and coronal
  // in Slicer (red, yellow and green) show the sky plane (XY), the
  // RA-velocity plane (XZ) and the velocity-Dec plane (ZY), as in the layouts
  // (see qSlicerAstroVolumeModule::setup).
  static const char* const views[3][2] = {{"Axial", "XY"}, {"Sagittal", "XZ"}, {"Coronal", "ZY"}};
  vtkNew<vtkMatrix3x3> matrices[3];
  vtkMRMLSliceNode::GetAxialSliceToRASMatrix(matrices[0]);
  vtkMRMLSliceNode::GetSagittalSliceToRASMatrix(matrices[1]);
  vtkMRMLSliceNode::GetCoronalSliceToRASMatrix(matrices[2]);

  std::string orientation = sliceNode->GetOrientation();
  std::string defaultOrientation =
    sliceNode->GetDefaultOrientation() ? sliceNode->GetDefaultOrientation() : "";

  const int wasModifying = sliceNode->StartModify();
  for (int i = 0; i < 3; ++i)
    {
    const std::string anatomical = names[i][0];
    const std::string astronomical = names[i][1];
    if (!sliceNode->HasSliceOrientationPreset(astronomical))
      {
      sliceNode->AddSliceOrientationPreset(astronomical, matrices[i]);
      }
    if (sliceNode->HasSliceOrientationPreset(anatomical))
      {
      sliceNode->RemoveSliceOrientationPreset(anatomical);
      }
    }
  for (int i = 0; i < 3; ++i)
    {
    if (orientation == views[i][0])
      {
      orientation = views[i][1];
      }
    if (defaultOrientation == views[i][0])
      {
      defaultOrientation = views[i][1];
      sliceNode->SetDefaultOrientation(views[i][1]);
      }
    }
  if (sliceNode->GetScene() &&
      orientation != vtkMRMLSliceNode::GetReformatOrientationName() &&
      sliceNode->GetOrientation() != orientation)
    {
    sliceNode->SetOrientation(orientation.c_str());
    }
  sliceNode->EndModify(wasModifying);
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::UpdateSliceOrientationPresets()
{
  if (!this->GetMRMLScene())
    {
    return;
    }
  // Default slice node: new slice nodes are created from it (note that the
  // Slicer core adds the anatomical presets to it, e.g. when the default
  // slice view settings are read)
  vtkSmartPointer<vtkMRMLNode> defaultNode = this->GetMRMLScene()->GetDefaultNodeByClass("vtkMRMLSliceNode");
  if (!defaultNode)
    {
    defaultNode.TakeReference(this->GetMRMLScene()->CreateNodeByClass("vtkMRMLSliceNode"));
    this->GetMRMLScene()->AddDefaultNode(defaultNode);
    }
  vtkSlicerAstroVolumeLogic::SetAstroSliceOrientationPresets(vtkMRMLSliceNode::SafeDownCast(defaultNode));

  // Slice nodes already in the scene
  vtkSmartPointer<vtkCollection> sliceNodes = vtkSmartPointer<vtkCollection>::Take
      (this->GetMRMLScene()->GetNodesByClass("vtkMRMLSliceNode"));
  for (int i = 0; i < sliceNodes->GetNumberOfItems(); i++)
    {
    vtkSlicerAstroVolumeLogic::SetAstroSliceOrientationPresets(
      vtkMRMLSliceNode::SafeDownCast(sliceNodes->GetItemAsObject(i)));
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::OnMRMLSceneEndClose()
{
  this->UpdateSliceOrientationPresets();
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::OnMRMLSceneEndImport()
{
  this->UpdateSliceOrientationPresets();
  // markups of the scene (their measurements were read as static copies)
  this->UpdateAllAstroMeasurements();

  std::vector<vtkMRMLNode*> segmentationNodes;
  this->GetMRMLScene()->GetNodesByClass("vtkMRMLSegmentationNode", segmentationNodes);
  for (vtkMRMLNode* node : segmentationNodes)
    {
    this->EnsureFITSMaskStorageNode(vtkMRMLSegmentationNode::SafeDownCast(node));
    }

  // contours and mask segmentations of the astronomical volumes (existing
  // ones are used, their references fixed)
  std::vector<vtkMRMLNode*> astroVolumes;
  this->GetMRMLScene()->GetNodesByClass("vtkMRMLAstroVolumeNode", astroVolumes);
  for (vtkMRMLNode* node : astroVolumes)
    {
    this->EnsureVolumeSegmentations(vtkMRMLAstroVolumeNode::SafeDownCast(node));
    }

  // modify CameraNodes already allocated
  vtkSmartPointer<vtkCollection> cameraNodes = vtkSmartPointer<vtkCollection>::Take
      (this->GetMRMLScene()->GetNodesByClass("vtkMRMLCameraNode"));

  for(int i = 0; i < cameraNodes->GetNumberOfItems(); i++)
    {
    vtkMRMLCameraNode* cameraNode =
        vtkMRMLCameraNode::SafeDownCast(cameraNodes->GetItemAsObject(i));
    if (cameraNode)
      {
      cameraNode->SetParallelScale(75.);
      }
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  if (!node || !this->GetMRMLScene())
    {
    return;
    }

  // astronomical slice orientations
  if (node->IsA("vtkMRMLSliceNode"))
    {
    vtkSlicerAstroVolumeLogic::SetAstroSliceOrientationPresets(vtkMRMLSliceNode::SafeDownCast(node));
    }

  // markups: astronomical measurements with the WCS
  if (vtkMRMLMarkupsNode* markupsNode = vtkMRMLMarkupsNode::SafeDownCast(node))
    {
    vtkSlicerAstroVolumeLogic::UpdateAstroMeasurements(markupsNode);
    }
  if (node->IsA("vtkMRMLSelectionNode"))
    {
    this->ObserveSelectionNode();
    }

  // astronomical volumes loaded from a file (storage node with a file name,
  // set after the node is added) get their contours and mask segmentations
  vtkMRMLAstroVolumeNode* astroVolume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  if (astroVolume && !vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(node))
    {
    // a cloned volume (derived data) has copied the references to the contours
    // and mask segmentations of its source: they are not its own
    if (!this->GetMRMLScene()->IsImporting())
      {
      const std::string geometryRole = vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole();
      for (const char* role : {vtkSlicerAstroVolumeLogic::GetContoursReferenceRole(),
                               vtkSlicerAstroVolumeLogic::GetMaskReferenceRole()})
        {
        vtkMRMLNode* segmentationNode = astroVolume->GetNodeReference(role);
        if (segmentationNode && segmentationNode->GetNodeReference(geometryRole.c_str()) &&
            segmentationNode->GetNodeReference(geometryRole.c_str()) != astroVolume)
          {
          astroVolume->RemoveNodeReferenceIDs(role);
          }
        }
      }
    vtkNew<vtkIntArray> volumeEvents;
    volumeEvents->InsertNextValue(vtkMRMLNode::ReferenceAddedEvent);
    volumeEvents->InsertNextValue(vtkMRMLNode::ReferenceModifiedEvent);
    vtkObserveMRMLNodeEventsMacro(astroVolume, volumeEvents.GetPointer());
    this->EnsureSegmentationsOfLoadedVolume(astroVolume);
    }

  // segmentations created before the astronomical data was loaded (e.g. the
  // mask of the AstroVolume module) are FITS masks too
  if (node->IsA("vtkMRMLAstroVolumeNode") && !this->GetMRMLScene()->IsImporting())
    {
    std::vector<vtkMRMLNode*> segmentationNodes;
    this->GetMRMLScene()->GetNodesByClass("vtkMRMLSegmentationNode", segmentationNodes);
    for (vtkMRMLNode* segmentationNode : segmentationNodes)
      {
      this->EnsureFITSMaskStorageNode(vtkMRMLSegmentationNode::SafeDownCast(segmentationNode));
      }
    }

  // transfer functions without points too close for the GPU lookup tables
  if (vtkMRMLVolumePropertyNode* volumePropertyNode = vtkMRMLVolumePropertyNode::SafeDownCast(node))
    {
    vtkNew<vtkIntArray> events;
    events->InsertNextValue(vtkCommand::ModifiedEvent);
    vtkObserveMRMLNodeEventsMacro(volumePropertyNode, events.GetPointer());
    this->EnsureVolumePropertyTransferFunctionGaps(volumePropertyNode);
    }

  // astronomical segmentations are saved as FITS masks by default
  if (vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node))
    {
    vtkNew<vtkIntArray> events;
    events->InsertNextValue(vtkMRMLNode::ReferenceAddedEvent);
    events->InsertNextValue(vtkMRMLNode::ReferenceModifiedEvent);
    events->InsertNextValue(vtkMRMLNode::ReferenceRemovedEvent);
    vtkObserveMRMLNodeEventsMacro(segmentationNode, events.GetPointer());
    if (!this->GetMRMLScene()->IsImporting())
      {
      this->EnsureFITSMaskStorageNode(segmentationNode);
      }
    }

  //change axes label names
  if (node->IsA("vtkMRMLViewNode"))
    {
    vtkMRMLViewNode *viewNode =
      vtkMRMLViewNode::SafeDownCast(node);

    viewNode->DisableModifiedEventOn();

    //setting Axes Labels
    viewNode->SetAxisLabel(0,"W");
    viewNode->SetAxisLabel(1,"E");
    viewNode->SetAxisLabel(2,"Z");
    viewNode->SetAxisLabel(3,"z");
    viewNode->SetAxisLabel(4,"S");
    viewNode->SetAxisLabel(5,"N");

    viewNode->DisableModifiedEventOff();

    //unregister RulerDisplayableManager
    vtkMRMLThreeDViewDisplayableManagerFactory::GetInstance()->
      UnRegisterDisplayableManager("vtkMRMLRulerDisplayableManager");
    }

  if (node->IsA("vtkMRMLSliceNode"))
    {
    vtkMRMLSliceNode *sliceNode =
      vtkMRMLSliceNode::SafeDownCast(node);

    sliceNode->DisableModifiedEventOn();

    // setting Axes Labels
    sliceNode->SetAxisLabel(0,"W");
    sliceNode->SetAxisLabel(1,"E");
    sliceNode->SetAxisLabel(2,"Z");
    sliceNode->SetAxisLabel(3,"z");
    sliceNode->SetAxisLabel(4,"S");
    sliceNode->SetAxisLabel(5,"N");

    sliceNode->SetAttribute("SlicerAstro.Beam", "off");

    sliceNode->DisableModifiedEventOff();

    //unregister RulerDisplayableManager
    vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->
      UnRegisterDisplayableManager("vtkMRMLRulerDisplayableManager");
    }

  if (node->IsA("vtkMRMLSegmentEditorNode"))
    {
    vtkSmartPointer<vtkCollection> col = vtkSmartPointer<vtkCollection>::Take(
      this->GetMRMLScene()->GetNodesByClass("vtkMRMLSegmentEditorNode"));
    vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
      this->GetMRMLScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
    if (!selectionNode)
      {
      return;
      }

    vtkMRMLSegmentEditorNode* segmentEditorNode =
      vtkMRMLSegmentEditorNode::SafeDownCast(col->GetItemAsObject(0));

    if (!segmentEditorNode)
      {
      selectionNode->RemoveNodeReferenceIDs("SegmentEditorNodeRef");
      return;
      }

    selectionNode->AddAndObserveNodeReferenceID("SegmentEditorNodeRef", segmentEditorNode->GetID());

    col->RemoveAllItems();
    }

  // update unit nodes
  if (node->IsA("vtkMRMLAstroVolumeNode"))
    {
    vtkSmartPointer<vtkCollection> listAstroVolumes = vtkSmartPointer<vtkCollection>::Take(
      this->GetMRMLScene()->GetNodesByClass("vtkMRMLAstroVolumeNode"));

    double min = std::numeric_limits<double>::max(), max = -std::numeric_limits<double>::max();
    for(int i = 0; i < listAstroVolumes->GetNumberOfItems(); i++)
      {
      vtkMRMLAstroVolumeNode* astroVolumeNode =
        vtkMRMLAstroVolumeNode::SafeDownCast(listAstroVolumes->GetItemAsObject(i));
      if (!astroVolumeNode)
        {
        continue;
        }

      double mint = StringToDouble(astroVolumeNode->GetAttribute("SlicerAstro.DATAMIN"));
      if (mint < min)
        {
        min = mint;
        }

      double maxt = StringToDouble(astroVolumeNode->GetAttribute("SlicerAstro.DATAMAX"));
      if (maxt > max)
        {
        max = maxt;
        }
      }

    vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
      this->GetMRMLScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
    if (selectionNode)
      {
      vtkMRMLUnitNode* unitNodeIntensity = selectionNode->GetUnitNode("intensity");
      unitNodeIntensity->SetMaximumValue(max);
      unitNodeIntensity->SetMinimumValue(min);
      unitNodeIntensity->SetDisplayCoefficient(1.);
      std::string temp = " ";
      if(max < 0.001)
        {
        temp += "\xB5";
        unitNodeIntensity->SetDisplayCoefficient(1000000.);
        }
      else
        {
        if(max < 1.)
          {
          temp += "m";
          unitNodeIntensity->SetDisplayCoefficient(1000.);
          }
        }

      temp += node->GetAttribute("SlicerAstro.BUNIT");
      unitNodeIntensity->SetPrecision(6);
      unitNodeIntensity->SetPrefix("");
      unitNodeIntensity->SetSuffix(temp.c_str());
      selectionNode->SetUnitNodeID("intensity", unitNodeIntensity->GetID());
      }
    }

  if (node->IsA("vtkMRMLAstroVolumeNode") ||
      node->IsA("vtkMRMLAstroLabelMapVolumeNode"))
    {
    // change axes label names
    vtkSmartPointer<vtkCollection> viewNodes = vtkSmartPointer<vtkCollection>::Take
      (this->GetMRMLScene()->GetNodesByClass("vtkMRMLViewNode"));

    for(int i = 0; i < viewNodes->GetNumberOfItems(); i++)
      {
      vtkMRMLViewNode* viewNode =
        vtkMRMLViewNode::SafeDownCast(viewNodes->GetItemAsObject(i));
      if (viewNode)
        {
        viewNode->DisableModifiedEventOn();

        viewNode->SetAxisLabel(0,"W");
        viewNode->SetAxisLabel(1,"E");
        viewNode->SetAxisLabel(2,"Z");
        viewNode->SetAxisLabel(3,"z");
        viewNode->SetAxisLabel(4,"S");
        viewNode->SetAxisLabel(5,"N");

        viewNode->DisableModifiedEventOff();
        }
      }

    vtkSmartPointer<vtkCollection> sliceNodes = vtkSmartPointer<vtkCollection>::Take
      (this->GetMRMLScene()->GetNodesByClass("vtkMRMLSliceNode"));

    for(int i = 0; i < sliceNodes->GetNumberOfItems(); i++)
      {
      vtkMRMLSliceNode* sliceNode =
        vtkMRMLSliceNode::SafeDownCast(sliceNodes->GetItemAsObject(i));
      if (sliceNode)
        {
        sliceNode->DisableModifiedEventOn();

        sliceNode->SetAxisLabel(0,"W");
        sliceNode->SetAxisLabel(1,"E");
        sliceNode->SetAxisLabel(2,"Z");
        sliceNode->SetAxisLabel(3,"z");
        sliceNode->SetAxisLabel(4,"S");
        sliceNode->SetAxisLabel(5,"N");

        sliceNode->SetAttribute("SlicerAstro.Beam", "off");

        sliceNode->DisableModifiedEventOff();
        }
      }

    // Astronomical orientations of the slice views (XY, XZ, ZY) for the first
    // astronomical volume of the scene only: the volumes created later by the
    // modules (moment maps, PV diagrams, models, ...) must not reset the
    // orientations chosen by the modules or the user.
    if (this->GetMRMLScene()->GetNumberOfNodesByClass("vtkMRMLAstroVolumeNode") == 1 &&
        !this->GetMRMLScene()->IsImporting())
      {
      const char* orientations[3][2] = {{"vtkMRMLSliceNodeRed", "XY"},
                                        {"vtkMRMLSliceNodeYellow", "XZ"},
                                        {"vtkMRMLSliceNodeGreen", "ZY"}};
      for (const char* const* orientation : orientations)
        {
        vtkMRMLSliceNode* defaultSliceNode = vtkMRMLSliceNode::SafeDownCast
          (this->GetMRMLScene()->GetNodeByID(orientation[0]));
        if (defaultSliceNode)
          {
          defaultSliceNode->SetOrientation(orientation[1]);
          }
        }
      }
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::OnMRMLSceneNodeRemoved(vtkMRMLNode *node)
{
  if (!node)
    {
    return;
    }

  if (node->IsA("vtkMRMLSegmentationNode"))
    {
    vtkUnObserveMRMLNodeMacro(node);
    }

  if (node->IsA("vtkMRMLSegmentEditorNode"))
    {
    vtkSmartPointer<vtkCollection> col = vtkSmartPointer<vtkCollection>::Take(
      this->GetMRMLScene()->GetNodesByClass("vtkMRMLSegmentEditorNode"));
    vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
      this->GetMRMLScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
    if (!selectionNode)
      {
      return;
      }

    vtkMRMLSegmentEditorNode* segmentEditorNode =
      vtkMRMLSegmentEditorNode::SafeDownCast(col->GetItemAsObject(0));

    if (!segmentEditorNode)
      {
      selectionNode->RemoveNodeReferenceIDs("SegmentEditorNodeRef");
      return;
      }

    selectionNode->AddAndObserveNodeReferenceID("SegmentEditorNodeRef", segmentEditorNode->GetID());

    col->RemoveAllItems();
    }

}

namespace
{
//----------------------------------------------------------------------------
ArchetypeVolumeNodeSet AstroVolumeNodeSetFactory(std::string& volumeName, vtkMRMLScene* scene, int options)
{
  ArchetypeVolumeNodeSet nodeSet(scene);

  // set up the Astro node's support nodes
  nodeSet.Scene->RegisterNodeClass(vtkNew<vtkMRMLAstroVolumeNode>().GetPointer());
  nodeSet.Scene->RegisterNodeClass(vtkNew<vtkMRMLAstroVolumeDisplayNode>().GetPointer());
  nodeSet.Scene->RegisterNodeClass(vtkNew<vtkMRMLAstroVolumeStorageNode>().GetPointer());

  vtkMRMLAstroVolumeDisplayNode* adisplayNode =
      vtkMRMLAstroVolumeDisplayNode::SafeDownCast(
        nodeSet.Scene->AddNewNodeByClass("vtkMRMLAstroVolumeDisplayNode"));
  adisplayNode->SetAutoWindowLevel(0);

  vtkMRMLAstroVolumeNode* astroNode =
      vtkMRMLAstroVolumeNode::SafeDownCast(
        nodeSet.Scene->AddNewNodeByClass("vtkMRMLAstroVolumeNode", volumeName));
  astroNode->SetAndObserveDisplayNodeID(adisplayNode->GetID());

  vtkMRMLAstroVolumeStorageNode* storageNode =
      vtkMRMLAstroVolumeStorageNode::SafeDownCast(
        nodeSet.Scene->AddNewNodeByClass("vtkMRMLAstroVolumeStorageNode"));
  storageNode->SetCenterImage(options & vtkSlicerVolumesLogic::CenterImage);
  astroNode->SetAndObserveStorageNodeID(storageNode->GetID());

  nodeSet.StorageNode = storageNode;
  nodeSet.DisplayNode = adisplayNode;
  nodeSet.Node = astroNode;

  return nodeSet;
}

//----------------------------------------------------------------------------
ArchetypeVolumeNodeSet AstroLabelMapVolumeNodeSetFactory(std::string& volumeName, vtkMRMLScene* scene, int options)
{
  ArchetypeVolumeNodeSet nodeSet(scene);

  // set up the AstroLabelMap node's support nodes
  nodeSet.Scene->RegisterNodeClass(vtkNew<vtkMRMLAstroLabelMapVolumeNode>().GetPointer());
  nodeSet.Scene->RegisterNodeClass(vtkNew<vtkMRMLAstroLabelMapVolumeDisplayNode>().GetPointer());
  nodeSet.Scene->RegisterNodeClass(vtkNew<vtkMRMLAstroVolumeStorageNode>().GetPointer());

  vtkMRMLAstroLabelMapVolumeDisplayNode* adisplayNode =
      vtkMRMLAstroLabelMapVolumeDisplayNode::SafeDownCast(
        nodeSet.Scene->AddNewNodeByClass("vtkMRMLAstroLabelMapVolumeDisplayNode"));

  vtkMRMLAstroLabelMapVolumeNode* astroNode =
      vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(
        nodeSet.Scene->AddNewNodeByClass("vtkMRMLAstroLabelMapVolumeNode", volumeName));
  astroNode->SetAndObserveDisplayNodeID(adisplayNode->GetID());

  vtkMRMLAstroVolumeStorageNode* storageNode =
      vtkMRMLAstroVolumeStorageNode::SafeDownCast(
        nodeSet.Scene->AddNewNodeByClass("vtkMRMLAstroVolumeStorageNode"));
  storageNode->SetCenterImage(options & vtkSlicerVolumesLogic::CenterImage);
  astroNode->SetAndObserveStorageNodeID(storageNode->GetID());

  nodeSet.StorageNode = storageNode;
  nodeSet.DisplayNode = adisplayNode;
  nodeSet.Node = astroNode;
  nodeSet.LabelMap = true;

  return nodeSet;
}

} // end of anonymous namespace

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::RegisterArchetypeVolumeNodeSetFactory(vtkSlicerVolumesLogic* volumesLogic)
{
  if (volumesLogic)
    {
    volumesLogic->PreRegisterArchetypeVolumeNodeSetFactory(AstroLabelMapVolumeNodeSetFactory);
    volumesLogic->PreRegisterArchetypeVolumeNodeSetFactory(AstroVolumeNodeSetFactory);
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::RegisterNodes()
{
  if(!this->GetMRMLScene())
    {
    return;
    }

  this->GetMRMLScene()->RegisterNodeClass(vtkNew<vtkMRMLAstroVolumeNode>().GetPointer());
  this->GetMRMLScene()->RegisterNodeClass(vtkNew<vtkMRMLAstroVolumeDisplayNode>().GetPointer());
  this->GetMRMLScene()->RegisterNodeClass(vtkNew<vtkMRMLAstroVolumeStorageNode>().GetPointer());
  this->GetMRMLScene()->RegisterNodeClass(vtkNew<vtkMRMLAstroSegmentationStorageNode>().GetPointer());
  this->GetMRMLScene()->RegisterNodeClass(vtkNew<vtkMRMLAstroLabelMapVolumeNode>().GetPointer());
  this->GetMRMLScene()->RegisterNodeClass(vtkNew<vtkMRMLAstroLabelMapVolumeDisplayNode>().GetPointer());

  if (!this->PresetsScene)
    {
    this->PresetsScene = vtkMRMLScene::New();
    }
  this->PresetsScene->RegisterNodeClass(vtkNew<vtkMRMLVolumePropertyNode>().GetPointer());
}

//----------------------------------------------------------------------------
vtkMRMLVolumePropertyNode* vtkSlicerAstroVolumeLogic::GetScenePresetNode(vtkMRMLNode* presetNode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLVolumePropertyNode* preset = vtkMRMLVolumePropertyNode::SafeDownCast(presetNode);
  if (!scene || !preset || !preset->GetName())
    {
    return nullptr;
    }
  if (preset->GetScene() == scene)
    {
    return preset;
    }

  std::string singletonTag = std::string("SlicerAstro.Preset.") + preset->GetName();
  vtkMRMLVolumePropertyNode* scenePreset = vtkMRMLVolumePropertyNode::SafeDownCast(
    scene->GetSingletonNode(singletonTag.c_str(), "vtkMRMLVolumePropertyNode"));
  if (scenePreset)
    {
    return scenePreset;
    }

  vtkNew<vtkMRMLVolumePropertyNode> newScenePreset;
  newScenePreset->Copy(preset);
  newScenePreset->SetName(preset->GetName());
  newScenePreset->SetSingletonTag(singletonTag.c_str());
  newScenePreset->SetHideFromEditors(true);
  return vtkMRMLVolumePropertyNode::SafeDownCast(scene->AddNode(newScenePreset));
}

//----------------------------------------------------------------------------
vtkMRMLScene *vtkSlicerAstroVolumeLogic::GetPresetsScene()
{
  if (!this->PresetsScene)
    {
    this->PresetsScene = vtkMRMLScene::New();
    }
  this->LoadPresets(this->PresetsScene);
  return this->PresetsScene;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::updateIntensityUnitsNode(vtkMRMLNode *astroVolumeNode)
{
  if (!astroVolumeNode)
    {
    return;
    }

  if (!(astroVolumeNode->IsA("vtkMRMLAstroVolumeNode")))
    {
    return;
    }

  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
    this->GetMRMLScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (!selectionNode)
    {
    return;
    }

  vtkMRMLUnitNode* intensityUnitNode = selectionNode->GetUnitNode("intensity");
  double max = StringToDouble(astroVolumeNode->GetAttribute("SlicerAstro.DATAMAX"));
  double min = StringToDouble(astroVolumeNode->GetAttribute("SlicerAstro.DATAMIN"));
  intensityUnitNode->SetMaximumValue(max);
  intensityUnitNode->SetMinimumValue(min);
  intensityUnitNode->SetDisplayCoefficient(1.);
  std::string temp = " ";
  if(max < 0.001)
    {
    temp += "\xB5";
    intensityUnitNode->SetDisplayCoefficient(1000000.);
    }
  else
    {
    if(max < 1.)
      {
      temp += "m";
      intensityUnitNode->SetDisplayCoefficient(1000.);
      }
    }

  temp += "Jy/beam";
  intensityUnitNode->SetPrecision(6);
  intensityUnitNode->SetPrefix("");
  intensityUnitNode->SetSuffix(temp.c_str());
  intensityUnitNode->SetAttribute("DisplayHint","");
  selectionNode->SetUnitNodeID("intensity", intensityUnitNode->GetID());
}

//---------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkSlicerAstroVolumeLogic::CloneAstroVolume(vtkMRMLScene *scene,
                                                                    vtkMRMLAstroVolumeNode *inputVolume,
                                                                    vtkMRMLAstroVolumeNode *outputVolume,
                                                                    const char *outputNameReference,
                                                                    const char *name, bool cloneImageData /* = true */)
{
  if (!scene || !inputVolume || !outputNameReference || !name)
    {
    return nullptr;
    }

  if (outputVolume)
    {
    std::string name;
    name = outputVolume->GetName();
    if (!name.compare(inputVolume->GetName()))
      {
      outputVolume = nullptr;
      }
    else if (name.find(outputNameReference) != std::string::npos)
      {
      vtkMRMLAstroVolumeStorageNode* astroStorage =
        vtkMRMLAstroVolumeStorageNode::SafeDownCast(outputVolume->GetStorageNode());
      scene->RemoveNode(astroStorage);
      scene->RemoveNode(outputVolume->GetDisplayNode());

      vtkMRMLVolumeRenderingDisplayNode *volumeRenderingDisplay =
        vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(outputVolume->GetDisplayNode());
      if (volumeRenderingDisplay)
        {
        scene->RemoveNode(volumeRenderingDisplay->GetROINode());
        scene->RemoveNode(volumeRenderingDisplay);
        }
      scene->RemoveNode(outputVolume);
      }
    }

  if (cloneImageData)
    {
    outputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
       (this->CloneVolume(scene, inputVolume, name, true));
    }
  else
    {
    // as vtkSlicerVolumesLogic::CloneVolume without the image data, but with a
    // shallow copy of the content: CloneVolume copies the voxels (deep copy of
    // the node) before removing them (temporary copy of the whole cube)
    vtkSmartPointer<vtkMRMLAstroVolumeNode> clone;
    clone.TakeReference(vtkMRMLAstroVolumeNode::SafeDownCast(scene->CreateNodeByClass(inputVolume->GetClassName())));
    if (!clone)
      {
      vtkErrorMacro("vtkSlicerAstroVolumeLogic::CloneAstroVolume : the volume could not be cloned.");
      return nullptr;
      }
    clone->SetScene(scene);
    clone->CopyContent(inputVolume, false);
    clone->SetAndObserveImageData(nullptr);
    clone->CopyReferences(inputVolume);
    clone->SetAndObserveStorageNodeID(nullptr);
    while (clone->GetNumberOfDisplayNodes() > 0)
      {
      clone->RemoveNthDisplayNodeID(0);
      }
    vtkMRMLDisplayNode* originalDisplayNode = inputVolume->GetVolumeDisplayNode();
    if (originalDisplayNode)
      {
      vtkSmartPointer<vtkMRMLDisplayNode> clonedDisplayNode;
      clonedDisplayNode.TakeReference(vtkMRMLDisplayNode::SafeDownCast(
        scene->CreateNodeByClass(originalDisplayNode->GetClassName())));
      if (clonedDisplayNode)
        {
        clonedDisplayNode->CopyWithScene(originalDisplayNode);
        scene->AddNode(clonedDisplayNode);
        clone->SetAndObserveDisplayNodeID(clonedDisplayNode->GetID());
        }
      }
    clone->SetName(scene->GetUniqueNameByString(name));
    scene->AddNode(clone);
    outputVolume = clone;
    }

  int ndnodes = outputVolume->GetNumberOfDisplayNodes();
  for (int ii = 0; ii < ndnodes; ii++)
    {
    vtkMRMLVolumeRenderingDisplayNode *dnode =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(
        outputVolume->GetNthDisplayNode(ii));
    if (dnode)
      {
      outputVolume->RemoveNthDisplayNodeID(ii);
      }
    }

  vtkNew<vtkMatrix4x4> transformationMatrix;
  inputVolume->GetRASToIJKMatrix(transformationMatrix.GetPointer());
  outputVolume->SetRASToIJKMatrix(transformationMatrix.GetPointer());
  outputVolume->SetAndObserveTransformNodeID(inputVolume->GetTransformNodeID());

  return outputVolume;
}

//---------------------------------------------------------------------------
vtkMRMLAstroLabelMapVolumeNode *vtkSlicerAstroVolumeLogic::CreateAndAddLabelVolume(vtkMRMLScene *scene,
                                                                                   vtkMRMLAstroVolumeNode *volumeNode,
                                                                                   const char *name)
{
  if (!scene || !volumeNode || !name)
    {
    return nullptr;
    }

  // Create a display node if the label node does not have one
  vtkNew<vtkMRMLAstroLabelMapVolumeDisplayNode> labelDisplayNode;
  // Set Generic Colors
  labelDisplayNode->SetAndObserveColorNodeID("vtkMRMLColorTableNodeFileGenericColors.txt");

  vtkMRMLAstroVolumeDisplayNode* astroVolumeDisplay = volumeNode->GetAstroVolumeDisplayNode();
  // Copy from astroVolumeDisplayNode
  labelDisplayNode->SetSpaceQuantities(astroVolumeDisplay->GetSpaceQuantities());
  labelDisplayNode->SetSpace(astroVolumeDisplay->GetSpace());
  labelDisplayNode->SetAttribute("SlicerAstro.NAXIS", volumeNode->GetAttribute("SlicerAstro.NAXIS"));

  struct wcsprm* labelWCS;
  struct wcsprm* volumeWCS;

  labelWCS = labelDisplayNode->GetWCSStruct();
  volumeWCS = astroVolumeDisplay->GetWCSStruct();

  if (!labelWCS || !volumeWCS)
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::CreateLabelVolumeFromVolume : "
                  "WCS not found!");
    return nullptr;
    }
  labelWCS->flag=-1;

  labelDisplayNode->SetWCSStatus(wcscopy(1, volumeWCS, labelWCS));
  if (labelDisplayNode->GetWCSStatus())
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::CreateLabelVolumeFromVolume : "
                  << vtkAstroWCSErrorMessage("wcscopy", labelDisplayNode->GetWCSStatus(), labelWCS));
    return nullptr;
    }

  labelDisplayNode->SetWCSStatus(wcsset(labelWCS));
  if (labelDisplayNode->GetWCSStatus())
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::CreateLabelVolumeFromVolume :"
                  << vtkAstroWCSErrorMessage("wcsset", labelDisplayNode->GetWCSStatus(), labelWCS));
    return nullptr;
    }

  scene->AddNode(labelDisplayNode.GetPointer());

  // Create a label node as copy of source volume
  vtkNew<vtkMRMLAstroLabelMapVolumeNode> labelNode;
  std::string uname = this->GetMRMLScene()->GetUniqueNameByString(name);
  labelNode->SetName(uname.c_str());
  labelNode->SetAndObserveDisplayNodeID(labelDisplayNode->GetID());

  // We need to copy from the volume node to get required attributes, but
  // the copy copies volumeNode's name as well.  So save the original name
  // and re-set the name after the copy.
  std::string origName(labelNode->GetName());
  labelNode->Copy(volumeNode);
  labelNode->SetAndObserveStorageNodeID(nullptr);
  labelNode->SetName(origName.c_str());
  labelNode->SetAndObserveDisplayNodeID(labelDisplayNode->GetID());

  std::vector<std::string> keys = volumeNode->GetAttributeNames();
  for (std::vector<std::string>::iterator kit = keys.begin(); kit != keys.end(); ++kit)
    {
    labelNode->SetAttribute((*kit).c_str(), volumeNode->GetAttribute((*kit).c_str()));
    }
  labelNode->SetAttribute("SlicerAstro.DATAMODEL", "MASK");
  labelNode->SetAttribute("SlicerAstro.BITPIX", "16");

  // Copy and set image data of the input volume to the label volume
  vtkNew<vtkImageData> imageData;
  imageData->DeepCopy(volumeNode->GetImageData());
  labelNode->SetAndObserveImageData(imageData.GetPointer());

  // Make an image data of the same size and shape as the input volume, but filled with zeros
  vtkSlicerVolumesLogic::ClearVolumeImageData(labelNode.GetPointer());
  labelNode->Modified();
  scene->AddNode(labelNode.GetPointer());

  vtkMRMLAstroVolumeStorageNode* storageNode =
      vtkMRMLAstroVolumeStorageNode::SafeDownCast(
        scene->AddNewNodeByClass("vtkMRMLAstroVolumeStorageNode"));
  storageNode->SetCenterImage(vtkSlicerVolumesLogic::CenterImage);
  labelNode->SetAndObserveStorageNodeID(storageNode->GetID());

  return labelNode.GetPointer();
}

//---------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::FitROIToInputVolume(vtkMRMLMarkupsROINode *roiNode, vtkMRMLAstroVolumeNode *inputVolume)
{
  if (!roiNode || !inputVolume)
    {
    return false;
    }

  vtkMRMLTransformNode* roiTransform = roiNode->GetParentTransformNode();
  if (roiTransform && !roiTransform->IsTransformToWorldLinear())
    {
    roiTransform = nullptr;
    roiNode->SetAndObserveTransformNodeID(nullptr);
    inputVolume->DeleteROIAlignmentTransformNode();
    }
  vtkNew<vtkMatrix4x4> worldToROI;
  vtkMRMLTransformNode::GetMatrixTransformBetweenNodes(nullptr, roiTransform, worldToROI.GetPointer());

  double volumeBounds_ROI[6] = { 0 }; // volume bounds in ROI's coordinate system
  inputVolume->GetSliceBounds(volumeBounds_ROI, worldToROI.GetPointer());

  double roiCenter[3] = { 0 };
  double roiRadius[3] = { 0 };
  for (int i = 0; i < 3; i++)
    {
    roiCenter[i] = (volumeBounds_ROI[i * 2 + 1] + volumeBounds_ROI[i * 2]) / 2;
    roiRadius[i] = (volumeBounds_ROI[i * 2 + 1] - volumeBounds_ROI[i * 2]) / 2;
    }
  roiNode->SetXYZ(roiCenter);
  roiNode->SetRadiusXYZ(roiRadius);

  return true;
}

//---------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SnapROIToVoxelGrid(vtkMRMLMarkupsROINode *roiNode, vtkMRMLAstroVolumeNode *inputVolume)
{
  if (!roiNode || !inputVolume)
    {
    return;
    }

  // Is it already aligned?
  if (this->IsROIAlignedWithInputVolume(roiNode, inputVolume))
    {
    // already aligned, nothing to do
    return;
    }

  double originalBounds_World[6] = { 0, -1, 0, -1, 0, -1 };
  roiNode->GetRASBounds(originalBounds_World);

  // If we don't transform it, is it aligned?
  if (roiNode->GetParentTransformNode() != nullptr)
    {
    roiNode->SetAndObserveTransformNodeID(nullptr);
    if (IsROIAlignedWithInputVolume(roiNode, inputVolume))
      {
      // ROI is aligned if it's not transformed, no need for ROI alignment transform
      inputVolume->DeleteROIAlignmentTransformNode();
      // Update ROI to approximately match original region
      roiNode->SetXYZ((originalBounds_World[1] + originalBounds_World[0]) / 2.0,
        (originalBounds_World[3] + originalBounds_World[2]) / 2.0,
        (originalBounds_World[5] + originalBounds_World[4]) / 2.0);
      roiNode->SetRadiusXYZ((originalBounds_World[1] - originalBounds_World[0]) / 2.0,
        (originalBounds_World[3] - originalBounds_World[2]) / 2.0,
        (originalBounds_World[5] - originalBounds_World[4]) / 2.0);
      return;
      }
    }

  // It's a non-trivial rotation, use the ROI alignment transform node to align
  vtkNew<vtkMatrix4x4> volumeRasToWorld;
  vtkMRMLTransformNode::GetMatrixTransformBetweenNodes(inputVolume->GetParentTransformNode(),
    nullptr, volumeRasToWorld.GetPointer());
  vtkNew<vtkMatrix4x4> volumeIJKToRAS;
  inputVolume->GetIJKToRASMatrix(volumeIJKToRAS.GetPointer());
  vtkNew<vtkMatrix4x4> volumeIJKToWorld;
  vtkMatrix4x4::Multiply4x4(volumeRasToWorld.GetPointer(), volumeIJKToRAS.GetPointer(), volumeIJKToWorld.GetPointer());
  double scale[3] = { 1.0 };
  vtkAddonMathUtilities::NormalizeOrientationMatrixColumns(volumeIJKToWorld.GetPointer(), scale);

  // Apply transform to ROI alignment transform
  if (!inputVolume->GetROIAlignmentTransformNode())
    {
    vtkNew<vtkMRMLTransformNode> roiTransformNode;
    roiTransformNode->SetName("Crop volume ROI alignment");
    inputVolume->GetScene()->AddNode(roiTransformNode.GetPointer());
    inputVolume->SetROIAlignmentTransformNodeID(roiTransformNode->GetID());
    }
  inputVolume->GetROIAlignmentTransformNode()->SetAndObserveTransformNodeID(nullptr);
  inputVolume->GetROIAlignmentTransformNode()->SetMatrixTransformToParent(volumeIJKToWorld.GetPointer());
  roiNode->SetAndObserveTransformNodeID(inputVolume->GetROIAlignmentTransformNode()->GetID());

  vtkNew<vtkMatrix4x4> worldToROITransformMatrix;
  inputVolume->GetROIAlignmentTransformNode()->GetMatrixTransformFromWorld(worldToROITransformMatrix.GetPointer());

  // Update ROI to approximately match original region
  const int numberOfCornerPoints = 8;
  double cornerPoints_World[numberOfCornerPoints][4] =
    {
    { originalBounds_World[0], originalBounds_World[2], originalBounds_World[4], 1 },
    { originalBounds_World[0], originalBounds_World[2], originalBounds_World[5], 1 },
    { originalBounds_World[0], originalBounds_World[3], originalBounds_World[4], 1 },
    { originalBounds_World[0], originalBounds_World[3], originalBounds_World[5], 1 },
    { originalBounds_World[1], originalBounds_World[2], originalBounds_World[4], 1 },
    { originalBounds_World[1], originalBounds_World[2], originalBounds_World[5], 1 },
    { originalBounds_World[1], originalBounds_World[3], originalBounds_World[4], 1 },
    { originalBounds_World[1], originalBounds_World[3], originalBounds_World[5], 1 }
    };
  vtkBoundingBox boundingBox_ROI;
  for (int i = 0; i < numberOfCornerPoints; i++)
    {
    double* cornerPoint_ROI = worldToROITransformMatrix->MultiplyDoublePoint(cornerPoints_World[i]);
    boundingBox_ROI.AddPoint(cornerPoint_ROI);
    }
  double center_ROI[3] = { 0 };
  boundingBox_ROI.GetCenter(center_ROI);
  roiNode->SetXYZ(center_ROI);
  double diameters_ROI[3] = { 0 };
  boundingBox_ROI.GetLengths(diameters_ROI);
  roiNode->SetRadiusXYZ(diameters_ROI[0] / 2, diameters_ROI[1] / 2, diameters_ROI[2] / 2);
}

//---------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::IsROIAlignedWithInputVolume(vtkMRMLMarkupsROINode *roiNode, vtkMRMLAstroVolumeNode *inputVolume)
{
  if (!roiNode || !inputVolume)
    {
    return false;
    }

  if (inputVolume->GetParentTransformNode()
    && !inputVolume->GetParentTransformNode()->IsTransformToWorldLinear())
    {
    // no misalignment, as if input volume is under a non-linear transform then we cannot align a ROI
    return true;
    }

  if (roiNode->GetParentTransformNode()
    && !roiNode->GetParentTransformNode()->IsTransformToWorldLinear())
    {
    // misaligned, as ROI node is under non-linear transform
    return false;
    }

  vtkNew<vtkMatrix4x4> volumeRasToROI;
  vtkMRMLTransformNode::GetMatrixTransformBetweenNodes(inputVolume->GetParentTransformNode(),
    roiNode->GetParentTransformNode(), volumeRasToROI.GetPointer());
  vtkNew<vtkMatrix4x4> volumeIJKToRAS;
  inputVolume->GetIJKToRASMatrix(volumeIJKToRAS.GetPointer());
  vtkNew<vtkMatrix4x4> volumeIJKToROI;
  vtkMatrix4x4::Multiply4x4(volumeRasToROI.GetPointer(), volumeIJKToRAS.GetPointer(), volumeIJKToROI.GetPointer());

  double scale[3] = { 1.0 };
  vtkAddonMathUtilities::NormalizeOrientationMatrixColumns(volumeIJKToROI.GetPointer(), scale);

  double tolerance = 0.001;
  for (int row = 0; row < 3; row++)
    {
    for (int column = 0; column < 3; column++)
      {
        double elemAbs = fabs(volumeIJKToROI->GetElement(row, column));
      if (elemAbs > tolerance && elemAbs < 1 - tolerance)
        {
        // axes are neither orthogonal nor parallel
        return false;
        }
      }
    }
  return true;
}

//---------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::CalculateROICropVolumeBounds(vtkMRMLMarkupsROINode *roiNode,
                                                             vtkMRMLAstroVolumeNode *inputVolume,
                                                             double outputExtent[6])
{
  outputExtent[0] = outputExtent[2] = outputExtent[4] = 0;
  outputExtent[1] = outputExtent[3] = outputExtent[5] = -1;
  if (!roiNode || !inputVolume || !inputVolume->GetImageData())
    {
    return false;
    }

  int originalImageExtents[6] = {0};
  inputVolume->GetImageData()->GetExtent(originalImageExtents);

  vtkMRMLTransformNode* roiTransform = roiNode->GetParentTransformNode();
  if (roiTransform && !roiTransform->IsTransformToWorldLinear())
    {
    vtkGenericWarningMacro("vtkSlicerAstroVolumeLogic::CalculateROICropVolumeBounds :"
                           "  ROI is transformed using a non-linear transform. The transformation will be ignored");
    roiTransform = nullptr;
    }

  if (inputVolume->GetParentTransformNode() && !inputVolume->GetParentTransformNode()->IsTransformToWorldLinear())
    {
    vtkGenericWarningMacro("vtkSlicerAstroVolumeLogic::CalculateROICropVolumeBounds :"
                           " voxel-based cropping of non-linearly transformed input volume is not supported");
    return false;
    }

  // ROI corners in the ROI object coordinate system (origin at the ROI center,
  // axes along the ROI axes); the object-to-world matrix includes the ROI
  // orientation and its parent transform.
  vtkNew<vtkMatrix4x4> worldToVolumeTransformMatrix;
  vtkMRMLTransformNode::GetMatrixTransformBetweenNodes(nullptr, inputVolume->GetParentTransformNode(),
    worldToVolumeTransformMatrix.GetPointer());

  vtkNew<vtkMatrix4x4> rasToIJK;
  inputVolume->GetRASToIJKMatrix(rasToIJK.GetPointer());

  vtkNew<vtkMatrix4x4> roiObjectToWorld;
  roiObjectToWorld->DeepCopy(roiNode->GetObjectToWorldMatrix());
  if (!roiTransform && roiNode->GetParentTransformNode())
    {
    // non-linear parent transform: ignored
    roiObjectToWorld->DeepCopy(roiNode->GetObjectToNodeMatrix());
    }

  vtkNew<vtkMatrix4x4> roiToVolumeIJKTransformMatrix;
  vtkMatrix4x4::Multiply4x4(worldToVolumeTransformMatrix, roiObjectToWorld, roiToVolumeIJKTransformMatrix);
  vtkMatrix4x4::Multiply4x4(rasToIJK, roiToVolumeIJKTransformMatrix, roiToVolumeIJKTransformMatrix);

  double roiRadius[3] = {0};
  roiNode->GetRadiusXYZ(roiRadius);

  const int numberOfCorners = 8;
  double volumeCorners_ROI[numberOfCorners][4];
  for (int corner = 0; corner < numberOfCorners; ++corner)
    {
    volumeCorners_ROI[corner][0] = (corner & 1) ? roiRadius[0] : -roiRadius[0];
    volumeCorners_ROI[corner][1] = (corner & 2) ? roiRadius[1] : -roiRadius[1];
    volumeCorners_ROI[corner][2] = (corner & 4) ? roiRadius[2] : -roiRadius[2];
    volumeCorners_ROI[corner][3] = 1.;
    }

  // Get ROI extent in IJK coordinate system
  double outputExtentDouble[6] = {0};
  for (int cornerPointIndex = 0; cornerPointIndex < numberOfCorners; cornerPointIndex++)
    {
    double volumeCorner_IJK[4] = {0, 0, 0, 1};
    roiToVolumeIJKTransformMatrix->MultiplyPoint(volumeCorners_ROI[cornerPointIndex], volumeCorner_IJK);
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
      {
      if (cornerPointIndex == 0 || volumeCorner_IJK[axisIndex] < outputExtentDouble[axisIndex * 2])
        {
        outputExtentDouble[axisIndex * 2] = volumeCorner_IJK[axisIndex];
        }
      if (cornerPointIndex == 0 || volumeCorner_IJK[axisIndex] > outputExtentDouble[axisIndex * 2 + 1])
        {
        outputExtentDouble[axisIndex * 2 + 1] = volumeCorner_IJK[axisIndex];
        }
      }
    }

  // Limit output extent to input extent
  int* inputExtent = inputVolume->GetImageData()->GetExtent();
  double tolerance = 0.001;
  for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
    {
    outputExtent[axisIndex * 2] = std::max(inputExtent[axisIndex * 2], int(ceil(outputExtentDouble[axisIndex * 2]+0.5-tolerance)));
    outputExtent[axisIndex * 2 + 1] = std::min(inputExtent[axisIndex * 2 + 1], int(floor(outputExtentDouble[axisIndex * 2 + 1]-0.5+tolerance)));
    }

  return true;
}

//---------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::CalculateSegmentCropVolumeBounds(vtkMRMLSegmentationNode *segmentationNode,
                                                                 vtkSegment *segment,
                                                                 vtkMRMLAstroVolumeNode *inputVolume,
                                                                 double outputExtent[])
{
  outputExtent[0] = outputExtent[2] = outputExtent[4] = 0;
  outputExtent[1] = outputExtent[3] = outputExtent[5] = -1;
  if (!segmentationNode || !segment || !inputVolume || !inputVolume->GetImageData())
    {
    return false;
    }

  int originalImageExtents[6] = {0};
  inputVolume->GetImageData()->GetExtent(originalImageExtents);

  vtkMRMLTransformNode* segmentTransform = segmentationNode->GetParentTransformNode();
  if (segmentTransform && !segmentTransform->IsTransformToWorldLinear())
    {
    vtkGenericWarningMacro("vtkSlicerAstroVolumeLogic::CalculateSegmentCropVolumeBounds :"
                           "  segment is transformed using a non-linear transform. The transformation will be ignored");
    segmentTransform = nullptr;
    }

  if (inputVolume->GetParentTransformNode() && !inputVolume->GetParentTransformNode()->IsTransformToWorldLinear())
    {
    vtkGenericWarningMacro("vtkSlicerAstroVolumeLogic::CalculateSegmentCropVolumeBounds :"
                           " voxel-based cropping of non-linearly transformed input volume is not supported");
    return false;
    }

  vtkNew<vtkMatrix4x4> segmentToVolumeTransformMatrix;
  vtkMRMLTransformNode::GetMatrixTransformBetweenNodes(segmentTransform, inputVolume->GetParentTransformNode(),
    segmentToVolumeTransformMatrix.GetPointer());

  vtkNew<vtkMatrix4x4> rasToIJK;
  inputVolume->GetRASToIJKMatrix(rasToIJK.GetPointer());

  vtkNew<vtkMatrix4x4> segmentToVolumeIJKTransformMatrix;
  vtkMatrix4x4::Multiply4x4(rasToIJK.GetPointer(), segmentToVolumeTransformMatrix.GetPointer(),
    segmentToVolumeIJKTransformMatrix.GetPointer());

  double SegmentExtent[6] = {0};
  segment->GetBounds(SegmentExtent);

  const int numberOfCorners = 8;
  double volumeCorners_Segment[numberOfCorners][4] =
    {
    { SegmentExtent[0], SegmentExtent[2], SegmentExtent[4], 1. },
    { SegmentExtent[1], SegmentExtent[2], SegmentExtent[4], 1. },
    { SegmentExtent[0], SegmentExtent[3], SegmentExtent[4], 1. },
    { SegmentExtent[1], SegmentExtent[3], SegmentExtent[4], 1. },
    { SegmentExtent[0], SegmentExtent[2], SegmentExtent[5], 1. },
    { SegmentExtent[1], SegmentExtent[2], SegmentExtent[5], 1. },
    { SegmentExtent[0], SegmentExtent[3], SegmentExtent[5], 1. },
    { SegmentExtent[1], SegmentExtent[3], SegmentExtent[5], 1. },
    };

  // Get ROI extent in IJK coordinate system
  double outputExtentDouble[6] = {0};
  for (int cornerPointIndex = 0; cornerPointIndex < numberOfCorners; cornerPointIndex++)
    {
    double volumeCorner_IJK[4] = {0, 0, 0, 1};
    segmentToVolumeIJKTransformMatrix->MultiplyPoint(volumeCorners_Segment[cornerPointIndex], volumeCorner_IJK);
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
      {
      if (cornerPointIndex == 0 || volumeCorner_IJK[axisIndex] < outputExtentDouble[axisIndex * 2])
        {
        outputExtentDouble[axisIndex * 2] = volumeCorner_IJK[axisIndex];
        }
      if (cornerPointIndex == 0 || volumeCorner_IJK[axisIndex] > outputExtentDouble[axisIndex * 2 + 1])
        {
        outputExtentDouble[axisIndex * 2 + 1] = volumeCorner_IJK[axisIndex];
        }
      }
    }

  // Limit output extent to input extent
  int* inputExtent = inputVolume->GetImageData()->GetExtent();
  double tolerance = 0.001;
  for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
    {
    outputExtent[axisIndex * 2] = std::max(inputExtent[axisIndex * 2], int(ceil(outputExtentDouble[axisIndex * 2]+0.5-tolerance)));
    outputExtent[axisIndex * 2 + 1] = std::min(inputExtent[axisIndex * 2 + 1], int(floor(outputExtentDouble[axisIndex * 2 + 1]-0.5+tolerance)));
    }

  return true;
}

//---------------------------------------------------------------------------
double vtkSlicerAstroVolumeLogic::CalculateDisplayThresholdInROI(vtkMRMLMarkupsROINode *roiNode,
                                                                 vtkMRMLAstroVolumeNode *inputVolume)
{
  if (!roiNode)
    {
    return 0.;
    }

  if (!inputVolume || !inputVolume->GetImageData())
   {
   return 0.;
   }

  // Calculate the noise as the std in a roi.
  // The DisplayThreshold = noise
  // 3D color function starts from 3 times the value of DisplayThreshold.

  int *dims = inputVolume->GetImageData()->GetDimensions();
  int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::CalculateSTDinROI : "
                  "imageData with more than one components.");
    return 0.;
    }
  const int DataType = inputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  float *inFPixel = nullptr;
  double *inDPixel = nullptr;
  switch (DataType)
    {
    case VTK_FLOAT:
      inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("vtkSlicerAstroVolumeLogic::CalculateRMSinROI : "
                    "attempt to allocate scalars of type not allowed");
      return 0.;
    }

  double noise = 0., mean = 0., roiBounds[6];

  this->CalculateROICropVolumeBounds(roiNode, inputVolume, roiBounds);

  const int extent[6] = {static_cast<int>(roiBounds[0]), static_cast<int>(roiBounds[1]),
                         static_cast<int>(roiBounds[2]), static_cast<int>(roiBounds[3]),
                         static_cast<int>(roiBounds[4]), static_cast<int>(roiBounds[5])};
  if (extent[1] < extent[0] || extent[3] < extent[2] || extent[5] < extent[4])
    {
    vtkWarningMacro("CalculateDisplayThresholdInROI: the ROI does not intersect the volume "
                    << (inputVolume->GetName() ? inputVolume->GetName() : "") << ".");
    return 0.;
    }

  // mean and standard deviation of the finite (not blank) voxels
  vtkIdType count = 0;
  switch (DataType)
    {
    case VTK_FLOAT:
      count = vtkSlicerAstroSMPTools::ExtentFiniteCount(inFPixel, dims, extent);
      if (count > 0)
        {
        mean = vtkSlicerAstroSMPTools::ExtentSum(inFPixel, dims, extent) / count;
        noise = sqrt(vtkSlicerAstroSMPTools::ExtentSum(inFPixel, dims, extent, mean) / count);
        }
      break;
    case VTK_DOUBLE:
      count = vtkSlicerAstroSMPTools::ExtentFiniteCount(inDPixel, dims, extent);
      if (count > 0)
        {
        mean = vtkSlicerAstroSMPTools::ExtentSum(inDPixel, dims, extent) / count;
        noise = sqrt(vtkSlicerAstroSMPTools::ExtentSum(inDPixel, dims, extent, mean) / count);
        }
      break;
    }
  if (count == 0)
    {
    vtkWarningMacro("CalculateDisplayThresholdInROI: the ROI contains only blank voxels.");
    return 0.;
    }

  inputVolume->SetDisplayThreshold(noise);

  return noise;
}

//---------------------------------------------------------------------------
namespace
{
// Per-thread histograms of the finite voxels (no locking), merged by the caller
template <typename T>
void FillHistograms(const T* data, vtkIdType numElements, double dataMin, double binSpacing, int numberOfBins,
                    vtkSMPThreadLocal<std::vector<vtkIdType>>& localHistograms)
{
  vtkSMPTools::For(0, numElements, [&](vtkIdType first, vtkIdType last)
    {
    std::vector<vtkIdType>& histogram = localHistograms.Local();
    for (vtkIdType i = first; i < last; ++i)
      {
      const double value = data[i];
      if (vtkSlicerAstroSMPTools::IsNaN(value))
        {
        continue;
        }
      // clamp: the maximum value falls in the last bin
      const int histoIndex = std::clamp(static_cast<int>((value - dataMin) / binSpacing), 0, numberOfBins - 1);
      ++histogram[histoIndex];
      }
    });
}
}

//---------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::CalculateHistogram(vtkMRMLAstroVolumeNode *inputVolume,
                                                   vtkIntArray *histoArray,
                                                   double binSpacing,
                                                   int numberOfBins)
{
  if (!inputVolume || !inputVolume->GetImageData())
   {
   return;
   }

  int *dims = inputVolume->GetImageData()->GetDimensions();
  int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::CalculateHistogram : "
                  "imageData with more than one components.");
    return;
    }
  const vtkIdType numElements = static_cast<vtkIdType>(dims[0]) * dims[1] * dims[2];
  const int DataType = inputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  if (DataType != VTK_FLOAT && DataType != VTK_DOUBLE)
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::CalculateHistogram : "
                  "attempt to allocate scalars of type not allowed");
    return;
    }
  if (numberOfBins <= 0 || binSpacing <= 0.)
    {
    return;
    }
  void* scalars = inputVolume->GetImageData()->GetScalarPointer(0,0,0);
  const double DATAMIN = StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMIN"));

  // Per-thread histograms (no locking), merged at the end.
  vtkSMPThreadLocal<std::vector<vtkIdType>> localHistograms(std::vector<vtkIdType>(numberOfBins, 0));
  if (DataType == VTK_FLOAT)
    {
    FillHistograms(static_cast<float*>(scalars), numElements, DATAMIN, binSpacing, numberOfBins, localHistograms);
    }
  else
    {
    FillHistograms(static_cast<double*>(scalars), numElements, DATAMIN, binSpacing, numberOfBins, localHistograms);
    }

  std::vector<vtkIdType> histogram(numberOfBins, 0);
  for (const std::vector<vtkIdType>& threadHistogram : localHistograms)
    {
    for (int bin = 0; bin < numberOfBins; ++bin)
      {
      histogram[bin] += threadHistogram[bin];
      }
    }
  histoArray->SetNumberOfValues(numberOfBins);
  for (int bin = 0; bin < numberOfBins; ++bin)
    {
    histoArray->SetValue(bin, static_cast<int>(histogram[bin]));
    }
}

//---------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SetReprojectFunction(const ReprojectFunctionType& function)
{
  this->ReprojectFunction = function;
}

//---------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::Reproject(vtkMRMLAstroReprojectParametersNode *pnode)
{
  this->LastReprojectError.clear();
  if (!pnode)
    {
    this->LastReprojectError = "no parameters";
    return false;
    }
  if (!this->ReprojectFunction)
    {
    this->LastReprojectError = "the AstroReproject module is not loaded";
    return false;
    }
  return this->ReprojectFunction(pnode, this->LastReprojectError);
}

//---------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::synchronizePresetsToVolumeNode(vtkMRMLNode *node)
{
  if (!node || !node->IsA("vtkMRMLAstroVolumeNode"))
    {
    return false;
    }

  double max = StringToDouble(node->GetAttribute("SlicerAstro.DATAMAX")) * 2.;
  double min = StringToDouble(node->GetAttribute("SlicerAstro.DATAMIN")) * 2.;
  double noise = StringToDouble(node->GetAttribute("SlicerAstro.DisplayThreshold"));

  if (noise < 0.000000001 || DoubleIsNaN(noise))
    {
    noise = (max - min) / 1000.;
    }

  double halfNoise = noise * 0.5;
  double noise3 = noise * 3.;
  if ((noise3 - halfNoise) < min)
    {
    noise3 = min + halfNoise;
    }
  if ((noise3 + halfNoise) > max)
    {
    noise3 = max - halfNoise;
    }

  double noise6 = noise * 6.;
  if ((noise6 - halfNoise) < min)
    {
    noise6 = min + halfNoise;
    }
  if ((noise6 + halfNoise) > max)
    {
    noise6 = max - halfNoise;
    }

  double noise7 = noise * 7.;
  if ((noise7 - halfNoise) < min)
    {
    noise7 = min + halfNoise;
    }
  if ((noise7 + halfNoise) > max)
    {
    noise7 = max - halfNoise;
    }

  double noise9 = noise * 9.;
  if ((noise9 - halfNoise) < min)
    {
    noise9 = min + halfNoise;
    }
  if ((noise9 + halfNoise) > max)
    {
    noise9 = max - halfNoise;
    }

  double noise12 = noise * 12.;
  if ((noise12 - halfNoise) < min)
    {
    noise12 = min + halfNoise;
    }
  if ((noise12 + halfNoise) > max)
    {
    noise12 = max - halfNoise;
    }

  double noise15 = noise * 15.;
  if ((noise15 - halfNoise) < min)
    {
    noise15 = min + halfNoise;
    }
  if ((noise15 + halfNoise) > max)
    {
    noise15 = max - halfNoise;
    }

  double noise18 = noise * 18.;
  if ((noise18 - halfNoise) < min)
    {
    noise18 = min + halfNoise;
    }
  if ((noise18 + halfNoise) > max)
    {
    noise18 = max - halfNoise;
    }

  double noise21 = noise * 21.;
  if ((noise21 - halfNoise) < min)
    {
    noise21 = min + halfNoise;
    }
  if ((noise21 + halfNoise) > max)
    {
    noise21 = max - halfNoise;
    }

  double noise24 = noise * 24.;
  if ((noise24 - halfNoise) < min)
    {
    noise24 = min + halfNoise;
    }
  if ((noise24 + halfNoise) > max)
    {
    noise24 = max - halfNoise;
    }

  vtkSmartPointer<vtkCollection> presets = vtkSmartPointer<vtkCollection>::Take(
      this->PresetsScene->GetNodesByClass("vtkMRMLVolumePropertyNode"));

  for(int i = 0; i < presets->GetNumberOfItems(); i++)
    {
    vtkMRMLVolumePropertyNode* volumePropertyNode =
        vtkMRMLVolumePropertyNode::SafeDownCast(presets->GetItemAsObject(i));
    if (!volumePropertyNode)
      {
      continue;
      }
    if(!strcmp(volumePropertyNode->GetName(),"LowConstantOpacity"))
      {
      vtkPiecewiseFunction *compositeOpacity =
          volumePropertyNode->GetScalarOpacity();
      compositeOpacity->RemoveAllPoints();
      compositeOpacity->AddPoint(min, 0.);
      compositeOpacity->AddPoint(noise3 - (noise3 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise3, 0.2);
      compositeOpacity->AddPoint(max, 0.2);

      vtkColorTransferFunction *color =
          volumePropertyNode->GetColor();
      color->RemoveAllPoints();
      color->AddRGBPoint(min, 0., 0., 0.);
      color->AddRGBPoint(noise3, 0., 0.3, 0.);
      color->AddRGBPoint(max, 0., 1., 0.);

      vtkPiecewiseFunction *gradientOpacity =
          volumePropertyNode->GetGradientOpacity();
      gradientOpacity->RemoveAllPoints();
      gradientOpacity->AddPoint(min, 1.);
      gradientOpacity->AddPoint(max, 1.);
      }
    if(!strcmp(volumePropertyNode->GetName(),"MediumConstantOpacity"))
      {
      vtkPiecewiseFunction *compositeOpacity =
          volumePropertyNode->GetScalarOpacity();
      compositeOpacity->RemoveAllPoints();
      compositeOpacity->AddPoint(min, 0.);
      compositeOpacity->AddPoint(noise3 - (noise3 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise3, 0.4);
      compositeOpacity->AddPoint(max, 0.4);

      vtkColorTransferFunction *color =
          volumePropertyNode->GetColor();
      color->RemoveAllPoints();
      color->AddRGBPoint(min, 0., 0., 0.);
      color->AddRGBPoint(noise3, 0., 0.3, 0.);
      color->AddRGBPoint(max, 0., 1., 0.);

      vtkPiecewiseFunction *gradientOpacity =
          volumePropertyNode->GetGradientOpacity();
      gradientOpacity->RemoveAllPoints();
      gradientOpacity->AddPoint(min, 1.);
      gradientOpacity->AddPoint(max, 1.);
      }
    if(!strcmp(volumePropertyNode->GetName(),"HighConstantOpacity"))
      {
      vtkPiecewiseFunction *compositeOpacity =
          volumePropertyNode->GetScalarOpacity();
      compositeOpacity->RemoveAllPoints();
      compositeOpacity->AddPoint(min, 0.);
      compositeOpacity->AddPoint(noise3 - (noise3 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise3, 0.7);
      compositeOpacity->AddPoint(max, 0.7);

      vtkColorTransferFunction *color =
          volumePropertyNode->GetColor();
      color->RemoveAllPoints();
      color->AddRGBPoint(min, 0., 0., 0.);
      color->AddRGBPoint(noise3, 0., 0.3, 0.);
      color->AddRGBPoint(max, 0., 1., 0.);

      vtkPiecewiseFunction *gradientOpacity =
          volumePropertyNode->GetGradientOpacity();
      gradientOpacity->RemoveAllPoints();
      gradientOpacity->AddPoint(min, 1.);
      gradientOpacity->AddPoint(max, 1.);
      }
    if(!strcmp(volumePropertyNode->GetName(),"OneSurfaceGreen"))
      {
      vtkPiecewiseFunction *compositeOpacity =
          volumePropertyNode->GetScalarOpacity();
      compositeOpacity->RemoveAllPoints();
      compositeOpacity->AddPoint(min, 0.);
      compositeOpacity->AddPoint(noise3 - (noise3 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise3, 0.6, 0.5, 0.2);
      compositeOpacity->AddPoint(noise3 + (noise3 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(max, 0.);

      vtkColorTransferFunction *color =
          volumePropertyNode->GetColor();
      color->RemoveAllPoints();
      color->AddRGBPoint(min, 0., 0., 0.);
      color->AddRGBPoint(noise3, 0., 1., 0.);
      color->AddRGBPoint(max, 0., 1., 0.);

      vtkPiecewiseFunction *gradientOpacity =
          volumePropertyNode->GetGradientOpacity();
      gradientOpacity->RemoveAllPoints();
      gradientOpacity->AddPoint(min, 1.);
      gradientOpacity->AddPoint(max, 1.);
      }
    if(!strcmp(volumePropertyNode->GetName(),"OneSurfaceWhite"))
      {
      vtkPiecewiseFunction *compositeOpacity =
          volumePropertyNode->GetScalarOpacity();
      compositeOpacity->RemoveAllPoints();
      compositeOpacity->AddPoint(min, 0.);
      compositeOpacity->AddPoint(noise3 - (noise3 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise3, 0.6, 0.5, 0.2);
      compositeOpacity->AddPoint(noise3 + (noise3 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(max, 0.);

      vtkColorTransferFunction *color =
          volumePropertyNode->GetColor();
      color->RemoveAllPoints();
      color->AddRGBPoint(min, 0., 0., 0.);
      color->AddRGBPoint(noise3, 1., 1., 1.);
      color->AddRGBPoint(max, 1., 1., 1.);

      vtkPiecewiseFunction *gradientOpacity =
          volumePropertyNode->GetGradientOpacity();
      gradientOpacity->RemoveAllPoints();
      gradientOpacity->AddPoint(min, 1.);
      gradientOpacity->AddPoint(max, 1.);
      }
    if(!strcmp(volumePropertyNode->GetName(),"TwoSurfaces"))
      {
      vtkPiecewiseFunction *compositeOpacity =
          volumePropertyNode->GetScalarOpacity();
      compositeOpacity->RemoveAllPoints();
      compositeOpacity->AddPoint(min, 0.);
      compositeOpacity->AddPoint(noise3 - (noise3 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise3, 0.5, 0.5, 0.2);
      compositeOpacity->AddPoint(noise3 + (noise3 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise7 - (noise7 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise7, 0.8, 0.5, 0.2);
      compositeOpacity->AddPoint(noise7 + (noise7 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(max, 0.);

      vtkColorTransferFunction *color =
          volumePropertyNode->GetColor();
      color->RemoveAllPoints();
      color->AddRGBPoint(min, 0., 0., 0.);
      color->AddRGBPoint(noise3, 0., 1., 0.);
      color->AddRGBPoint(noise7, 0., 0., 1.);
      color->AddRGBPoint(max, 0., 0., 1.);

      vtkPiecewiseFunction *gradientOpacity =
          volumePropertyNode->GetGradientOpacity();
      gradientOpacity->RemoveAllPoints();
      gradientOpacity->AddPoint(min, 1.);
      gradientOpacity->AddPoint(max, 1.);
      }
    if(!strcmp(volumePropertyNode->GetName(),"ThreeSurfaces"))
      {
      vtkPiecewiseFunction *compositeOpacity =
         volumePropertyNode->GetScalarOpacity();
      compositeOpacity->RemoveAllPoints();
      compositeOpacity->AddPoint(min, 0.);
      compositeOpacity->AddPoint(noise3 - (noise3 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise3, 0.5, 0.5, 0.2);
      compositeOpacity->AddPoint(noise3 + (noise3 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise7 - (noise7 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise7, 0.65, 0.5, 0.2);
      compositeOpacity->AddPoint(noise7 + (noise7 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise15 - (noise15 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise15, 0.8, 0.5, 0.2);
      compositeOpacity->AddPoint(noise15 + (noise15 / 5.), 0., 0.5, 0.2);
      compositeOpacity->AddPoint(max, 0.);

      vtkColorTransferFunction *color =
          volumePropertyNode->GetColor();
      color->RemoveAllPoints();
      color->AddRGBPoint(min, 0., 0., 0.);
      color->AddRGBPoint(noise3, 0., 1., 0.);
      color->AddRGBPoint(noise7, 0., 0., 1.);
      color->AddRGBPoint(noise15, 1., 0., 0.);
      color->AddRGBPoint(max, 1., 0., 0.);

      vtkPiecewiseFunction *gradientOpacity =
          volumePropertyNode->GetGradientOpacity();
      gradientOpacity->RemoveAllPoints();
      gradientOpacity->AddPoint(min, 1.);
      gradientOpacity->AddPoint(max, 1.);
      }
    if(!strcmp(volumePropertyNode->GetName(),"BrightSurfaces"))
      {
      vtkPiecewiseFunction *compositeOpacity =
         volumePropertyNode->GetScalarOpacity();
      compositeOpacity->RemoveAllPoints();
      compositeOpacity->AddPoint(min, 0.);
      compositeOpacity->AddPoint(noise3 - noise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise3, 0.2, 0.5, 0.2);
      compositeOpacity->AddPoint(noise3 + noise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise6 - halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise6, 0.3, 0.5, 0.2);
      compositeOpacity->AddPoint(noise6 + halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise9 - halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise9, 0.4, 0.5, 0.2);
      compositeOpacity->AddPoint(noise9 + halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise12 - halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise12, 0.5, 0.5, 0.2);
      compositeOpacity->AddPoint(noise12 + halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise15 - halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise15, 0.6, 0.5, 0.2);
      compositeOpacity->AddPoint(noise15 + halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise18 - halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise18, 0.7, 0.5, 0.2);
      compositeOpacity->AddPoint(noise18 + halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise21 - halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise21, 0.8, 0.5, 0.2);
      compositeOpacity->AddPoint(noise21 + halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise24 - halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(noise24, 0.9, 0.5, 0.2);
      compositeOpacity->AddPoint(noise24 + halfNoise, 0., 0.5, 0.2);
      compositeOpacity->AddPoint(max, 0.);

      vtkColorTransferFunction *color =
          volumePropertyNode->GetColor();
      color->RemoveAllPoints();
      color->AddRGBPoint(min, 1., 0., 0.098);
      color->AddRGBPoint(noise3, 1., 0., 0.098);
      color->AddRGBPoint(noise6, 0.98, 0.631, 0.);
      color->AddRGBPoint(noise9, 0.592, 0.996, 0.);
      color->AddRGBPoint(noise12, 0., 0.945, 0.161);
      color->AddRGBPoint(noise15, 0., 1., 0.906);
      color->AddRGBPoint(noise18, 0., 0.326, 0.996);
      color->AddRGBPoint(noise21, 0.435, 0., 0.988);
      color->AddRGBPoint(noise24, 0.977, 0., 0.792);
      color->AddRGBPoint(max, 0.977, 0., 0.792);

      vtkPiecewiseFunction *gradientOpacity =
          volumePropertyNode->GetGradientOpacity();
      gradientOpacity->RemoveAllPoints();
      gradientOpacity->AddPoint(min, 1.);
      gradientOpacity->AddPoint(max, 1.);
      }
    }
    return true;
}

//---------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::LoadPresets(vtkMRMLScene *scene)
{
  if (!this->PresetsScene)
    {
    return false;
    }

  if (this->GetModuleShareDirectory().empty())
    {
    vtkErrorMacro(<< "Failed to load presets: Share directory *NOT* set !");
    return false;
    }

  std::string presetFileName = this->GetModuleShareDirectory() + "/presets.xml";
  scene->SetURL(presetFileName.c_str());
  int connected = scene->Connect();
  if (!connected)
    {
    vtkErrorMacro(<< "Failed to load presets [" << presetFileName << "]");
    return false;
    }

  if (!this->GetMRMLScene())
    {
    vtkErrorMacro(<< "Failed to load presets: MRMLScene not found.");
    return false;
    }
  vtkSmartPointer<vtkCollection> presets = vtkSmartPointer<vtkCollection>::Take(
      this->PresetsScene->GetNodesByClass("vtkMRMLVolumePropertyNode"));

  // the presets stay in the presets scene: the MRML scene gets its own nodes
  // (a node in two scenes gets a new ID when added to the second one, which
  // breaks the node ID cache of the first one)
  for(int i = 0; i < presets->GetNumberOfItems(); i++)
    {
    this->GetScenePresetNode(vtkMRMLNode::SafeDownCast(presets->GetItemAsObject(i)));
    }

  return true;
}

//----------------------------------------------------------------------------
namespace
{
// Positions of the points shifted so that consecutive points are at least
// minimumGap apart; the points are moved toward the inside of the range
// (first and last points stay at their place when possible).
void ExtendFiniteRange(double range[2], const double otherRange[2])
{
  if (otherRange[1] >= otherRange[0] && std::isfinite(otherRange[0]) && std::isfinite(otherRange[1]))
    {
    range[0] = std::min(range[0], otherRange[0]);
    range[1] = std::max(range[1], otherRange[1]);
    }
}

//----------------------------------------------------------------------------
std::vector<double> SpreadPositions(const std::vector<double>& positions, double minimumGap)
{
  std::vector<double> spread(positions);
  const size_t n = spread.size();
  for (size_t i = 1; i < n; ++i)
    {
    spread[i] = std::max(spread[i], spread[i - 1] + minimumGap);
    }
  if (n > 1 && spread[n - 1] > positions[n - 1])
    {
    // Shift back from the end, keeping the last point at its place
    spread[n - 1] = std::max(positions[n - 1], spread[0] + minimumGap * (n - 1));
    for (size_t i = n - 1; i-- > 0;)
      {
      spread[i] = std::min(spread[i], spread[i + 1] - minimumGap);
      }
    }
  return spread;
}

bool HasGapSmallerThan(const std::vector<double>& positions, double minimumGap)
{
  for (size_t i = 1; i < positions.size(); ++i)
    {
    // tolerance for the rounding of gaps already widened to minimumGap
    if (positions[i] - positions[i - 1] < minimumGap * (1. - 1e-6))
      {
      return true;
      }
    }
  return false;
}
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::EnsureMinimumTransferFunctionGap(vtkVolumeProperty* volumeProperty,
                                                                 const double scalarRange[2])
{
  if (!volumeProperty || !scalarRange || !(scalarRange[1] > scalarRange[0]))
    {
    return false;
    }
  // 1/10000 of the range: the lookup table textures stay below 16384 texels
  const double minimumGap = (scalarRange[1] - scalarRange[0]) * 1e-4;
  bool modified = false;

  vtkPiecewiseFunction* opacity = volumeProperty->GetScalarOpacity();
  if (opacity && opacity->GetSize() > 1)
    {
    std::vector<std::array<double, 4>> nodes(opacity->GetSize());
    std::vector<double> positions(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i)
      {
      opacity->GetNodeValue(static_cast<int>(i), nodes[i].data());
      positions[i] = nodes[i][0];
      }
    if (HasGapSmallerThan(positions, minimumGap))
      {
      std::vector<double> spread = SpreadPositions(positions, minimumGap);
      vtkNew<vtkPiecewiseFunction> newOpacity;
      for (size_t i = 0; i < nodes.size(); ++i)
        {
        newOpacity->AddPoint(spread[i], nodes[i][1], nodes[i][2], nodes[i][3]);
        }
      opacity->DeepCopy(newOpacity);
      modified = true;
      }
    }

  vtkColorTransferFunction* color = volumeProperty->GetRGBTransferFunction();
  if (color && color->GetSize() > 1)
    {
    std::vector<std::array<double, 6>> nodes(color->GetSize());
    std::vector<double> positions(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i)
      {
      color->GetNodeValue(static_cast<int>(i), nodes[i].data());
      positions[i] = nodes[i][0];
      }
    if (HasGapSmallerThan(positions, minimumGap))
      {
      std::vector<double> spread = SpreadPositions(positions, minimumGap);
      vtkNew<vtkColorTransferFunction> newColor;
      newColor->SetColorSpace(color->GetColorSpace());
      for (size_t i = 0; i < nodes.size(); ++i)
        {
        newColor->AddRGBPoint(spread[i], nodes[i][1], nodes[i][2], nodes[i][3], nodes[i][4], nodes[i][5]);
        }
      color->DeepCopy(newColor);
      modified = true;
      }
    }

  return modified;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::IsAstroSegmentation(vtkMRMLSegmentationNode* segmentationNode)
{
  if (!segmentationNode || !segmentationNode->GetSegmentation())
    {
    return false;
    }
  // masks: binary labelmaps (contours are closed surfaces)
  if (vtkSlicerAstroVolumeLogic::IsContoursSegmentation(segmentationNode) ||
      segmentationNode->GetSegmentation()->GetSourceRepresentationName() !=
        vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName())
    {
    return false;
    }
  if (segmentationNode->GetAttribute("SlicerAstro.NAXIS1"))
    {
    return true;
    }
  vtkMRMLNode* referenceNode = segmentationNode->GetNodeReference(
    vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str());
  if (referenceNode)
    {
    return referenceNode->IsA("vtkMRMLAstroVolumeNode");
    }
  // no reference volume yet (e.g. a new segmentation): astronomical if the
  // scene has astronomical data
  vtkMRMLScene* scene = segmentationNode->GetScene();
  return scene && scene->GetFirstNodeByClass("vtkMRMLAstroVolumeNode") != nullptr;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::EnsureFITSMaskStorageNode(vtkMRMLSegmentationNode* segmentationNode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!segmentationNode || !scene || segmentationNode->GetScene() != scene || scene->IsClosing())
    {
    return false;
    }
  vtkMRMLStorageNode* storageNode = segmentationNode->GetStorageNode();
  if (storageNode && storageNode->IsA("vtkMRMLAstroSegmentationStorageNode") &&
      (!storageNode->GetFileName() || !*storageNode->GetFileName()) &&
      !vtkSlicerAstroVolumeLogic::IsAstroSegmentation(segmentationNode))
    {
    // the default FITS storage of a segmentation that is not (or no longer)
    // astronomical, e.g. its reference volume is a medical image or it became
    // contours: Slicer's default storage is used instead
    segmentationNode->SetAndObserveStorageNodeID(nullptr);
    scene->RemoveNode(storageNode);
    return false;
    }
  if (storageNode)
    {
    // an existing storage node (e.g. .seg.nrrd chosen by the user) is kept
    return storageNode->IsA("vtkMRMLAstroSegmentationStorageNode");
    }
  if (!vtkSlicerAstroVolumeLogic::IsAstroSegmentation(segmentationNode))
    {
    return false;
    }
  vtkMRMLNode* newStorageNode = scene->AddNewNodeByClass("vtkMRMLAstroSegmentationStorageNode");
  if (!newStorageNode)
    {
    return false;
    }
  segmentationNode->SetAndObserveStorageNodeID(newStorageNode->GetID());
  vtkSlicerAstroVolumeLogic::SetBinaryLabelmapDisplay(segmentationNode);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetMaskReferenceVolume(vtkMRMLSegmentationNode* segmentationNode,
                                                       vtkMRMLAstroVolumeNode* volumeNode)
{
  if (!segmentationNode || !segmentationNode->GetSegmentation() ||
      !volumeNode || !volumeNode->GetImageData() ||
      vtkSlicerAstroVolumeLogic::IsContoursSegmentation(segmentationNode) ||
      segmentationNode->GetSegmentation()->GetNumberOfSegments() > 0 ||
      segmentationNode->GetNodeReference(
        vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str()))
    {
    return false;
    }
  segmentationNode->SetReferenceImageGeometryParameterFromVolumeNode(volumeNode);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::NameMaskAfterReferenceVolume(vtkMRMLSegmentationNode* segmentationNode)
{
  vtkMRMLScene* scene = segmentationNode ? segmentationNode->GetScene() : nullptr;
  if (!scene || !segmentationNode->GetName() ||
      vtkSlicerAstroVolumeLogic::IsContoursSegmentation(segmentationNode))
    {
    return false;
    }
  vtkMRMLAstroVolumeNode* volumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(segmentationNode->GetNodeReference(
    vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str()));
  if (!volumeNode || !volumeNode->GetName())
    {
    return false;
    }
  // only the default names given by Slicer (the user's names are kept)
  static const std::regex defaultName("^Segmentation(_[0-9]+)?$");
  if (!std::regex_match(std::string(segmentationNode->GetName()), defaultName))
    {
    return false;
    }
  std::string name = scene->GenerateUniqueName(std::string(volumeNode->GetName()) + "_mask");
  segmentationNode->SetName(name.c_str());
  return true;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::EnsureVolumePropertyTransferFunctionGaps(vtkMRMLVolumePropertyNode* volumePropertyNode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (this->UpdatingTransferFunctionGaps || !scene || scene->IsImporting() || scene->IsClosing() ||
      !volumePropertyNode || !volumePropertyNode->GetVolumeProperty())
    {
    return;
    }
  vtkVolumeProperty* volumeProperty = volumePropertyNode->GetVolumeProperty();

  // range: the transfer functions and the data of the volumes rendered with them
  double range[2] = {VTK_DOUBLE_MAX, VTK_DOUBLE_MIN};
  if (volumeProperty->GetScalarOpacity() && volumeProperty->GetScalarOpacity()->GetSize() > 1)
    {
    ExtendFiniteRange(range, volumeProperty->GetScalarOpacity()->GetRange());
    }
  if (volumeProperty->GetRGBTransferFunction() && volumeProperty->GetRGBTransferFunction()->GetSize() > 1)
    {
    ExtendFiniteRange(range, volumeProperty->GetRGBTransferFunction()->GetRange());
    }
  bool constantData = false;
  std::vector<vtkMRMLNode*> referencingNodes;
  scene->GetReferencingNodes(volumePropertyNode, referencingNodes);
  for (vtkMRMLNode* node : referencingNodes)
    {
    vtkMRMLVolumeRenderingDisplayNode* renderingDisplayNode = vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(node);
    vtkMRMLVolumeNode* volumeNode = renderingDisplayNode ? renderingDisplayNode->GetVolumeNode() : nullptr;
    vtkImageData* imageData = volumeNode ? volumeNode->GetImageData() : nullptr;
    if (imageData && imageData->GetPointData() && imageData->GetPointData()->GetScalars())
      {
      double dataRange[2] = {0., 0.};
      imageData->GetPointData()->GetScalars()->GetFiniteRange(dataRange);
      ExtendFiniteRange(range, dataRange);
      // e.g. a model being created, still empty
      constantData = constantData || dataRange[0] == dataRange[1];
      }
    }
  if (range[1] < range[0])
    {
    return;
    }
  // a volume with constant data (e.g. a model being created, still empty)
  // has a degenerate range: the lookup tables of VTK then use a default
  // range, so the gaps must be significant with respect to a unit range
  if (constantData && range[1] - range[0] < 1.)
    {
    const double center = 0.5 * (range[0] + range[1]);
    const double halfWidth = std::max(0.5, fabs(center));
    range[0] = center - halfWidth;
    range[1] = center + halfWidth;
    }

  this->UpdatingTransferFunctionGaps = true;
  vtkSlicerAstroVolumeLogic::EnsureMinimumTransferFunctionGap(volumeProperty, range);
  this->UpdatingTransferFunctionGaps = false;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::EstimateBeamFromNoise(vtkMRMLAstroVolumeNode* volumeNode, double beam[3])
{
  beam[0] = beam[1] = beam[2] = 0.;
  vtkImageData* imageData = volumeNode ? volumeNode->GetImageData() : nullptr;
  if (!imageData || !imageData->GetPointData() || !imageData->GetPointData()->GetScalars())
    {
    return false;
    }
  vtkDataArray* scalars = imageData->GetPointData()->GetScalars();
  int dims[3] = {0, 0, 0};
  imageData->GetDimensions(dims);
  const double cdelt1 = StringToDouble(volumeNode->GetAttribute("SlicerAstro.CDELT1"));
  const double cdelt2 = StringToDouble(volumeNode->GetAttribute("SlicerAstro.CDELT2"));
  const int maxLag = std::min(24, std::min(dims[0], dims[1]) / 4);
  if (maxLag < 3 || cdelt1 == 0. || cdelt2 == 0.)
    {
    return false;
    }

  // autocorrelation of the noise (lags up to maxLag) of the first and last channels
  const int size = 2 * maxLag + 1;
  std::vector<double> acf(static_cast<size_t>(size) * size, 0.);
  std::vector<double> counts(acf.size(), 0.);
  std::vector<int> channels = {0};
  if (dims[2] > 1)
    {
    channels.push_back(dims[2] - 1);
    }
  const vtkIdType sliceSize = static_cast<vtkIdType>(dims[0]) * dims[1];
  std::vector<double> plane(static_cast<size_t>(sliceSize));
  for (int channel : channels)
    {
    // channel without its mean (NaN: blank)
    double sum = 0.;
    vtkIdType finite = 0;
    for (vtkIdType index = 0; index < sliceSize; index++)
      {
      const double value = scalars->GetComponent(channel * sliceSize + index, 0);
      plane[index] = value;
      if (std::isfinite(value))
        {
        sum += value;
        finite++;
        }
      }
    if (finite < sliceSize / 2)
      {
      continue;
      }
    const double mean = sum / finite;
    for (double& value : plane)
      {
      value = std::isfinite(value) ? value - mean : std::numeric_limits<double>::quiet_NaN();
      }
    for (int dy = -maxLag; dy <= maxLag; dy++)
      {
      for (int dx = -maxLag; dx <= maxLag; dx++)
        {
        double product = 0.;
        double count = 0.;
        for (int y = std::max(0, -dy); y < std::min(dims[1], dims[1] - dy); y++)
          {
          const double* row = plane.data() + static_cast<vtkIdType>(y) * dims[0];
          const double* shiftedRow = plane.data() + static_cast<vtkIdType>(y + dy) * dims[0];
          for (int x = std::max(0, -dx); x < std::min(dims[0], dims[0] - dx); x++)
            {
            const double value = row[x] * shiftedRow[x + dx];
            if (std::isfinite(value))
              {
              product += value;
              count += 1.;
              }
            }
          }
        const size_t lag = static_cast<size_t>(dy + maxLag) * size + (dx + maxLag);
        acf[lag] += product;
        counts[lag] += count;
        }
      }
    }
  const size_t center = static_cast<size_t>(maxLag) * size + maxLag;
  if (counts[center] <= 0. || acf[center] <= 0.)
    {
    return false;
    }
  const double acfCenter = acf[center] / counts[center];
  for (size_t lag = 0; lag < acf.size(); lag++)
    {
    acf[lag] = counts[lag] > 0. ? (acf[lag] / counts[lag]) / acfCenter : 0.;
    }

  // region above half maximum connected to the center (no sidelobes)
  std::vector<char> region(acf.size(), 0);
  std::vector<size_t> stack = {center};
  region[center] = 1;
  bool touchesBorder = false;
  while (!stack.empty())
    {
    const size_t lag = stack.back();
    stack.pop_back();
    const int lx = static_cast<int>(lag % size);
    const int ly = static_cast<int>(lag / size);
    if (lx == 0 || ly == 0 || lx == size - 1 || ly == size - 1)
      {
      touchesBorder = true;
      }
    const int neighbours[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const int* step : neighbours)
      {
      const int nx = lx + step[0];
      const int ny = ly + step[1];
      if (nx < 0 || ny < 0 || nx >= size || ny >= size)
        {
        continue;
        }
      const size_t neighbour = static_cast<size_t>(ny) * size + nx;
      if (!region[neighbour] && acf[neighbour] >= 0.5)
        {
        region[neighbour] = 1;
        stack.push_back(neighbour);
        }
      }
    }
  // not correlated (a single pixel) or beam too large for the lags
  int regionSize = 0;
  for (char inside : region)
    {
    regionSize += inside;
    }
  if (regionSize < 5 || touchesBorder)
    {
    return false;
    }

  // second moments of the half maximum ellipse (East, North offsets in
  // degrees): for a uniform ellipse the moment along an axis is HWHM^2 / 4
  double cee = 0., cnn = 0., cen = 0.;
  for (size_t lag = 0; lag < acf.size(); lag++)
    {
    if (!region[lag])
      {
      continue;
      }
    const double east = cdelt1 * (static_cast<int>(lag % size) - maxLag);
    const double north = cdelt2 * (static_cast<int>(lag / size) - maxLag);
    cee += east * east;
    cnn += north * north;
    cen += east * north;
    }
  // pixel area: each lag is a pixel, the continuous ellipse adds 1/12 pixel^2
  cee = cee / regionSize + cdelt1 * cdelt1 / 12.;
  cnn = cnn / regionSize + cdelt2 * cdelt2 / 12.;
  cen /= regionSize;
  const double trace = cee + cnn;
  const double delta = sqrt(std::max(0., 0.25 * (cee - cnn) * (cee - cnn) + cen * cen));
  const double majorMoment = 0.5 * trace + delta;
  const double minorMoment = std::max(0., 0.5 * trace - delta);
  // FWHM of the autocorrelation = 2 * HWHM = 4 sqrt(moment); beam = FWHM / sqrt(2)
  beam[0] = 4. * sqrt(majorMoment) / sqrt(2.);
  beam[1] = 4. * sqrt(minorMoment) / sqrt(2.);
  // position angle of the major axis, from North to East
  const double angle = 0.5 * atan2(2. * cen, cnn - cee);
  double positionAngle = vtkMath::DegreesFromRadians(angle);
  while (positionAngle < 0.)
    {
    positionAngle += 180.;
    }
  while (positionAngle >= 180.)
    {
    positionAngle -= 180.;
    }
  beam[2] = positionAngle;
  return beam[0] > 0. && beam[1] > 0.;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::IsMaskSegmentation(vtkMRMLSegmentationNode* segmentationNode)
{
  return segmentationNode && segmentationNode->GetSegmentation() &&
         !vtkSlicerAstroVolumeLogic::IsContoursSegmentation(segmentationNode) &&
         segmentationNode->GetSegmentation()->GetSourceRepresentationName() ==
           vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName();
}

//----------------------------------------------------------------------------
vtkMRMLSegmentationNode* vtkSlicerAstroVolumeLogic::GetMaskForVolume(vtkMRMLScene* scene, vtkMRMLVolumeNode* volumeNode,
                                                                    vtkMRMLSegmentationNode* preferredSegmentationNode)
{
  if (!scene || !volumeNode)
    {
    return nullptr;
    }
  const std::string role = vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole();
  if (vtkSlicerAstroVolumeLogic::IsMaskSegmentation(preferredSegmentationNode) &&
      preferredSegmentationNode->GetNodeReference(role.c_str()) == volumeNode)
    {
    return preferredSegmentationNode;
    }
  // the mask referenced by the volume
  vtkMRMLSegmentationNode* referencedMask = vtkMRMLSegmentationNode::SafeDownCast(
    volumeNode->GetNodeReference(vtkSlicerAstroVolumeLogic::GetMaskReferenceRole()));
  if (vtkSlicerAstroVolumeLogic::IsMaskSegmentation(referencedMask) &&
      referencedMask->GetNodeReference(role.c_str()) == volumeNode)
    {
    return referencedMask;
    }
  vtkMRMLSegmentationNode* mask = nullptr;
  std::vector<vtkMRMLNode*> nodes;
  scene->GetNodesByClass("vtkMRMLSegmentationNode", nodes);
  for (vtkMRMLNode* node : nodes)
    {
    vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node);
    if (vtkSlicerAstroVolumeLogic::IsMaskSegmentation(segmentationNode) &&
        segmentationNode->GetNodeReference(role.c_str()) == volumeNode)
      {
      mask = segmentationNode;
      }
    }
  return mask;
}

//----------------------------------------------------------------------------
vtkMRMLSegmentationNode* vtkSlicerAstroVolumeLogic::GetContoursSegmentationForVolume(
  vtkMRMLAstroVolumeNode* volumeNode, bool create)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene || !volumeNode || !volumeNode->GetID() || !volumeNode->GetScene())
    {
    return nullptr;
    }
  const char* contoursRole = vtkSlicerAstroVolumeLogic::GetContoursReferenceRole();
  const std::string geometryRole = vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole();
  const std::string contoursName = std::string(volumeNode->GetName() ? volumeNode->GetName() : "") + "_Contours";

  vtkMRMLSegmentationNode* contours = vtkMRMLSegmentationNode::SafeDownCast(
    volumeNode->GetNodeReference(contoursRole));
  // contours of another volume (e.g. reference copied with a cloned volume)
  if (contours && contours->GetNodeReference(geometryRole.c_str()) &&
      contours->GetNodeReference(geometryRole.c_str()) != volumeNode)
    {
    contours = nullptr;
    }
  if (!vtkSlicerAstroVolumeLogic::IsContoursSegmentation(contours))
    {
    // contours of the volume in the scene (e.g. a scene saved without the reference)
    contours = nullptr;
    std::vector<vtkMRMLNode*> nodes;
    scene->GetNodesByClass("vtkMRMLSegmentationNode", nodes);
    for (vtkMRMLNode* node : nodes)
      {
      vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node);
      if (vtkSlicerAstroVolumeLogic::IsContoursSegmentation(segmentationNode) &&
          (segmentationNode->GetNodeReference(geometryRole.c_str()) == volumeNode ||
           (segmentationNode->GetName() && contoursName == segmentationNode->GetName())))
        {
        contours = segmentationNode;
        break;
        }
      }
    }
  if (!contours && create)
    {
    vtkSmartPointer<vtkMRMLSegmentationNode> newContours = vtkSmartPointer<vtkMRMLSegmentationNode>::Take(
      vtkMRMLSegmentationNode::SafeDownCast(scene->CreateNodeByClass("vtkMRMLSegmentationNode")));
    newContours->SetName(scene->GetUniqueNameByString(contoursName.c_str()));
    // marked as contours before being added (never listed as a mask)
    newContours->SetAttribute(vtkSlicerAstroVolumeLogic::GetContoursAttributeName(), "true");
    scene->AddNode(newContours);
    newContours->CreateDefaultDisplayNodes();
    vtkSlicerAstroVolumeLogic::SetContoursDisplay(newContours);
    contours = newContours;
    }
  if (contours)
    {
    if (contours->GetNodeReference(geometryRole.c_str()) != volumeNode)
      {
      contours->SetReferenceImageGeometryParameterFromVolumeNode(volumeNode);
      }
    if (volumeNode->GetNodeReference(contoursRole) != contours)
      {
      volumeNode->SetNodeReferenceID(contoursRole, contours->GetID());
      }
    }
  return contours;
}

//----------------------------------------------------------------------------
vtkMRMLSegmentationNode* vtkSlicerAstroVolumeLogic::GetMaskSegmentationForVolume(
  vtkMRMLAstroVolumeNode* volumeNode, bool create)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene || !volumeNode || !volumeNode->GetID() || !volumeNode->GetScene())
    {
    return nullptr;
    }
  const char* maskRole = vtkSlicerAstroVolumeLogic::GetMaskReferenceRole();
  const std::string geometryRole = vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole();
  vtkMRMLSegmentationNode* referencedMask = vtkMRMLSegmentationNode::SafeDownCast(
    volumeNode->GetNodeReference(maskRole));
  vtkMRMLSegmentationNode* mask = vtkSlicerAstroVolumeLogic::GetMaskForVolume(scene, volumeNode, referencedMask);
  if (!mask && vtkSlicerAstroVolumeLogic::IsMaskSegmentation(referencedMask) &&
      !referencedMask->GetNodeReference(geometryRole.c_str()))
    {
    // referenced mask without reference geometry (e.g. older scene)
    mask = referencedMask;
    mask->SetReferenceImageGeometryParameterFromVolumeNode(volumeNode);
    }
  if (!mask && create)
    {
    // a mask of this cube loaded before it (same geometry, no reference volume)
    std::vector<vtkMRMLNode*> nodes;
    scene->GetNodesByClass("vtkMRMLSegmentationNode", nodes);
    for (vtkMRMLNode* node : nodes)
      {
      vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node);
      if (segmentationNode && segmentationNode->GetSegmentation()->GetNumberOfSegments() > 0 &&
          vtkSlicerAstroVolumeLogic::FindVolumeOfMask(scene, segmentationNode) == volumeNode)
        {
        segmentationNode->SetNodeReferenceID(geometryRole.c_str(), volumeNode->GetID());
        mask = segmentationNode;
        break;
        }
      }
    }
  if (!mask && create)
    {
    // the empty default segmentation of Slicer ("Segmentation", e.g. created
    // by the Segment Editor before the data was loaded) becomes the mask
    std::vector<vtkMRMLNode*> nodes;
    scene->GetNodesByClass("vtkMRMLSegmentationNode", nodes);
    for (vtkMRMLNode* node : nodes)
      {
      vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node);
      if (vtkSlicerAstroVolumeLogic::IsMaskSegmentation(segmentationNode) &&
          !segmentationNode->GetNodeReference(geometryRole.c_str()) &&
          segmentationNode->GetSegmentation()->GetNumberOfSegments() == 0 &&
          vtkSlicerAstroVolumeLogic::SetMaskReferenceVolume(segmentationNode, volumeNode))
        {
        vtkSlicerAstroVolumeLogic::NameMaskAfterReferenceVolume(segmentationNode);
        vtkSlicerAstroVolumeLogic::SetBinaryLabelmapDisplay(segmentationNode);
        if (segmentationNode->GetDisplayNode())
          {
          segmentationNode->GetDisplayNode()->SetOpacity(vtkSlicerAstroVolumeLogic::GetDefaultMaskOpacity());
          }
        mask = segmentationNode;
        break;
        }
      }
    }
  if (!mask && create)
    {
    vtkSmartPointer<vtkMRMLSegmentationNode> newMask = vtkSmartPointer<vtkMRMLSegmentationNode>::Take(
      vtkMRMLSegmentationNode::SafeDownCast(scene->CreateNodeByClass("vtkMRMLSegmentationNode")));
    newMask->SetName(scene->GetUniqueNameByString((std::string(volumeNode->GetName()) + "_mask").c_str()));
    scene->AddNode(newMask);
    newMask->CreateDefaultDisplayNodes();
    if (newMask->GetDisplayNode())
      {
      newMask->GetDisplayNode()->SetOpacity(vtkSlicerAstroVolumeLogic::GetDefaultMaskOpacity());
      }
    vtkSlicerAstroVolumeLogic::SetMaskReferenceVolume(newMask, volumeNode);
    vtkSlicerAstroVolumeLogic::SetBinaryLabelmapDisplay(newMask);
    mask = newMask;
    }
  if (mask && volumeNode->GetNodeReference(maskRole) != mask)
    {
    volumeNode->SetNodeReferenceID(maskRole, mask->GetID());
    }
  return mask;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::EnsureSegmentationsOfLoadedVolume(vtkMRMLAstroVolumeNode* volumeNode)
{
  // a volume loaded from a file: data and a storage node with a file name
  // (not the volumes derived by the modules, which have no storage node);
  // the volumes of an imported scene are handled at the end of the import
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (this->EnsuringVolumeSegmentations || !scene || scene->IsImporting() || scene->IsClosing() ||
      !volumeNode || !volumeNode->GetScene() || !volumeNode->GetImageData() ||
      !volumeNode->GetStorageNode() || !volumeNode->GetStorageNode()->GetFileName() ||
      vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(volumeNode))
    {
    return;
    }
  this->EnsuringVolumeSegmentations = true;
  this->EnsureVolumeSegmentations(volumeNode);
  this->EnsuringVolumeSegmentations = false;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::EnsureVolumeSegmentations(vtkMRMLAstroVolumeNode* volumeNode)
{
  if (!volumeNode || vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(volumeNode))
    {
    return;
    }
  this->GetContoursSegmentationForVolume(volumeNode, true);
  this->GetMaskSegmentationForVolume(volumeNode, true);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(vtkMRMLCameraNode* cameraNode, vtkMRMLVolumeNode* volumeNode,
                                                     const double directionOfProjection[3], const double viewUp[3],
                                                     const double* focalPoint /*= nullptr*/)
{
  if (!cameraNode || !volumeNode || !volumeNode->GetImageData())
    {
    return false;
    }
  double bounds[6] = {1., -1., 1., -1., 1., -1.};
  volumeNode->GetRASBounds(bounds);
  if (bounds[0] > bounds[1] || bounds[2] > bounds[3] || bounds[4] > bounds[5])
    {
    return false;
    }

  double center[3] = {0., 0., 0.}, focal[3] = {0., 0., 0.};
  double radius = 0.;
  for (int axis = 0; axis < 3; axis++)
    {
    center[axis] = 0.5 * (bounds[2 * axis] + bounds[2 * axis + 1]);
    const double halfSize = 0.5 * (bounds[2 * axis + 1] - bounds[2 * axis]);
    radius += halfSize * halfSize;
    focal[axis] = focalPoint ? focalPoint[axis] : center[axis];
    }
  radius = sqrt(radius);
  if (focalPoint)
    {
    // the whole volume around a focal point that is not its center
    radius += sqrt(vtkMath::Distance2BetweenPoints(center, focal));
    }
  radius = std::max(radius, 1.);

  double direction[3] = {directionOfProjection[0], directionOfProjection[1], directionOfProjection[2]};
  if (vtkMath::Normalize(direction) < 1.E-12)
    {
    return false;
    }

  // distance where the bounding sphere fits in the view angle
  double viewAngle = cameraNode->GetViewAngle();
  if (viewAngle <= 1.)
    {
    viewAngle = 30.;
    }
  const double distance = 1.05 * radius / sin(vtkMath::RadiansFromDegrees(0.5 * viewAngle));

  double position[3] = {0., 0., 0.};
  for (int axis = 0; axis < 3; axis++)
    {
    position[axis] = focal[axis] - direction[axis] * distance;
    }

  int wasModifying = cameraNode->StartModify();
  cameraNode->SetFocalPoint(focal);
  cameraNode->SetPosition(position);
  cameraNode->SetViewUp(viewUp[0], viewUp[1], viewUp[2]);
  cameraNode->SetParallelScale(1.05 * radius);
  cameraNode->ResetClippingRange();
  cameraNode->EndModify(wasModifying);
  return true;
}

//----------------------------------------------------------------------------
vtkMRMLCameraNode* vtkSlicerAstroVolumeLogic::GetThreeDViewCameraNode(vtkMRMLScene* scene, const char* viewNodeID)
{
  vtkMRMLViewNode* viewNode = (scene && viewNodeID) ?
    vtkMRMLViewNode::SafeDownCast(scene->GetNodeByID(viewNodeID)) : nullptr;
  if (!viewNode || !viewNode->GetLayoutName())
    {
    return nullptr;
    }
  return vtkMRMLViewLogic::GetCameraNode(scene, viewNode->GetLayoutName());
}

void vtkSlicerAstroVolumeLogic::SetBinaryLabelmapDisplay(vtkMRMLSegmentationNode* segmentationNode)
{
  if (!segmentationNode)
    {
    return;
    }
  const char* labelmapName = vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName();
  const char* surfaceName = vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName();
  vtkMRMLSegmentationDisplayNode* displayNode =
    vtkMRMLSegmentationDisplayNode::SafeDownCast(segmentationNode->GetDisplayNode());
  if (displayNode)
    {
    // "Show 3D" on: the binary labelmap is shown in the 3D views too
    displayNode->SetPreferredDisplayRepresentationName2D(labelmapName);
    displayNode->SetPreferredDisplayRepresentationName3D(labelmapName);
    displayNode->SetVisibility3D(true);
    }
  vtkSegmentation* segmentation = segmentationNode->GetSegmentation();
  if (segmentation && segmentation->GetSourceRepresentationName() != surfaceName &&
      segmentation->ContainsRepresentation(surfaceName))
    {
    segmentation->RemoveRepresentation(surfaceName);
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData)
{
  vtkMRMLSelectionNode* selectionNode = vtkMRMLSelectionNode::SafeDownCast(caller);
  if (selectionNode && event == vtkCommand::ModifiedEvent)
    {
    // measurements of the lines with the WCS of the new active volume
    const std::string activeVolumeID = selectionNode->GetActiveVolumeID() ? selectionNode->GetActiveVolumeID() : "";
    if (activeVolumeID != this->MeasurementsActiveVolumeID)
      {
      this->MeasurementsActiveVolumeID = activeVolumeID;
      this->UpdateAllAstroMeasurements();
      }
    return;
    }
  vtkMRMLVolumePropertyNode* volumePropertyNode = vtkMRMLVolumePropertyNode::SafeDownCast(caller);
  if (volumePropertyNode && event == vtkCommand::ModifiedEvent)
    {
    this->EnsureVolumePropertyTransferFunctionGaps(volumePropertyNode);
    return;
    }
  vtkMRMLAstroVolumeNode* astroVolume = vtkMRMLAstroVolumeNode::SafeDownCast(caller);
  if (astroVolume && (event == vtkMRMLNode::ReferenceAddedEvent || event == vtkMRMLNode::ReferenceModifiedEvent))
    {
    this->EnsureSegmentationsOfLoadedVolume(astroVolume);
    return;
    }
  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(caller);
  if (segmentationNode && (event == vtkMRMLNode::ReferenceAddedEvent ||
                           event == vtkMRMLNode::ReferenceModifiedEvent ||
                           event == vtkMRMLNode::ReferenceRemovedEvent))
    {
    // reference geometry set to an astronomical volume, or storage node removed
    if (this->GetMRMLScene() && !this->GetMRMLScene()->IsImporting())
      {
      vtkSlicerAstroVolumeLogic::NameMaskAfterReferenceVolume(segmentationNode);
      this->ReplaceEmptyMaskOfReferenceVolume(segmentationNode);
      bool fitsMask = this->EnsureFITSMaskStorageNode(segmentationNode);
      // display node created after the FITS storage (e.g. AddNewNodeByClass
      // followed by CreateDefaultDisplayNodes): binary labelmap display in 2D
      // and 3D. A representation already chosen is kept.
      vtkMRMLSegmentationDisplayNode* displayNode =
        vtkMRMLSegmentationDisplayNode::SafeDownCast(segmentationNode->GetDisplayNode());
      if (fitsMask && displayNode && !displayNode->GetPreferredDisplayRepresentationName3D())
        {
        vtkSlicerAstroVolumeLogic::SetBinaryLabelmapDisplay(segmentationNode);
        }
      }
    return;
    }
  this->Superclass::ProcessMRMLNodesEvents(caller, event, callData);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::HasGeometryOfVolume(vtkMRMLSegmentationNode* segmentationNode,
                                                    vtkMRMLVolumeNode* volumeNode)
{
  if (!segmentationNode || !segmentationNode->GetSegmentation() || !volumeNode || !volumeNode->GetImageData())
    {
    return false;
    }
  const std::string geometry = segmentationNode->GetSegmentation()->GetConversionParameter(
    vtkSegmentationConverter::GetReferenceImageGeometryParameterName());
  vtkNew<vtkOrientedImageData> maskGeometry;
  if (geometry.empty() || !vtkSegmentationConverter::DeserializeImageGeometry(geometry, maskGeometry, false))
    {
    return false;
    }
  int maskExtent[6] = { 0, -1, 0, -1, 0, -1 };
  maskGeometry->GetExtent(maskExtent);
  int volumeExtent[6] = { 0, -1, 0, -1, 0, -1 };
  volumeNode->GetImageData()->GetExtent(volumeExtent);
  for (int i = 0; i < 6; i++)
    {
    if (maskExtent[i] != volumeExtent[i])
      {
      return false;
      }
    }
  vtkNew<vtkMatrix4x4> maskToWorld;
  maskGeometry->GetImageToWorldMatrix(maskToWorld);
  vtkNew<vtkMatrix4x4> volumeToWorld;
  volumeNode->GetIJKToRASMatrix(volumeToWorld);
  for (int i = 0; i < 3; i++)
    {
    for (int j = 0; j < 4; j++)
      {
      const double a = maskToWorld->GetElement(i, j);
      const double b = volumeToWorld->GetElement(i, j);
      if (fabs(a - b) > 1.e-6 * std::max(1., std::max(fabs(a), fabs(b))))
        {
        return false;
        }
      }
    }
  return true;
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkSlicerAstroVolumeLogic::FindVolumeOfMask(vtkMRMLScene* scene,
                                                                   vtkMRMLSegmentationNode* segmentationNode)
{
  if (!scene || !vtkSlicerAstroVolumeLogic::IsMaskSegmentation(segmentationNode) ||
      segmentationNode->GetNodeReference(vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str()))
    {
    return nullptr;
    }
  std::vector<vtkMRMLNode*> volumes;
  scene->GetNodesByClass("vtkMRMLAstroVolumeNode", volumes);
  for (vtkMRMLNode* node : volumes)
    {
    vtkMRMLAstroVolumeNode* volumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(node);
    if (vtkSlicerAstroVolumeLogic::HasGeometryOfVolume(segmentationNode, volumeNode))
      {
      return volumeNode;
      }
    }
  return nullptr;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::ReplaceEmptyMaskOfReferenceVolume(vtkMRMLSegmentationNode* segmentationNode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene || scene->IsImporting() || scene->IsClosing() ||
      !vtkSlicerAstroVolumeLogic::IsMaskSegmentation(segmentationNode))
    {
    return false;
    }
  vtkMRMLAstroVolumeNode* volumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(segmentationNode->GetNodeReference(
    vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str()));
  if (!volumeNode)
    {
    return false;
    }
  const char* maskRole = vtkSlicerAstroVolumeLogic::GetMaskReferenceRole();
  vtkMRMLSegmentationNode* currentMask = vtkMRMLSegmentationNode::SafeDownCast(volumeNode->GetNodeReference(maskRole));
  if (!currentMask || currentMask == segmentationNode)
    {
    return false;
    }
  // the loaded (or filled) mask: segments or a file; the automatic mask:
  // empty and never saved
  vtkMRMLStorageNode* storageNode = segmentationNode->GetStorageNode();
  const bool hasData = segmentationNode->GetSegmentation()->GetNumberOfSegments() > 0 ||
                       (storageNode && storageNode->GetFileName() && *storageNode->GetFileName());
  vtkMRMLStorageNode* currentStorage = currentMask->GetStorageNode();
  const bool currentEmpty = currentMask->GetSegmentation()->GetNumberOfSegments() == 0 &&
                            !(currentStorage && currentStorage->GetFileName() && *currentStorage->GetFileName());
  if (!hasData || !currentEmpty)
    {
    return false;
    }
  volumeNode->SetNodeReferenceID(maskRole, segmentationNode->GetID());
  scene->RemoveNode(currentMask);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::IsContoursSegmentation(vtkMRMLSegmentationNode* segmentationNode)
{
  if (!segmentationNode)
    {
    return false;
    }
  const char* contours = segmentationNode->GetAttribute(vtkSlicerAstroVolumeLogic::GetContoursAttributeName());
  if (contours && !strcmp(contours, "true"))
    {
    return true;
    }
  // contours of the AstroVolume, AstroSmoothing and AstroModeling modules in
  // scenes saved without the attribute
  const char* name = segmentationNode->GetName();
  return name && (!strcmp(name, "AstroContoursSeg") || !strcmp(name, "AstroSmoothingContoursSeg") ||
                  !strcmp(name, "AstroModelingContoursSeg"));
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::PrepareContoursUpdate(vtkMRMLSegmentationNode* segmentationNode)
{
  if (!segmentationNode || !segmentationNode->GetSegmentation())
    {
    return;
    }
  segmentationNode->SetAttribute(vtkSlicerAstroVolumeLogic::GetContoursAttributeName(), "true");
  vtkSegmentation* segmentation = segmentationNode->GetSegmentation();
  const char* labelmapName = vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName();
  segmentation->CreateRepresentation(labelmapName);
  segmentation->SetSourceRepresentationName(labelmapName);
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SetContoursDisplay(vtkMRMLSegmentationNode* segmentationNode)
{
  if (!segmentationNode || !segmentationNode->GetSegmentation())
    {
    return;
    }
  segmentationNode->SetAttribute(vtkSlicerAstroVolumeLogic::GetContoursAttributeName(), "true");
  vtkSegmentation* segmentation = segmentationNode->GetSegmentation();
  const char* labelmapName = vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName();
  const char* surfaceName = vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName();
  if (segmentation->GetSourceRepresentationName() != labelmapName)
    {
    segmentation->CreateRepresentation(labelmapName);
    segmentation->SetSourceRepresentationName(labelmapName);
    }
  // no closed surfaces by default (slow for large data): see SetContoursSmooth2D
  if (segmentation->ContainsRepresentation(surfaceName))
    {
    segmentation->RemoveRepresentation(surfaceName);
    }
  vtkMRMLSegmentationDisplayNode* displayNode =
    vtkMRMLSegmentationDisplayNode::SafeDownCast(segmentationNode->GetDisplayNode());
  if (displayNode)
    {
    displayNode->SetPreferredDisplayRepresentationName2D(labelmapName);
    displayNode->SetPreferredDisplayRepresentationName3D(labelmapName);
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SetContoursSmooth2D(vtkMRMLSegmentationNode* segmentationNode, bool smooth)
{
  vtkMRMLSegmentationDisplayNode* displayNode = segmentationNode ?
    vtkMRMLSegmentationDisplayNode::SafeDownCast(segmentationNode->GetDisplayNode()) : nullptr;
  if (!displayNode || !segmentationNode->GetSegmentation())
    {
    return;
    }
  const char* labelmapName = vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName();
  const char* surfaceName = vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName();
  const bool hasSurfaces = segmentationNode->GetSegmentation()->ContainsRepresentation(surfaceName);
  displayNode->SetPreferredDisplayRepresentationName2D(smooth && hasSurfaces ? surfaceName : labelmapName);
}

//----------------------------------------------------------------------------
namespace
{
template <typename T>
bool ContainsLabel(const T* values, vtkIdType numberOfValues, int label)
{
  const T labelValue = static_cast<T>(label);
  for (vtkIdType index = 0; index < numberOfValues; ++index)
    {
    if (values[index] == labelValue)
      {
      return true;
      }
    }
  return false;
}
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::HasNonEmptySegments(vtkMRMLSegmentationNode* segmentationNode)
{
  vtkSegmentation* segmentation = segmentationNode ? segmentationNode->GetSegmentation() : nullptr;
  if (!segmentation)
    {
    return false;
    }
  const char* labelmapName = vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName();
  const char* surfaceName = vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName();
  std::vector<std::string> segmentIDs;
  segmentation->GetSegmentIDs(segmentIDs);
  for (const std::string& segmentID : segmentIDs)
    {
    vtkSegment* segment = segmentation->GetSegment(segmentID);
    if (!segment)
      {
      continue;
      }
    vtkOrientedImageData* labelmap = vtkOrientedImageData::SafeDownCast(segment->GetRepresentation(labelmapName));
    vtkDataArray* scalars = labelmap && labelmap->GetPointData() ? labelmap->GetPointData()->GetScalars() : nullptr;
    if (scalars && scalars->GetNumberOfTuples() > 0)
      {
      const vtkIdType numberOfValues = scalars->GetNumberOfTuples() * scalars->GetNumberOfComponents();
      bool found = false;
      switch (scalars->GetDataType())
        {
        vtkTemplateMacro(found = ContainsLabel(static_cast<const VTK_TT*>(scalars->GetVoidPointer(0)),
                                               numberOfValues, segment->GetLabelValue()));
        }
      if (found)
        {
        return true;
        }
      continue;
      }
    vtkPolyData* surface = vtkPolyData::SafeDownCast(segment->GetRepresentation(surfaceName));
    if (surface && surface->GetNumberOfPoints() > 0)
      {
      return true;
      }
    }
  return false;
}
