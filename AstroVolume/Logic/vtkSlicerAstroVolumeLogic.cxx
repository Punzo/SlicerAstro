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

// MRML nodes includes
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroModelingParametersNode.h>
#include <vtkMRMLAstroSmoothingParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSliceViewDisplayableManagerFactory.h>
#include <vtkMRMLThreeDViewDisplayableManagerFactory.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLViewNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLVolumePropertyNode.h>

//VTK includes
#include <vtkCacheManager.h>
#include <vtkCollection.h>
#include <vtkColorTransferFunction.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

// WCS includes
#include "wcslib.h"

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

  // Events that use the default priority.  Don't care the order they
  // are triggered
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::EndImportEvent);

  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());

  this->ProcessMRMLSceneEvents(newScene, vtkMRMLScene::EndBatchProcessEvent, 0);
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
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  if (!node)
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

  //check WCS and update unit nodes
  if (node->IsA("vtkMRMLAstroVolumeNode"))
    {
    vtkSmartPointer<vtkCollection> listAstroVolumes = vtkSmartPointer<vtkCollection>::Take(
      this->GetMRMLScene()->GetNodesByClass("vtkMRMLAstroVolumeNode"));

    bool WCS = false;
    double min = std::numeric_limits<double>::max(), max = -std::numeric_limits<double>::max();
    for(int i = 0; i < listAstroVolumes->GetNumberOfItems(); i++)
      {
      vtkMRMLAstroVolumeNode* astroVolumeNode =
        vtkMRMLAstroVolumeNode::SafeDownCast(listAstroVolumes->GetItemAsObject(i));
      if (astroVolumeNode)
        {
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
        vtkMRMLAstroVolumeDisplayNode* astroVolumeDisplayNode =
             astroVolumeNode->GetAstroVolumeDisplayNode();
        if(!astroVolumeDisplayNode)
          {
          continue;
          }
        if(astroVolumeDisplayNode->GetWCSStatus() != 0 && WCS)
          {
          vtkWarningMacro("Both WCS and non-WCS compatible Volumes have been added to the Scene."<<endl
                        <<"It may results in odd behaviours."<<endl);
          }
        if(astroVolumeDisplayNode->GetWCSStatus() == 0)
          {
          WCS = true;
          }
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

      temp += "Jy/beam";
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

        sliceNode->DisableModifiedEventOff();
        }
      }

    vtkMRMLSliceNode* sliceNodeRed = vtkMRMLSliceNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
    sliceNodeRed->SetOrientation("XY");

    vtkMRMLSliceNode* sliceNodeYellow = vtkMRMLSliceNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
    sliceNodeYellow->SetOrientation("XZ");

    vtkMRMLSliceNode* sliceNodeGreen = vtkMRMLSliceNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
    sliceNodeGreen->SetOrientation("ZY");
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
  vtkNew<vtkMRMLAstroVolumeNode> astroNode;
  astroNode->SetName(volumeName.c_str());
  nodeSet.Scene->AddNode(astroNode.GetPointer());

  vtkNew<vtkMRMLAstroVolumeDisplayNode> adisplayNode;
  adisplayNode->SetAutoWindowLevel(0);
  nodeSet.Scene->AddNode(adisplayNode.GetPointer());
  astroNode->SetAndObserveDisplayNodeID(adisplayNode->GetID());

  vtkNew<vtkMRMLAstroVolumeStorageNode> storageNode;
  storageNode->SetCenterImage(options & vtkSlicerVolumesLogic::CenterImage);
  nodeSet.Scene->AddNode(storageNode.GetPointer());
  astroNode->SetAndObserveStorageNodeID(storageNode->GetID());

  nodeSet.StorageNode = storageNode.GetPointer();
  nodeSet.DisplayNode = adisplayNode.GetPointer();
  nodeSet.Node = astroNode.GetPointer();

  return nodeSet;
}

};


namespace
{
//----------------------------------------------------------------------------
ArchetypeVolumeNodeSet AstroLabelMapVolumeNodeSetFactory(std::string& volumeName, vtkMRMLScene* scene, int options)
{
  ArchetypeVolumeNodeSet nodeSet(scene);

  // set up the AstroLabelMap node's support nodes
  vtkNew<vtkMRMLAstroLabelMapVolumeNode> astroLabelMapNode;
  astroLabelMapNode->SetName(volumeName.c_str());
  nodeSet.Scene->AddNode(astroLabelMapNode.GetPointer());

  vtkNew<vtkMRMLAstroLabelMapVolumeDisplayNode> lmdisplayNode;
  nodeSet.Scene->AddNode(lmdisplayNode.GetPointer());
  astroLabelMapNode->SetAndObserveDisplayNodeID(lmdisplayNode->GetID());

  vtkNew<vtkMRMLAstroVolumeStorageNode> storageNode;
  storageNode->SetCenterImage(options & vtkSlicerVolumesLogic::CenterImage);
  nodeSet.Scene->AddNode(storageNode.GetPointer());
  astroLabelMapNode->SetAndObserveStorageNodeID(storageNode->GetID());

  nodeSet.StorageNode = storageNode.GetPointer();
  nodeSet.DisplayNode = lmdisplayNode.GetPointer();
  nodeSet.Node = astroLabelMapNode.GetPointer();

  nodeSet.LabelMap = true;

  return nodeSet;
}

};

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
}

