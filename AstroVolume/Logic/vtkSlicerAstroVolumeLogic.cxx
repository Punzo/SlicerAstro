// STD includes
#include <algorithm>

// Slicer includes
#include <vtkSlicerUnitsLogic.h>
#include <vtkSlicerVolumesLogic.h>

// AstroVolume includes
#include <vtkSlicerAstroVolumeLogic.h>

// MRML nodes includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkCacheManager.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLVolumePropertyNode.h>
#include <vtkMRMLViewNode.h>
#include <vtkMRMLSliceNode.h>

//VTK includes
#include <vtkObjectFactory.h>
#include <vtkNew.h>
#include <vtkCollection.h>
#include <vtkSmartPointer.h>
#include <vtkColorTransferFunction.h>
#include <vtkImageData.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>


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

  //change axes label names
  if (node->IsA("vtkMRMLViewNode"))
    {
    vtkMRMLViewNode *viewNode =
      vtkMRMLViewNode::SafeDownCast(node);

    viewNode->SetAxisLabel(0,"W");
    viewNode->SetAxisLabel(1,"E");
    viewNode->SetAxisLabel(2,"Z");
    viewNode->SetAxisLabel(3,"z");
    viewNode->SetAxisLabel(4,"S");
    viewNode->SetAxisLabel(5,"N");
    }

  if (node->IsA("vtkMRMLSliceNode"))
    {
    vtkMRMLSliceNode *sliceNode =
      vtkMRMLSliceNode::SafeDownCast(node);

    sliceNode->SetAxisLabel(0,"W");
    sliceNode->SetAxisLabel(1,"E");
    sliceNode->SetAxisLabel(2,"Z");
    sliceNode->SetAxisLabel(3,"z");
    sliceNode->SetAxisLabel(4,"S");
    sliceNode->SetAxisLabel(5,"N");
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
          vtkMRMLAstroVolumeNode::SafeDownCast
          (listAstroVolumes->GetItemAsObject(i));
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
      unitNode1->SetAttribute("DisplayHint","");
      selectionNode->SetUnitNodeID("intensity", unitNode1->GetID());

      vtkMRMLAstroVolumeNode* astroVolumeNode =
          vtkMRMLAstroVolumeNode::SafeDownCast(node);

      if (!strcmp(astroVolumeNode->GetAttribute("SlicerAstro.CUNIT1"), "DEGREE"))
        {
        vtkMRMLUnitNode* unitNode2 = selectionNode->GetUnitNode("length");
        unitNode2->SetMaximumValue(360.);
        unitNode2->SetMinimumValue(-180.);
        unitNode2->SetDisplayCoefficient(1.);
        unitNode2->SetPrefix("");
        unitNode2->SetSuffix("\xB0");
        unitNode2->SetAttribute("DisplayHint","DegreeAsArcMinutesArcSeconds");
        unitNode2->SetPrecision(3);
        selectionNode->SetUnitNodeID("length", unitNode2->GetID());
        }
      else
        {
        vtkWarningMacro("The loaded volume has not the spatial axes in degree.")
        }
      }
   }

  if (node->IsA("vtkMRMLAstroLabelMapVolumeNode"))
    {
    vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
      this->GetMRMLScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
    if (selectionNode)
      {
      vtkMRMLAstroLabelMapVolumeNode* astroLabelMapVolumeNode =
          vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(node);

      if (!strcmp(astroLabelMapVolumeNode->GetAttribute("SlicerAstro.CUNIT1"), "DEGREE"))
        {
        vtkMRMLUnitNode* unitNode2 = selectionNode->GetUnitNode("length");
        unitNode2->SetMaximumValue(360.);
        unitNode2->SetMinimumValue(-180.);
        unitNode2->SetDisplayCoefficient(1.);
        unitNode2->SetPrefix("");
        unitNode2->SetSuffix("\xB0");
        unitNode2->SetAttribute("DisplayHint","DegreeAsArcMinutesArcSeconds");
        unitNode2->SetPrecision(3);
        selectionNode->SetUnitNodeID("length", unitNode2->GetID());
        }
      else
        {
        vtkWarningMacro("The loaded volume has not the spatial axes in degree.")
        }
      }
    }

  if (node->IsA("vtkMRMLAstroVolumeNode") ||
      node->IsA("vtkMRMLAstroLabelMapVolumeNode"))
    {
    vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
      this->GetMRMLScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
    if (selectionNode)
      {
      vtkMRMLUnitNode* unitNode3 = selectionNode->GetUnitNode("velocity");

      unitNode3->SetDisplayCoefficient(0.001);
      unitNode3->SetSuffix("km/s");
      unitNode3->SetPrefix("");
      unitNode3->SetPrecision(3);
      unitNode3->SetAttribute("DisplayHint","");
      selectionNode->SetUnitNodeID("velocity", unitNode3->GetID());

      vtkMRMLUnitNode* unitNode4 = selectionNode->GetUnitNode("frequency");
      unitNode4->SetDisplayCoefficient(0.000001);
      unitNode4->SetPrefix("");
      unitNode4->SetPrecision(2);
      unitNode4->SetSuffix("MHz");
      unitNode4->SetAttribute("DisplayHint","");
      selectionNode->SetUnitNodeID("frequency", unitNode4->GetID());

      }

    // change axes label names
    vtkSmartPointer<vtkCollection> viewNodes = vtkSmartPointer<vtkCollection>::Take(
        this->GetMRMLScene()->GetNodesByClass("vtkMRMLViewNode"));

    for(int i = 0; i < viewNodes->GetNumberOfItems(); i++)
      {
      vtkMRMLViewNode* viewNode =
          vtkMRMLViewNode::SafeDownCast(viewNodes->GetItemAsObject(i));

      viewNode->SetAxisLabel(0,"W");
      viewNode->SetAxisLabel(1,"E");
      viewNode->SetAxisLabel(2,"Z");
      viewNode->SetAxisLabel(3,"z");
      viewNode->SetAxisLabel(4,"S");
      viewNode->SetAxisLabel(5,"N");
      }

    vtkSmartPointer<vtkCollection> sliceNodes = vtkSmartPointer<vtkCollection>::Take(
        this->GetMRMLScene()->GetNodesByClass("vtkMRMLSliceNode"));

    for(int i = 0; i < sliceNodes->GetNumberOfItems(); i++)
      {
      vtkMRMLSliceNode* sliceNode =
          vtkMRMLSliceNode::SafeDownCast(sliceNodes->GetItemAsObject(i));

      sliceNode->SetAxisLabel(0,"W");
      sliceNode->SetAxisLabel(1,"E");
      sliceNode->SetAxisLabel(2,"Z");
      sliceNode->SetAxisLabel(3,"z");
      sliceNode->SetAxisLabel(4,"S");
      sliceNode->SetAxisLabel(5,"N");
      }
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

bool vtkSlicerAstroVolumeLogic::synchronizePresetsToVolumeNode(vtkMRMLNode *node)
{
  if (node && (node->IsA("vtkMRMLAstroVolumeNode") ||
               node->IsA("vtkMRMLAstroLabelMapVolumeNode")))
    {
    double max = StringToDouble(node->GetAttribute("SlicerAstro.DATAMAX")) * 2.;
    double min = StringToDouble(node->GetAttribute("SlicerAstro.DATAMIN")) * 2.;
    double noise = StringToDouble(node->GetAttribute("SlicerAstro.RMS"));
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
        if(!strcmp(volumePropertyNode->GetName(),"LowCostantOpacity"))
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
        if(!strcmp(volumePropertyNode->GetName(),"MediumCostantOpacity"))
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
        if(!strcmp(volumePropertyNode->GetName(),"HighCostantOpacity"))
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
        if(!strcmp(volumePropertyNode->GetName(),"TwoSurface"))
          {
           vtkPiecewiseFunction *compositeOpacity =
               volumePropertyNode->GetScalarOpacity();
           compositeOpacity->RemoveAllPoints();
           compositeOpacity->AddPoint(min, 0.);
           compositeOpacity->AddPoint(noise3 - (noise3 / 5.), 0., 0.5, 0.2);
           compositeOpacity->AddPoint(noise3, 0.4, 0.5, 0.2);
           compositeOpacity->AddPoint(noise3 + (noise3 / 5.), 0., 0.5, 0.);
           compositeOpacity->AddPoint(noise7 - (noise7 / 5.), 0., 0.5, 0.2);
           compositeOpacity->AddPoint(noise7, 0.7, 0.5, 0.2);
           compositeOpacity->AddPoint(noise7 + (noise7 / 5.), 0., 0.5, 0.);
           compositeOpacity->AddPoint(max, 0.);

           vtkColorTransferFunction *color =
               volumePropertyNode->GetColor();
           color->RemoveAllPoints();
           color->AddRGBPoint(min, 0., 0., 0.);
           color->AddRGBPoint(noise3, 1., 1., 1.);
           color->AddRGBPoint(noise7, 0., 1., 0.);
           color->AddRGBPoint(max, 0., 1., 0.);

           vtkPiecewiseFunction *gradientOpacity =
               volumePropertyNode->GetGradientOpacity();
           gradientOpacity->RemoveAllPoints();
           gradientOpacity->AddPoint(min, 1.);
           gradientOpacity->AddPoint(max, 1.);
          }
        if(!strcmp(volumePropertyNode->GetName(),"ThreeSurface"))
          {
           vtkPiecewiseFunction *compositeOpacity =
               volumePropertyNode->GetScalarOpacity();
           compositeOpacity->RemoveAllPoints();
           compositeOpacity->AddPoint(min, 0.);
           compositeOpacity->AddPoint(noise3 - (noise3 / 5.), 0., 0.5, 0.2);
           compositeOpacity->AddPoint(noise3, 0.4, 0.5, 0.2);
           compositeOpacity->AddPoint(noise3 + (noise3 / 5.), 0., 0.5, 0.);
           compositeOpacity->AddPoint(noise7 - (noise7 / 5.), 0., 0.5, 0.2);
           compositeOpacity->AddPoint(noise7, 0.5, 0.5, 0.2);
           compositeOpacity->AddPoint(noise7 + (noise7 / 5.), 0., 0.5, 0.);
           compositeOpacity->AddPoint(noise15 - (noise15 / 5.), 0., 0.5, 0.2);
           compositeOpacity->AddPoint(noise15, 0.7, 0.5, 0.2);
           compositeOpacity->AddPoint(noise15 + (noise15 / 5.), 0., 0.5, 0.);
           compositeOpacity->AddPoint(max, 0.);

           vtkColorTransferFunction *color =
               volumePropertyNode->GetColor();
           color->RemoveAllPoints();
           color->AddRGBPoint(min, 0., 0., 0.);
           color->AddRGBPoint(noise3, 1., 1., 1.);
           color->AddRGBPoint(noise7, 0., 1., 0.);
           color->AddRGBPoint(noise15, 0., 0., 1.);
           color->AddRGBPoint(max, 0., 0., 1.);

           vtkPiecewiseFunction *gradientOpacity =
               volumePropertyNode->GetGradientOpacity();
           gradientOpacity->RemoveAllPoints();
           gradientOpacity->AddPoint(min, 1.);
           gradientOpacity->AddPoint(max, 1.);
          }
        }
      }
    return 0;
    }
  else
    {
    return 1;
    }
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


