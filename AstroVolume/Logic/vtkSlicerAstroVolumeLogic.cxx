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

// Slicer includes
#include <vtkSlicerVolumesLogic.h>

// AstroVolume includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroConfigure.h>

// MRML nodes includes
#include <vtkMRMLAnnotationROINode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroReprojectParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLCameraNode.h>
#include <vtkMRMLColorTableNode.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLNode.h>
#include <vtkMRMLPlotChartNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
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

//VTK includes
#include <vtkAddonMathUtilities.h>
#include <vtkBoundingBox.h>
#include <vtkCacheManager.h>
#include <vtkCollection.h>
#include <vtkColorTransferFunction.h>
#include <vtkGeneralTransform.h>
#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkSegment.h>
#include <vtkSmartPointer.h>

// WCS includes
#include "wcslib.h"

// OpenMP includes
#ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
#include <omp.h>
#endif

#include <iostream>
#include <sys/time.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroVolumeLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic::vtkSlicerAstroVolumeLogic()
{
  this->PresetsScene = 0;
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

//----------------------------------------------------------------------------
bool FloatIsNaN(float Value)
{
  return isNaN<float>(Value);
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
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::OnMRMLSceneEndImport()
{
  // set Slice Default Node
  vtkSmartPointer<vtkMRMLNode> defaultNode = vtkMRMLSliceNode::SafeDownCast
      (this->GetMRMLScene()->GetDefaultNodeByClass("vtkMRMLSliceNode"));
  if (!defaultNode)
    {
    vtkMRMLNode * foo = this->GetMRMLScene()->CreateNodeByClass("vtkMRMLSliceNode");
    defaultNode.TakeReference(foo);
    this->GetMRMLScene()->AddDefaultNode(defaultNode);
    }
  vtkMRMLSliceNode * defaultSliceNode = vtkMRMLSliceNode::SafeDownCast(defaultNode);
  defaultSliceNode->RemoveSliceOrientationPreset("Axial");
  defaultSliceNode->RemoveSliceOrientationPreset("Sagittal");
  defaultSliceNode->RemoveSliceOrientationPreset("Coronal");

  // modify SliceNodes already allocated
  vtkSmartPointer<vtkCollection> sliceNodes = vtkSmartPointer<vtkCollection>::Take
      (this->GetMRMLScene()->GetNodesByClass("vtkMRMLSliceNode"));

  for(int i = 0; i < sliceNodes->GetNumberOfItems(); i++)
    {
    vtkMRMLSliceNode* sliceNode =
        vtkMRMLSliceNode::SafeDownCast(sliceNodes->GetItemAsObject(i));
    if (sliceNode)
      {
      sliceNode->DisableModifiedEventOn();
      sliceNode->RemoveSliceOrientationPreset("Axial");
      sliceNode->RemoveSliceOrientationPreset("Sagittal");
      sliceNode->RemoveSliceOrientationPreset("Coronal");
      sliceNode->DisableModifiedEventOff();
      }
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
vtkMRMLAstroLabelMapVolumeNode *vtkSlicerAstroVolumeLogic::CreateAndAddLabelVolume(vtkMRMLScene *scene,
                                                                                   vtkMRMLAstroVolumeNode *volumeNode,
                                                                                   const char *name)
{
  if (!scene || !volumeNode || !name)
    {
    return NULL;
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
    return NULL;
    }
  labelWCS->flag=-1;

  labelDisplayNode->SetWCSStatus(wcscopy(1, volumeWCS, labelWCS));
  if (labelDisplayNode->GetWCSStatus())
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::CreateLabelVolumeFromVolume : "
                  "wcscopy ERROR "<<labelDisplayNode->GetWCSStatus()<<":\n"<<
                  "Message from "<<labelWCS->err->function<<
                  "at line "<<labelWCS->err->line_no<<" of file "<<labelWCS->err->file<<
                  ": \n"<<labelWCS->err->msg<<"\n");
    return NULL;
    }

  labelDisplayNode->SetWCSStatus(wcsset(labelWCS));
  if (labelDisplayNode->GetWCSStatus())
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::CreateLabelVolumeFromVolume :"
                  "wcsset ERROR "<<labelDisplayNode->GetWCSStatus()<<":\n"<<
                  "Message from "<<labelWCS->err->function<<
                  "at line "<<labelWCS->err->line_no<<" of file "<<labelWCS->err->file<<
                  ": \n"<<labelWCS->err->msg<<"\n");
    return NULL;
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
  labelNode->SetAndObserveStorageNodeID(NULL);
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
bool vtkSlicerAstroVolumeLogic::FitROIToInputVolume(vtkMRMLAnnotationROINode *roiNode, vtkMRMLAstroVolumeNode *inputVolume)
{
  if (!roiNode || !inputVolume)
    {
    return false;
    }

  vtkMRMLTransformNode* roiTransform = roiNode->GetParentTransformNode();
  if (roiTransform && !roiTransform->IsTransformToWorldLinear())
    {
    roiTransform = NULL;
    roiNode->SetAndObserveTransformNodeID(NULL);
    inputVolume->DeleteROIAlignmentTransformNode();
    }
  vtkNew<vtkMatrix4x4> worldToROI;
  vtkMRMLTransformNode::GetMatrixTransformBetweenNodes(NULL, roiTransform, worldToROI.GetPointer());

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
void vtkSlicerAstroVolumeLogic::SnapROIToVoxelGrid(vtkMRMLAnnotationROINode *roiNode, vtkMRMLAstroVolumeNode *inputVolume)
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
  if (roiNode->GetParentTransformNode() != NULL)
    {
    roiNode->SetAndObserveTransformNodeID(NULL);
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
    NULL, volumeRasToWorld.GetPointer());
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
  inputVolume->GetROIAlignmentTransformNode()->SetAndObserveTransformNodeID(NULL);
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
bool vtkSlicerAstroVolumeLogic::IsROIAlignedWithInputVolume(vtkMRMLAnnotationROINode *roiNode, vtkMRMLAstroVolumeNode *inputVolume)
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
bool vtkSlicerAstroVolumeLogic::CalculateROICropVolumeBounds(vtkMRMLAnnotationROINode *roiNode,
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
    roiTransform = NULL;
    }

  if (inputVolume->GetParentTransformNode() && !inputVolume->GetParentTransformNode()->IsTransformToWorldLinear())
    {
    vtkGenericWarningMacro("vtkSlicerAstroVolumeLogic::CalculateROICropVolumeBounds :"
                           " voxel-based cropping of non-linearly transformed input volume is not supported");
    return -1;
    }

  vtkNew<vtkMatrix4x4> roiToVolumeTransformMatrix;
  vtkMRMLTransformNode::GetMatrixTransformBetweenNodes(roiTransform, inputVolume->GetParentTransformNode(),
    roiToVolumeTransformMatrix.GetPointer());

  vtkNew<vtkMatrix4x4> rasToIJK;
  inputVolume->GetRASToIJKMatrix(rasToIJK.GetPointer());

  vtkNew<vtkMatrix4x4> roiToVolumeIJKTransformMatrix;
  vtkMatrix4x4::Multiply4x4(rasToIJK.GetPointer(), roiToVolumeTransformMatrix.GetPointer(),
    roiToVolumeIJKTransformMatrix.GetPointer());

  double roiXYZ[3] = {0};
  roiNode->GetXYZ(roiXYZ);
  double roiRadius[3] = {0};
  roiNode->GetRadiusXYZ(roiRadius);

  const int numberOfCorners = 8;
  double volumeCorners_ROI[numberOfCorners][4] =
    {
    { roiXYZ[0] - roiRadius[0], roiXYZ[1] - roiRadius[1], roiXYZ[2] - roiRadius[2], 1. },
    { roiXYZ[0] + roiRadius[0], roiXYZ[1] - roiRadius[1], roiXYZ[2] - roiRadius[2], 1. },
    { roiXYZ[0] - roiRadius[0], roiXYZ[1] + roiRadius[1], roiXYZ[2] - roiRadius[2], 1. },
    { roiXYZ[0] + roiRadius[0], roiXYZ[1] + roiRadius[1], roiXYZ[2] - roiRadius[2], 1. },
    { roiXYZ[0] - roiRadius[0], roiXYZ[1] - roiRadius[1], roiXYZ[2] + roiRadius[2], 1. },
    { roiXYZ[0] + roiRadius[0], roiXYZ[1] - roiRadius[1], roiXYZ[2] + roiRadius[2], 1. },
    { roiXYZ[0] - roiRadius[0], roiXYZ[1] + roiRadius[1], roiXYZ[2] + roiRadius[2], 1. },
    { roiXYZ[0] + roiRadius[0], roiXYZ[1] + roiRadius[1], roiXYZ[2] + roiRadius[2], 1. },
    };

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
    segmentTransform = NULL;
    }

  if (inputVolume->GetParentTransformNode() && !inputVolume->GetParentTransformNode()->IsTransformToWorldLinear())
    {
    vtkGenericWarningMacro("vtkSlicerAstroVolumeLogic::CalculateSegmentCropVolumeBounds :"
                           " voxel-based cropping of non-linearly transformed input volume is not supported");
    return -1;
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
double vtkSlicerAstroVolumeLogic::CalculateDisplayThresholdInROI(vtkMRMLAnnotationROINode *roiNode,
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
  int numSlice = dims[0] * dims[1];
  const int DataType = inputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  float *inFPixel = NULL;
  double *inDPixel = NULL;
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
  int cont = 0, firstElement, lastElement;

  this->CalculateROICropVolumeBounds(roiNode, inputVolume, roiBounds);

  cont = (roiBounds[1] - roiBounds[0]) *
         (roiBounds[3] - roiBounds[2]) *
         (roiBounds[5] - roiBounds[4]);

  firstElement = roiBounds[0] + roiBounds[2] * dims[0] +
                 roiBounds[4] * numSlice;

  lastElement = (roiBounds[1] + roiBounds[3] * dims[0] +
                roiBounds[5] * numSlice) + 1;

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  omp_set_num_threads(omp_get_num_procs());
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  switch (DataType)
    {
    case VTK_FLOAT:
      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp parallel for schedule(static) reduction(+:mean)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      for (int elemCnt = firstElement; elemCnt < lastElement; elemCnt++)
        { 
        int ref  = (int) floor(elemCnt / dims[0]);
        ref *= dims[0];
        int x = elemCnt - ref;
        ref = (int) floor(elemCnt / numSlice);
        ref *= numSlice;
        ref = elemCnt - ref;
        int y = (int) floor(ref / dims[0]);
        if (x < roiBounds[0] || x > roiBounds[1] ||
            y < roiBounds[2] || y > roiBounds[3])
          {
          continue;
          }
        if (FloatIsNaN(*(inFPixel + elemCnt)))
          {
          continue;
          }
        mean += *(inFPixel + elemCnt);
        }
      mean /= cont;

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp parallel for schedule(static) reduction(+:noise)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      for (int elemCnt = firstElement; elemCnt < lastElement; elemCnt++)
        {
        int ref  = (int) floor(elemCnt / dims[0]);
        ref *= dims[0];
        int x = elemCnt - ref;
        ref = (int) floor(elemCnt / numSlice);
        ref *= numSlice;
        ref = elemCnt - ref;
        int y = (int) floor(ref / dims[0]);
        if (x < roiBounds[0] || x > roiBounds[1] ||
            y < roiBounds[2] || y > roiBounds[3])
          {
          continue;
          }
        if (FloatIsNaN(*(inFPixel + elemCnt)))
          {
          continue;
          }
        noise += (*(inFPixel + elemCnt) - mean) * (*(inFPixel + elemCnt) - mean);
        }
      noise = sqrt(noise / cont);
      break;
    case VTK_DOUBLE:
      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp parallel for schedule(static) reduction(+:mean)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      for (int elemCnt = firstElement; elemCnt < lastElement; elemCnt++)
        {
        int ref  = (int) floor(elemCnt / dims[0]);
        ref *= dims[0];
        int x = elemCnt - ref;
        ref = (int) floor(elemCnt / numSlice);
        ref *= numSlice;
        ref = elemCnt - ref;
        int y = (int) floor(ref / dims[0]);
        if (x < roiBounds[0] || x > roiBounds[1] ||
            y < roiBounds[2] || y > roiBounds[3])
          {
          continue;
          }
        if (DoubleIsNaN(*(inDPixel + elemCnt)))
          {
          continue;
          }
        mean += *(inDPixel + elemCnt);
        }
      mean /= cont;

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp parallel for schedule(static) reduction(+:noise)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      for (int elemCnt = firstElement; elemCnt < lastElement; elemCnt++)
        {
        int ref  = (int) floor(elemCnt / dims[0]);
        ref *= dims[0];
        int x = elemCnt - ref;
        ref = (int) floor(elemCnt / numSlice);
        ref *= numSlice;
        ref = elemCnt - ref;
        int y = (int) floor(ref / dims[0]);
        if (x < roiBounds[0] || x > roiBounds[1] ||
            y < roiBounds[2] || y > roiBounds[3])
          {
          continue;
          }
        if (DoubleIsNaN(*(inDPixel + elemCnt)))
          {
          continue;
          }
        noise += (*(inDPixel + elemCnt) - mean) * (*(inDPixel + elemCnt) - mean);
        }
      noise = sqrt(noise / cont);
      break;
    }

  inFPixel = NULL;
  inDPixel = NULL;
  delete inFPixel;
  delete inDPixel;

  inputVolume->SetDisplayThreshold(noise);

  return noise;
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
  int numElements = dims[0] * dims[1] * dims[2];
  const int DataType = inputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  float *inFPixel = NULL;
  double *inDPixel = NULL;
  switch (DataType)
    {
    case VTK_FLOAT:
      inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("vtkSlicerAstroVolumeLogic::CalculateHistogram : "
                    "attempt to allocate scalars of type not allowed");
      return;
    }

  histoArray->SetNumberOfValues(numberOfBins);
  for (int histoIndex = 0; histoIndex < histoArray->GetNumberOfValues(); histoIndex++)
    {
    histoArray->SetValue(histoIndex, 0);
    }

  double DATAMIN = StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMIN"));

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  omp_set_num_threads(omp_get_num_procs());
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  #pragma omp parallel for schedule(dynamic)
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int histoIndex = 0;
    switch (DataType)
      {
      case VTK_FLOAT:
        if (FloatIsNaN(*(inFPixel + elemCnt)))
          {
          continue;
          }
        histoIndex = (int) ((*(inFPixel + elemCnt) - DATAMIN) / binSpacing);
        break;
      case VTK_DOUBLE:
        if (DoubleIsNaN(*(inDPixel + elemCnt)))
          {
          continue;
          }
        histoIndex = (int) ((*(inDPixel + elemCnt) - DATAMIN) / binSpacing);
        break;
      }

    #pragma omp critical
    histoArray->SetValue(histoIndex, histoArray->GetValue(histoIndex) + 1);
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

  vtkMRMLAstroVolumeDisplayNode *inputVolumeDisplay =
    inputVolume->GetAstroVolumeDisplayNode();
  if (!inputVolumeDisplay)
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "inputVolumeDisplay not found.");
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

  vtkMRMLAstroVolumeDisplayNode *referenceVolumeDisplay =
    referenceVolume->GetAstroVolumeDisplayNode();
  if (!referenceVolumeDisplay)
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::Reproject : "
                  "referenceVolumeDisplay not found.");
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
  imageDataTemp->SetDimensions(referenceDims[0], referenceDims[1], inputDims[2]);
  imageDataTemp->SetSpacing(1.,1.,1.);
  imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);

  // copy data into the Astro Volume object
  outputVolume->SetAndObserveImageData(imageDataTemp.GetPointer());

  float *inFPixel = NULL;
  float *outFPixel = NULL;
  double *inDPixel = NULL;
  double *outDPixel = NULL;
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

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  int numProcs = 0;
  if (pnode->GetCores() == 0)
    {
    numProcs = omp_get_num_procs();
    }
  else
    {
    numProcs = pnode->GetCores();
    }

  omp_set_num_threads(numProcs);
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  // Calculate the 2D interpolation grid
  double ***referenceGrid=NULL;
  referenceGrid = new double**[referenceDims[0]];
  for (int ii = 0; ii < referenceDims[0]; ii++)
    {
    referenceGrid[ii] = new double*[referenceDims[1]];
    for (int jj = 0; jj < referenceDims[1]; jj++)
      {
      referenceGrid[ii][jj] = new double[2];
      }
    }

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  #pragma omp parallel for schedule(dynamic) shared(referenceGrid)
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
  for (int ii = 0; ii < referenceDims[0]; ii++)
    {
    for (int jj = 0; jj < referenceDims[1]; jj++)
      {
      double ijk[3] = {0.}, world[3] = {0.};
      ijk[0] = ii;
      ijk[1] = jj;
      referenceVolumeDisplay->GetReferenceSpace(ijk, world);
      inputVolumeDisplay->GetIJKSpace(world, ijk);
      referenceGrid[ii][jj][0] = ijk[0];
      referenceGrid[ii][jj][1] = ijk[1];
      }
    }

  pnode->SetStatus(10);

  // Interpolate
  int numElements = referenceDims[0] * referenceDims[1] * inputDims[2];

  if (pnode->GetInterpolationOrder() == vtkMRMLAstroReprojectParametersNode::NearestNeighbour)
    {
    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, inFPixel, inDPixel, outFPixel, outDPixel, cancel, status)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int stat = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (stat == -1 && omp_get_thread_num() == 0)
      #else
      if (stat == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }
      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

      if (!cancel)
        {
        int ref  = (int) floor(elemCnt / referenceDims[0]);
        ref *= referenceDims[0];
        int ii = elemCnt - ref;
        ref = (int) floor(elemCnt / referenceSliceDim);
        int kk = ref;
        ref *= referenceSliceDim;
        ref = elemCnt - ref;
        int jj = (int) floor(ref / referenceDims[0]);

        double x = referenceGrid[ii][jj][0];
        int x1 = round(x);
        bool x1Inside = x1 > 0 && x1 < inputDims[0];

        double y = referenceGrid[ii][jj][1];
        int y1 = round(y);
        bool y1Inside = y1 > 0 && y1 < inputDims[1];

        if (!x1Inside || !y1Inside)
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
           *(outFPixel + elemCnt) = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x1);
           break;
         case VTK_DOUBLE:
           *(outDPixel + elemCnt) = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x1);
           break;
          }

        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        if (omp_get_thread_num() == 0)
          {
          if(elemCnt / (numElements / (numProcs * 100)) > status)
            {
            status += 10;
            pnode->SetStatus(status);
            }
          }
        #else
        if(elemCnt / (numElements / 100) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        }
      }
    }
  else if (pnode->GetInterpolationOrder() == vtkMRMLAstroReprojectParametersNode::Bilinear)
    {
    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, inFPixel, inDPixel, outFPixel, outDPixel, cancel, status)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int stat = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (stat == -1 && omp_get_thread_num() == 0)
      #else
      if (stat == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }
      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

      if (!cancel)
        {
        int ref  = (int) floor(elemCnt / referenceDims[0]);
        ref *= referenceDims[0];
        int ii = elemCnt - ref;
        ref = (int) floor(elemCnt / referenceSliceDim);
        int kk = ref;
        ref *= referenceSliceDim;
        ref = elemCnt - ref;
        int jj = (int) floor(ref / referenceDims[0]);

        double x = referenceGrid[ii][jj][0];
        int x1 = floor(x);
        bool x1Inside = x1 > 0 && x1 < inputDims[0];
        int x2 = ceil(x);
        bool x2Inside = x2 > 0 && x2 < inputDims[0];

        double y = referenceGrid[ii][jj][1];
        int y1 = floor(y);
        bool y1Inside = y1 > 0 && y1 < inputDims[1];
        int y2 = ceil(y);
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
              F11 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x1);
              break;
            case VTK_DOUBLE:
              F11 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x1);
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
              F21 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x2);
              break;
            case VTK_DOUBLE:
              F21 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x2);
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
              F12 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y2 + x1);
              break;
            case VTK_DOUBLE:
              F12 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y2 + x1);
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
             F22 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y2 + x2);
             break;
           case VTK_DOUBLE:
             F22 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y2 + x2);
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

        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        if (omp_get_thread_num() == 0)
          {
          if(elemCnt / (numElements / (numProcs * 100)) > status)
            {
            status += 10;
            pnode->SetStatus(status);
            }
          }
        #else
        if(elemCnt / (numElements / 100) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        }
      }
    }
  else if (pnode->GetInterpolationOrder() == vtkMRMLAstroReprojectParametersNode::Bicubic)
    {
    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, inFPixel, inDPixel, outFPixel, outDPixel, cancel, status)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int stat = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (stat == -1 && omp_get_thread_num() == 0)
      #else
      if (stat == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }
      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

      if (!cancel)
        {
        int ref  = (int) floor(elemCnt / referenceDims[0]);
        ref *= referenceDims[0];
        int ii = elemCnt - ref;
        ref = (int) floor(elemCnt / referenceSliceDim);
        int kk = ref;
        ref *= referenceSliceDim;
        ref = elemCnt - ref;
        int jj = (int) floor(ref / referenceDims[0]);

        double x = referenceGrid[ii][jj][0];
        int x1 = floor(x) - 1;
        bool x1Inside = x1 > 0 && x1 < inputDims[0];
        int x2 = floor(x);
        bool x2Inside = x2 > 0 && x2 < inputDims[0];
        int x3 = ceil(x);
        bool x3Inside = x3 > 0 && x3 < inputDims[0];
        int x4 = ceil(x) + 1;
        bool x4Inside = x4 > 0 && x4 < inputDims[0];

        double y = referenceGrid[ii][jj][1];
        int y1 = floor(y) - 1;
        bool y1Inside = y1 > 0 && y1 < inputDims[1];
        int y2 = floor(y);
        bool y2Inside = y2 > 0 && y2 < inputDims[1];
        int y3 = ceil(y);
        bool y3Inside = y3 > 0 && y3 < inputDims[1];
        int y4 = ceil(y) + 1;
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
              F11 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x1);
              break;
            case VTK_DOUBLE:
              F11 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x1);
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
              F21 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x2);
              break;
            case VTK_DOUBLE:
              F21 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x2);
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
              F31 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x3);
              break;
            case VTK_DOUBLE:
              F31 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x3);
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
              F41 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x4);
              break;
            case VTK_DOUBLE:
              F41 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x4);
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
              F12 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y2 + x1);
              break;
            case VTK_DOUBLE:
              F12 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y2 + x1);
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
              F22 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y2 + x2);
              break;
            case VTK_DOUBLE:
              F22 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y2 + x2);
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
              F32 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y2 + x3);
              break;
            case VTK_DOUBLE:
              F32 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y2 + x3);
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
              F42 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y2 + x4);
              break;
            case VTK_DOUBLE:
              F42 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y2 + x4);
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
              F13 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y3 + x1);
              break;
            case VTK_DOUBLE:
              F13 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y3 + x1);
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
              F23 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y3 + x2);
              break;
            case VTK_DOUBLE:
              F23 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y3 + x2);
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
              F33 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y3 + x3);
              break;
            case VTK_DOUBLE:
              F33 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y3 + x3);
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
              F43 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y3 + x4);
              break;
            case VTK_DOUBLE:
              F43 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y3 + x4);
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
              F14 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y4 + x1);
              break;
            case VTK_DOUBLE:
              F14 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y4 + x1);
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
              F24 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y4 + x2);
              break;
            case VTK_DOUBLE:
              F24 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y4 + x2);
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
              F34 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y4 + x3);
              break;
            case VTK_DOUBLE:
              F34 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y4 + x3);
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
              F44 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y4 + x4);
              break;
            case VTK_DOUBLE:
              F44 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y4 + x4);
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

        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        if (omp_get_thread_num() == 0)
          {
          if(elemCnt / (numElements / (numProcs * 100)) > status)
            {
            status += 10;
            pnode->SetStatus(status);
            }
          }
        #else
        if(elemCnt / (numElements / 100) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        }
      }
    }

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  vtkDebugMacro("Reprojection Time : "<<mtime<<" ms.");

  inFPixel = NULL;
  outFPixel = NULL;
  inDPixel = NULL;
  outDPixel = NULL;

  delete inFPixel;
  delete outFPixel;
  delete inDPixel;
  delete outDPixel;

  for(int ii = 0; ii < referenceDims[0]; ii++)
    {
    for(int jj = 0; jj < referenceDims[1]; jj++)
      {
      delete[] referenceGrid[ii][jj];
      }
    delete[] referenceGrid[ii];
    }
  delete referenceGrid;

  if (cancel)
    {
    pnode->SetStatus(100);
    return false;
    }

  gettimeofday(&start, NULL);

  int wasModifying = outputVolume->StartModify();
  this->CenterVolume(outputVolume);
  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateDisplayThresholdAttributes();
  outputVolume->EndModify(wasModifying);

  this->CenterVolume(referenceVolume);

  pnode->SetStatus(100);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

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
