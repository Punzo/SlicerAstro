// STD includes
#include <algorithm>

// Slicer includes
#include <vtkSlicerVolumesLogic.h>

// AstroVolume includes
#include <vtkSlicerAstroVolumeLogic.h>

// MRML nodes includes
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSliceViewDisplayableManagerFactory.h>

#include <vtkMRMLUnitNode.h>
#include <vtkMRMLThreeDViewDisplayableManagerFactory.h>
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
bool vtkSlicerAstroVolumeLogic::synchronizePresetsToVolumeNode(vtkMRMLNode *node)
{
  if (!node || !node->IsA("vtkMRMLAstroVolumeNode"))
    {
    return false;
    }

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
