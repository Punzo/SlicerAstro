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


vtkSlicerAstroVolumeLogic::~vtkSlicerAstroVolumeLogic()
{
}


void vtkSlicerAstroVolumeLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}


//---------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::InitializeEventListeners()
{
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
  this->SetAndObserveMRMLSceneEventsInternal(this->GetMRMLScene(), events.GetPointer());
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
  vtkNew<vtkFloatArray> priorities;

  float normalPriority = 0.0;

  // Events that use the default priority.  Don't care the order they
  // are triggered
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  priorities->InsertNextValue(normalPriority);


  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer(), priorities.GetPointer());

  this->ProcessMRMLSceneEvents(newScene, vtkCommand::ModifiedEvent, 0);
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
      unitNode1->SetMaximumValue(std::stod(astrovolumeNode->GetAttribute("SlicerAstro.DATAMAX")));
      unitNode1->SetMinimumValue(std::stod(astrovolumeNode->GetAttribute("SlicerAstro.DATAMIN")));
      unitNode1->SetSuffix(astrovolumeNode->GetAttribute("SlicerAstro.BUNIT"));
      unitNode1->SetPrecision(9);
      selectionNode->SetUnitNodeID("intensity", unitNode1->GetID());
      //here put some if max<0.01 then use milli, etc.

      vtkMRMLUnitNode* unitNode2 = selectionNode->GetUnitNode("length"); // Note: most often quantity will be a const string like "length" or "time"
      unitNode2->SetMaximumValue(360.);
      unitNode2->SetMinimumValue(-180.);
      unitNode2->SetSuffix("\xB0");
      selectionNode->SetUnitNodeID("length", unitNode2->GetID());

      }
    //put a check for the velocity chose between km/s or m/s automatically.
    }
}

//---------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic
::OnMRMLSceneNodeRemoved(vtkMRMLNode* vtkNotUsed(node))
{
}


namespace
{

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

