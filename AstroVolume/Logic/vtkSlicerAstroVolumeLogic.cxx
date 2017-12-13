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
#include <vtkMRMLAnnotationROINode.h>
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
#include <vtkGeneralTransform.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
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

//----------------------------------------------------------------------------
template <typename T> std::string NumberToString(T V)
{
  std::string stringValue;
  std::stringstream strstream;
  strstream << V;
  strstream >> stringValue;
  return stringValue;
}

//----------------------------------------------------------------------------
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
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
bool FloatIsNaN(double Value)
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
  if (!selectionNode)
    {
    return;
    }

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
double vtkSlicerAstroVolumeLogic::CalculateRMSinROI(vtkMRMLAnnotationROINode *roiNode,
                                                    vtkMRMLAstroVolumeNode *inputVolume)
{
  if (!roiNode)
    {
    return 0.;
    }

  if (inputVolume == NULL)
   {
   vtkErrorMacro("vtkSlicerAstroVolumeLogic::CalculateRMSinROI : "
                 "inputVolume not found.");
   return false;
   }

  if (inputVolume->GetImageData() == NULL)
   {
   vtkErrorMacro("vtkSlicerAstroVolumeLogic::CalculateRMSinROI : "
                 "imageData not allocated.");
   return false;
   }

  //We calculate the noise as the std of 6 slices of the datacube.
  int *dims = inputVolume->GetImageData()->GetDimensions();
  int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic::CalculateRMSinROI : "
                  "imageData with more than one components.");
    return 0.;
    }
  int numSlice = dims[0] * dims[1];
  const int DataType = inputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  float *outFPixel = NULL;
  double *outDPixel = NULL;
  switch (DataType)
    {
    case VTK_FLOAT:
      outFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      outDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("vtkSlicerAstroVolumeLogic::CalculateRMSinROI : "
                    "attempt to allocate scalars of type not allowed");
      return false;
    }

  double noise = 0., mean = 0., roiBounds[6], volumeBounds[6];
  int cont = 0;

  roiNode->GetRASBounds(roiBounds);
  inputVolume->GetRASBounds(volumeBounds);

  for (int ii = 0; ii < 6; ii++)
    {
    if (fabs(roiBounds[ii]) - fabs(volumeBounds[ii]) > 1E-06)
      {
      roiBounds[ii] = volumeBounds[ii];
      }
    }

  vtkNew<vtkGeneralTransform> RAStoIJKTransform;
  RAStoIJKTransform->Identity();
  RAStoIJKTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
  inputVolume->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());

  double ijkMin[3], RASMin[3], ijkMax[3], RASMax[3];
  RASMin[0] = roiBounds[0];
  RASMin[1] = roiBounds[2];
  RASMin[2] = roiBounds[4];
  RASMax[0] = roiBounds[1];
  RASMax[1] = roiBounds[3];
  RASMax[2] = roiBounds[5];

  RAStoIJKTransform->TransformPoint(RASMin, ijkMin);
  RAStoIJKTransform->TransformPoint(RASMax, ijkMax);

  roiBounds[0] = (int) ijkMin[0];
  roiBounds[2] = (int) ijkMin[1];
  roiBounds[4] = (int) ijkMin[2];
  roiBounds[1] = (int) ijkMax[0];
  roiBounds[3] = (int) ijkMax[1];
  roiBounds[5] = (int) ijkMax[2];

  if (roiBounds[0] > roiBounds[1])
    {
    int temp = roiBounds[0];
    roiBounds[0] = roiBounds[1];
    roiBounds[1] = temp;
    }

  if (roiBounds[2] > roiBounds[3])
    {
    int temp = roiBounds[2];
    roiBounds[2] = roiBounds[3];
    roiBounds[3] = temp;
    }

  if (roiBounds[4] > roiBounds[5])
    {
    int temp = roiBounds[4];
    roiBounds[4] = roiBounds[5];
    roiBounds[5] = temp;
    }

  cont = (roiBounds[1] - roiBounds[0]) *
         (roiBounds[3] - roiBounds[2]) *
         (roiBounds[5] - roiBounds[4]);

  int firstElement, lastElement;

  firstElement = roiBounds[0] + roiBounds[2] * dims[0] +
                 roiBounds[4] * numSlice;

  lastElement = roiBounds[1] + roiBounds[3] * dims[0] +
                roiBounds[5] * numSlice;


  switch (DataType)
    {
    case VTK_FLOAT:
      for (int elemCnt = firstElement; elemCnt <= lastElement; elemCnt++)
        { 
        int ref  = (int) floor(elemCnt / dims[0]);
        ref *= dims[0];
        int x = elemCnt - ref;
        ref = (int) floor(elemCnt / numSlice);
        ref *= numSlice;
        ref = elemCnt - ref;
        int y = (int) floor(ref / dims[0]);
        if (x < roiBounds[0] ||  x > roiBounds[1] ||
            y < roiBounds[2] ||  y > roiBounds[3])
          {
          continue;
          }
        if (FloatIsNaN(*(outFPixel + elemCnt)))
          {
          continue;
          }
        mean += *(outFPixel + elemCnt);
        }
      mean /= cont;
      for (int elemCnt = firstElement; elemCnt <= lastElement; elemCnt++)
        {
        int ref  = (int) floor(elemCnt / dims[0]);
        ref *= dims[0];
        int x = elemCnt - ref;
        ref = (int) floor(elemCnt / numSlice);
        ref *= numSlice;
        ref = elemCnt - ref;
        int y = (int) floor(ref / dims[0]);
        if (x < roiBounds[0] ||  x > roiBounds[1] ||
            y < roiBounds[2] ||  y > roiBounds[3])
          {
          continue;
          }
        if (FloatIsNaN(*(outFPixel + elemCnt)))
          {
          continue;
          }
        noise += (*(outFPixel + elemCnt) - mean) * (*(outFPixel+elemCnt) - mean);
        }
      noise = sqrt(noise / cont);
      break;
    case VTK_DOUBLE:
      for (int elemCnt = firstElement; elemCnt <= lastElement; elemCnt++)
        {
        int ref  = (int) floor(elemCnt / dims[0]);
        ref *= dims[0];
        int x = elemCnt - ref;
        ref = (int) floor(elemCnt / numSlice);
        ref *= numSlice;
        ref = elemCnt - ref;
        int y = (int) floor(ref / dims[0]);
        if (x < roiBounds[0] ||  x > roiBounds[1] ||
            y < roiBounds[2] ||  y > roiBounds[3])
          {
          continue;
          }
        if (DoubleIsNaN(*(outDPixel + elemCnt)))
          {
          continue;
          }
        mean += *(outDPixel + elemCnt);
        }
      mean /= cont;
      for (int elemCnt = firstElement; elemCnt <= lastElement; elemCnt++)
        {
        int ref  = (int) floor(elemCnt / dims[0]);
        ref *= dims[0];
        int x = elemCnt - ref;
        ref = (int) floor(elemCnt / numSlice);
        ref *= numSlice;
        ref = elemCnt - ref;
        int y = (int) floor(ref / dims[0]);
        if (x < roiBounds[0] ||  x > roiBounds[1] ||
            y < roiBounds[2] ||  y > roiBounds[3])
          {
          continue;
          }
        if (DoubleIsNaN(*(outDPixel + elemCnt)))
          {
          continue;
          }
        noise += (*(outDPixel + elemCnt) - mean) * (*(outDPixel+elemCnt) - mean);
        }
      noise = sqrt(noise / cont);
      break;
    }

  outFPixel = NULL;
  outDPixel = NULL;
  delete outFPixel;
  delete outDPixel;

  inputVolume->Set3DDisplayThreshold(noise);
  inputVolume->SetAttribute("SlicerAstro.3DDisplayThresholdMean", DoubleToString(mean).c_str());

  return noise;
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
  double noise = StringToDouble(node->GetAttribute("SlicerAstro.3DDisplayThreshold"));
  if (noise < 0.000000001)
    {
    noise = (max - min) / 100.;
    }
  double halfNoise = noise * 0.5;
  double noise3 = noise * 3.;
  double noise6 = noise * 6.;
  double noise7 = noise * 7.;
  double noise9 = noise * 9.;
  double noise12 = noise * 12.;
  double noise15 = noise * 15.;
  double noise18 = noise * 18.;
  double noise21 = noise * 21.;
  double noise24 = noise * 24.;

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
    if(!strcmp(volumePropertyNode->GetName(),"OneSurface"))
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
