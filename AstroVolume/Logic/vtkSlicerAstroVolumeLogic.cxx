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
#include <type_traits>
#include <vector>

// Slicer includes
#include <vtkSlicerVolumesLogic.h>

// STD includes
#include <array>

// AstroVolume includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroConfigure.h>
#include <vtkSlicerAstroSMPTools.h>

// MRML nodes includes
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

// OpenMP includes

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

  std::vector<vtkMRMLNode*> segmentationNodes;
  this->GetMRMLScene()->GetNodesByClass("vtkMRMLSegmentationNode", segmentationNodes);
  for (vtkMRMLNode* node : segmentationNodes)
    {
    this->EnsureFITSMaskStorageNode(vtkMRMLSegmentationNode::SafeDownCast(node));
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

    vtkMRMLSliceNode* sliceNodeRed = vtkMRMLSliceNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
    if (sliceNodeRed && !this->GetMRMLScene()->IsImporting())
      {
      sliceNodeRed->SetOrientation("XY");
      }

    vtkMRMLSliceNode* sliceNodeYellow = vtkMRMLSliceNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
    if (sliceNodeYellow && !this->GetMRMLScene()->IsImporting())
      {
      sliceNodeYellow->SetOrientation("XZ");
      }

    vtkMRMLSliceNode* sliceNodeGreen = vtkMRMLSliceNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
    if (sliceNodeGreen && !this->GetMRMLScene()->IsImporting())
      {
      sliceNodeGreen->SetOrientation("ZY");
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

  outputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
     (this->CloneVolume(scene, inputVolume, name, cloneImageData));

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
bool vtkSlicerAstroVolumeLogic::Reproject(vtkMRMLAstroReprojectParametersNode *pnode)
{
  if (!this->GetMRMLScene())
    {
    return false;
    }

  if (!pnode)
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "parameterNode not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if (!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "inputVolume not found.");
    return false;
    }

  if (!strcmp(inputVolume->GetAttribute("SlicerAstro.DSS"), "1") &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CDELT1"), "1.0") &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CDELT2"), "1.0"))
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "input volume has WCS generated from old (and not accepted) PPO, AMDX, AMDY matrices. "
                  "It is not possible to continue with the reprojection.");
    return false;
    }

  vtkMRMLAstroVolumeDisplayNode *inputVolumeDisplay =
    inputVolume->GetAstroVolumeDisplayNode();
  if (!inputVolumeDisplay || !inputVolumeDisplay->GetWCSStruct())
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "inputVolumeDisplay not found.");
    return false;
    }

  if (!strcmp(inputVolumeDisplay->GetSpace(), "IJK"))
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "inputVolumeDisplay has not WCS.");
    return false;
    }

  vtkMRMLAstroVolumeNode *referenceVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetReferenceVolumeNodeID()));
  if (!referenceVolume || !referenceVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "referenceVolume not found.");
    return false;
    }

  if (!strcmp(referenceVolume->GetAttribute("SlicerAstro.DSS"), "1") &&
      !strcmp(referenceVolume->GetAttribute("SlicerAstro.CDELT1"), "1.0") &&
      !strcmp(referenceVolume->GetAttribute("SlicerAstro.CDELT2"), "1.0"))
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "reference volume has WCS generated from old (and not accepted) PPO, AMDX, AMDY matrices. "
                  "It is not possible to continue with the reprojection.");
    return false;
    }

  vtkMRMLAstroVolumeDisplayNode *referenceVolumeDisplay =
    referenceVolume->GetAstroVolumeDisplayNode();
  if (!referenceVolumeDisplay || !referenceVolumeDisplay->GetWCSStruct())
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "referenceVolumeDisplay not found.");
    return false;
    }

  if (!strcmp(referenceVolumeDisplay->GetSpace(), "IJK"))
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "referenceVolumeDisplay has not WCS.");
    return false;
    }

  // Check if the images dimensionality
  int nInput = inputVolumeDisplay->GetWCSStruct()->naxis;
  int nReference = referenceVolumeDisplay->GetWCSStruct()->naxis;
  if (nInput != 2 && nInput != 3 && nReference != 2 && nReference != 3)
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "the dimensionalities of the input and reference data has to be 2 or 3.");
    return false;
    }

  // Check that the input volume has not already been reprojected to the reference volume
  if (!pnode->GetReprojectRotation() &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CDELT1"), referenceVolume->GetAttribute("SlicerAstro.CDELT1")) &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CDELT2"), referenceVolume->GetAttribute("SlicerAstro.CDELT2")) &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CROTA1"), referenceVolume->GetAttribute("SlicerAstro.CROTA1")) &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CROTA2"), referenceVolume->GetAttribute("SlicerAstro.CROTA2")) &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CRPIX1"), referenceVolume->GetAttribute("SlicerAstro.CRPIX1")) &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CRPIX2"), referenceVolume->GetAttribute("SlicerAstro.CRPIX2")) &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CRVAL1"), referenceVolume->GetAttribute("SlicerAstro.CRVAL1")) &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CRVAL2"), referenceVolume->GetAttribute("SlicerAstro.CRVAL2")) &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CTYPE1"), referenceVolume->GetAttribute("SlicerAstro.CTYPE1")) &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CTYPE2"), referenceVolume->GetAttribute("SlicerAstro.CTYPE2")) &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CUNIT1"), referenceVolume->GetAttribute("SlicerAstro.CUNIT1")) &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.CUNIT2"), referenceVolume->GetAttribute("SlicerAstro.CUNIT2")) &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.NAXIS1"), referenceVolume->GetAttribute("SlicerAstro.NAXIS1")) &&
      !strcmp(inputVolume->GetAttribute("SlicerAstro.NAXIS2"), referenceVolume->GetAttribute("SlicerAstro.NAXIS2")))
  {
  vtkWarningMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "the input and reference volume keywords relative to the WCS coordinates match. "
                  "No reprojection is needed.");
  return false;
  }

  // Check that the input volume has not already been reprojected to the reference volume
  if (pnode->GetReprojectRotation() &&
      StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA2")) < 1.E-6)
  {
  vtkWarningMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "the input volume is already aligned to the north pole. "
                  "No reprojection is needed.");
  return false;
  }

  // Check if the images need rotation or change of equinox
  bool needToPreRotateInput = false;      
  if (fabs(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA2"))) > 1.E-6 &&
      !pnode->GetReprojectRotation())
    {
    needToPreRotateInput = true;
    }

  bool needToPreRotateReference = false;
  if (fabs(StringToDouble(referenceVolume->GetAttribute("SlicerAstro.CROTA2"))) > 1.E-6 &&
      !pnode->GetReprojectRotation())
    {
    needToPreRotateReference = true;
    }

  bool needToPreReprojectCelestialSystem = false;
  if (fabs(inputVolumeDisplay->GetWCSStruct()->equinox - referenceVolumeDisplay->GetWCSStruct()->equinox) > 1.E-6)
    {
    needToPreReprojectCelestialSystem = true;
    }

  if (needToPreRotateInput || needToPreRotateReference ||
      needToPreReprojectCelestialSystem)
    {
    if (needToPreRotateInput)
      {
      vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                    "the input volume is rotated respect to the north pole. "
                    "Please create a new reprojected (north aligned) volume in the reprojection module.");
      }

    if (needToPreRotateReference)
      {
      vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                    "the reference volume is rotated respect to the north pole. "
                    "Please create a new reprojected (north aligned) volume in the reprojection module.");
      }

    if (needToPreReprojectCelestialSystem)
      {
      vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                    "the two datasets have different equinox/radesys. "
                    "Please check that they are consistent (e.g., both B1950 or J2000). "
                    "SlicerAstro does not provide tools to convert system of reference.");
      }

    return false;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if (!outputVolume || !outputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "outputVolume not found.");
    return false;
    }

  const int *inputDims = inputVolume->GetImageData()->GetDimensions();
  const int inputSliceDim = inputDims[0] * inputDims[1];
  const int *referenceDims = referenceVolume->GetImageData()->GetDimensions();
  const int referenceLengthX = referenceDims[0];
  const int referenceLengthY = referenceDims[1];
  const int referenceSliceDim = referenceDims[0] * referenceDims[1];
  const int inputNumComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int referenceNumComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (inputNumComponents > 1 || referenceNumComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "imageData with more than one components.");
    return false;
    }

  // Create empty data of spatial dimensions equal to the reference data
  // and of velocity dimension equal to the input volume
  // (the reprojection takes place only for the spatial axis)
  vtkNew<vtkImageData> imageDataTemp;
  imageDataTemp->SetDimensions(referenceLengthX, referenceLengthY, inputDims[2]);
  imageDataTemp->SetSpacing(1.,1.,1.);
  imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);

  // copy data into the Astro Volume object
  outputVolume->SetAndObserveImageData(imageDataTemp.GetPointer());

  float *inFPixel = nullptr;
  float *outFPixel = nullptr;
  double *inDPixel = nullptr;
  double *outDPixel = nullptr;
  const int DataType = inputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return false;
    }

  bool cancel = false;
  int status = 0;
  const double NaN = sqrt(-1);


  std::chrono::steady_clock::time_point start, end;

  long mtime = 0;

  start = std::chrono::steady_clock::now();

  pnode->SetStatus(1);

  // Calculate the 2D interpolation grid
  std::vector<std::vector<std::vector<double> > > referenceGrid;
  referenceGrid.resize(referenceLengthX);
  for (int jj = 0; jj < referenceLengthX; jj++)
    {
    referenceGrid[jj].resize(referenceLengthY);
    for (int kk = 0; kk < referenceLengthY; kk++)
      {
      referenceGrid[jj][kk].resize(2);
      }
    }

  double shift = 0.5;
  if (pnode->GetInterpolationOrder() == vtkMRMLAstroReprojectParametersNode::NearestNeighbour)
    {
    shift = 0.;
    }
  // WCS are not thread safe, no OpenMP

  bool overlay = false;
  for (int ii = 0; ii < referenceLengthX; ii++)
    {  
    for (int jj = 0; jj < referenceLengthY; jj++)
      {
      double ijk[3] = {0.}, world[3] = {0.};
      ijk[0] = ii + shift;
      ijk[1] = jj + shift;

      referenceVolumeDisplay->GetReferenceSpace(ijk, world);
      inputVolumeDisplay->GetIJKSpace(world, ijk);

      referenceGrid[ii][jj][0] = ijk[0];
      referenceGrid[ii][jj][1] = ijk[1];

      if (!overlay)
        {
        if (ijk[0] > 0 && ijk[0] < inputDims[0] &&
            ijk[1] > 0 && ijk[1] < inputDims[1])
          {
          overlay = true;
          }
        }
      }

    if(ii / (referenceLengthX / 10.) > status)
      {
      status += 1.;
      pnode->SetStatus(status);
      }
    }

  pnode->SetStatus(10);

  if (!overlay)
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "datasets WCS have no overlay");
    return false;
    }

  // Interpolate
  int numElements = referenceLengthX * referenceLengthY * inputDims[2];

  if (pnode->GetInterpolationOrder() == vtkMRMLAstroReprojectParametersNode::NearestNeighbour)
    {
    {
    vtkSlicerAstroSMPTools::ProgressReporter<vtkMRMLAstroReprojectParametersNode> progress(pnode, (numElements) - (0), 10, 100);
    vtkSMPTools::For(0, numElements, [&](vtkIdType firstIndex, vtkIdType lastIndex)
      {
      if (progress.IsCancelled())
        {
        return;
        }
      for (vtkIdType elemCnt = firstIndex; elemCnt < lastIndex; elemCnt++)
        {
        int ref  = (int) floor(elemCnt / referenceLengthX);
        ref *= referenceLengthX;
        int ii = elemCnt - ref;

        ref = (int) floor(elemCnt / referenceSliceDim);
        int kk = ref;
        ref *= referenceSliceDim;
        ref = elemCnt - ref;
        int jj = (int) floor(ref / referenceLengthX);

        int x = (int) round(referenceGrid[ii][jj][0]);
        bool xInside = x > 0 && x < inputDims[0];

        int y = (int) round(referenceGrid[ii][jj][1]);
        bool yInside = y > 0 && y < inputDims[1];

        if (!xInside || !yInside)
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              *(outFPixel + elemCnt) = NaN;
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) = NaN;
              break;
            }
          continue;
          }

        switch (DataType)
          {
         case VTK_FLOAT:
           *(outFPixel + elemCnt) = *(inFPixel + inputSliceDim * kk + inputDims[0] * y + x);
           break;
         case VTK_DOUBLE:
           *(outDPixel + elemCnt) = *(inDPixel + inputSliceDim * kk + inputDims[0] * y + x);
           break;
          }
        }
      progress.Done(lastIndex - firstIndex);
      });
    cancel = progress.IsCancelled();
    }
    }
  else if (pnode->GetInterpolationOrder() == vtkMRMLAstroReprojectParametersNode::Bilinear)
    {
    {
    vtkSlicerAstroSMPTools::ProgressReporter<vtkMRMLAstroReprojectParametersNode> progress(pnode, (numElements) - (0), 10, 100);
    vtkSMPTools::For(0, numElements, [&](vtkIdType firstIndex, vtkIdType lastIndex)
      {
      if (progress.IsCancelled())
        {
        return;
        }
      for (vtkIdType elemCnt = firstIndex; elemCnt < lastIndex; elemCnt++)
        {
        int ref  = (int) floor(elemCnt / referenceLengthX);
        ref *= referenceLengthX;
        int ii = elemCnt - ref;

        ref = (int) floor(elemCnt / referenceSliceDim);
        int kk = ref;
        ref *= referenceSliceDim;
        ref = elemCnt - ref;
        int jj = (int) floor(ref / referenceLengthX);

        double x = referenceGrid[ii][jj][0];
        int x1 = (int) floor(x);
        bool x1Inside = x1 > 0 && x1 < inputDims[0];
        int x2 = (int) ceil(x);
        bool x2Inside = x2 > 0 && x2 < inputDims[0];

        double y = referenceGrid[ii][jj][1];
        int y1 = (int) floor(y);
        bool y1Inside = y1 > 0 && y1 < inputDims[1];
        int y2 = (int) ceil(y);
        bool y2Inside = y2 > 0 && y2 < inputDims[1];

        if ((!x1Inside && !x2Inside) || (!y1Inside && !y2Inside))
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              *(outFPixel + elemCnt) = NaN;
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) = NaN;
              break;
            }
          continue;
          }

        double F11 = 0., F12 = 0., F21 = 0., F22 = 0.;

        if (!x1Inside || !y1Inside)
          {
          F11 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F11 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y1 + x1);
              break;
            case VTK_DOUBLE:
              F11 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y1 + x1);
              break;
            }
          }

        if (!x2Inside || !y1Inside)
          {
          F21 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F21 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y1 + x2);
              break;
            case VTK_DOUBLE:
              F21 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y1 + x2);
              break;
            }
          }

        if (!x1Inside || !y2Inside)
          {
          F12 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F12 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y2 + x1);
              break;
            case VTK_DOUBLE:
              F12 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y2 + x1);
              break;
            }
          }

        if (!x2Inside || !y2Inside)
          {
          F22 = NaN;
          }
        else
          {
          switch (DataType)
            {
           case VTK_FLOAT:
             F22 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y2 + x2);
             break;
           case VTK_DOUBLE:
             F22 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y2 + x2);
             break;
            }
          }

        double deltax = 1. / (x2 - x1);
        double x2x = (x2 - x);
        double xx1 = (x - x1) ;
        double x2xdeltax = x2x * deltax;
        double xx1deltax = xx1 * deltax;
        double F1 = x2xdeltax * F11 + xx1deltax * F21;
        double F2 = x2xdeltax * F12 + xx1deltax * F22;
        double deltay = 1. / (y2 - y1);

        switch (DataType)
          {
         case VTK_FLOAT:
           *(outFPixel + elemCnt) =  (y2 - y) * deltay * F1 + (y - y1) * deltay * F2;
           break;
         case VTK_DOUBLE:
           *(outDPixel + elemCnt) =  (y2 - y) * deltay * F1 + (y - y1) * deltay * F2;
           break;
          }
        }
      progress.Done(lastIndex - firstIndex);
      });
    cancel = progress.IsCancelled();
    }
    }
  else if (pnode->GetInterpolationOrder() == vtkMRMLAstroReprojectParametersNode::Bicubic)
    {
    {
    vtkSlicerAstroSMPTools::ProgressReporter<vtkMRMLAstroReprojectParametersNode> progress(pnode, (numElements) - (0), 10, 100);
    vtkSMPTools::For(0, numElements, [&](vtkIdType firstIndex, vtkIdType lastIndex)
      {
      if (progress.IsCancelled())
        {
        return;
        }
      for (vtkIdType elemCnt = firstIndex; elemCnt < lastIndex; elemCnt++)
        {
        int ref  = (int) floor(elemCnt / referenceLengthX);
        ref *= referenceLengthX;
        int ii = elemCnt - ref;

        ref = (int) floor(elemCnt / referenceSliceDim);
        int kk = ref;
        ref *= referenceSliceDim;
        ref = elemCnt - ref;
        int jj = (int) floor(ref / referenceLengthX);

        double x = referenceGrid[ii][jj][0];
        int x1 = (int) floor(x) - 1;
        bool x1Inside = x1 > 0 && x1 < inputDims[0];
        int x2 = (int) floor(x);
        bool x2Inside = x2 > 0 && x2 < inputDims[0];
        int x3 = (int) ceil(x);
        bool x3Inside = x3 > 0 && x3 < inputDims[0];
        int x4 = (int) ceil(x) + 1;
        bool x4Inside = x4 > 0 && x4 < inputDims[0];

        double y = referenceGrid[ii][jj][1];
        int y1 = (int) floor(y) - 1;
        bool y1Inside = y1 > 0 && y1 < inputDims[1];
        int y2 = (int) floor(y);
        bool y2Inside = y2 > 0 && y2 < inputDims[1];
        int y3 = (int) ceil(y);
        bool y3Inside = y3 > 0 && y3 < inputDims[1];
        int y4 = (int) ceil(y) + 1;
        bool y4Inside = y4 > 0 && y4 < inputDims[1];

        if ((!x1Inside && !x4Inside) || (!y1Inside && !y4Inside))
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              *(outFPixel + elemCnt) = NaN;
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) = NaN;
              break;
            }
          continue;
          }

        double F11 = 0., F12 = 0., F13 = 0., F14 = 0.;
        double F21 = 0., F22 = 0., F23 = 0., F24 = 0.;
        double F31 = 0., F32 = 0., F33 = 0., F34 = 0.;
        double F41 = 0., F42 = 0., F43 = 0., F44 = 0.;
        if (!x1Inside || !y1Inside)
          {
          F11 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F11 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y1 + x1);
              break;
            case VTK_DOUBLE:
              F11 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y1 + x1);
              break;
            }
          }

        if (!x2Inside || !y1Inside)
          {
          F21 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F21 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y1 + x2);
              break;
            case VTK_DOUBLE:
              F21 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y1 + x2);
              break;
            }
          }

        if (!x3Inside || !y1Inside)
          {
          F31 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F31 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y1 + x3);
              break;
            case VTK_DOUBLE:
              F31 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y1 + x3);
              break;
            }
          }

        if (!x4Inside || !y1Inside)
          {
          F41 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F41 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y1 + x4);
              break;
            case VTK_DOUBLE:
              F41 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y1 + x4);
              break;
            }
          }

        if (!x1Inside || !y2Inside)
          {
          F12 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F12 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y2 + x1);
              break;
            case VTK_DOUBLE:
              F12 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y2 + x1);
              break;
            }
          }

        if (!x2Inside || !y2Inside)
          {
          F22 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F22 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y2 + x2);
              break;
            case VTK_DOUBLE:
              F22 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y2 + x2);
              break;
            }
          }

        if (!x3Inside || !y2Inside)
          {
          F32 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F32 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y2 + x3);
              break;
            case VTK_DOUBLE:
              F32 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y2 + x3);
              break;
            }
          }

        if (!x4Inside || !y2Inside)
          {
          F42 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F42 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y2 + x4);
              break;
            case VTK_DOUBLE:
              F42 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y2 + x4);
              break;
            }
          }

        if (!x1Inside || !y3Inside)
          {
          F13 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F13 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y3 + x1);
              break;
            case VTK_DOUBLE:
              F13 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y3 + x1);
              break;
            }
          }

        if (!x2Inside || !y3Inside)
          {
          F23 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F23 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y3 + x2);
              break;
            case VTK_DOUBLE:
              F23 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y3 + x2);
              break;
            }
          }

        if (!x3Inside || !y3Inside)
          {
          F33 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F33 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y3 + x3);
              break;
            case VTK_DOUBLE:
              F33 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y3 + x3);
              break;
            }
          }

        if (!x4Inside || !y3Inside)
          {
          F43 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F43 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y3 + x4);
              break;
            case VTK_DOUBLE:
              F43 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y3 + x4);
              break;
            }
          }

        if (!x1Inside || !y4Inside)
          {
          F14 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F14 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y4 + x1);
              break;
            case VTK_DOUBLE:
              F14 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y4 + x1);
              break;
            }
          }

        if (!x2Inside || !y4Inside)
          {
          F24 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F24 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y4 + x2);
              break;
            case VTK_DOUBLE:
              F24 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y4 + x2);
              break;
            }
          }

        if (!x3Inside || !y4Inside)
          {
          F34 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F34 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y4 + x3);
              break;
            case VTK_DOUBLE:
              F34 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y4 + x3);
              break;
            }
          }

        if (!x4Inside || !y4Inside)
          {
          F44 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F44 = *(inFPixel + inputSliceDim * kk + inputDims[0] * y4 + x4);
              break;
            case VTK_DOUBLE:
              F44 = *(inDPixel + inputSliceDim * kk + inputDims[0] * y4 + x4);
              break;
            }
          }

        double deltaY = y - y2;

        double F1 = F12 + 0.5 * deltaY * (F13 - F11 + deltaY * (2. * F11 - 5. * F12 + 4. * F13 - F14 + deltaY * (3. * (F12 - F13) + F14 - F11)));
        double F2 = F22 + 0.5 * deltaY * (F23 - F21 + deltaY * (2. * F21 - 5. * F22 + 4. * F23 - F24 + deltaY * (3. * (F22 - F23) + F24 - F21)));
        double F3 = F32 + 0.5 * deltaY * (F33 - F31 + deltaY * (2. * F31 - 5. * F32 + 4. * F33 - F34 + deltaY * (3. * (F32 - F33) + F34 - F31)));
        double F4 = F42 + 0.5 * deltaY * (F43 - F41 + deltaY * (2. * F41 - 5. * F42 + 4. * F43 - F44 + deltaY * (3. * (F42 - F43) + F44 - F41)));

        double deltaX = x - x2;
        double F = F2 + 0.5 * deltaX * (F3 - F1 + deltaX * (2. * F1 - 5. * F2 + 4. * F3 - F4 + deltaX * (3. * (F2 - F3) + F4 - F1)));

        switch (DataType)
          {
         case VTK_FLOAT:
           *(outFPixel + elemCnt) = F;
           break;
         case VTK_DOUBLE:
           *(outDPixel + elemCnt) = F;
           break;
          }
        }
      progress.Done(lastIndex - firstIndex);
      });
    cancel = progress.IsCancelled();
    }
    }

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
  vtkDebugMacro("Reprojection Time : "<<mtime<<" ms.");

  inFPixel = nullptr;
  outFPixel = nullptr;
  inDPixel = nullptr;
  outDPixel = nullptr;

  delete inFPixel;
  delete outFPixel;
  delete inDPixel;
  delete outDPixel;

  referenceGrid.clear();
  referenceGrid.shrink_to_fit();

  if (cancel)
    {
    pnode->SetStatus(100);
    return false;
    }

  start = std::chrono::steady_clock::now();

  int wasModifying = outputVolume->StartModify();
  this->CenterVolume(outputVolume);
  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateDisplayThresholdAttributes();
  outputVolume->EndModify(wasModifying);

  this->CenterVolume(referenceVolume);

  pnode->SetStatus(100);

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

  vtkDebugMacro("Update Time : "<<mtime<<" ms.");

  return true;
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

  for(int i = 0; i < presets->GetNumberOfItems(); i++)
    {
    vtkMRMLVolumePropertyNode* volumePropertyNode =
        vtkMRMLVolumePropertyNode::SafeDownCast(presets->GetItemAsObject(i));
    if (volumePropertyNode)
      {
      this->GetMRMLScene()->AddNode(volumePropertyNode);
      }
    }

  return true;
}