//----------------------------------------------------------------------------
vtkMRMLScene *vtkSlicerAstroVolumeLogic::GetPresetsScene()
{
  if (!this->PresetsScene)
    {
    this->PresetsScene = vtkMRMLScene::New();
    this->LoadPresets(this->PresetsScene);
    }
  return this->PresetsScene;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::updateUnitsNodes(vtkMRMLNode *astroVolumeNode)
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
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode1 = selectionNode->GetUnitNode("intensity");
    double max = StringToDouble(astroVolumeNode->GetAttribute("SlicerAstro.DATAMAX"));
    double min = StringToDouble(astroVolumeNode->GetAttribute("SlicerAstro.DATAMIN"));
    unitNode1->SetMaximumValue(max);
    unitNode1->SetMinimumValue(min);
    unitNode1->SetDisplayCoefficient(1.);
    std::string temp = " ";
    if(max < 0.001)
      {
      temp += "\xB5";
      unitNode1->SetDisplayCoefficient(1000000.);
      }
    else
      {
      if(max < 1.)
        {
        temp += "m";
        unitNode1->SetDisplayCoefficient(1000.);
        }
      }

    temp += "Jy/beam";
    unitNode1->SetPrecision(6);
    unitNode1->SetPrefix("");
    unitNode1->SetSuffix(temp.c_str());
    unitNode1->SetAttribute("DisplayHint","");
    selectionNode->SetUnitNodeID("intensity", unitNode1->GetID());
    }

}

//---------------------------------------------------------------------------
vtkMRMLAstroLabelMapVolumeNode *vtkSlicerAstroVolumeLogic::CreateAndAddLabelVolume(vtkMRMLScene *scene,
                                                                                   vtkMRMLAstroVolumeNode *volumeNode,
                                                                                   const char *name)
{
  if (scene == NULL || volumeNode == NULL || name == NULL)
    {
    return NULL;
    }

  // Create a volume node as copy of source volume
  vtkNew<vtkMRMLAstroLabelMapVolumeNode> labelNode;
  std::string uname = this->GetMRMLScene()->GetUniqueNameByString(name);
  labelNode->SetName(uname.c_str());
  scene->AddNode(labelNode.GetPointer());

  this->CreateLabelVolumeFromVolume(scene, labelNode.GetPointer(), volumeNode);

  // Make an image data of the same size and shape as the input volume, but filled with zeros
  vtkSlicerVolumesLogic::ClearVolumeImageData(labelNode.GetPointer());

  return labelNode.GetPointer();
}

