// STD includes
#include <algorithm>

// Slicer includes
#include <vtkSlicerUnitsLogic.h>
#include <vtkSlicerVolumesLogic.h>

// AstroVolume includes
#include "vtkSlicerAstroVolumeLogic.h"

// MRML nodes includes
#include "vtkMRMLAstroVolumeNode.h"
#include "vtkMRMLAstroVolumeDisplayNode.h"
#include "vtkMRMLAstroVolumeStorageNode.h"
#include "vtkCacheManager.h"
#include "vtkDataIOManager.h"
#include "vtkMRMLLabelMapVolumeDisplayNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLVolumeArchetypeStorageNode.h"
#include "vtkMRMLTransformNode.h"
#include "vtkMRMLNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLVolumeNode.h"
#include "vtkMRMLUnitNode.h"
#include "vtkMRMLSelectionNode.h"
#include "vtkMRMLSliceCompositeNode.h"

//VTK includes
#include <vtkDoubleArray.h>
#include "vtkObjectFactory.h"
#include <vtkStringArray.h>
#include "vtkNew.h"
#include <vtkFloatArray.h>



//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroVolumeLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic::vtkSlicerAstroVolumeLogic()
{
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic::~vtkSlicerAstroVolumeLogic()
{
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
  //events->InsertNextValue(vtkMRMLSliceCompositeNode::ReferencedNodeModifiedEvent);

  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());

  this->ProcessMRMLSceneEvents(newScene, vtkMRMLScene::EndBatchProcessEvent, 0);
  //I think I can also overload ProcessMRMLSceneEvents -> if event
  //vtkMRMLSliceCompositeNode::ReferencedNodeModifiedEvent do something
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  if (!node)
    {
    return;
    }

  vtkMRMLAstroVolumeNode* astrovolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(
      this->GetMRMLScene()->GetNthNodeByClass(0, "vtkMRMLAstroVolumeNode"));
    if(astrovolumeNode)
      {
      vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
        this->GetMRMLScene()->GetNthNodeByClass(0, "vtkMRMLSelectionNode"));
      if (selectionNode)
        {
        vtkMRMLUnitNode* unitNode1 = selectionNode->GetUnitNode("intensity");
        double max = StringToDouble(astrovolumeNode->GetAttribute("SlicerAstro.DATAMAX"));
        double min = StringToDouble(astrovolumeNode->GetAttribute("SlicerAstro.DATAMIN"));
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

        temp += astrovolumeNode->GetAttribute("SlicerAstro.BUNIT");
        unitNode1->SetPrecision(6);
        unitNode1->SetSuffix(temp.c_str());
        selectionNode->SetUnitNodeID("intensity", unitNode1->GetID());

        vtkMRMLUnitNode* unitNode2 = selectionNode->GetUnitNode("length");
        unitNode2->SetMaximumValue(360.);
        unitNode2->SetMinimumValue(-180.);
        unitNode2->SetSuffix("\xB0");
        selectionNode->SetUnitNodeID("length", unitNode2->GetID());

        vtkMRMLUnitNode* unitNode3 = selectionNode->GetUnitNode("velocity");
        unitNode3->SetDisplayCoefficient(0.001);
        unitNode3->SetPrecision(2);
        unitNode3->SetSuffix(" km/s");
        selectionNode->SetUnitNodeID("velocity", unitNode3->GetID());

        }
      }


  //I think I should use node in some way IsA call a this->dosomething()