//----------------------------------------------------------------------------
namespace
{
// Positions of the points shifted so that consecutive points are at least
// minimumGap apart; the points are moved toward the inside of the range
// (first and last points stay at their place when possible).
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
  return referenceNode && referenceNode->IsA("vtkMRMLAstroVolumeNode");
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
    displayNode->SetPreferredDisplayRepresentationName2D(labelmapName);
    displayNode->SetPreferredDisplayRepresentationName3D(labelmapName);
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
  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(caller);
  if (segmentationNode && (event == vtkMRMLNode::ReferenceAddedEvent ||
                           event == vtkMRMLNode::ReferenceModifiedEvent ||
                           event == vtkMRMLNode::ReferenceRemovedEvent))
    {
    // reference geometry set to an astronomical volume, or storage node removed
    if (this->GetMRMLScene() && !this->GetMRMLScene()->IsImporting())
      {
      this->EnsureFITSMaskStorageNode(segmentationNode);
      }
    return;
    }
  this->Superclass::ProcessMRMLNodesEvents(caller, event, callData);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::IsContoursSegmentation(vtkMRMLSegmentationNode* segmentationNode)
{
  const char* contours = segmentationNode ?
    segmentationNode->GetAttribute(vtkSlicerAstroVolumeLogic::GetContoursAttributeName()) : nullptr;
  return contours && !strcmp(contours, "true");
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
void vtkSlicerAstroVolumeLogic::SetClosedSurfaceContours(vtkMRMLSegmentationNode* segmentationNode)
{
  if (!segmentationNode || !segmentationNode->GetSegmentation())
    {
    return;
    }
  segmentationNode->SetAttribute(vtkSlicerAstroVolumeLogic::GetContoursAttributeName(), "true");
  vtkSegmentation* segmentation = segmentationNode->GetSegmentation();
  const char* labelmapName = vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName();
  const char* surfaceName = vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName();
  segmentation->CreateRepresentation(surfaceName);
  segmentation->SetSourceRepresentationName(surfaceName);
  segmentation->RemoveRepresentation(labelmapName);
  vtkMRMLSegmentationDisplayNode* displayNode =
    vtkMRMLSegmentationDisplayNode::SafeDownCast(segmentationNode->GetDisplayNode());
  if (displayNode)
    {
    displayNode->SetPreferredDisplayRepresentationName2D(surfaceName);
    displayNode->SetPreferredDisplayRepresentationName3D(surfaceName);
    }
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
