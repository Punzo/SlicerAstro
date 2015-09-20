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
#include "vtkMRMLScene.h"
#include "vtkMRMLNode.h"
#include "vtkMRMLVolumeNode.h"
#include "vtkMRMLUnitNode.h"
#include "vtkMRMLSelectionNode.h"
#include "vtkMRMLAstroLabelMapVolumeNode.h"
#include "vtkMRMLAstroLabelMapVolumeDisplayNode.h"

//VTK includes
#include <vtkDoubleArray.h>
#include <vtkObjectFactory.h>
#include <vtkStringArray.h>
#include <vtkNew.h>
#include <vtkFloatArray.h>
#include <vtkCollection.h>
#include <vtkSmartPointer.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroVolumeLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic::vtkSlicerAstroVolumeLogic()
{
  UnitInit = false;
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

  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());

  this->ProcessMRMLSceneEvents(newScene, vtkMRMLScene::EndBatchProcessEvent, 0);
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  if (!node)
    {
    return;
    }

  if (node->IsA("vtkMRMLAstroVolumeNode"))
    {

    vtkSmartPointer<vtkCollection> listAstroVolumes = vtkSmartPointer<vtkCollection>::Take(
        this->GetMRMLScene()->GetNodesByClass("vtkMRMLAstroVolumeNode"));

    bool WCS = false;
    double min = std::numeric_limits<double>::max(), max = -std::numeric_limits<double>::max();
    for(int i = 0; i < listAstroVolumes->GetNumberOfItems(); i++)
      {
      vtkMRMLAstroVolumeNode* astrovolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(listAstroVolumes->GetItemAsObject(i));
      if (astrovolumeNode)
        {
        double mint = StringToDouble(astrovolumeNode->GetAttribute("SlicerAstro.DATAMIN"));
        if (mint < min)
          {
          min = mint;
          }
        double maxt = StringToDouble(astrovolumeNode->GetAttribute("SlicerAstro.DATAMAX"));
        if (maxt > max)
          {
          max = maxt;
          }

        if(astrovolumeNode->GetWCSStatus() != 0 && WCS)
          {
          vtkWarningMacro("Both WCS and non-WCS compatible Volumes have been added to the Scene."<<endl
                        <<"It may results in odd behaviours."<<endl);
          }
        if(astrovolumeNode->GetWCSStatus() == 0)
          {
          WCS = true;
          }
        }
      }

    if(!UnitInit)
      {
      vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
        this->GetMRMLScene()->GetNthNodeByClass(0, "vtkMRMLSelectionNode"));
      if (selectionNode)
        {
        vtkMRMLUnitNode* unitNode1 = selectionNode->GetUnitNode("intensity");
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
        unitNode1->SetSecondSuffix("");
        unitNode1->SetThirdSuffix("");
        selectionNode->SetUnitNodeID("intensity", unitNode1->GetID());

        vtkMRMLUnitNode* unitNode2 = selectionNode->GetUnitNode("length");
        unitNode2->SetMaximumValue(360.);
        unitNode2->SetMinimumValue(-180.);
        unitNode2->SetDisplayCoefficient(1.);
        unitNode2->SetPrefix("");
        unitNode2->SetSuffix("\xB0");
        unitNode2->SetSecondSuffix("\x27");
        unitNode2->SetThirdSuffix("\x22");
        unitNode2->SetPrecision(3);
        selectionNode->SetUnitNodeID("length", unitNode2->GetID());

        vtkMRMLUnitNode* unitNode3 = selectionNode->GetUnitNode("velocity");
        unitNode3->SetDisplayCoefficient(0.001);
        unitNode3->SetPrefix("");
        unitNode3->SetPrecision(3);
        unitNode3->SetSuffix("km/s");
        unitNode3->SetSecondSuffix("");
        unitNode3->SetThirdSuffix("");
        selectionNode->SetUnitNodeID("velocity", unitNode3->GetID());

        vtkMRMLUnitNode* unitNode4 = selectionNode->GetUnitNode("frequency");
        unitNode4->SetDisplayCoefficient(0.000000001);
        unitNode4->SetPrefix("");
        unitNode4->SetPrecision(6);
        unitNode4->SetSuffix("GHz");
        unitNode4->SetSecondSuffix("");
        unitNode4->SetThirdSuffix("");
        selectionNode->SetUnitNodeID("frequency", unitNode4->GetID());

        UnitInit = true;
        }
      }
    }
}

//---------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{

  if (!node)
    {
    return;
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