/*
  vtkMRMLSliceCompositeNode* SliceCompositeNode = vtkMRMLSliceCompositeNode::SafeDownCast(
    this->GetMRMLScene()->GetNthNodeByClass(0, "vtkMRMLSliceCompositeNode"));

  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
    this->GetMRMLScene()->GetNthNodeByClass(0, "vtkMRMLSelectionNode"));

  if (SliceCompositeNode && selectionNode)
    {
     vtkMRMLVolumeNode *volumeNode = vtkMRMLVolumeNode::SafeDownCast(
      this->GetMRMLScene()->GetNodeByID(SliceCompositeNode->GetBackgroundVolumeID()));
    if(!volumeNode)
      {
      //This should be unnecessary (when you add a volume is always a Background,
      //but let's mantain it for the moment
      volumeNode = vtkMRMLVolumeNode::SafeDownCast(
        this->GetMRMLScene()->GetNodeByID(SliceCompositeNode->GetForegroundVolumeID()));
      }

    if(volumeNode)
      {
      vtkMRMLAstroVolumeNode* astrovolumeNode =
        vtkMRMLAstroVolumeNode::SafeDownCast(volumeNode);
      //here put a if on WCS
      vtkMRMLUnitNode* unitNode1 = selectionNode->GetUnitNode("intensity");
      double max = StringToDouble(astrovolumeNode->GetAttribute("SlicerAstro.DATAMAX"));
      double min = StringToDouble(astrovolumeNode->GetAttribute("SlicerAstro.DATAMIN"));
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

      temp += astrovolumeNode->GetAttribute("SlicerAstro.BUNIT");
      unitNode1->SetPrecision(6);
      unitNode1->SetSuffix(temp.c_str());
      selectionNode->SetUnitNodeID("intensity", unitNode1->GetID());

      vtkMRMLUnitNode* unitNode2 = selectionNode->GetUnitNode("length");
      unitNode2->SetMaximumValue(360.);
      unitNode2->SetMinimumValue(-180.);
      unitNode2->SetSuffix("\xB0");
      selectionNode->SetUnitNodeID("length", unitNode2->GetID());

      vtkMRMLUnitNode* unitNode3 = selectionNode->GetUnitNode("velocity");
      unitNode3->SetDisplayCoefficient(0.001);
      unitNode3->SetPrecision(1);
      unitNode3->SetSuffix(" km/s");
      selectionNode->SetUnitNodeID("velocity", unitNode3->GetID());

      }
    }*/
}

void vtkSlicerAstroVolumeLogic::OnMRMLNodeModified(vtkMRMLNode *node)
{
  //assert(node);
  /*if (this->GetMRMLScene()->IsBatchProcessing())
    {
    return;
    }*/
//boh pare che non funziona
  //cout<<"bella"<<endl;
  /*
  if (node == )
    {
      cout<<SliceCompositeNode->GetBackgroundVolumeID()<<endl;
    }*/

}

//---------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{

  if (!node)
    {
    return;
    }
  //here we should put if the node removed is astro and there are no Foreground and Backgreound
  //then reput the units in default.

}

namespace
{
//----------------------------------------------------------------------------
ArchetypeVolumeNodeSet AstroVolumeNodeSetFactory(std::string& volumeName, vtkMRMLScene* scene, int options)
{
  ArchetypeVolumeNodeSet nodeSet(scene);

  // set up the Astro scalar node's support nodes
  vtkNew<vtkMRMLAstroVolumeNode> astroNode;
  astroNode->SetName(volumeName.c_str());
  nodeSet.Scene->AddNode(astroNode.GetPointer());

  vtkNew<vtkMRMLAstroVolumeDisplayNode> adisplayNode;
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

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::RegisterArchetypeVolumeNodeSetFactory(vtkSlicerVolumesLogic* volumesLogic)
{
  if (volumesLogic)
    {
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
}

//----------------------------------------------------------------------------
int vtkSlicerAstroVolumeLogic::SaveArchetypeVolume(const char *filename, vtkMRMLVolumeNode *volumeNode)
{
  if (volumeNode == NULL || filename == NULL)
    {
    return 0;
    }

  vtkMRMLAstroVolumeStorageNode *storageNode1 = NULL;
  vtkMRMLStorageNode *storageNode = NULL;
  vtkMRMLStorageNode *snode = volumeNode->GetStorageNode();

  if (snode != NULL)
    {
    storageNode1 = vtkMRMLAstroVolumeStorageNode::SafeDownCast(snode);
    }

  bool useURI = false;
  if (this->GetMRMLScene() &&
      this->GetMRMLScene()->GetCacheManager())
    {
    useURI = this->GetMRMLScene()->GetCacheManager()->IsRemoteReference(filename);
    }

  // Use FITS writer if we are dealing with Astro volumes

  if (volumeNode->IsA("vtkMRMLAstroVolumeNode"))
    {

    if (storageNode1 == NULL)
      {
      storageNode1 = vtkMRMLAstroVolumeStorageNode::New();
      storageNode1->SetScene(this->GetMRMLScene());
      this->GetMRMLScene()->AddNode(storageNode1);
      volumeNode->SetAndObserveStorageNodeID(storageNode1->GetID());
      storageNode1->Delete();
      }
    if (useURI)
      {
      storageNode1->SetURI(filename);
      }
    else
      {
      storageNode1->SetFileName(filename);
      }
    storageNode = storageNode1;
    }
  else vtkErrorMacro("FITSWriter handle only AstroVolume");

  int res = storageNode->WriteData(volumeNode);
  return res;
}