//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode*
vtkSlicerAstroVolumeLogic::CloneVolume(vtkMRMLVolumeNode *volumeNode, const char *name)
{
  return Self::CloneVolume(this->GetMRMLScene(), volumeNode, name);
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode*
vtkSlicerAstroVolumeLogic::
CloneVolume (vtkMRMLScene *scene, vtkMRMLVolumeNode *volumeNode, const char *name, bool cloneImageData/*=true*/)
{
  if ( scene == NULL || volumeNode == NULL )
    {
    // no valid object is available, so we cannot log error
    return NULL;
    }

  // clone the display node if possible
  vtkSmartPointer<vtkMRMLDisplayNode> clonedDisplayNode;
  if (volumeNode->GetDisplayNode())
    {
    clonedDisplayNode.TakeReference((vtkMRMLDisplayNode*)scene->CreateNodeByClass(volumeNode->GetDisplayNode()->GetClassName()));
    }
  if (clonedDisplayNode.GetPointer())
    {
    clonedDisplayNode->CopyWithScene(volumeNode->GetDisplayNode());
    scene->AddNode(clonedDisplayNode);
    }

  // clone the volume node
  vtkSmartPointer<vtkMRMLAstroVolumeNode> clonedVolumeNode;
  clonedVolumeNode.TakeReference((vtkMRMLAstroVolumeNode*)scene->CreateNodeByClass(volumeNode->GetClassName()));
  if ( !clonedVolumeNode.GetPointer() )
    {
    vtkErrorWithObjectMacro(volumeNode, "Could not clone volume!");
    return NULL;
    }

  clonedVolumeNode->CopyWithScene(volumeNode);
  clonedVolumeNode->SetAndObserveStorageNodeID(NULL);
  std::string uname = scene->GetUniqueNameByString(name);
  clonedVolumeNode->SetName(uname.c_str());
  if ( clonedDisplayNode )
    {
    clonedVolumeNode->SetAndObserveDisplayNodeID(clonedDisplayNode->GetID());
    }

  if (cloneImageData)
    {
    // copy over the volume's data
    if (volumeNode->GetImageData())
      {
      vtkNew<vtkImageData> clonedVolumeData;
      clonedVolumeData->DeepCopy(volumeNode->GetImageData());
      clonedVolumeNode->SetAndObserveImageData( clonedVolumeData.GetPointer() );
      }
    else
      {
      vtkErrorWithObjectMacro(scene, "CloneVolume: The ImageData of VolumeNode with ID "
                              << volumeNode->GetID() << " is null !");
      }
    }
  else
    {
    clonedVolumeNode->SetAndObserveImageData(NULL);
    }

  // add the cloned volume to the scene
  scene->AddNode(clonedVolumeNode.GetPointer());

  return clonedVolumeNode.GetPointer();
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkSlicerAstroVolumeLogic::
CloneVolumeWithoutImageData(vtkMRMLScene *scene, vtkMRMLVolumeNode *volumeNode, const char *name)
{
  return vtkSlicerAstroVolumeLogic::CloneVolume(scene, volumeNode, name, /*cloneImageData:*/ false );
}