//---------------------------------------------------------------------------
vtkMRMLAstroLabelMapVolumeNode *vtkSlicerAstroVolumeLogic::CreateLabelVolumeFromVolume(vtkMRMLScene *scene,
                                                                                       vtkMRMLAstroLabelMapVolumeNode *labelNode,
                                                                                       vtkMRMLAstroVolumeNode *inputVolume)
{
  if (scene == NULL || labelNode == NULL || inputVolume == NULL)
    {
    return NULL;
    }

  // Create a display node if the label node does not have one
  vtkSmartPointer<vtkMRMLAstroLabelMapVolumeDisplayNode> labelDisplayNode =
    vtkMRMLAstroLabelMapVolumeDisplayNode::SafeDownCast(labelNode->GetDisplayNode());
  if (labelDisplayNode.GetPointer() == NULL)
    {
    vtkMRMLAstroVolumeDisplayNode* astroVolumeDisplay = inputVolume->GetAstroVolumeDisplayNode();
    labelDisplayNode = vtkSmartPointer<vtkMRMLAstroLabelMapVolumeDisplayNode>::New();
    labelDisplayNode->SetSpaceQuantities(astroVolumeDisplay->GetSpaceQuantities());
    labelDisplayNode->SetSpace(astroVolumeDisplay->GetSpace());
    labelDisplayNode->SetAttribute("SlicerAstro.NAXIS", inputVolume->GetAttribute("SlicerAstro.NAXIS"));

    struct wcsprm* labelWCS;
    struct wcsprm* volumeWCS;

    labelWCS = labelDisplayNode->GetWCSStruct();
    volumeWCS = astroVolumeDisplay->GetWCSStruct();

    labelWCS->flag=-1;

    labelDisplayNode->SetWCSStatus(wcscopy(1, volumeWCS, labelWCS));
    if (labelDisplayNode->GetWCSStatus())
      {
      vtkErrorMacro("wcscopy ERROR "<<labelDisplayNode->GetWCSStatus()<<":\n"<<
                    "Message from "<<labelWCS->err->function<<
                    "at line "<<labelWCS->err->line_no<<" of file "<<labelWCS->err->file<<
                    ": \n"<<labelWCS->err->msg<<"\n");
      }

    labelDisplayNode->SetWCSStatus(wcsset(labelWCS));
    if (labelDisplayNode->GetWCSStatus())
      {
      vtkErrorMacro("wcsset ERROR "<<labelDisplayNode->GetWCSStatus()<<":\n"<<
                    "Message from "<<labelWCS->err->function<<
                    "at line "<<labelWCS->err->line_no<<" of file "<<labelWCS->err->file<<
                    ": \n"<<labelWCS->err->msg<<"\n");
      }

    scene->AddNode(labelDisplayNode);
    }

  // We need to copy from the volume node to get required attributes, but
  // the copy copies inputVolume's name as well.  So save the original name
  // and re-set the name after the copy.
  std::string origName(labelNode->GetName());
  labelNode->Copy(inputVolume);
  labelNode->SetAndObserveStorageNodeID(NULL);
  labelNode->SetName(origName.c_str());
  labelNode->SetAndObserveDisplayNodeID(labelDisplayNode->GetID());

  // Associate labelmap with the source volume
  //TODO: Obsolete, replace mechanism with node references
  if (inputVolume->GetID())
    {
    labelNode->SetAttribute("AssociatedNodeID", inputVolume->GetID());
    }

  std::vector<std::string> keys = inputVolume->GetAttributeNames();
  for (std::vector<std::string>::iterator kit = keys.begin(); kit != keys.end(); ++kit)
    {
    labelNode->SetAttribute((*kit).c_str(), inputVolume->GetAttribute((*kit).c_str()));
    }
  labelNode->SetAttribute("SlicerAstro.DATAMODEL", "MASK");
  labelNode->SetAttribute("SlicerAstro.BITPIX", "16");

  // Set the display node to have a label map lookup table
  this->SetAndObserveColorToDisplayNode(labelDisplayNode,
                                        /* labelMap = */ 1, /* filename= */ 0);

  // Copy and set image data of the input volume to the label volume
  vtkNew<vtkImageData> imageData;
  imageData->DeepCopy(inputVolume->GetImageData());
  labelNode->SetAndObserveImageData(imageData.GetPointer());

  return labelNode;
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
  double noise = StringToDouble(node->GetAttribute("SlicerAstro.RMS"));
  if (noise < 0.000000001)
    {
    noise = (max - min) / 100.;
    }
  double noise3 = noise * 3.;
  double noise7 = noise * 7.;
  double noise15 = noise * 15.;

  vtkSmartPointer<vtkCollection> presets = vtkSmartPointer<vtkCollection>::Take(
      this->PresetsScene->GetNodesByClass("vtkMRMLVolumePropertyNode"));

  for(int i = 0; i < presets->GetNumberOfItems(); i++)
    {
    vtkMRMLVolumePropertyNode* volumePropertyNode =
        vtkMRMLVolumePropertyNode::SafeDownCast(presets->GetItemAsObject(i));
    if (volumePropertyNode)
      {
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
      if(!strcmp(volumePropertyNode->GetName(),"OneSurface"))
        {
         vtkPiecewiseFunction *compositeOpacity =
             volumePropertyNode->GetScalarOpacity();
         compositeOpacity->RemoveAllPoints();
         compositeOpacity->AddPoint(min, 0.);
         compositeOpacity->AddPoint(noise3 - (noise3 / 5.), 0., 0.5, 0.2);
         compositeOpacity->AddPoint(noise3, 0.6, 0.5, 0.2);
         compositeOpacity->AddPoint(noise3 + (noise3 / 5.), 0., 0.5, 0.);
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
         compositeOpacity->AddPoint(noise3 + (noise3 / 5.), 0., 0.5, 0.);
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
         compositeOpacity->AddPoint(noise3 + (noise3 / 5.), 0., 0.5, 0.);
         compositeOpacity->AddPoint(noise7 - (noise7 / 5.), 0., 0.5, 0.2);
         compositeOpacity->AddPoint(noise7, 0.8, 0.5, 0.2);
         compositeOpacity->AddPoint(noise7 + (noise7 / 5.), 0., 0.5, 0.);
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
         compositeOpacity->AddPoint(noise3 + (noise3 / 5.), 0., 0.5, 0.);
         compositeOpacity->AddPoint(noise7 - (noise7 / 5.), 0., 0.5, 0.2);
         compositeOpacity->AddPoint(noise7, 0.65, 0.5, 0.2);
         compositeOpacity->AddPoint(noise7 + (noise7 / 5.), 0., 0.5, 0.);
         compositeOpacity->AddPoint(noise15 - (noise15 / 5.), 0., 0.5, 0.2);
         compositeOpacity->AddPoint(noise15, 0.8, 0.5, 0.2);
         compositeOpacity->AddPoint(noise15 + (noise15 / 5.), 0., 0.5, 0.);
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
      }
    }

    return true;
}

//---------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::LoadPresets(vtkMRMLScene *scene)
{
  this->PresetsScene->RegisterNodeClass(vtkNew<vtkMRMLVolumePropertyNode>().GetPointer());

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
  return true;
}
