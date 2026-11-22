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
#include <vtkSlicerTerminologiesModuleLogic.h>
#include <vtkSlicerTerminologyType.h>
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
#include <vtkMRMLAstroReprojectParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroSegmentationStorageNode.h>
#include <vtkMRMLMessageCollection.h>
#include <vtkMRMLStorageNode.h>
#include <vtkMRMLAstroTerminology.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLCameraNode.h>
#include <vtkMRMLColorTableNode.h>
#include <vtkMRMLColorTableStorageNode.h>
#include <vtkMRMLProceduralColorNode.h>
#include <vtkMRMLProceduralColorStorageNode.h>
#include <vtkMRMLColorNode.h>
#include <vtkLookupTable.h>
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
#include <vtkMRMLSliceLayerLogic.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLApplicationLogic.h>
#include <vtkMRMLSliceViewDisplayableManagerFactory.h>
#include <vtkMRMLThreeDViewDisplayableManagerFactory.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLViewNode.h>
#include <vtkMRMLTableNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkSlicerVolumeRenderingLogic.h>
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
#include <vtkTransform.h>
#include <vtkImageData.h>
#include <vtkImageThreshold.h>
#include <vtkDoubleArray.h>
#include <cctype>
#include <set>
#include <vtkStringArray.h>
#include <vtksys/SystemTools.hxx>
#include <vtkFITSReader.h>
#include <fitsio.h>
#include <vtkSMPTools.h>
#include <vtkSegmentation.h>
#include <vtkIntArray.h>
#include <vtkTable.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSegmentationConverter.h>
#include <vtkPolyData.h>
#include <vtkOrientedImageData.h>
#include <vtkOrientedImageDataResample.h>
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
int StringToInt(const char* str)
{
  return str ? StringToNumber<int>(str) : 0;
}

//----------------------------------------------------------------------------
std::string DoubleToString(double value)
{
  std::ostringstream stream;
  stream.precision(17);
  stream << value;
  return stream.str();
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
  this->AddAstroColorTables();
  this->ApplySceneSettings();
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SetSaveCompressed(bool compressed)
{
  this->SaveCompressed = compressed;
  this->ApplySceneSettings();
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SetDefaultVolumeRenderingQuality(int quality)
{
  this->DefaultVolumeRenderingQuality = quality;
  this->ApplySceneSettings();
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::ApplySceneSettings()
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene)
    {
    return;
    }
  // the write extension of new astro volume storage nodes
  vtkNew<vtkMRMLAstroVolumeStorageNode> storageDefaults;
  storageDefaults->SetDefaultWriteFileExtension(this->SaveCompressed ? "fits.gz" : "fits");
  scene->AddDefaultNode(storageDefaults);
  if (this->DefaultVolumeRenderingQuality < 0)
    {
    return;
    }
  // the quality of the 3D views (and of new ones)
  vtkNew<vtkMRMLViewNode> viewDefaults;
  viewDefaults->SetVolumeRenderingQuality(this->DefaultVolumeRenderingQuality);
  scene->AddDefaultNode(viewDefaults);
  std::vector<vtkMRMLNode*> viewNodes;
  scene->GetNodesByClass("vtkMRMLViewNode", viewNodes);
  for (vtkMRMLNode* node : viewNodes)
    {
    vtkMRMLViewNode* viewNode = vtkMRMLViewNode::SafeDownCast(node);
    if (viewNode && viewNode->GetVolumeRenderingQuality() != this->DefaultVolumeRenderingQuality)
      {
      viewNode->SetVolumeRenderingQuality(this->DefaultVolumeRenderingQuality);
      }
    }
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
namespace
{
struct ColorMapStop
{
  double Position;
  unsigned int Rgb;
};

// Colour-blind safe maps of the SlicerAstro design (lightness increases
// monotonically in the sequential maps; the diverging one has blue and orange
// arms of similar lightness)
const ColorMapStop MapObservatoryStops[] =
{
  {0., 0x070A12}, {0.17, 0x1B1F4A}, {0.33, 0x2B3F8C}, {0.50, 0x1F8FA6},
  {0.67, 0x7FD3C4}, {0.83, 0xF2B24C}, {1., 0xFFF3D6}
};
const ColorMapStop MapDopplerStops[] =
{
  {0., 0x2F6BFF}, {0.25, 0x4FD1E0}, {0.5, 0xE8ECF4}, {0.75, 0xF2B24C}, {1., 0xE5672E}
};
const ColorMapStop MapEmberStops[] =
{
  {0., 0x12081F}, {0.2, 0x45155E}, {0.4, 0x8E2470}, {0.6, 0xD24A4F},
  {0.8, 0xF59A3B}, {1., 0xFCEFB4}
};

struct ColorMapDefinition
{
  const char* Name;
  const char* Description;
  const ColorMapStop* Stops;
  int NumberOfStops;
  /// first entry dark: blank (NaN) pixels of maps, shown with the lowest
  /// colour of the table, stay in the background
  bool DarkFirstEntry;
};

const ColorMapDefinition ColorMaps[] =
{
  {"Observatory", "Intensity: ink, indigo, teal, amber, cream (colour-blind safe).",
   MapObservatoryStops, static_cast<int>(sizeof(MapObservatoryStops) / sizeof(MapObservatoryStops[0])), false},
  {"Doppler", "Velocity: blue (approaching), light grey (systemic), vermillion (receding) (colour-blind safe).",
   MapDopplerStops, static_cast<int>(sizeof(MapDopplerStops) / sizeof(MapDopplerStops[0])), true},
  {"Ember", "Sequential: aubergine, magenta, orange, light yellow; e.g. velocity dispersion (colour-blind safe).",
   MapEmberStops, static_cast<int>(sizeof(MapEmberStops) / sizeof(MapEmberStops[0])), false},
};

const ColorMapDefinition* FindColorMap(const char* name)
{
  if (!name)
    {
    return nullptr;
    }
  for (const ColorMapDefinition& map : ColorMaps)
    {
    if (!strcmp(map.Name, name))
      {
      return &map;
      }
    }
  return nullptr;
}

void InterpolateColorMap(const ColorMapDefinition& map, double value, double rgb[3])
{
  value = std::isfinite(value) ? std::max(0., std::min(1., value)) : 0.;
  int i = 1;
  while (i < map.NumberOfStops - 1 && value > map.Stops[i].Position)
    {
    i++;
    }
  const ColorMapStop& a = map.Stops[i - 1];
  const ColorMapStop& b = map.Stops[i];
  const double t = b.Position > a.Position ? (value - a.Position) / (b.Position - a.Position) : 0.;
  for (int c = 0; c < 3; c++)
    {
    const int shift = 16 - 8 * c;
    const double ca = ((a.Rgb >> shift) & 0xFF) / 255.;
    const double cb = ((b.Rgb >> shift) & 0xFF) / 255.;
    rgb[c] = ca + std::max(0., std::min(1., t)) * (cb - ca);
    }
}
} // end of anonymous namespace

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::GetColorMapColor(const char* name, double value, double rgb[3])
{
  const ColorMapDefinition* map = FindColorMap(name);
  if (!map)
    {
    return false;
    }
  InterpolateColorMap(*map, value, rgb);
  return true;
}

//----------------------------------------------------------------------------
const char* vtkSlicerAstroVolumeLogic::GetMomentMapColorTableName(int order)
{
  return order == 1 ? "Doppler" : (order == 2 ? "Ember" : "Observatory");
}

//----------------------------------------------------------------------------
const char* vtkSlicerAstroVolumeLogic::GetDefaultColorTableName(vtkMRMLVolumeNode* volumeNode)
{
  const char* datamodel = volumeNode ? volumeNode->GetAttribute("SlicerAstro.DATAMODEL") : nullptr;
  if (datamodel && !strcmp(datamodel, "FIRSTMOMENTMAP"))
    {
    return vtkSlicerAstroVolumeLogic::GetMomentMapColorTableName(1);
    }
  if (datamodel && !strcmp(datamodel, "SECONDMOMENTMAP"))
    {
    return vtkSlicerAstroVolumeLogic::GetMomentMapColorTableName(2);
    }
  return "Observatory";
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetDefaultColorTable(vtkMRMLVolumeNode* volumeNode)
{
  vtkMRMLScene* scene = volumeNode ? volumeNode->GetScene() : nullptr;
  vtkMRMLDisplayNode* displayNode = volumeNode ? volumeNode->GetDisplayNode() : nullptr;
  vtkMRMLNode* table = scene ? scene->GetFirstNodeByName(
    vtkSlicerAstroVolumeLogic::GetDefaultColorTableName(volumeNode)) : nullptr;
  if (!displayNode || !table || !table->IsA("vtkMRMLColorTableNode"))
    {
    return false;
    }
  displayNode->SetAndObserveColorNodeID(table->GetID());
  return true;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::AddAstroColorTables()
{
  if (!this->GetMRMLScene() || this->GetMRMLScene()->IsClosing())
    {
    return;
    }

  // colour-blind safe maps of SlicerAstro (Observatory, Doppler, Ember)
  for (const ColorMapDefinition& map : ColorMaps)
    {
    if (this->GetMRMLScene()->GetFirstNodeByName(map.Name))
      {
      continue;
      }
    vtkNew<vtkMRMLColorTableNode> table;
    table->SetType(vtkMRMLColorTableNode::User);
    table->SetName(map.Name);
    // singleton (fixed ID): a scene saved with it refers to the same node
    table->SetSingletonTag((std::string("SlicerAstro.") + map.Name).c_str());
    table->SetDescription(map.Description);
    table->SetNumberOfColors(256);
    for (int i = 0; i < 256; i++)
      {
      double rgb[3];
      InterpolateColorMap(map, i / 255., rgb);
      table->SetColor(i, rgb[0], rgb[1], rgb[2], 1.);
      }
    if (map.DarkFirstEntry)
      {
      table->SetColor(0, 7 / 255., 10 / 255., 18 / 255., 1.);
      }
    table->SetNamesFromColors();
    this->GetMRMLScene()->AddNode(table.GetPointer());
    }

  // Add Astro 2D color functions
  vtkSmartPointer<vtkCollection> colorNodes = vtkSmartPointer<vtkCollection>::Take
      (this->GetMRMLScene()->GetNodesByClassByName("vtkMRMLColorTableNode","Heat"));
  if (colorNodes->GetNumberOfItems() == 0)
    {
    vtkNew<vtkMRMLColorTableNode> HeatColorTableNode;
    HeatColorTableNode->SetType(vtkMRMLColorTableNode::User);
    HeatColorTableNode->SetName("Heat");
    // singleton (fixed ID): a scene saved with it refers to the same node
    // when loaded, instead of adding a copy
    HeatColorTableNode->SetSingletonTag("SlicerAstro.Heat");
    HeatColorTableNode->SetDescription("A scale from red to yellow.");
    HeatColorTableNode->SetNumberOfColors(256);

    // Red component
    vtkNew<vtkLookupTable> RedLookupTable;
    RedLookupTable->SetNumberOfTableValues(85);
    RedLookupTable->SetTableRange(0, 85);
    RedLookupTable->SetHueRange(0, 0);
    RedLookupTable->SetSaturationRange(1,1);
    RedLookupTable->SetValueRange(0.,1);
    RedLookupTable->SetRampToLinear();
    RedLookupTable->ForceBuild();

    // Green component
    vtkNew<vtkLookupTable> GreenLookupTable;
    GreenLookupTable->SetNumberOfTableValues(256);
    GreenLookupTable->SetTableRange(0, 256);
    GreenLookupTable->SetHueRange(0.333, 0.333);
    GreenLookupTable->SetSaturationRange(1,1);
    GreenLookupTable->SetValueRange(0.,1);
    GreenLookupTable->SetRampToLinear();
    GreenLookupTable->ForceBuild();

    // Blue component
    vtkNew<vtkLookupTable> BlueLookupTable;
    BlueLookupTable->SetNumberOfTableValues(85);
    BlueLookupTable->SetTableRange(0, 85);
    BlueLookupTable->SetHueRange(0.667, 0.667);
    BlueLookupTable->SetSaturationRange(1,1);
    BlueLookupTable->SetValueRange(0,1);
    BlueLookupTable->SetRampToLinear();
    BlueLookupTable->ForceBuild();

    for (int ii = 0; ii < 85; ii++)
      {
      double RGBRed[3], RGBGreen[3], RGBA[4];
      RedLookupTable->GetTableValue(ii, RGBRed);
      GreenLookupTable->GetTableValue(ii, RGBGreen);

      RGBA[0] = RGBRed[0];
      RGBA[1] = RGBGreen[1];
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      HeatColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 85; ii < 171; ii++)
      {
      double RGBGreen[3], RGBA[4];
      GreenLookupTable->GetTableValue(ii, RGBGreen);

      RGBA[0] = 1.;
      RGBA[1] = RGBGreen[1];
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      HeatColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 171; ii < 256; ii++)
      {
      double RGBGreen[3], RGBBlue[3], RGBA[4];
      GreenLookupTable->GetTableValue(ii, RGBGreen);
      BlueLookupTable->GetTableValue(ii - 171, RGBBlue);

      RGBA[0] = 1.;
      RGBA[1] = RGBGreen[1];
      RGBA[2] = RGBBlue[2];
      RGBA[3] = 1.;
      HeatColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }

    HeatColorTableNode->SetNamesFromColors();
    this->GetMRMLScene()->AddNode(HeatColorTableNode.GetPointer());
    }

  colorNodes = vtkSmartPointer<vtkCollection>::Take
      (this->GetMRMLScene()->GetNodesByClassByName("vtkMRMLColorTableNode","Ronekers"));
  if (colorNodes->GetNumberOfItems() == 0)
    {
    vtkNew<vtkMRMLColorTableNode> RonekersColorTableNode;
    RonekersColorTableNode->SetType(vtkMRMLColorTableNode::User);
    RonekersColorTableNode->SetName("Ronekers");
    // singleton (fixed ID): a scene saved with it refers to the same node
    // when loaded, instead of adding a copy
    RonekersColorTableNode->SetSingletonTag("SlicerAstro.Ronekers");
    RonekersColorTableNode->SetDescription("Discrete rainbow color function. Very useful to visualize for Astro HI datasets.");
    RonekersColorTableNode->SetNumberOfColors(256);

    for (int ii = 0; ii < 28; ii++)
      {
      double RGBA[4];

      RGBA[0] = 0.199;
      RGBA[1] = 0.199;
      RGBA[2] = 0.199;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 28; ii < 53; ii++)
      {
      double RGBA[4];

      RGBA[0] = 0.473;
      RGBA[1] = 0.;
      RGBA[2] = 0.606;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 53; ii < 78; ii++)
      {
      double RGBA[4];

      RGBA[0] = 0.;
      RGBA[1] = 0.;
      RGBA[2] = 0.781;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 78; ii < 103; ii++)
      {
      double RGBA[4];

      RGBA[0] = 0.371;
      RGBA[1] = 0.652;
      RGBA[2] = 0.922;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 103; ii < 128; ii++)
      {
      double RGBA[4];

      RGBA[0] = 0.;
      RGBA[1] = 0.566;
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 128; ii < 153; ii++)
      {
      double RGBA[4];

      RGBA[0] = 0.;
      RGBA[1] = 0.961;
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 153; ii < 178; ii++)
      {
      double RGBA[4];

      RGBA[0] = 1.;
      RGBA[1] = 1.;
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 178; ii < 203; ii++)
      {
      double RGBA[4];

      RGBA[0] = 1.;
      RGBA[1] = 0.691;
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 203; ii < 228; ii++)
      {
      double RGBA[4];

      RGBA[0] = 1.;
      RGBA[1] = 0.;
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 228; ii < 256; ii++)
      {
      double RGBA[4];

      RGBA[0] = 1.;
      RGBA[1] = 1.;
      RGBA[2] = 1.;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }

    RonekersColorTableNode->SetNamesFromColors();
    this->GetMRMLScene()->AddNode(RonekersColorTableNode.GetPointer());
    }

  colorNodes = vtkSmartPointer<vtkCollection>::Take
      (this->GetMRMLScene()->GetNodesByClassByName("vtkMRMLColorTableNode","VelocityField"));
  if (colorNodes->GetNumberOfItems() == 0)
    {
    vtkNew<vtkMRMLColorTableNode> VelocityFieldColorTableNode;
    VelocityFieldColorTableNode->SetType(vtkMRMLColorTableNode::User);
    VelocityFieldColorTableNode->SetName("VelocityField");
    // singleton (fixed ID): a scene saved with it refers to the same node
    // when loaded, instead of adding a copy
    VelocityFieldColorTableNode->SetSingletonTag("SlicerAstro.VelocityField");
    VelocityFieldColorTableNode->SetDescription("A scale from blue to red.");
    VelocityFieldColorTableNode->SetNumberOfColors(256);

    // Red component
    vtkNew<vtkLookupTable> RedLookupTable;
    RedLookupTable->SetNumberOfTableValues(128);
    RedLookupTable->SetTableRange(0, 128);
    RedLookupTable->SetHueRange(0, 0);
    RedLookupTable->SetSaturationRange(1,1);
    RedLookupTable->SetValueRange(0.,1);
    RedLookupTable->SetRampToLinear();
    RedLookupTable->ForceBuild();

    // Green component
    vtkNew<vtkLookupTable> GreenLookupTable;
    GreenLookupTable->SetNumberOfTableValues(128);
    GreenLookupTable->SetTableRange(0, 128);
    GreenLookupTable->SetHueRange(0.333, 0.333);
    GreenLookupTable->SetSaturationRange(1,1);
    GreenLookupTable->SetValueRange(0.,1);
    GreenLookupTable->SetRampToLinear();
    GreenLookupTable->ForceBuild();

    // Blue component
    vtkNew<vtkLookupTable> BlueLookupTable;
    BlueLookupTable->SetNumberOfTableValues(128);
    BlueLookupTable->SetTableRange(0, 128);
    BlueLookupTable->SetHueRange(0.667, 0.667);
    BlueLookupTable->SetSaturationRange(1,1);
    BlueLookupTable->SetValueRange(0,1);
    BlueLookupTable->SetRampToLinear();
    BlueLookupTable->ForceBuild();

    VelocityFieldColorTableNode->GetLookupTable()->SetTableValue(0, 0, 0, 0);
    for (int ii = 1; ii < 128; ii++)
      {
      double RGBBlue[3], RGBGreen[3], RGBA[4];
      int blueIndex = 128 - ii;
      BlueLookupTable->GetTableValue(blueIndex, RGBBlue);
      GreenLookupTable->GetTableValue(ii, RGBGreen);

      RGBA[0] = 0.;
      RGBA[1] = RGBGreen[1];
      RGBA[2] = RGBBlue[2];
      RGBA[3] = 1.;

      VelocityFieldColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 128; ii < 255; ii++)
      {
      double RGBGreen[3], RGBRed[3], RGBA[4];
      int redIndex = ii - 128;
      int greeIndex = 128 - (redIndex);
      GreenLookupTable->GetTableValue(greeIndex, RGBGreen);
      RedLookupTable->GetTableValue(redIndex, RGBRed);

      RGBA[0] = RGBRed[0];
      RGBA[1] = RGBGreen[1];
      RGBA[2] = 0.;
      RGBA[3] = 1.;

      VelocityFieldColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    VelocityFieldColorTableNode->GetLookupTable()->SetTableValue(255, 0, 0, 0);
    VelocityFieldColorTableNode->SetNamesFromColors();
    this->GetMRMLScene()->AddNode(VelocityFieldColorTableNode.GetPointer());
    }

  colorNodes = vtkSmartPointer<vtkCollection>::Take
      (this->GetMRMLScene()->GetNodesByClassByName("vtkMRMLColorTableNode","Rainbow"));
  if (colorNodes->GetNumberOfItems() == 0)
    {
    // Add Rainbow
    vtkNew<vtkMRMLColorTableNode> RainbowTableNode;
    RainbowTableNode->SetName("Rainbow");
    RainbowTableNode->SetType(vtkMRMLColorTableNode::User);
    RainbowTableNode->SetDescription("Goes from red to purple, passing through the colors of the rainbow in between.");
    RainbowTableNode->SetNumberOfColors(256);
    RainbowTableNode->GetLookupTable()->SetNumberOfTableValues(256);
    RainbowTableNode->GetLookupTable()->SetHueRange(0., 0.8);
    RainbowTableNode->GetLookupTable()->SetSaturationRange(1,1);
    RainbowTableNode->GetLookupTable()->SetValueRange(1,1);
    RainbowTableNode->GetLookupTable()->SetRampToLinear();
    RainbowTableNode->GetLookupTable()->ForceBuild();
    RainbowTableNode->SetColor(0, 0, 0, 0);
    RainbowTableNode->SetColor(255, 0, 0, 0);
    RainbowTableNode->SetNamesFromColors();
    this->GetMRMLScene()->AddNode(RainbowTableNode.GetPointer());
    }
  else
    {
    vtkMRMLColorTableNode* RainbowTableNode = vtkMRMLColorTableNode::SafeDownCast(colorNodes->GetItemAsObject(0));
    RainbowTableNode->SetName("Rainbow");
    RainbowTableNode->SetType(vtkMRMLColorTableNode::User);
    RainbowTableNode->SetDescription("Goes from red to purple, passing through the colors of the rainbow in between.");
    RainbowTableNode->SetNumberOfColors(256);
    RainbowTableNode->GetLookupTable()->SetNumberOfTableValues(256);
    RainbowTableNode->GetLookupTable()->SetHueRange(0., 0.8);
    RainbowTableNode->GetLookupTable()->SetSaturationRange(1,1);
    RainbowTableNode->GetLookupTable()->SetValueRange(1,1);
    RainbowTableNode->GetLookupTable()->SetRampToLinear();
    RainbowTableNode->GetLookupTable()->ForceBuild();
    RainbowTableNode->SetColor(0, 0, 0, 0);
    RainbowTableNode->SetColor(255, 0, 0, 0);
    RainbowTableNode->SetNamesFromColors();
    }

  // Remove unwanted 2D color functions
  colorNodes = vtkSmartPointer<vtkCollection>::Take(
      this->GetMRMLScene()->GetNodesByClass("vtkMRMLColorTableNode"));
  for (int ii = 0; ii < colorNodes->GetNumberOfItems(); ii++)
    {
    vtkMRMLColorTableNode* tempColorTableNode = vtkMRMLColorTableNode::SafeDownCast
            (colorNodes->GetItemAsObject(ii));
    if (!tempColorTableNode)
      {
      continue;
      }
    if (!strcmp(tempColorTableNode->GetName(), "Grey") ||
        !strcmp(tempColorTableNode->GetName(), "Heat") ||
        !strcmp(tempColorTableNode->GetName(), "Ronekers") ||
        !strcmp(tempColorTableNode->GetName(), "VelocityField") ||
        !strcmp(tempColorTableNode->GetName(), "Observatory") ||
        !strcmp(tempColorTableNode->GetName(), "Doppler") ||
        !strcmp(tempColorTableNode->GetName(), "Ember") ||
        !strcmp(tempColorTableNode->GetName(), "Rainbow"))
      {
      tempColorTableNode->SetAttribute("SlicerAstro.AddFunctions", "on");
      tempColorTableNode->SetAttribute("SlicerAstro.Reverse", "off");
      tempColorTableNode->SetAttribute("SlicerAstro.Inverse", "off");
      tempColorTableNode->SetAttribute("SlicerAstro.Log", "off");
      continue;
      }
    if (!strcmp(tempColorTableNode->GetName(), "GenericColors") ||
        !strcmp(tempColorTableNode->GetName(), "DarkBrightChartColors") ||
        !strcmp(tempColorTableNode->GetName(), "Random"))
      {
      continue;
      }
    // color table of the astronomical terminology of the segments
    if (tempColorTableNode->GetSingletonTag() &&
        !strcmp(tempColorTableNode->GetSingletonTag(), vtkSlicerAstroVolumeLogic::GetAstroTerminologyColorTableSingletonTag()))
      {
      continue;
      }

    vtkMRMLColorTableStorageNode* tempColorTableStorageNode =
      vtkMRMLColorTableStorageNode::SafeDownCast
        (tempColorTableNode->GetStorageNode());
    this->GetMRMLScene()->RemoveNode(tempColorTableStorageNode);
    this->GetMRMLScene()->RemoveNode(tempColorTableNode);
    }

  vtkSmartPointer<vtkCollection> ProceduralColorTableNodeCol =
    vtkSmartPointer<vtkCollection>::Take(
      this->GetMRMLScene()->GetNodesByClass("vtkMRMLProceduralColorNode"));
  for (int ii = 0; ii < ProceduralColorTableNodeCol->GetNumberOfItems(); ii++)
    {
    vtkMRMLProceduralColorNode* tempProceduralColorTableNode = vtkMRMLProceduralColorNode::SafeDownCast
            (ProceduralColorTableNodeCol->GetItemAsObject(ii));
    if (!tempProceduralColorTableNode)
      {
      continue;
      }

    vtkMRMLProceduralColorStorageNode* tempProceduralColorTableStorageNode =
      vtkMRMLProceduralColorStorageNode::SafeDownCast
        (tempProceduralColorTableNode->GetStorageNode());
    this->GetMRMLScene()->RemoveNode(tempProceduralColorTableStorageNode);
    this->GetMRMLScene()->RemoveNode(tempProceduralColorTableNode);
    }

  // GenericColors, DarkBrightChartColors and Random (color tables of Slicer,
  // e.g. for segments and labels) are kept unmodified: copies of them were
  // saved with the scene and could not be read again (Random)

  // Ensure Lookup Table is updated
  colorNodes = vtkSmartPointer<vtkCollection>::Take(
      this->GetMRMLScene()->GetNodesByClass("vtkMRMLColorTableNode"));
  for (int ii = 0; ii < colorNodes->GetNumberOfItems(); ii++)
    {
    vtkMRMLColorTableNode* tempColorTableNode = vtkMRMLColorTableNode::SafeDownCast
            (colorNodes->GetItemAsObject(ii));
    if (!tempColorTableNode)
      {
      continue;
      }
    tempColorTableNode->GetLookupTable()->Modified();
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::OnMRMLSceneEndClose()
{
  this->ShownActiveVolumeID.clear();
  this->UpdateSliceOrientationPresets();
  // the closing of the scene emptied the singleton color table
  this->AddAstroTerminologyColorTable();
  this->AddAstroColorTables();
}

//----------------------------------------------------------------------------
const char* vtkSlicerAstroVolumeLogic::GetIncompatibleSceneMessage()
{
  return "This scene was saved with a SlicerAstro version older than 2.0 and is not compatible with "
         "SlicerAstro 2: masks were label map volumes (now segmentations) and the parameters of the "
         "modules changed. Masks, mask inputs and module settings of the scene are not restored. "
         "Load the FITS files of the data (and masks) instead, and save a new scene.";
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::OnMRMLSceneEndImport()
{
  // scenes of SlicerAstro < 2.0: their astronomical volumes have no scene format
  this->IncompatibleSceneImported = false;
  std::vector<vtkMRMLNode*> importedVolumes;
  this->GetMRMLScene()->GetNodesByClass("vtkMRMLAstroVolumeNode", importedVolumes);
  for (vtkMRMLNode* node : importedVolumes)
    {
    vtkMRMLAstroVolumeNode* volumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(node);
    if (volumeNode && volumeNode->GetSceneFormat() < vtkMRMLAstroVolumeNode::GetCurrentSceneFormat())
      {
      this->IncompatibleSceneImported = true;
      }
    }
  if (this->IncompatibleSceneImported)
    {
    vtkErrorMacro("vtkSlicerAstroVolumeLogic: " << vtkSlicerAstroVolumeLogic::GetIncompatibleSceneMessage());
    this->InvokeEvent(vtkSlicerAstroVolumeLogic::IncompatibleSceneEvent);
    }

  this->UpdateSliceOrientationPresets();
  // markups of the scene (their measurements were read as static copies)
  this->UpdateAllAstroMeasurements();

  std::vector<vtkMRMLNode*> segmentationNodes;
  this->GetMRMLScene()->GetNodesByClass("vtkMRMLSegmentationNode", segmentationNodes);
  for (vtkMRMLNode* node : segmentationNodes)
    {
    this->EnsureFITSMaskStorageNode(vtkMRMLSegmentationNode::SafeDownCast(node));
    vtkSlicerAstroVolumeLogic::EnsureAstroTerminology(vtkMRMLSegmentationNode::SafeDownCast(node));
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

  // the active volume of the scene (its references, e.g. the 3D color
  // function, are restored now)
  this->ShownActiveVolumeID.clear();
  this->ShowSelectedActiveVolume();
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
  if (astroVolume)
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
      vtkSlicerAstroVolumeLogic::EnsureAstroTerminology(vtkMRMLSegmentationNode::SafeDownCast(segmentationNode));
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
      vtkSlicerAstroVolumeLogic::EnsureAstroTerminology(segmentationNode);
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

  if (node->IsA("vtkMRMLAstroVolumeNode"))
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
  // astronomical data are always centered on their voxel grid (the RAS
  // coordinates are the voxel coordinates around the center): cubes and
  // masks overlap whatever the order and the way of loading
  (void)options;
  storageNode->SetCenterImage(1);
  // the extension and the part of the file chosen for this load (volume
  // reader of the AstroVolume module)
  vtkMRMLAstroVolumeStorageNode::ApplyNextReadOptions(storageNode);
  astroNode->SetAndObserveStorageNodeID(storageNode->GetID());

  nodeSet.StorageNode = storageNode;
  nodeSet.DisplayNode = adisplayNode;
  nodeSet.Node = astroNode;

  return nodeSet;
}

} // end of anonymous namespace

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
  this->GetMRMLScene()->RegisterNodeClass(vtkNew<vtkMRMLAstroSegmentationStorageNode>().GetPointer());

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

  std::string singletonTag = std::string("SlicerAstro.Preset.") + vtkMRMLAstroVolumeNode::GetPresetIdentifier(preset);
  vtkMRMLVolumePropertyNode* scenePreset = vtkMRMLVolumePropertyNode::SafeDownCast(
    scene->GetSingletonNode(singletonTag.c_str(), "vtkMRMLVolumePropertyNode"));
  if (scenePreset)
    {
    return scenePreset;
    }

  vtkNew<vtkMRMLVolumePropertyNode> newScenePreset;
  newScenePreset->Copy(preset);
  newScenePreset->SetName(preset->GetName());
  // the identifier (attributes are not copied by Copy)
  newScenePreset->SetAttribute(vtkMRMLAstroVolumeNode::GetPresetIdentifierAttributeName(),
                               vtkMRMLAstroVolumeNode::GetPresetIdentifier(preset).c_str());
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
  // loaded once (until a load succeeds): reloading would replace the preset
  // nodes, and the preset selectors observing the presets scene would select
  // (and apply to the active volume) the first preset
  if (!this->PresetsLoaded)
    {
    this->PresetsLoaded = this->LoadPresets(this->PresetsScene);
    return this->PresetsScene;
    }
  // the MRML scene keeps its own copies (singletons), recreated after a
  // scene close
  vtkSmartPointer<vtkCollection> presets = vtkSmartPointer<vtkCollection>::Take(
    this->PresetsScene->GetNodesByClass("vtkMRMLVolumePropertyNode"));
  if (this->GetMRMLScene())
    {
    for (int i = 0; i < presets->GetNumberOfItems(); i++)
      {
      this->GetScenePresetNode(vtkMRMLNode::SafeDownCast(presets->GetItemAsObject(i)));
      }
    }
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
    clone->SetName(vtkSlicerAstroVolumeLogic::GetFreeNodeName(scene, name ? name : "Volume").c_str());
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

  // Voxels of the segment (effective extent of its binary labelmap: the
  // labelmap image itself can cover the whole reference geometry)
  const std::string segmentID = segmentationNode->GetSegmentation() ?
    segmentationNode->GetSegmentation()->GetSegmentIdBySegment(segment) : std::string();
  vtkNew<vtkOrientedImageData> segmentLabelmap;
  if (segmentID.empty() ||
      !segmentationNode->GetBinaryLabelmapRepresentation(segmentID, segmentLabelmap.GetPointer()))
    {
    return false;
    }
  int effectiveExtent[6] = {0, -1, 0, -1, 0, -1};
  if (!vtkOrientedImageDataResample::CalculateEffectiveExtent(segmentLabelmap.GetPointer(), effectiveExtent) ||
      effectiveExtent[0] > effectiveExtent[1] || effectiveExtent[2] > effectiveExtent[3] ||
      effectiveExtent[4] > effectiveExtent[5])
    {
    // empty segment
    return false;
    }

  // labelmap voxel edges -> volume IJK
  vtkNew<vtkMatrix4x4> labelmapToSegment;
  segmentLabelmap->GetImageToWorldMatrix(labelmapToSegment.GetPointer());
  vtkMatrix4x4::Multiply4x4(segmentToVolumeIJKTransformMatrix.GetPointer(), labelmapToSegment.GetPointer(),
    segmentToVolumeIJKTransformMatrix.GetPointer());
  const double SegmentExtent[6] =
    {
    effectiveExtent[0] - 0.5, effectiveExtent[1] + 0.5,
    effectiveExtent[2] - 0.5, effectiveExtent[3] + 0.5,
    effectiveExtent[4] - 0.5, effectiveExtent[5] + 0.5
    };

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
//----------------------------------------------------------------------------
// mask[volume index] = 1 where labelmap == labelValue, over the voxels of the
// labelmap extent inside the volume extent (labelmap on the volume grid)
template <typename T>
void FillSegmentMask(vtkImageData* labelmap, double labelValue, const int volumeExtent[6], unsigned char* mask)
{
  const int* labelmapExtent = labelmap->GetExtent();
  int extent[6] = {0, -1, 0, -1, 0, -1};
  for (int axis = 0; axis < 3; axis++)
    {
    extent[2 * axis] = std::max(labelmapExtent[2 * axis], volumeExtent[2 * axis]);
    extent[2 * axis + 1] = std::min(labelmapExtent[2 * axis + 1], volumeExtent[2 * axis + 1]);
    if (extent[2 * axis] > extent[2 * axis + 1])
      {
      return;
      }
    }
  const T label = static_cast<T>(labelValue);
  const vtkIdType nx = volumeExtent[1] - volumeExtent[0] + 1;
  const vtkIdType ny = volumeExtent[3] - volumeExtent[2] + 1;
  vtkSMPTools::For(extent[4], extent[5] + 1, [&](vtkIdType firstZ, vtkIdType lastZ)
    {
    for (vtkIdType z = firstZ; z < lastZ; z++)
      {
      for (int y = extent[2]; y <= extent[3]; y++)
        {
        const T* in = static_cast<const T*>(labelmap->GetScalarPointer(extent[0], y, static_cast<int>(z)));
        unsigned char* out = mask + ((z - volumeExtent[4]) * ny + (y - volumeExtent[2])) * nx + (extent[0] - volumeExtent[0]);
        for (int x = extent[0]; x <= extent[1]; x++, in++, out++)
          {
          *out = (*in == label) ? 1 : 0;
          }
        }
      }
    });
}

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
bool vtkSlicerAstroVolumeLogic::CreateSegmentMask(vtkMRMLSegmentationNode* segmentationNode, const char* segmentID,
                                                  vtkMRMLVolumeNode* volumeNode, vtkImageData* mask)
{
  std::string error;
  if (!vtkSlicerAstroVolumeLogic::CreateSegmentMask(segmentationNode, segmentID, volumeNode, mask, error))
    {
    vtkGenericWarningMacro("vtkSlicerAstroVolumeLogic::CreateSegmentMask: " << error);
    return false;
    }
  return true;
}

//---------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::CreateSegmentMask(vtkMRMLSegmentationNode* segmentationNode, const char* segmentID,
                                                  vtkMRMLVolumeNode* volumeNode, vtkImageData* mask, std::string& error)
{
  vtkImageData* volumeImage = volumeNode ? volumeNode->GetImageData() : nullptr;
  if (!mask || !volumeImage)
    {
    error = "the data have no voxels.";
    return false;
    }
  vtkSegmentation* segmentation = segmentationNode ? segmentationNode->GetSegmentation() : nullptr;
  vtkSegment* segment = (segmentation && segmentID) ? segmentation->GetSegment(segmentID) : nullptr;
  if (!segment)
    {
    error = std::string("the mask segment ") + (segmentID ? segmentID : "") + " was not found" +
            (segmentationNode && segmentationNode->GetName() ? std::string(" in ") + segmentationNode->GetName() : "") + ".";
    return false;
    }

  int volumeExtent[6] = {0, -1, 0, -1, 0, -1};
  volumeImage->GetExtent(volumeExtent);
  mask->SetExtent(volumeExtent);
  mask->SetOrigin(volumeImage->GetOrigin());
  mask->SetSpacing(volumeImage->GetSpacing());
  mask->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  unsigned char* maskVoxels = static_cast<unsigned char*>(mask->GetScalarPointer());
  std::fill(maskVoxels, maskVoxels + mask->GetNumberOfPoints(), static_cast<unsigned char>(0));

  // mask of another data volume (its reference geometry): placed with the
  // WCS when the two volumes are on the same voxel lattice (e.g. a cube and
  // its cutout, a 2D image of the same sky grid), otherwise refused (the
  // positions in the 3D views of independently loaded data do not match)
  vtkMRMLAstroVolumeNode* referenceVolume = vtkMRMLAstroVolumeNode::SafeDownCast(segmentationNode->GetNodeReference(
    vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str()));
  vtkMRMLAstroVolumeNode* astroVolume = vtkMRMLAstroVolumeNode::SafeDownCast(volumeNode);
  if (referenceVolume && astroVolume && referenceVolume != astroVolume && referenceVolume->GetImageData())
    {
    int offset[3] = {0, 0, 0};
    const int referenceAxes = StringToInt(referenceVolume->GetAttribute("SlicerAstro.NAXIS"));
    const int volumeAxes = StringToInt(astroVolume->GetAttribute("SlicerAstro.NAXIS"));
    if (!vtkSlicerAstroVolumeLogic::GetGridOffset(referenceVolume, astroVolume, offset) ||
        !(referenceAxes == volumeAxes || (referenceAxes == 3 && volumeAxes == 2)))
      {
      error = std::string("the mask ") + (segmentationNode->GetName() ? segmentationNode->GetName() : "") +
              " is on the grid of " + referenceVolume->GetName() + ", which is not the voxel grid of " +
              astroVolume->GetName() + ": copy the mask to " + astroVolume->GetName() +
              " (AstroVolume module, Masks, Copy).";
      return false;
      }
    vtkNew<vtkImageData> referenceMask;
    if (!vtkSlicerAstroVolumeLogic::CreateSegmentMask(segmentationNode, segmentID, referenceVolume,
                                                      referenceMask, error))
      {
      return false;
      }
    int referenceDimensions[3] = {0, 0, 0};
    referenceMask->GetDimensions(referenceDimensions);
    int dimensions[3] = {0, 0, 0};
    mask->GetDimensions(dimensions);
    const bool projection = referenceAxes == 3 && volumeAxes == 2;
    const unsigned char* referenceVoxels = static_cast<unsigned char*>(referenceMask->GetScalarPointer());
    for (int k = 0; k < referenceDimensions[2]; k++)
      {
      const int vk = projection ? 0 : k + offset[2];
      if (vk < 0 || vk >= dimensions[2])
        {
        continue;
        }
      for (int j = 0; j < referenceDimensions[1]; j++)
        {
        const int vj = j + offset[1];
        if (vj < 0 || vj >= dimensions[1])
          {
          continue;
          }
        const unsigned char* row = referenceVoxels +
          (static_cast<vtkIdType>(k) * referenceDimensions[1] + j) * referenceDimensions[0];
        unsigned char* maskRow = maskVoxels + (static_cast<vtkIdType>(vk) * dimensions[1] + vj) * dimensions[0];
        for (int i = 0; i < referenceDimensions[0]; i++)
          {
          const int vi = i + offset[0];
          if (row[i] && vi >= 0 && vi < dimensions[0])
            {
            maskRow[vi] = 1;
            }
          }
        }
      }
    mask->Modified();
    return true;
    }

  // binary labelmap of the segment: the one of the segmentation (shared by
  // the segments of a layer, each with its label value), or converted for
  // this segment only (e.g. closed surfaces drawn as source)
  const std::string labelmapName = vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName();
  vtkSmartPointer<vtkOrientedImageData> labelmap = vtkOrientedImageData::SafeDownCast(segment->GetRepresentation(labelmapName));
  double labelValue = segment->GetLabelValue();
  if (!labelmap)
    {
    vtkNew<vtkSegmentation> single;
    single->SetSourceRepresentationName(segmentation->GetSourceRepresentationName());
    single->CopyConversionParameters(segmentation);
    single->CopySegmentFromSegmentation(segmentation, segmentID);
    vtkSegment* copy = single->GetSegment(segmentID);
    if (!copy || !single->CreateRepresentation(labelmapName))
      {
      error = std::string("the mask segment ") + segmentID + " could not be converted to a binary labelmap.";
      return false;
      }
    labelmap = vtkOrientedImageData::SafeDownCast(copy->GetRepresentation(labelmapName));
    labelValue = copy->GetLabelValue();
    }
  if (!labelmap || !labelmap->GetPointData() || !labelmap->GetPointData()->GetScalars() ||
      labelmap->GetNumberOfScalarComponents() != 1)
    {
    // empty segment: empty mask
    return true;
    }

  // voxel grid of the volume (its RAS geometry) as reference
  vtkNew<vtkOrientedImageData> reference;
  vtkNew<vtkMatrix4x4> ijkToRAS;
  volumeNode->GetIJKToRASMatrix(ijkToRAS);
  reference->SetGeometryFromImageToWorldMatrix(ijkToRAS);
  reference->SetExtent(volumeExtent);

  // transform from the segmentation to the volume (parent transforms)
  vtkNew<vtkGeneralTransform> segmentationToVolume;
  vtkMRMLTransformNode::GetTransformBetweenNodes(segmentationNode->GetParentTransformNode(),
                                                 volumeNode->GetParentTransformNode(), segmentationToVolume);
  vtkNew<vtkTransform> segmentationToVolumeLinear;
  const bool identity = vtkMRMLTransformNode::IsGeneralTransformLinear(segmentationToVolume, segmentationToVolumeLinear) &&
                        segmentationToVolumeLinear->GetMatrix()->IsIdentity();

  vtkSmartPointer<vtkOrientedImageData> source = labelmap;
  if (!identity || !vtkOrientedImageDataResample::DoGeometriesMatch(reference, labelmap))
    {
    source = vtkSmartPointer<vtkOrientedImageData>::New();
    if (!vtkOrientedImageDataResample::ResampleOrientedImageToReferenceOrientedImage(
          labelmap, reference, source, false, false, identity ? nullptr : segmentationToVolume.GetPointer()))
      {
      error = std::string("the mask segment ") + segmentID + " could not be resampled to the voxels of the data.";
      return false;
      }
    }
  switch (source->GetScalarType())
    {
    vtkTemplateMacro(FillSegmentMask<VTK_TT>(source, labelValue, volumeExtent, maskVoxels));
    default:
      error = "unsupported scalar type of the mask segment.";
      return false;
    }
  mask->Modified();
  return true;
}

//---------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SetReprojectFunction(const ReprojectFunctionType& function)
{
  this->ReprojectFunction = function;
}

//---------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::Reproject(vtkMRMLAstroReprojectParametersNode *pnode)
{
  bool result = false;
  if (!this->StartReproject(pnode, [this, &result](bool success, const std::string& error)
      {
      result = success;
      this->LastReprojectError = error;
      }, true))
    {
    return false;
    }
  return result;
}

//---------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::StartReproject(vtkMRMLAstroReprojectParametersNode* pnode,
                                               const ReprojectDoneType& done, bool synchronous)
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
  return this->ReprojectFunction(pnode, done, synchronous, this->LastReprojectError);
}

//---------------------------------------------------------------------------
namespace
{
//----------------------------------------------------------------------------
/// A thin surface of a 3D colour function at a multiple of the noise
struct PresetShell
{
  double Sigma;
  double Opacity;
  unsigned int Rgb;
};

typedef std::vector<std::pair<double, unsigned int> > ColorStops;

//----------------------------------------------------------------------------
ColorStops ObservatoryStops()
{
  return {{0., 0x2B3F8C}, {0.25, 0x1F8FA6}, {0.5, 0x7FD3C4}, {0.75, 0xF2B24C}, {1., 0xFFF3D6}};
}

//----------------------------------------------------------------------------
/// magma and viridis (perceptually uniform colour maps of matplotlib, CC0),
/// without their darkest part (invisible on the dark background)
ColorStops MagmaStops()
{
  return {{0., 0x3B0F70}, {0.25, 0x8C2981}, {0.5, 0xDE4968}, {0.75, 0xFE9F6D}, {1., 0xFCFDBF}};
}

//----------------------------------------------------------------------------
ColorStops ViridisStops()
{
  return {{0., 0x440154}, {0.25, 0x3B528B}, {0.5, 0x21918C}, {0.75, 0x5EC962}, {1., 0xFDE725}};
}

//----------------------------------------------------------------------------
void RgbComponents(unsigned int rgb, double components[3])
{
  components[0] = ((rgb >> 16) & 0xFF) / 255.;
  components[1] = ((rgb >> 8) & 0xFF) / 255.;
  components[2] = (rgb & 0xFF) / 255.;
}

//----------------------------------------------------------------------------
unsigned int ColorMapRgb(const ColorStops& stops, double value)
{
  value = std::max(0., std::min(1., value));
  for (size_t i = 1; i < stops.size(); i++)
    {
    if (value <= stops[i].first)
      {
      const double t = (value - stops[i - 1].first) / (stops[i].first - stops[i - 1].first);
      double a[3], b[3];
      RgbComponents(stops[i - 1].second, a);
      RgbComponents(stops[i].second, b);
      unsigned int rgb = 0;
      for (int c = 0; c < 3; c++)
        {
        rgb = (rgb << 8) | static_cast<unsigned int>(std::lround(255. * (a[c] + t * (b[c] - a[c]))));
        }
      return rgb;
      }
    }
  return stops.back().second;
}

//----------------------------------------------------------------------------
/// Thin surfaces at multiples of the noise (half width 0.2 level, at most
/// half the noise for high levels); negative multiples are absorption;
/// levels outside of the data are skipped
void SetPresetShells(vtkMRMLVolumePropertyNode* node, std::vector<PresetShell> shells,
                     double noise, double min, double max)
{
  std::sort(shells.begin(), shells.end(),
            [](const PresetShell& a, const PresetShell& b) { return a.Sigma < b.Sigma; });
  vtkPiecewiseFunction* opacity = node->GetScalarOpacity();
  vtkColorTransferFunction* color = node->GetColor();
  opacity->RemoveAllPoints();
  color->RemoveAllPoints();
  opacity->AddPoint(min, 0.);
  double rgb[3];
  RgbComponents(shells.front().Rgb, rgb);
  color->AddRGBPoint(min, rgb[0], rgb[1], rgb[2]);
  for (const PresetShell& shell : shells)
    {
    const double level = shell.Sigma * noise;
    const double absoluteSigma = std::fabs(shell.Sigma);
    const double halfWidth = (absoluteSigma < 10. ? 0.2 * absoluteSigma : 0.5) * noise;
    if (level - halfWidth <= min || level + halfWidth >= max)
      {
      continue;
      }
    opacity->AddPoint(level - halfWidth, 0., 0.5, 0.2);
    opacity->AddPoint(level, shell.Opacity, 0.5, 0.2);
    opacity->AddPoint(level + halfWidth, 0., 0.5, 0.2);
    RgbComponents(shell.Rgb, rgb);
    color->AddRGBPoint(level, rgb[0], rgb[1], rgb[2]);
    }
  opacity->AddPoint(max, 0.);
  RgbComponents(shells.back().Rgb, rgb);
  color->AddRGBPoint(max, rgb[0], rgb[1], rgb[2]);
  vtkPiecewiseFunction* gradientOpacity = node->GetGradientOpacity();
  gradientOpacity->RemoveAllPoints();
  gradientOpacity->AddPoint(min, 1.);
  gradientOpacity->AddPoint(max, 1.);
}

//----------------------------------------------------------------------------
/// Emission from 3 to 25 times the noise: opacity rising from 0.04 to 0.7,
/// colours of a colour map, levels spaced logarithmically
void SetPresetEmission(vtkMRMLVolumePropertyNode* node, const ColorStops& stops,
                       double noise, double min, double max)
{
  vtkPiecewiseFunction* opacity = node->GetScalarOpacity();
  vtkColorTransferFunction* color = node->GetColor();
  opacity->RemoveAllPoints();
  color->RemoveAllPoints();
  double rgb[3];
  RgbComponents(ColorMapRgb(stops, 0.), rgb);
  opacity->AddPoint(min, 0.);
  color->AddRGBPoint(min, rgb[0], rgb[1], rgb[2]);
  const double start = 0.85 * 3. * noise;
  if (start > min && start < max)
    {
    opacity->AddPoint(start, 0.);
    }
  double lastOpacity = 0.;
  for (int k = 0; k <= 8; k++)
    {
    const double t = k / 8.;
    const double level = 3. * noise * std::pow(25. / 3., t);
    if (level <= min || level >= max)
      {
      continue;
      }
    lastOpacity = 0.04 + (0.7 - 0.04) * std::pow(t, 1.3);
    opacity->AddPoint(level, lastOpacity);
    RgbComponents(ColorMapRgb(stops, t), rgb);
    color->AddRGBPoint(level, rgb[0], rgb[1], rgb[2]);
    }
  opacity->AddPoint(max, lastOpacity);
  RgbComponents(ColorMapRgb(stops, 1.), rgb);
  color->AddRGBPoint(max, rgb[0], rgb[1], rgb[2]);
  vtkPiecewiseFunction* gradientOpacity = node->GetGradientOpacity();
  gradientOpacity->RemoveAllPoints();
  gradientOpacity->AddPoint(min, 1.);
  gradientOpacity->AddPoint(max, 1.);
}
} // end of anonymous namespace

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
    const std::string preset = vtkMRMLAstroVolumeNode::GetPresetIdentifier(volumePropertyNode);
    if (preset == "LowConstantOpacity")
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
    if (preset == "MediumConstantOpacity")
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
    if (preset == "HighConstantOpacity")
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
    if (preset == "OneSurfaceGreen")
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
    if (preset == "OneSurfaceWhite")
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
    if (preset == "TwoSurfaces")
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
    if (preset == "ThreeSurfaces")
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
    if (preset == "BrightSurfaces")
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
    // colour-blind safe surfaces and emission ramps (Observatory, magma and
    // viridis colour maps)
    if (preset == "ObservatorySurfaces")
      {
      // Okabe-Ito sky blue, yellow and vermillion: distinct for colour-blind
      // people and bright on the dark background
      std::vector<PresetShell> shells = {{3., 0.3, 0x56B4E9}, {7., 0.45, 0xF0E442}, {15., 0.9, 0xD55E00}};
      SetPresetShells(volumePropertyNode, shells, noise, min, max);
      }
    if (preset == "EmissionAbsorption")
      {
      // emission as ObservatorySurfaces; absorption (negative values, e.g.
      // against a continuum source) in Okabe-Ito reddish purple, lighter
      // for deeper absorption
      std::vector<PresetShell> shells = {{-15., 0.9, 0xF7DDEE}, {-7., 0.45, 0xE3A5CB}, {-3., 0.3, 0xCC79A7},
                                         {3., 0.3, 0x56B4E9}, {7., 0.45, 0xF0E442}, {15., 0.9, 0xD55E00}};
      SetPresetShells(volumePropertyNode, shells, noise, min, max);
      }
    if (preset == "ObservatoryShells")
      {
      std::vector<PresetShell> shells;
      for (int k = 1; k <= 8; k++)
        {
        PresetShell shell;
        shell.Sigma = 3. * k;
        shell.Opacity = 0.12 + 0.1 * (k - 1);
        shell.Rgb = ColorMapRgb(ObservatoryStops(), (k - 1) / 7.);
        shells.push_back(shell);
        }
      SetPresetShells(volumePropertyNode, shells, noise, min, max);
      }
    if (preset == "ObservatoryEmission")
      {
      SetPresetEmission(volumePropertyNode, ObservatoryStops(), noise, min, max);
      }
    if (preset == "MagmaEmission")
      {
      SetPresetEmission(volumePropertyNode, MagmaStops(), noise, min, max);
      }
    if (preset == "ViridisEmission")
      {
      SetPresetEmission(volumePropertyNode, ViridisStops(), noise, min, max);
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
namespace
{
/// Points of a transfer function (vtkColorTransferFunction: 6 values per
/// node, vtkPiecewiseFunction: 4), except the first and the last, moved as
/// vtkControlPointsItem::MovePoints does: in the direction of the offset
/// first, a point does not pass its neighbours.
template <class FunctionType, int NodeSize>
void MoveFunctionPoints(FunctionType* function, double offset)
{
  const int size = function ? function->GetSize() : 0;
  if (size < 3 || offset == 0.)
    {
    return;
    }
  const int start = offset < 0. ? 1 : size - 2;
  const int end = offset < 0. ? size - 1 : 0;
  const int step = offset < 0. ? 1 : -1;
  for (int index = start; index != end; index += step)
    {
    double node[NodeSize];
    double previous[NodeSize];
    double next[NodeSize];
    function->GetNodeValue(index, node);
    function->GetNodeValue(index - 1, previous);
    function->GetNodeValue(index + 1, next);
    node[0] = std::min(std::max(previous[0], node[0] + offset), next[0]);
    function->SetNodeValue(index, node);
    }
}

/// Position of a point, kept between its neighbours
template <class FunctionType, int NodeSize>
void SetFunctionPointPosition(FunctionType* function, int index, double x)
{
  double node[NodeSize];
  double neighbour[NodeSize];
  function->GetNodeValue(index - 1, neighbour);
  x = std::max(neighbour[0], x);
  function->GetNodeValue(index + 1, neighbour);
  x = std::min(x, neighbour[0]);
  function->GetNodeValue(index, node);
  node[0] = x;
  function->SetNodeValue(index, node);
}

/// Points of a transfer function, except the first and the last, spread
/// away from (factor > 0) or towards (factor < 0) the centre of these
/// points, as vtkControlPointsItem::SpreadPoints does
template <class FunctionType, int NodeSize>
void SpreadFunctionPoints(FunctionType* function, double factor)
{
  const int size = function ? function->GetSize() : 0;
  const int count = size - 2;
  if (count <= 0 || factor == 0.)
    {
    return;
    }
  double node[NodeSize];
  function->GetNodeValue(1, node);
  const double minimum = node[0];
  function->GetNodeValue(count, node);
  const double maximum = node[0];
  const double center = (minimum + maximum) / 2.;

  // left part (point k is the node k + 1)
  int start = 0;
  int end = count;
  int step = 1;
  int median = -1;
  if (factor < 0.)
    {
    for (int k = 0; k < count; k++)
      {
      function->GetNodeValue(k + 1, node);
      if (node[0] > center)
        {
        median = k;
        break;
        }
      }
    if (median == -1)
      {
      median = count - 1;
      }
    start = median - 1;
    end = -1;
    step = -1;
    }
  int k = start;
  for (; k != end; k += step)
    {
    function->GetNodeValue(k + 1, node);
    const double x = node[0];
    if (x > center || (k != start && x == center))
      {
      break;
      }
    const double shift = -factor * (minimum != center ? (center - x) / (center - minimum) : std::fabs(x) / 100.);
    SetFunctionPointPosition<FunctionType, NodeSize>(function, k + 1, std::min(x + shift, center));
    }
  // right part
  start = count - 1;
  end = k - 1;
  step = -1;
  if (factor < 0.)
    {
    start = median;
    end = count;
    step = 1;
    }
  for (k = start; k != end; k += step)
    {
    function->GetNodeValue(k + 1, node);
    const double x = node[0];
    const double shift = factor * (maximum != center ? (x - center) / (maximum - center) : std::fabs(x) / 100.);
    SetFunctionPointPosition<FunctionType, NodeSize>(function, k + 1, std::max(x + shift, center));
    }
}
} // end of anonymous namespace

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::MoveTransferFunctionPoints(vtkVolumeProperty* volumeProperty, double offset)
{
  if (!volumeProperty || offset == 0. || !std::isfinite(offset))
    {
    return;
    }
  MoveFunctionPoints<vtkPiecewiseFunction, 4>(volumeProperty->GetScalarOpacity(), offset);
  MoveFunctionPoints<vtkColorTransferFunction, 6>(volumeProperty->GetRGBTransferFunction(), offset);
  volumeProperty->Modified();
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SpreadTransferFunctionPoints(vtkVolumeProperty* volumeProperty, double factor)
{
  if (!volumeProperty || factor == 0. || !std::isfinite(factor))
    {
    return;
    }
  SpreadFunctionPoints<vtkPiecewiseFunction, 4>(volumeProperty->GetScalarOpacity(), factor);
  SpreadFunctionPoints<vtkColorTransferFunction, 6>(volumeProperty->GetRGBTransferFunction(), factor);
  volumeProperty->Modified();
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::ShiftOpacities(vtkVolumeProperty* volumeProperty, double shift)
{
  vtkPiecewiseFunction* opacities = volumeProperty ? volumeProperty->GetScalarOpacity() : nullptr;
  if (!opacities || shift == 0. || !std::isfinite(shift))
    {
    return;
    }
  for (int index = 0; index < opacities->GetSize(); index++)
    {
    double value[4];
    opacities->GetNodeValue(index, value);
    if (std::fabs(value[1]) < 1.e-4)
      {
      continue;
      }
    value[1] = std::min(value[1] + shift, 1.);
    if (std::fabs(value[1]) < 1.e-4)
      {
      value[1] = 1.e-3;
      }
    opacities->SetNodeValue(index, value);
    }
  volumeProperty->Modified();
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SetVolumeRenderingLogic(vtkSlicerVolumeRenderingLogic* volumeRenderingLogic)
{
  this->VolumeRenderingLogic = volumeRenderingLogic;
}

//----------------------------------------------------------------------------
vtkSlicerVolumeRenderingLogic* vtkSlicerAstroVolumeLogic::GetVolumeRenderingLogic()
{
  return this->VolumeRenderingLogic;
}

//----------------------------------------------------------------------------
vtkMRMLVolumeRenderingDisplayNode* vtkSlicerAstroVolumeLogic::GetVolumeRenderingDisplayNode(
  vtkMRMLAstroVolumeNode* volumeNode, bool create)
{
  if (!volumeNode || !volumeNode->GetScene())
    {
    return nullptr;
    }
  vtkMRMLVolumeRenderingDisplayNode* displayNode = nullptr;
  for (int index = 0; index < volumeNode->GetNumberOfDisplayNodes() && !displayNode; index++)
    {
    displayNode = vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(volumeNode->GetNthDisplayNode(index));
    }
  if (!displayNode && create && this->VolumeRenderingLogic && volumeNode->GetImageData())
    {
    displayNode = this->VolumeRenderingLogic->CreateDefaultVolumeRenderingNodes(volumeNode);
    // hidden until shown (SetVolumeRenderingVisibility, with its color
    // function)
    if (displayNode)
      {
      displayNode->SetVisibility(false);
      }
    // the default transfer functions of Slicer (rendered until a preset is
    // applied) with gaps wide enough for the lookup table textures
    if (displayNode && displayNode->GetVolumePropertyNode())
      {
      this->EnsureVolumePropertyTransferFunctionGaps(displayNode->GetVolumePropertyNode());
      }
    }
  return displayNode;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::ApplyPreset(vtkMRMLAstroVolumeNode* volumeNode, vtkMRMLVolumePropertyNode* presetNode,
                                            bool resetShifts)
{
  vtkVolumeProperty* presetProperty = presetNode ? presetNode->GetVolumeProperty() : nullptr;
  if (!volumeNode || !presetProperty || !presetProperty->GetRGBTransferFunction() ||
      !this->GetVolumeRenderingDisplayNode(volumeNode, true))
    {
    return false;
    }
  // the presets adapted to the noise and the range of the data
  this->synchronizePresetsToVolumeNode(volumeNode);
  volumeNode->SetPresetNode(this->GetScenePresetNode(presetNode));
  vtkMRMLAstroVolumeDisplayNode* volumeDisplay = volumeNode->GetAstroVolumeDisplayNode();
  if (volumeDisplay && resetShifts)
    {
    const int wasModifying = volumeDisplay->StartModify();
    volumeDisplay->SetPresetOffset(0.);
    volumeDisplay->SetOldPresetOffset(0.);
    volumeDisplay->SetPresetStretch(0.);
    volumeDisplay->SetOldPresetStretch(0.);
    volumeDisplay->SetOpacityShift(0.);
    volumeDisplay->SetOldOpacityShift(0.);
    volumeDisplay->EndModify(wasModifying);
    }
  return this->CopyPresetToVolume(volumeNode, presetNode);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::CopyPresetToVolume(vtkMRMLAstroVolumeNode* volumeNode,
                                                   vtkMRMLVolumePropertyNode* presetNode)
{
  vtkVolumeProperty* presetProperty = presetNode ? presetNode->GetVolumeProperty() : nullptr;
  vtkMRMLVolumeRenderingDisplayNode* displayNode = this->GetVolumeRenderingDisplayNode(volumeNode, false);
  vtkMRMLVolumePropertyNode* propertyNode = displayNode ? displayNode->GetVolumePropertyNode() : nullptr;
  if (!propertyNode || !presetProperty || !presetProperty->GetRGBTransferFunction())
    {
    return false;
    }
  const double* range = presetProperty->GetRGBTransferFunction()->GetRange();
  if (range[0] > range[1])
    {
    return false;
    }
  const int wasModifying = propertyNode->StartModify();
  propertyNode->CopyContent(presetNode);
  // the copy of the preset of this volume: "<preset> · <volume>"
  const std::string name = std::string(presetNode->GetName() ? presetNode->GetName() : "") + " \xC2\xB7 " +
    (volumeNode->GetName() ? volumeNode->GetName() : "");
  propertyNode->SetName(name.c_str());
  propertyNode->SetAttribute(vtkMRMLAstroVolumeNode::GetPresetIdentifierAttributeName(),
                             vtkMRMLAstroVolumeNode::GetPresetIdentifier(presetNode).c_str());
  propertyNode->EndModify(wasModifying);
  volumeNode->SetVolumePropertyNode(propertyNode);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::PrepareVolumeRendering(vtkMRMLAstroVolumeNode* volumeNode)
{
  const char* naxis = volumeNode ? volumeNode->GetAttribute("SlicerAstro.NAXIS") : nullptr;
  if (!volumeNode || !volumeNode->GetImageData() || !naxis || StringToInt(naxis) < 3)
    {
    return false;
    }
  vtkMRMLVolumeRenderingDisplayNode* displayNode = this->GetVolumeRenderingDisplayNode(volumeNode, true);
  if (!displayNode)
    {
    return false;
    }
  if (volumeNode->GetVolumePropertyNode() && volumeNode->GetPresetNode())
    {
    // the color function of the volume is kept (with its shifts): the
    // presets are only adapted to the noise of the data
    this->synchronizePresetsToVolumeNode(volumeNode);
    if (displayNode->GetVolumePropertyNode() != volumeNode->GetVolumePropertyNode())
      {
      displayNode->SetAndObserveVolumePropertyNodeID(volumeNode->GetVolumePropertyNode()->GetID());
      }
    return true;
    }
  vtkMRMLScene* presets = this->GetPresetsScene();
  std::vector<vtkMRMLNode*> presetNodes;
  if (presets)
    {
    presets->GetNodesByClass("vtkMRMLVolumePropertyNode", presetNodes);
    }
  const int index = vtkMRMLAstroVolumeNode::DefaultPreset;
  if (index < 0 || index >= static_cast<int>(presetNodes.size()))
    {
    return false;
    }
  return this->ApplyPreset(volumeNode, vtkMRMLVolumePropertyNode::SafeDownCast(presetNodes[index]), true);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetVolumeRenderingVisibility(vtkMRMLAstroVolumeNode* volumeNode, bool visible,
                                                             bool centerCameras)
{
  if (visible && !this->PrepareVolumeRendering(volumeNode))
    {
    return false;
    }
  vtkMRMLVolumeRenderingDisplayNode* displayNode = this->GetVolumeRenderingDisplayNode(volumeNode, false);
  if (!displayNode)
    {
    return !visible;
    }
  displayNode->SetVisibility(visible);
  vtkMRMLScene* scene = volumeNode->GetScene();
  if (!visible || !centerCameras || !scene)
    {
    return true;
    }
  // looking along the velocity axis, north up, centered on the cube
  const double directionOfProjection[3] = {0., -1., 0.};
  const double viewUp[3] = {0., 0., 1.};
  std::vector<vtkMRMLNode*> cameras;
  scene->GetNodesByClass("vtkMRMLCameraNode", cameras);
  for (vtkMRMLNode* node : cameras)
    {
    vtkMRMLCameraNode* camera = vtkMRMLCameraNode::SafeDownCast(node);
    const char* layoutName = camera ? camera->GetLayoutName() : nullptr;
    vtkMRMLViewNode* view = layoutName ?
      vtkMRMLViewNode::SafeDownCast(scene->GetSingletonNode(layoutName, "vtkMRMLViewNode")) : nullptr;
    if (camera && (!view || displayNode->IsDisplayableInView(view->GetID())))
      {
      vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(camera, volumeNode, directionOfProjection, viewUp);
      }
    }
  return true;
}

//----------------------------------------------------------------------------
namespace
{
const char* ShownInPlaceOfAttribute = "SlicerAstro.ShownInPlaceOf";
const char* ShownReplacedAttribute = "SlicerAstro.ShownReplaced";
const char* ShownReplacedOffsetAttribute = "SlicerAstro.ShownReplacedOffset";
// 3D views of the product and of the dataset before ShowProductInViews
// ("hidden", or the view node IDs separated by spaces; empty: all views)
const char* ShownProductRenderingAttribute = "SlicerAstro.ShownProductRendering";
const char* ShownDatasetRenderingAttribute = "SlicerAstro.ShownDatasetRendering";

std::string RenderingState(vtkMRMLVolumeRenderingDisplayNode* rendering)
{
  if (!rendering || !rendering->GetVisibility())
    {
    return "hidden";
    }
  std::string ids;
  for (const std::string& id : rendering->GetViewNodeIDs())
    {
    ids += (ids.empty() ? "" : " ") + id;
    }
  return ids;
}

std::vector<std::string> RenderingViews(const std::string& state)
{
  std::vector<std::string> ids;
  std::string id;
  for (char c : state)
    {
    if (c == ' ')
      {
      if (!id.empty())
        {
        ids.push_back(id);
        }
      id.clear();
      }
    else
      {
      id += c;
      }
    }
  if (!id.empty())
    {
    ids.push_back(id);
    }
  return ids;
}

int VolumeDimensions(vtkMRMLVolumeNode* volume)
{
  const char* naxis = volume ? volume->GetAttribute("SlicerAstro.NAXIS") : nullptr;
  if (naxis)
    {
    return atoi(naxis);
    }
  vtkImageData* image = volume ? volume->GetImageData() : nullptr;
  return image && image->GetDimensions()[2] > 1 ? 3 : 2;
}

vtkMRMLSliceNode* SliceNodeOf(vtkMRMLScene* scene, vtkMRMLSliceCompositeNode* composite)
{
  const char* layoutName = composite ? composite->GetLayoutName() : nullptr;
  return layoutName ? vtkMRMLSliceNode::SafeDownCast(scene->GetSingletonNode(layoutName, "vtkMRMLSliceNode")) : nullptr;
}
} // end of anonymous namespace

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::IsOnSameSkyGrid(vtkMRMLVolumeNode* volume, vtkMRMLVolumeNode* other, bool sameVoxels)
{
  if (!volume || !other || !volume->GetImageData() || !other->GetImageData())
    {
    return false;
    }
  if (volume == other)
    {
    return true;
    }
  int dimsA[3], dimsB[3];
  volume->GetImageData()->GetDimensions(dimsA);
  other->GetImageData()->GetDimensions(dimsB);
  if (dimsA[0] != dimsB[0] || dimsA[1] != dimsB[1] || (sameVoxels && dimsA[2] != dimsB[2]))
    {
    return false;
    }
  vtkNew<vtkMatrix4x4> a, b;
  volume->GetIJKToRASMatrix(a);
  other->GetIJKToRASMatrix(b);
  double scale = 0.;
  for (int row = 0; row < 3; row++)
    {
    for (int column = 0; column < 3; column++)
      {
      scale = std::max(scale, std::fabs(a->GetElement(row, column)));
      }
    }
  scale = scale > 0. ? scale : 1.;
  const double tolerance = 1e-6 * scale;
  const int columns = sameVoxels ? 3 : 2;
  for (int column = 0; column < columns; column++)
    {
    for (int row = 0; row < 3; row++)
      {
      if (std::fabs(a->GetElement(row, column) - b->GetElement(row, column)) > tolerance)
        {
        return false;
        }
      }
    }
  // origins: equal, or (sky grid only) apart along the spectral axis
  double delta[3], axis[3];
  for (int row = 0; row < 3; row++)
    {
    delta[row] = b->GetElement(row, 3) - a->GetElement(row, 3);
    axis[row] = a->GetElement(row, 2);
    }
  if (!sameVoxels)
    {
    const double norm = vtkMath::Norm(axis);
    if (norm > 0.)
      {
      const double along = vtkMath::Dot(delta, axis) / norm;
      for (int row = 0; row < 3; row++)
        {
        delta[row] -= along * axis[row] / norm;
        }
      }
    }
  return vtkMath::Norm(delta) <= 10. * tolerance;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::IsProductShownInViews(vtkMRMLAstroVolumeNode* product)
{
  return product && product->GetAttribute(ShownInPlaceOfAttribute);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::ShowProductInViews(vtkMRMLAstroVolumeNode* product, vtkMRMLAstroVolumeNode* dataset)
{
  vtkMRMLScene* scene = product ? product->GetScene() : nullptr;
  if (!scene || !dataset || product == dataset || !product->GetImageData() || !product->GetID() || !dataset->GetID())
    {
    return false;
    }
  vtkMRMLApplicationLogic* appLogic = this->GetMRMLApplicationLogic();
  std::vector<vtkMRMLNode*> composites;
  scene->GetNodesByClass("vtkMRMLSliceCompositeNode", composites);
  std::vector<vtkMRMLNode*> volumes;
  scene->GetNodesByClass("vtkMRMLAstroVolumeNode", volumes);
  const bool sameSky = vtkSlicerAstroVolumeLogic::IsOnSameSkyGrid(dataset, product);
  // background of a view replaced by this product: the dataset, or another
  // product of the dataset shown in its place
  std::function<bool(vtkMRMLNode*)> showsDataset = [dataset](vtkMRMLNode* node)
    {
    vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
    const char* shownInPlaceOf = volume ? volume->GetAttribute(ShownInPlaceOfAttribute) : nullptr;
    return volume && (volume == dataset || (shownInPlaceOf && !strcmp(shownInPlaceOf, dataset->GetID())));
    };

  if (VolumeDimensions(product) >= 3)
    {
    // a cube: in place of the dataset in all the views
    bool replaced = false;
    for (vtkMRMLNode* node : composites)
      {
      vtkMRMLSliceCompositeNode* composite = vtkMRMLSliceCompositeNode::SafeDownCast(node);
      vtkMRMLNode* background = composite && composite->GetBackgroundVolumeID() ?
        scene->GetNodeByID(composite->GetBackgroundVolumeID()) : nullptr;
      if (!composite || !background || VolumeDimensions(vtkMRMLVolumeNode::SafeDownCast(background)) < 3 ||
          !showsDataset(background))
        {
        continue;
        }
      composite->SetBackgroundVolumeID(product->GetID());
      replaced = true;
      vtkMRMLSliceNode* sliceNode = SliceNodeOf(scene, composite);
      vtkMRMLSliceLogic* sliceLogic = appLogic && sliceNode ? appLogic->GetSliceLogic(sliceNode) : nullptr;
      if (!sameSky && sliceLogic)
        {
        sliceLogic->FitSliceToAll();
        }
      }
    if (!replaced)
      {
      // the dataset in no view: the product in the views of the layout
      for (vtkMRMLNode* node : composites)
        {
        vtkMRMLSliceCompositeNode* composite = vtkMRMLSliceCompositeNode::SafeDownCast(node);
        vtkMRMLSliceNode* sliceNode = SliceNodeOf(scene, composite);
        if (!composite || !sliceNode || !sliceNode->IsMappedInLayout())
          {
          continue;
          }
        composite->SetBackgroundVolumeID(product->GetID());
        vtkMRMLSliceLogic* sliceLogic = appLogic ? appLogic->GetSliceLogic(sliceNode) : nullptr;
        if (sliceLogic)
          {
          sliceLogic->FitSliceToAll();
          }
        }
      }
    // 3D views: the product rendered where the dataset (or another of its
    // products) was rendered, and where it was rendered already
    vtkMRMLVolumeRenderingDisplayNode* datasetRendering = this->GetVolumeRenderingDisplayNode(dataset, false);
    vtkMRMLVolumeRenderingDisplayNode* productRendering = this->GetVolumeRenderingDisplayNode(product, false);
    const std::string productState = RenderingState(productRendering);
    const std::string datasetState = RenderingState(datasetRendering);
    std::vector<std::string> viewIDs = datasetRendering ? datasetRendering->GetViewNodeIDs() : std::vector<std::string>();
    bool rendered = datasetRendering && datasetRendering->GetVisibility();
    for (vtkMRMLNode* node : volumes)
      {
      vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
      if (!volume || volume == product || !showsDataset(volume))
        {
        continue;
        }
      vtkMRMLVolumeRenderingDisplayNode* rendering = this->GetVolumeRenderingDisplayNode(volume, false);
      if (rendering && rendering->GetVisibility())
        {
        if (volume != dataset)
          {
          // another product shown in place of the dataset: its views
          viewIDs = rendering->GetViewNodeIDs();
          }
        rendered = true;
        this->SetVolumeRenderingVisibility(volume, false, false);
        }
      if (volume != dataset)
        {
        volume->RemoveAttribute(ShownInPlaceOfAttribute);
        }
      }
    if (rendered && this->PrepareVolumeRendering(product))
      {
      vtkMRMLVolumeRenderingDisplayNode* rendering = this->GetVolumeRenderingDisplayNode(product, false);
      if (rendering)
        {
        // the views of the dataset and the views the product had (empty:
        // all the views)
        std::vector<std::string> own = RenderingViews(productState);
        if (productState != "hidden" && !viewIDs.empty())
          {
          if (own.empty())
            {
            viewIDs.clear();
            }
          for (const std::string& id : own)
            {
            if (std::find(viewIDs.begin(), viewIDs.end(), id) == viewIDs.end())
              {
              viewIDs.push_back(id);
              }
            }
          }
        rendering->SetViewNodeIDs(viewIDs);
        }
      this->SetVolumeRenderingVisibility(product, true, !sameSky);
      }
    // the 3D views restored by HideProductInViews (the first product shown
    // in place of the dataset keeps the state of the dataset)
    const char* earlierDatasetState = nullptr;
    for (vtkMRMLNode* node : volumes)
      {
      vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
      if (volume && volume != product && volume->GetAttribute(ShownDatasetRenderingAttribute) &&
          !volume->GetAttribute(ShownInPlaceOfAttribute))
        {
        earlierDatasetState = volume->GetAttribute(ShownDatasetRenderingAttribute);
        volume->RemoveAttribute(ShownDatasetRenderingAttribute);
        volume->RemoveAttribute(ShownProductRenderingAttribute);
        }
      }
    product->SetAttribute(ShownProductRenderingAttribute, productState.c_str());
    product->SetAttribute(ShownDatasetRenderingAttribute,
                          earlierDatasetState ? earlierDatasetState : datasetState.c_str());
    product->SetAttribute(ShownInPlaceOfAttribute, dataset->GetID());
    return true;
    }

  // a 2D map: one channel-map view, the one already showing a map of the
  // dataset, otherwise yellow (in the layout), otherwise red
  vtkMRMLSliceCompositeNode* target = nullptr;
  vtkMRMLAstroVolumeNode* previousMap = nullptr;
  for (vtkMRMLNode* node : composites)
    {
    vtkMRMLSliceCompositeNode* composite = vtkMRMLSliceCompositeNode::SafeDownCast(node);
    vtkMRMLAstroVolumeNode* background = composite && composite->GetBackgroundVolumeID() ?
      vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(composite->GetBackgroundVolumeID())) : nullptr;
    if (background && background != product && VolumeDimensions(background) < 3 &&
        background->GetAttribute(ShownInPlaceOfAttribute) && showsDataset(background))
      {
      target = composite;
      previousMap = background;
      break;
      }
    }
  for (const char* name : {"Yellow", "Red"})
    {
    if (target)
      {
      break;
      }
    vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(scene->GetSingletonNode(name, "vtkMRMLSliceNode"));
    if (sliceNode && sliceNode->IsMappedInLayout())
      {
      target = vtkMRMLSliceCompositeNode::SafeDownCast(scene->GetSingletonNode(name, "vtkMRMLSliceCompositeNode"));
      }
    }
  vtkMRMLSliceNode* sliceNode = SliceNodeOf(scene, target);
  if (!target || !sliceNode)
    {
    return false;
    }
  vtkMRMLSliceLogic* sliceLogic = appLogic ? appLogic->GetSliceLogic(sliceNode) : nullptr;
  vtkMRMLVolumeNode* previous = target->GetBackgroundVolumeID() ?
    vtkMRMLVolumeNode::SafeDownCast(scene->GetNodeByID(target->GetBackgroundVolumeID())) : nullptr;
  // what the view showed before the maps (restored by HideProductInViews)
  std::string replacedID = previous && previous->GetID() ? previous->GetID() : "";
  std::string replacedOffset = DoubleToString(sliceLogic ? sliceLogic->GetSliceOffset() : sliceNode->GetSliceOffset());
  if (previousMap)
    {
    const char* id = previousMap->GetAttribute(ShownReplacedAttribute);
    const char* offset = previousMap->GetAttribute(ShownReplacedOffsetAttribute);
    replacedID = id ? id : "";
    replacedOffset = offset ? offset : replacedOffset;
    previousMap->RemoveAttribute(ShownInPlaceOfAttribute);
    previousMap->RemoveAttribute(ShownReplacedAttribute);
    previousMap->RemoveAttribute(ShownReplacedOffsetAttribute);
    }
  const bool keepView = previous && vtkSlicerAstroVolumeLogic::IsOnSameSkyGrid(previous, product) &&
                        sliceNode->GetOrientation() == "XY";
  target->SetBackgroundVolumeID(product->GetID());
  target->SetForegroundVolumeID(nullptr);
  if (!keepView)
    {
    // another orientation first: the slice is recomputed for the new data
    sliceNode->SetOrientation("XZ");
    sliceNode->SetOrientation("XY");
    if (sliceLogic)
      {
      sliceLogic->FitSliceToAll();
      }
    }
  else if (sliceLogic)
    {
    // the plane of the map (zoom and pan kept)
    double bounds[6] = {0., 0., 0., 0., 0., 0.};
    sliceLogic->GetVolumeSliceBounds(product, bounds);
    sliceLogic->SetSliceOffset(0.5 * (bounds[4] + bounds[5]));
    }
  product->SetAttribute(ShownReplacedAttribute, replacedID.c_str());
  product->SetAttribute(ShownReplacedOffsetAttribute, replacedOffset.c_str());
  product->SetAttribute(ShownInPlaceOfAttribute, dataset->GetID());
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::HideProductInViews(vtkMRMLAstroVolumeNode* product)
{
  vtkMRMLScene* scene = product ? product->GetScene() : nullptr;
  const char* datasetID = product ? product->GetAttribute(ShownInPlaceOfAttribute) : nullptr;
  if (!scene || !datasetID)
    {
    return false;
    }
  vtkMRMLAstroVolumeNode* dataset = vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(datasetID));
  vtkMRMLApplicationLogic* appLogic = this->GetMRMLApplicationLogic();
  std::vector<vtkMRMLNode*> composites;
  scene->GetNodesByClass("vtkMRMLSliceCompositeNode", composites);
  const char* replacedAttribute = product->GetAttribute(ShownReplacedAttribute);
  const char* offsetAttribute = product->GetAttribute(ShownReplacedOffsetAttribute);
  vtkMRMLVolumeNode* replaced = replacedAttribute && *replacedAttribute ?
    vtkMRMLVolumeNode::SafeDownCast(scene->GetNodeByID(replacedAttribute)) : dataset;
  for (vtkMRMLNode* node : composites)
    {
    vtkMRMLSliceCompositeNode* composite = vtkMRMLSliceCompositeNode::SafeDownCast(node);
    if (!composite || !composite->GetBackgroundVolumeID() ||
        strcmp(composite->GetBackgroundVolumeID(), product->GetID()))
      {
      continue;
      }
    composite->SetBackgroundVolumeID(replaced ? replaced->GetID() : nullptr);
    vtkMRMLSliceNode* sliceNode = SliceNodeOf(scene, composite);
    vtkMRMLSliceLogic* sliceLogic = appLogic && sliceNode ? appLogic->GetSliceLogic(sliceNode) : nullptr;
    if (!sliceLogic || !replaced)
      {
      continue;
      }
    if (!vtkSlicerAstroVolumeLogic::IsOnSameSkyGrid(replaced, product))
      {
      sliceLogic->FitSliceToAll();
      }
    else if (offsetAttribute && VolumeDimensions(product) < 3)
      {
      // the channel shown before the map
      sliceLogic->SetSliceOffset(StringToDouble(offsetAttribute));
      }
    }
  // 3D views: the product and the dataset as they were before
  vtkMRMLVolumeRenderingDisplayNode* rendering = this->GetVolumeRenderingDisplayNode(product, false);
  const char* productStateAttribute = product->GetAttribute(ShownProductRenderingAttribute);
  const char* datasetStateAttribute = product->GetAttribute(ShownDatasetRenderingAttribute);
  const std::string productState = productStateAttribute ? productStateAttribute : "hidden";
  const std::string datasetState = datasetStateAttribute ? datasetStateAttribute :
    (rendering && rendering->GetVisibility() ? RenderingState(rendering) : std::string("hidden"));
  if (rendering && rendering->GetVisibility())
    {
    if (productState == "hidden")
      {
      this->SetVolumeRenderingVisibility(product, false, false);
      }
    else
      {
      rendering->SetViewNodeIDs(RenderingViews(productState));
      }
    if (dataset && datasetState != "hidden" && this->PrepareVolumeRendering(dataset))
      {
      vtkMRMLVolumeRenderingDisplayNode* datasetRendering = this->GetVolumeRenderingDisplayNode(dataset, false);
      if (datasetRendering)
        {
        datasetRendering->SetViewNodeIDs(RenderingViews(datasetState));
        }
      this->SetVolumeRenderingVisibility(dataset, true, !vtkSlicerAstroVolumeLogic::IsOnSameSkyGrid(dataset, product));
      }
    }
  product->RemoveAttribute(ShownProductRenderingAttribute);
  product->RemoveAttribute(ShownDatasetRenderingAttribute);
  product->RemoveAttribute(ShownReplacedAttribute);
  product->RemoveAttribute(ShownReplacedOffsetAttribute);
  product->RemoveAttribute(ShownInPlaceOfAttribute);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::ToggleProductInViews(vtkMRMLAstroVolumeNode* product, vtkMRMLAstroVolumeNode* dataset)
{
  return vtkSlicerAstroVolumeLogic::IsProductShownInViews(product) ? this->HideProductInViews(product) :
                                                                      this->ShowProductInViews(product, dataset);
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::HideProductsInViews(vtkMRMLAstroVolumeNode* dataset)
{
  vtkMRMLScene* scene = dataset ? dataset->GetScene() : nullptr;
  if (!scene || !dataset->GetID())
    {
    return;
    }
  std::vector<vtkMRMLNode*> volumes;
  scene->GetNodesByClass("vtkMRMLAstroVolumeNode", volumes);
  // maps first: they restore what their view showed (possibly a cube
  // product, restored next)
  for (int pass = 0; pass < 2; pass++)
    {
    for (vtkMRMLNode* node : volumes)
      {
      vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
      const char* shownInPlaceOf = volume ? volume->GetAttribute(ShownInPlaceOfAttribute) : nullptr;
      if (shownInPlaceOf && !strcmp(shownInPlaceOf, dataset->GetID()) &&
          (VolumeDimensions(volume) >= 3) == (pass == 1))
        {
        this->HideProductInViews(volume);
        }
      }
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::ShowSelectedActiveVolume()
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene || scene->IsImporting() || scene->IsClosing() || scene->IsRestoring() || scene->IsBatchProcessing())
    {
    return;
    }
  vtkMRMLSelectionNode* selectionNode =
    vtkMRMLSelectionNode::SafeDownCast(scene->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  const std::string activeVolumeID =
    (selectionNode && selectionNode->GetActiveVolumeID()) ? selectionNode->GetActiveVolumeID() : "";
  if (activeVolumeID == this->ShownActiveVolumeID)
    {
    return;
    }
  this->ShownActiveVolumeID = activeVolumeID;
  vtkMRMLAstroVolumeNode* volumeNode = activeVolumeID.empty() ? nullptr :
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(activeVolumeID.c_str()));
  if (volumeNode)
    {
    this->ShowActiveVolume(volumeNode);
    }
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::ShowActiveVolume(vtkMRMLAstroVolumeNode* volumeNode)
{
  vtkMRMLScene* scene = volumeNode ? volumeNode->GetScene() : nullptr;
  if (!scene || scene->IsImporting())
    {
    return false;
    }
  const bool cube = this->PrepareVolumeRendering(volumeNode);
  if (!this->AutoShowActiveVolume)
    {
    return true;
    }
  vtkMRMLApplicationLogic* appLogic = this->GetMRMLApplicationLogic();
  if (appLogic)
    {
    appLogic->PropagateBackgroundVolumeSelection(1);
    }
  // one cube in the 3D views
  std::vector<vtkMRMLNode*> volumes;
  scene->GetNodesByClass("vtkMRMLAstroVolumeNode", volumes);
  for (vtkMRMLNode* node : volumes)
    {
    vtkMRMLAstroVolumeNode* other = vtkMRMLAstroVolumeNode::SafeDownCast(node);
    vtkMRMLVolumeRenderingDisplayNode* otherDisplay = (other && other != volumeNode) ?
      this->GetVolumeRenderingDisplayNode(other, false) : nullptr;
    if (otherDisplay)
      {
      otherDisplay->SetVisibility(false);
      }
    }
  vtkImageData* imageData = volumeNode->GetImageData();
  const double megabytes = imageData ? static_cast<double>(imageData->GetNumberOfPoints()) *
    imageData->GetScalarSize() * imageData->GetNumberOfScalarComponents() / (1024. * 1024.) : 0.;
  if (cube && megabytes <= this->AutoShow3DMaximumMegabytes)
    {
    this->SetVolumeRenderingVisibility(volumeNode, true, true);
    }
  return true;
}

//----------------------------------------------------------------------------
namespace
{
vtkVolumeProperty* VolumePropertyOf(vtkMRMLAstroVolumeNode* volumeNode)
{
  vtkMRMLVolumePropertyNode* propertyNode = volumeNode ?
    vtkMRMLVolumePropertyNode::SafeDownCast(volumeNode->GetVolumePropertyNode()) : nullptr;
  return propertyNode ? propertyNode->GetVolumeProperty() : nullptr;
}
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetColorFunctionShift(vtkMRMLAstroVolumeNode* volumeNode, double shift)
{
  vtkVolumeProperty* volumeProperty = VolumePropertyOf(volumeNode);
  vtkMRMLAstroVolumeDisplayNode* volumeDisplay = volumeNode ? volumeNode->GetAstroVolumeDisplayNode() : nullptr;
  if (!volumeProperty || !volumeDisplay)
    {
    return false;
    }
  if (volumeDisplay->GetOldPresetOffset() == shift)
    {
    // already applied: only the stored value may differ
    if (volumeDisplay->GetPresetOffset() != shift)
      {
      volumeDisplay->SetPresetOffset(shift);
      }
    return true;
    }
  vtkSlicerAstroVolumeLogic::MoveTransferFunctionPoints(volumeProperty, shift - volumeDisplay->GetOldPresetOffset());
  const int wasModifying = volumeDisplay->StartModify();
  volumeDisplay->SetPresetOffset(shift);
  volumeDisplay->SetOldPresetOffset(shift);
  volumeDisplay->EndModify(wasModifying);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetColorFunctionStretch(vtkMRMLAstroVolumeNode* volumeNode, double stretch)
{
  vtkVolumeProperty* volumeProperty = VolumePropertyOf(volumeNode);
  vtkMRMLAstroVolumeDisplayNode* volumeDisplay = volumeNode ? volumeNode->GetAstroVolumeDisplayNode() : nullptr;
  if (!volumeProperty || !volumeDisplay)
    {
    return false;
    }
  if (volumeDisplay->GetOldPresetStretch() == stretch)
    {
    // already applied: only the stored value may differ
    if (volumeDisplay->GetPresetStretch() != stretch)
      {
      volumeDisplay->SetPresetStretch(stretch);
      }
    return true;
    }
  vtkSlicerAstroVolumeLogic::SpreadTransferFunctionPoints(volumeProperty,
                                                          stretch - volumeDisplay->GetOldPresetStretch());
  const int wasModifying = volumeDisplay->StartModify();
  volumeDisplay->SetPresetStretch(stretch);
  volumeDisplay->SetOldPresetStretch(stretch);
  volumeDisplay->EndModify(wasModifying);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetColorFunctionOpacityShift(vtkMRMLAstroVolumeNode* volumeNode, double shift)
{
  vtkVolumeProperty* volumeProperty = VolumePropertyOf(volumeNode);
  vtkMRMLAstroVolumeDisplayNode* volumeDisplay = volumeNode ? volumeNode->GetAstroVolumeDisplayNode() : nullptr;
  if (!volumeProperty || !volumeDisplay)
    {
    return false;
    }
  if (volumeDisplay->GetOldOpacityShift() == shift)
    {
    // already applied: only the stored value may differ
    if (volumeDisplay->GetOpacityShift() != shift)
      {
      volumeDisplay->SetOpacityShift(shift);
      }
    return true;
    }
  vtkSlicerAstroVolumeLogic::ShiftOpacities(volumeProperty, shift - volumeDisplay->GetOldOpacityShift());
  const int wasModifying = volumeDisplay->StartModify();
  volumeDisplay->SetOpacityShift(shift);
  volumeDisplay->SetOldOpacityShift(shift);
  volumeDisplay->EndModify(wasModifying);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetVolumeRenderingQuality(vtkMRMLAstroVolumeNode* volumeNode, int quality)
{
  vtkMRMLVolumeRenderingDisplayNode* displayNode = this->GetVolumeRenderingDisplayNode(volumeNode, false);
  if (!displayNode || !displayNode->GetScene())
    {
    return false;
    }
  std::vector<vtkMRMLNode*> viewNodes;
  displayNode->GetScene()->GetNodesByClass("vtkMRMLViewNode", viewNodes);
  for (vtkMRMLNode* node : viewNodes)
    {
    vtkMRMLViewNode* viewNode = vtkMRMLViewNode::SafeDownCast(node);
    if (viewNode && displayNode->IsDisplayableInView(viewNode->GetID()))
      {
      viewNode->SetVolumeRenderingQuality(quality);
      }
    }
  displayNode->Modified();
  return true;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroVolumeLogic::GetVolumeRenderingQuality(vtkMRMLAstroVolumeNode* volumeNode)
{
  vtkMRMLVolumeRenderingDisplayNode* displayNode = this->GetVolumeRenderingDisplayNode(volumeNode, false);
  if (!displayNode || !displayNode->GetScene())
    {
    return -1;
    }
  std::vector<vtkMRMLNode*> viewNodes;
  displayNode->GetScene()->GetNodesByClass("vtkMRMLViewNode", viewNodes);
  for (vtkMRMLNode* node : viewNodes)
    {
    vtkMRMLViewNode* viewNode = vtkMRMLViewNode::SafeDownCast(node);
    if (viewNode && displayNode->IsDisplayableInView(viewNode->GetID()))
      {
      return viewNode->GetVolumeRenderingQuality();
      }
    }
  return -1;
}

//----------------------------------------------------------------------------
vtkMRMLMarkupsROINode* vtkSlicerAstroVolumeLogic::GetVolumeRenderingROINode(vtkMRMLAstroVolumeNode* volumeNode,
                                                                           bool create)
{
  vtkMRMLVolumeRenderingDisplayNode* displayNode = this->GetVolumeRenderingDisplayNode(volumeNode, create);
  if (!displayNode)
    {
    return nullptr;
    }
  vtkMRMLMarkupsROINode* roiNode = displayNode->GetMarkupsROINode();
  if (!roiNode && create && this->VolumeRenderingLogic)
    {
    // created on request by the Volume Rendering module, fitted to the volume
    roiNode = vtkMRMLMarkupsROINode::SafeDownCast(this->VolumeRenderingLogic->CreateROINode(displayNode));
    }
  return roiNode;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetVolumeRenderingCropping(vtkMRMLAstroVolumeNode* volumeNode, bool crop)
{
  vtkMRMLVolumeRenderingDisplayNode* displayNode = this->GetVolumeRenderingDisplayNode(volumeNode, crop);
  if (!displayNode)
    {
    return !crop;
    }
  vtkMRMLMarkupsROINode* roiNode = this->GetVolumeRenderingROINode(volumeNode, crop);
  displayNode->SetCroppingEnabled(crop);
  return !crop || roiNode;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetVolumeRenderingROIVisibility(vtkMRMLAstroVolumeNode* volumeNode, bool visible)
{
  vtkMRMLMarkupsROINode* roiNode = this->GetVolumeRenderingROINode(volumeNode, visible);
  if (roiNode)
    {
    roiNode->SetDisplayVisibility(visible);
    }
  return !visible || roiNode;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::FitVolumeRenderingROI(vtkMRMLAstroVolumeNode* volumeNode)
{
  vtkMRMLVolumeRenderingDisplayNode* displayNode = this->GetVolumeRenderingDisplayNode(volumeNode, true);
  if (!displayNode || !this->VolumeRenderingLogic || !this->GetVolumeRenderingROINode(volumeNode, true))
    {
    return false;
    }
  this->VolumeRenderingLogic->FitROIToVolume(displayNode);
  return true;
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
bool vtkSlicerAstroVolumeLogic::HasFITSMaskHeader(vtkMRMLSegmentationNode* segmentationNode)
{
  if (!segmentationNode)
    {
    return false;
    }
  if (segmentationNode->GetAttribute("SlicerAstro.NAXIS1"))
    {
    return true;
    }
  vtkMRMLAstroVolumeNode* referenceVolume = vtkMRMLAstroVolumeNode::SafeDownCast(segmentationNode->GetNodeReference(
    vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str()));
  return referenceVolume && referenceVolume->GetAttribute("SlicerAstro.NAXIS1");
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SaveFITSMask(vtkMRMLSegmentationNode* segmentationNode,
                                             vtkMRMLAstroVolumeNode* referenceVolume, const char* fileName)
{
  this->LastMaskSaveError.clear();
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!segmentationNode || !scene || segmentationNode->GetScene() != scene || !fileName || !*fileName)
    {
    this->LastMaskSaveError = "No mask or no file name.";
    return false;
    }
  if (vtkSlicerAstroVolumeLogic::IsContoursSegmentation(segmentationNode))
    {
    this->LastMaskSaveError = "Contours are not saved as FITS masks.";
    return false;
    }
  if (!vtkSlicerAstroVolumeLogic::HasNonEmptySegments(segmentationNode))
    {
    this->LastMaskSaveError = "The mask has no segments or its segments are empty: nothing to save.";
    return false;
    }
  // the FITS header and the geometry of the mask: the ones of the mask read
  // from file, or of the astronomical volume used as reference
  if (!vtkSlicerAstroVolumeLogic::HasFITSMaskHeader(segmentationNode))
    {
    if (!referenceVolume)
      {
      this->LastMaskSaveError = std::string("The segmentation ") + segmentationNode->GetName() +
        " has no FITS header: give an astronomical data volume as reference.";
      return false;
      }
    segmentationNode->SetReferenceImageGeometryParameterFromVolumeNode(referenceVolume);
    }

  // FITS mask storage (replaces e.g. a .seg.nrrd storage node)
  vtkMRMLStorageNode* storageNode = segmentationNode->GetStorageNode();
  if (storageNode && !storageNode->IsA("vtkMRMLAstroSegmentationStorageNode"))
    {
    segmentationNode->SetAndObserveStorageNodeID(nullptr);
    scene->RemoveNode(storageNode);
    }
  this->EnsureFITSMaskStorageNode(segmentationNode);
  storageNode = segmentationNode->GetStorageNode();
  if (!storageNode)
    {
    this->LastMaskSaveError = "The FITS mask storage could not be created.";
    return false;
    }
  std::string path = fileName;
  const std::string lower = vtksys::SystemTools::LowerCase(path);
  if (!vtksys::SystemTools::StringEndsWith(lower, ".fits") && !vtksys::SystemTools::StringEndsWith(lower, ".fits.gz"))
    {
    path += ".fits";
    }
  storageNode->SetFileName(path.c_str());
  storageNode->GetUserMessages()->ClearMessages();
  if (!storageNode->WriteData(segmentationNode))
    {
    this->LastMaskSaveError = "The mask could not be saved as FITS mask: " +
      storageNode->GetUserMessages()->GetAllMessagesAsString();
    return false;
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::EnsureAstroTerminology(vtkMRMLSegmentationNode* segmentationNode)
{
  if (!segmentationNode || !segmentationNode->GetSegmentation())
    {
    return false;
    }
  const std::string maskEntry = vtkMRMLAstroTerminology::GetMaskEntry();
  const std::string contourEntry = vtkMRMLAstroTerminology::GetContourEntry();
  const std::string currentEntry = vtkSlicerTerminologiesModuleLogic::GetDefaultTerminologyEntryAsString(segmentationNode);
  const bool setByAstro = currentEntry == maskEntry || currentEntry == contourEntry;
  const bool contours = vtkSlicerAstroVolumeLogic::IsContoursSegmentation(segmentationNode);
  if (!contours && !vtkSlicerAstroVolumeLogic::IsAstroSegmentation(segmentationNode))
    {
    // e.g. the reference volume is a medical image: Slicer's default
    if (setByAstro)
      {
      vtkSlicerTerminologiesModuleLogic::SetDefaultTerminologyEntryAsString(segmentationNode, std::string());
      }
    return false;
    }
  const std::string astroEntry = contours ? contourEntry : maskEntry;
  if ((currentEntry.empty() || setByAstro) && currentEntry != astroEntry)
    {
    vtkSlicerTerminologiesModuleLogic::SetDefaultTerminologyEntryAsString(segmentationNode, astroEntry);
    }
  return true;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SetTerminologiesLogic(vtkSlicerTerminologiesModuleLogic* terminologiesLogic)
{
  this->TerminologiesLogic = terminologiesLogic;
}

//----------------------------------------------------------------------------
vtkMRMLColorTableNode* vtkSlicerAstroVolumeLogic::AddAstroTerminologyColorTable()
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkSlicerTerminologiesModuleLogic* terminologiesLogic = this->TerminologiesLogic;
  if (!scene || !terminologiesLogic)
    {
    return nullptr;
    }

  // the types of the terminology, category by category
  const std::string contextName = vtkMRMLAstroTerminology::GetContextName();
  std::vector<vtkSlicerTerminologiesModuleLogic::CodeIdentifier> categories;
  if (!terminologiesLogic->GetCategoriesInTerminology(contextName, categories) || categories.empty())
    {
    vtkErrorMacro("AddAstroTerminologyColorTable: the terminology '" << contextName << "' is not loaded.");
    return nullptr;
    }
  std::vector<vtkSlicerTerminologiesModuleLogic::CodeIdentifier> typeCategories;
  std::vector<vtkSlicerTerminologiesModuleLogic::CodeIdentifier> types;
  for (const vtkSlicerTerminologiesModuleLogic::CodeIdentifier& category : categories)
    {
    std::vector<vtkSlicerTerminologiesModuleLogic::CodeIdentifier> categoryTypes;
    terminologiesLogic->GetTypesInTerminologyCategory(contextName, category, categoryTypes);
    for (const vtkSlicerTerminologiesModuleLogic::CodeIdentifier& type : categoryTypes)
      {
      typeCategories.push_back(category);
      types.push_back(type);
      }
    }

  // the singleton table of the scene (emptied when a scene is closed), else a new one
  vtkSmartPointer<vtkMRMLColorTableNode> colorTable = vtkMRMLColorTableNode::SafeDownCast(scene->GetSingletonNode(
    vtkSlicerAstroVolumeLogic::GetAstroTerminologyColorTableSingletonTag(), "vtkMRMLColorTableNode"));
  const bool inScene = colorTable != nullptr;
  if (inScene && colorTable->GetNumberOfColors() == static_cast<int>(types.size()) + 1 &&
      colorTable->GetContainsTerminology())
    {
    return colorTable;
    }
  if (!inScene)
    {
    colorTable = vtkSmartPointer<vtkMRMLColorTableNode>::Take(
      vtkMRMLColorTableNode::SafeDownCast(scene->CreateNodeByClass("vtkMRMLColorTableNode")));
    }
  MRMLNodeModifyBlocker blocker(colorTable);
  colorTable->SetType(vtkMRMLColorTableNode::User);
  colorTable->SetName("SlicerAstro");
  colorTable->SetSingletonTag(vtkSlicerAstroVolumeLogic::GetAstroTerminologyColorTableSingletonTag());
  colorTable->SetDescription("Astronomical terminology of the segments (sources, galaxies, gas structures, "
                             "kinematic and analysis regions) with their recommended colors.");
  colorTable->SetNumberOfColors(static_cast<int>(types.size()) + 1);
  colorTable->SetColor(0, "Background", 0., 0., 0., 0.);
  for (size_t ii = 0; ii < types.size(); ++ii)
    {
    const int index = static_cast<int>(ii) + 1;
    vtkNew<vtkSlicerTerminologyType> typeObject;
    double rgb[3] = { 0.5, 0.5, 0.5 };
    if (terminologiesLogic->GetTypeInTerminologyCategory(contextName, typeCategories[ii], types[ii], typeObject))
      {
      unsigned char recommended[3] = { 128, 128, 128 };
      typeObject->GetRecommendedDisplayRGBValue(recommended);
      for (int c = 0; c < 3; ++c)
        {
        rgb[c] = recommended[c] / 255.;
        }
      }
    colorTable->SetColor(index, types[ii].CodeMeaning.c_str(), rgb[0], rgb[1], rgb[2], 1.);
    colorTable->SetTerminology(index,
      typeCategories[ii].CodingSchemeDesignator, typeCategories[ii].CodeValue, typeCategories[ii].CodeMeaning,
      types[ii].CodingSchemeDesignator, types[ii].CodeValue, types[ii].CodeMeaning);
    }
  if (!inScene)
    {
    scene->AddNode(colorTable);
    }
  return colorTable;
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
  std::string name = vtkSlicerAstroVolumeLogic::GetFreeNodeName(scene, std::string(volumeNode->GetName()) + "_mask",
                                                                segmentationNode);
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

  // one modification of the node: the views are rendered with both
  // functions corrected (an event per function rendered the color function
  // with its gaps still too small)
  this->UpdatingTransferFunctionGaps = true;
  const int wasModifying = volumePropertyNode->StartModify();
  vtkSlicerAstroVolumeLogic::EnsureMinimumTransferFunctionGap(volumeProperty, range);
  volumePropertyNode->EndModify(wasModifying);
  this->UpdatingTransferFunctionGaps = false;
}

//----------------------------------------------------------------------------
namespace
{
double FiniteAttribute(vtkMRMLNode* node, const char* name)
{
  const char* value = node ? node->GetAttribute(name) : nullptr;
  if (!value || !strcmp(value, "UNDEFINED"))
    {
    return std::numeric_limits<double>::quiet_NaN();
    }
  char* end = nullptr;
  const double number = std::strtod(value, &end);
  return end != value ? number : std::numeric_limits<double>::quiet_NaN();
}

void UpdateSliceViews(vtkMRMLScene* scene)
{
  if (!scene)
    {
    return;
    }
  std::vector<vtkMRMLNode*> sliceNodes;
  scene->GetNodesByClass("vtkMRMLSliceNode", sliceNodes);
  for (vtkMRMLNode* sliceNode : sliceNodes)
    {
    sliceNode->Modified();
    }
}
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkSlicerAstroVolumeLogic::GetSourceCatalogueVolume(vtkMRMLTableNode* table)
{
  vtkMRMLScene* scene = table ? table->GetScene() : nullptr;
  const char* referenceID = table ? table->GetAttribute("SlicerAstro.SoFiAReference") : nullptr;
  vtkMRMLNode* reference = (scene && referenceID) ? scene->GetNodeByID(referenceID) : nullptr;
  if (reference && reference->IsA("vtkMRMLSegmentationNode"))
    {
    reference = reference->GetNodeReference(vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str());
    }
  return vtkMRMLAstroVolumeNode::SafeDownCast(reference);
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::GetSourceCatalogues(vtkMRMLAstroVolumeNode* volumeNode, vtkCollection* tables)
{
  if (!tables)
    {
    return;
    }
  tables->RemoveAllItems();
  vtkMRMLScene* scene = volumeNode ? volumeNode->GetScene() : nullptr;
  if (!scene)
    {
    return;
    }
  std::vector<vtkMRMLNode*> nodes;
  scene->GetNodesByClass("vtkMRMLTableNode", nodes);
  for (vtkMRMLNode* node : nodes)
    {
    vtkMRMLTableNode* table = vtkMRMLTableNode::SafeDownCast(node);
    if (table && table->GetAttribute("SlicerAstro.SoFiASources") &&
        vtkSlicerAstroVolumeLogic::GetSourceCatalogueVolume(table) == volumeNode)
      {
      tables->AddItem(table);
      }
    }
}

//----------------------------------------------------------------------------
vtkMRMLMarkupsFiducialNode* vtkSlicerAstroVolumeLogic::GetSourceCataloguePoints(vtkMRMLTableNode* table)
{
  vtkMRMLScene* scene = table ? table->GetScene() : nullptr;
  const char* pointsID = table ? table->GetAttribute("SlicerAstro.SoFiASources") : nullptr;
  return (scene && pointsID) ? vtkMRMLMarkupsFiducialNode::SafeDownCast(scene->GetNodeByID(pointsID)) : nullptr;
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroVolumeLogic::SelectSource(vtkMRMLTableNode* table, int row)
{
  vtkMRMLMarkupsFiducialNode* points = vtkSlicerAstroVolumeLogic::GetSourceCataloguePoints(table);
  if (!points || row < 0 || row >= points->GetNumberOfControlPoints())
    {
    return std::string();
    }
  const int wasModifying = points->StartModify();
  for (int index = 0; index < points->GetNumberOfControlPoints(); index++)
    {
    points->SetNthControlPointSelected(index, index == row);
    }
  points->EndModify(wasModifying);
  double position[3] = {0., 0., 0.};
  points->GetNthControlPointPositionWorld(row, position);
  vtkMRMLSliceNode::JumpAllSlices(points->GetScene(), position[0], position[1], position[2],
                                  vtkMRMLSliceNode::CenteredJumpSlice);
  const std::string description = points->GetNthControlPointDescription(row);
  return description.compare(0, 3, "id ") == 0 ? description.substr(3) : std::string();
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetSelectedSources(vtkMRMLTableNode* table, vtkIntArray* rows)
{
  vtkMRMLMarkupsFiducialNode* points = vtkSlicerAstroVolumeLogic::GetSourceCataloguePoints(table);
  if (!points)
    {
    return false;
    }
  std::vector<bool> selected(points->GetNumberOfControlPoints(), false);
  for (vtkIdType index = 0; rows && index < rows->GetNumberOfValues(); index++)
    {
    const int row = rows->GetValue(index);
    if (row >= 0 && row < static_cast<int>(selected.size()))
      {
      selected[row] = true;
      }
    }
  const int wasModifying = points->StartModify();
  for (int index = 0; index < static_cast<int>(selected.size()); index++)
    {
    if (points->GetNthControlPointSelected(index) != selected[index])
      {
      points->SetNthControlPointSelected(index, selected[index]);
      }
    }
  points->EndModify(wasModifying);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::DeleteSources(vtkMRMLTableNode* table, vtkIntArray* rows)
{
  vtkTable* data = table ? table->GetTable() : nullptr;
  if (!data || !rows)
    {
    return false;
    }
  vtkMRMLMarkupsFiducialNode* points = vtkSlicerAstroVolumeLogic::GetSourceCataloguePoints(table);
  std::vector<int> sorted;
  for (vtkIdType index = 0; index < rows->GetNumberOfValues(); index++)
    {
    sorted.push_back(rows->GetValue(index));
    }
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  const int wasModifying = table->StartModify();
  const int wasModifyingPoints = points ? points->StartModify() : 0;
  for (std::vector<int>::reverse_iterator it = sorted.rbegin(); it != sorted.rend(); ++it)
    {
    if (*it >= 0 && *it < data->GetNumberOfRows())
      {
      data->RemoveRow(*it);
      }
    if (points && *it >= 0 && *it < points->GetNumberOfControlPoints())
      {
      points->RemoveNthControlPoint(*it);
      }
    }
  data->Modified();
  table->Modified();
  if (points)
    {
    points->EndModify(wasModifyingPoints);
    }
  table->EndModify(wasModifying);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::RemoveSourceCatalogue(vtkMRMLTableNode* table)
{
  vtkMRMLScene* scene = table ? table->GetScene() : nullptr;
  if (!scene)
    {
    return false;
    }
  vtkMRMLMarkupsFiducialNode* points = vtkSlicerAstroVolumeLogic::GetSourceCataloguePoints(table);
  if (points)
    {
    scene->RemoveNode(points);
    }
  scene->RemoveNode(table);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetSourcePointsLocked(vtkMRMLTableNode* table, bool locked)
{
  vtkMRMLMarkupsFiducialNode* points = vtkSlicerAstroVolumeLogic::GetSourceCataloguePoints(table);
  if (!points)
    {
    return false;
    }
  // the points are locked one by one by the catalogue reader
  const int wasModifying = points->StartModify();
  points->SetLocked(locked);
  for (int index = 0; index < points->GetNumberOfControlPoints(); index++)
    {
    points->SetNthControlPointLocked(index, locked);
    }
  points->EndModify(wasModifying);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::GetSourcePointsLocked(vtkMRMLTableNode* table)
{
  vtkMRMLMarkupsFiducialNode* points = vtkSlicerAstroVolumeLogic::GetSourceCataloguePoints(table);
  if (!points)
    {
    return false;
    }
  if (points->GetLocked())
    {
    return true;
    }
  for (int index = 0; index < points->GetNumberOfControlPoints(); index++)
    {
    if (!points->GetNthControlPointLocked(index))
      {
      return false;
      }
    }
  return points->GetNumberOfControlPoints() > 0;
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroVolumeLogic::GetSourceSegmentID(vtkMRMLSegmentationNode* mask, const char* sourceID)
{
  vtkSegmentation* segmentation = mask ? mask->GetSegmentation() : nullptr;
  if (!segmentation || !sourceID || !*sourceID)
    {
    return std::string();
    }
  std::vector<std::string> segmentIDs;
  segmentation->GetSegmentIDs(segmentIDs);
  for (const std::string& segmentID : segmentIDs)
    {
    std::string tag;
    vtkSegment* segment = segmentation->GetSegment(segmentID);
    if (segment && segment->GetTag("SlicerAstro.SourceID", tag) && tag == sourceID)
      {
      return segmentID;
      }
    }
  return std::string();
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetContoursChannel(vtkMRMLAstroVolumeNode* image, vtkMRMLAstroVolumeNode* cube,
                                                   double channel)
{
  vtkImageData* imageData = image ? image->GetImageData() : nullptr;
  vtkImageData* cubeData = cube ? cube->GetImageData() : nullptr;
  if (!imageData || !cubeData || StringToInt(image->GetAttribute("SlicerAstro.NAXIS")) != 2 ||
      StringToInt(cube->GetAttribute("SlicerAstro.NAXIS")) != 3)
    {
    return false;
    }
  int dims[3] = {0, 0, 0};
  int cubeDims[3] = {0, 0, 0};
  imageData->GetDimensions(dims);
  cubeData->GetDimensions(cubeDims);
  // the origin of the centered image, moved along its third axis (the
  // velocity axis of the centered cube) to the channel
  vtkNew<vtkMatrix4x4> ijkToRAS;
  image->GetIJKToRASMatrix(ijkToRAS);
  const double ijkOrigin[4] = {-dims[0] * 0.5, -dims[1] * 0.5, channel - cubeDims[2] * 0.5, 0.};
  double rasOrigin[4] = {0., 0., 0., 0.};
  ijkToRAS->MultiplyPoint(ijkOrigin, rasOrigin);
  image->SetOrigin(rasOrigin);

  // the XY views follow the image
  double center[4] = {dims[0] * 0.5, dims[1] * 0.5, 0., 1.};
  double centerRAS[4] = {0., 0., 0., 1.};
  image->GetIJKToRASMatrix(ijkToRAS);
  ijkToRAS->MultiplyPoint(center, centerRAS);
  vtkMRMLScene* scene = image->GetScene();
  std::vector<vtkMRMLNode*> sliceNodes;
  if (scene)
    {
    scene->GetNodesByClass("vtkMRMLSliceNode", sliceNodes);
    }
  for (vtkMRMLNode* node : sliceNodes)
    {
    vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(node);
    if (sliceNode && sliceNode->GetOrientation() == "XY")
      {
      sliceNode->JumpSliceByOffsetting(centerRAS[0], centerRAS[1], centerRAS[2]);
      }
    }
  return true;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroVolumeLogic::GetContoursChannel(vtkMRMLAstroVolumeNode* image, vtkMRMLAstroVolumeNode* cube,
                                                  double* velocity)
{
  if (velocity)
    {
    *velocity = std::numeric_limits<double>::quiet_NaN();
    }
  vtkImageData* imageData = image ? image->GetImageData() : nullptr;
  vtkImageData* cubeData = cube ? cube->GetImageData() : nullptr;
  if (!imageData || !cubeData || StringToInt(image->GetAttribute("SlicerAstro.NAXIS")) != 2 ||
      StringToInt(cube->GetAttribute("SlicerAstro.NAXIS")) != 3)
    {
    return -1;
    }
  int dims[3] = {0, 0, 0};
  int cubeDims[3] = {0, 0, 0};
  imageData->GetDimensions(dims);
  cubeData->GetDimensions(cubeDims);
  vtkNew<vtkMatrix4x4> imageIJKToRAS;
  vtkNew<vtkMatrix4x4> cubeRASToIJK;
  image->GetIJKToRASMatrix(imageIJKToRAS);
  cube->GetRASToIJKMatrix(cubeRASToIJK);
  const double center[4] = {dims[0] * 0.5, dims[1] * 0.5, 0., 1.};
  double ras[4] = {0., 0., 0., 1.};
  double cubeIJK[4] = {0., 0., 0., 1.};
  imageIJKToRAS->MultiplyPoint(center, ras);
  cubeRASToIJK->MultiplyPoint(ras, cubeIJK);
  const int channel = static_cast<int>(std::lround(cubeIJK[2]));
  if (channel < 0 || channel >= cubeDims[2])
    {
    return -1;
    }
  vtkMRMLAstroVolumeDisplayNode* cubeDisplay = cube->GetAstroVolumeDisplayNode();
  const double ijk[3] = {cubeDims[0] * 0.5, cubeDims[1] * 0.5, static_cast<double>(channel)};
  double world[3] = {0., 0., 0.};
  if (velocity && cubeDisplay && cubeDisplay->GetReferenceSpace(ijk, world))
    {
    const char* unit = cube->GetAttribute("SlicerAstro.CUNIT3");
    std::string lower = unit ? vtksys::SystemTools::LowerCase(unit) : std::string();
    lower.erase(std::remove(lower.begin(), lower.end(), ' '), lower.end());
    *velocity = lower == "m/s" ? world[2] * 0.001 : world[2];
    }
  return channel;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::GetBeam(vtkMRMLAstroVolumeNode* volumeNode, double beam[3])
{
  beam[0] = beam[1] = beam[2] = 0.;
  const double bmaj = FiniteAttribute(volumeNode, "SlicerAstro.BMAJ");
  const double bmin = FiniteAttribute(volumeNode, "SlicerAstro.BMIN");
  const double bpa = FiniteAttribute(volumeNode, "SlicerAstro.BPA");
  if (!(bmaj > 0.) || !(bmin > 0.))
    {
    return false;
    }
  beam[0] = bmaj;
  beam[1] = bmin;
  beam[2] = std::isfinite(bpa) ? bpa : 0.;
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetBeam(vtkMRMLAstroVolumeNode* volumeNode, const double beam[3])
{
  if (!volumeNode || !(beam[0] > 0.) || !(beam[1] > 0.) || !std::isfinite(beam[0]) || !std::isfinite(beam[1]))
    {
    return false;
    }
  const double bmaj = std::max(beam[0], beam[1]);
  const double bmin = std::min(beam[0], beam[1]);
  const double bpa = std::isfinite(beam[2]) ? beam[2] : 0.;
  const int wasModifying = volumeNode->StartModify();
  volumeNode->SetAttribute("SlicerAstro.BMAJ", DoubleToString(bmaj).c_str());
  volumeNode->SetAttribute("SlicerAstro.BMIN", DoubleToString(bmin).c_str());
  volumeNode->SetAttribute("SlicerAstro.BPA", DoubleToString(bpa).c_str());
  volumeNode->EndModify(wasModifying);
  // the moment maps of a cube have its beam (e.g. a beam estimated or
  // entered after the maps were computed)
  for (int order = 0; order < 3; order++)
    {
    vtkMRMLAstroVolumeNode* map = vtkSlicerAstroVolumeLogic::GetMomentMap(volumeNode, order);
    if (map && map != volumeNode)
      {
      const int wasModifyingMap = map->StartModify();
      map->SetAttribute("SlicerAstro.BMAJ", DoubleToString(bmaj).c_str());
      map->SetAttribute("SlicerAstro.BMIN", DoubleToString(bmin).c_str());
      map->SetAttribute("SlicerAstro.BPA", DoubleToString(bpa).c_str());
      map->EndModify(wasModifyingMap);
      }
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::HasNoise(vtkMRMLAstroVolumeNode* volumeNode)
{
  if (!volumeNode)
    {
    return false;
    }
  const char* datamodel = volumeNode->GetAttribute("SlicerAstro.DATAMODEL");
  if (datamodel && (!strcmp(datamodel, "FIRSTMOMENTMAP") || !strcmp(datamodel, "SECONDMOMENTMAP")))
    {
    return false;
    }
  const char* bunit = volumeNode->GetAttribute("SlicerAstro.BUNIT");
  std::string unit = bunit ? bunit : "";
  unit.erase(std::remove(unit.begin(), unit.end(), ' '), unit.end());
  std::transform(unit.begin(), unit.end(), unit.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return unit != "km/s" && unit != "m/s" && unit != "km.s-1" && unit != "m.s-1";
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetVelocityDefinition(vtkMRMLAstroVolumeNode* volumeNode, bool optical)
{
  vtkMRMLAstroVolumeDisplayNode* displayNode = volumeNode ? volumeNode->GetAstroVolumeDisplayNode() : nullptr;
  if (!displayNode)
    {
    return false;
    }
  const bool changed = optical ? displayNode->SetOpticalVelocityDefinition() :
    displayNode->SetRadioVelocityDefinition();
  if (changed)
    {
    volumeNode->Modified();
    UpdateSliceViews(volumeNode->GetScene());
    }
  return changed;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::IsOpticalVelocityDefinition(vtkMRMLAstroVolumeNode* volumeNode)
{
  vtkMRMLAstroVolumeDisplayNode* displayNode = volumeNode ? volumeNode->GetAstroVolumeDisplayNode() : nullptr;
  return displayNode && !strncmp(displayNode->GetVelocityDefinition().c_str(), "VOPT", 4);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetRightAscensionSexagesimal(vtkMRMLAstroVolumeNode* volumeNode, bool sexagesimal)
{
  vtkMRMLAstroVolumeDisplayNode* displayNode = volumeNode ? volumeNode->GetAstroVolumeDisplayNode() : nullptr;
  if (!displayNode || !displayNode->SetSpaceQuantity(0, sexagesimal ? "time" : "length"))
    {
    return false;
    }
  volumeNode->Modified();
  UpdateSliceViews(volumeNode->GetScene());
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::IsRightAscensionSexagesimal(vtkMRMLAstroVolumeNode* volumeNode)
{
  vtkMRMLAstroVolumeDisplayNode* displayNode = volumeNode ? volumeNode->GetAstroVolumeDisplayNode() : nullptr;
  vtkStringArray* quantities = displayNode ? displayNode->GetSpaceQuantities() : nullptr;
  return quantities && quantities->GetNumberOfValues() > 0 && quantities->GetValue(0) == "time";
}

//----------------------------------------------------------------------------
double vtkSlicerAstroVolumeLogic::GetChannelWidth(vtkMRMLAstroVolumeNode* volumeNode)
{
  vtkMRMLAstroVolumeDisplayNode* displayNode = volumeNode ? volumeNode->GetAstroVolumeDisplayNode() : nullptr;
  const double channels = FiniteAttribute(volumeNode, "SlicerAstro.NAXIS3");
  if (!displayNode || !(channels > 1.) || FiniteAttribute(volumeNode, "SlicerAstro.NAXIS") < 3.)
    {
    return 0.;
    }
  const double nx = FiniteAttribute(volumeNode, "SlicerAstro.NAXIS1");
  const double ny = FiniteAttribute(volumeNode, "SlicerAstro.NAXIS2");
  double ijk[3] = {std::isfinite(nx) ? nx * 0.5 : 0., std::isfinite(ny) ? ny * 0.5 : 0., 0.};
  double first[3] = {0., 0., 0.};
  double second[3] = {0., 0., 0.};
  if (!displayNode->GetReferenceSpace(ijk, first))
    {
    return 0.;
    }
  ijk[2] = 1.;
  if (!displayNode->GetReferenceSpace(ijk, second))
    {
    return 0.;
    }
  const char* unit = volumeNode->GetAttribute("SlicerAstro.CUNIT3");
  std::string lower = unit ? vtksys::SystemTools::LowerCase(unit) : std::string();
  lower.erase(std::remove(lower.begin(), lower.end(), ' '), lower.end());
  const double factor = lower == "m/s" ? 0.001 : 1.;
  const double width = std::fabs(second[2] - first[2]) * factor;
  return std::isfinite(width) ? width : 0.;
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
    newContours->SetName(vtkSlicerAstroVolumeLogic::GetFreeNodeName(scene, contoursName).c_str());
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
  if (!mask && create && this->AttachMasksToVolume(volumeNode) > 0)
    {
    // a FITS mask of this cube loaded before it (same voxel lattice, WCS)
    mask = vtkSlicerAstroVolumeLogic::GetMaskForVolume(scene, volumeNode, referencedMask);
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
    newMask->SetName(vtkSlicerAstroVolumeLogic::GetFreeNodeName(scene, std::string(volumeNode->GetName()) + "_mask").c_str());
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
    // one mask per dataset, named after it ("<data>_mask"; the names of a
    // scene are kept)
    const std::string maskName = std::string(volumeNode->GetName() ? volumeNode->GetName() : "") + "_mask";
    if (!scene->IsImporting() && volumeNode->GetName() && (!mask->GetName() || maskName != mask->GetName()))
      {
      mask->SetName(vtkSlicerAstroVolumeLogic::GetFreeNodeName(scene, maskName, mask).c_str());
      }
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
      !volumeNode->GetStorageNode() || !volumeNode->GetStorageNode()->GetFileName())
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
  if (!volumeNode)
    {
    return;
    }
  // FITS masks loaded before the cube (same voxel lattice) become its masks
  this->AttachMasksToVolume(volumeNode);
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
    // the new active volume is shown (3D rendering prepared and, with
    // AutoShowActiveVolume, shown in the views)
    this->ShowSelectedActiveVolume();
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
      vtkSlicerAstroVolumeLogic::EnsureAstroTerminology(segmentationNode);
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
double vtkSlicerAstroVolumeLogic::GetJyBeamToJyFactor(vtkMRMLVolumeNode* volumeNode)
{
  if (!volumeNode)
    {
    return 0.;
    }
  const char* unit = volumeNode->GetAttribute("SlicerAstro.BUNIT");
  std::string bunit = unit ? unit : "";
  std::transform(bunit.begin(), bunit.end(), bunit.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  bunit.erase(std::remove(bunit.begin(), bunit.end(), ' '), bunit.end());
  if (bunit != "JY/BEAM")
    {
    return 0.;
    }
  double values[4] = {0., 0., 0., 0.};
  const char* keys[4] = {"SlicerAstro.BMAJ", "SlicerAstro.BMIN", "SlicerAstro.CDELT1", "SlicerAstro.CDELT2"};
  for (int index = 0; index < 4; index++)
    {
    const char* value = volumeNode->GetAttribute(keys[index]);
    if (!value || !strcmp(value, "UNDEFINED"))
      {
      return 0.;
      }
    values[index] = StringToDouble(value);
    }
  if (!(values[0] > 0.) || !(values[1] > 0.) || values[2] == 0. || values[3] == 0.)
    {
    return 0.;
    }
  const double beamArea = vtkMath::Pi() / (4. * log(2.)) * values[0] * values[1];
  return fabs(values[2] * values[3]) / beamArea;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::HasColorFunctionOptions(vtkMRMLColorNode* colorNode)
{
  const char* value = colorNode ? colorNode->GetAttribute("SlicerAstro.AddFunctions") : nullptr;
  return value && !strcmp(value, "on");
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::GetColorFunctionOption(vtkMRMLColorNode* colorNode, const char* option)
{
  if (!colorNode || !option)
    {
    return false;
    }
  const char* value = colorNode->GetAttribute((std::string("SlicerAstro.") + option).c_str());
  return value && !strcmp(value, "on");
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetColorFunctionOption(vtkMRMLColorNode* colorNode, const char* option, bool on)
{
  vtkLookupTable* lookupTable = colorNode ? colorNode->GetLookupTable() : nullptr;
  if (!lookupTable || !option)
    {
    return false;
    }
  const std::string name = option;
  if (name != "Reverse" && name != "Inverse" && name != "Log")
    {
    return false;
    }
  const bool current = GetColorFunctionOption(colorNode, option);
  int wasModifying = colorNode->StartModify();
  if (name == "Log")
    {
    // the scale is set every time (idempotent)
    if (on)
      {
      lookupTable->SetScaleToLog10();
      }
    else
      {
      lookupTable->SetScaleToLinear();
      }
    }
  else if (current != on)
    {
    const vtkIdType colors = lookupTable->GetNumberOfTableValues();
    if (name == "Reverse")
      {
      for (vtkIdType ii = 0; ii < colors / 2; ii++)
        {
        double first[4], last[4];
        lookupTable->GetTableValue(ii, first);
        lookupTable->GetTableValue(colors - 1 - ii, last);
        lookupTable->SetTableValue(ii, last);
        lookupTable->SetTableValue(colors - 1 - ii, first);
        }
      }
    else
      {
      for (vtkIdType ii = 0; ii < colors; ii++)
        {
        double rgba[4];
        lookupTable->GetTableValue(ii, rgba);
        rgba[0] = 1. - rgba[0];
        rgba[1] = 1. - rgba[1];
        rgba[2] = 1. - rgba[2];
        lookupTable->SetTableValue(ii, rgba);
        }
      }
    colorNode->SetNamesFromColors();
    }
  colorNode->SetAttribute(("SlicerAstro." + name).c_str(), on ? "on" : "off");
  colorNode->Modified();
  colorNode->EndModify(wasModifying);
  return true;
}

namespace
{
//----------------------------------------------------------------------------
/// The finite voxels of a volume as doubles (at most maxSamples, regularly)
std::vector<double> SampleFiniteVoxels(vtkMRMLVolumeNode* volume, vtkIdType maxSamples)
{
  std::vector<double> values;
  vtkImageData* image = volume ? volume->GetImageData() : nullptr;
  vtkDataArray* scalars = image && image->GetPointData() ? image->GetPointData()->GetScalars() : nullptr;
  if (!scalars)
    {
    return values;
    }
  const vtkIdType count = scalars->GetNumberOfTuples();
  const vtkIdType step = std::max<vtkIdType>(1, count / std::max<vtkIdType>(1, maxSamples));
  values.reserve(static_cast<size_t>(count / step + 1));
  for (vtkIdType i = 0; i < count; i += step)
    {
    const double value = scalars->GetComponent(i, 0);
    if (std::isfinite(value))
      {
      values.push_back(value);
      }
    }
  return values;
}
} // end of anonymous namespace

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::ComputeHistogram(vtkMRMLVolumeNode* volume, int bins, vtkDoubleArray* centres,
                                                 vtkDoubleArray* counts, vtkIdType maxSamples)
{
  if (!centres || !counts || bins < 1)
    {
    return false;
    }
  centres->Initialize();
  counts->Initialize();
  const std::vector<double> values = SampleFiniteVoxels(volume, maxSamples);
  if (values.empty())
    {
    return false;
    }
  const std::pair<std::vector<double>::const_iterator, std::vector<double>::const_iterator> range =
    std::minmax_element(values.begin(), values.end());
  const double low = *range.first;
  double high = *range.second;
  if (!(high > low))
    {
    high = low + 1.;
    }
  const double width = (high - low) / bins;
  std::vector<double> histogram(static_cast<size_t>(bins), 0.);
  for (double value : values)
    {
    const int bin = std::min(bins - 1, static_cast<int>((value - low) / width));
    histogram[static_cast<size_t>(bin)] += 1.;
    }
  centres->SetNumberOfValues(bins);
  counts->SetNumberOfValues(bins);
  for (int i = 0; i < bins; i++)
    {
    centres->SetValue(i, low + (i + 0.5) * width);
    counts->SetValue(i, histogram[static_cast<size_t>(i)]);
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::ComputeHistogramInRange(vtkMRMLVolumeNode* volume, int bins, double minimum,
                                                        double maximum, vtkDoubleArray* centres,
                                                        vtkDoubleArray* counts, vtkIdType maxSamples)
{
  if (!centres || !counts || bins < 1 || !(maximum > minimum) || !std::isfinite(minimum) || !std::isfinite(maximum))
    {
    return false;
    }
  centres->Initialize();
  counts->Initialize();
  const std::vector<double> values = SampleFiniteVoxels(volume, maxSamples);
  if (values.empty())
    {
    return false;
    }
  const double width = (maximum - minimum) / bins;
  std::vector<double> histogram(static_cast<size_t>(bins), 0.);
  for (double value : values)
    {
    if (value < minimum || value > maximum)
      {
      continue;
      }
    const int bin = std::min(bins - 1, static_cast<int>((value - minimum) / width));
    histogram[static_cast<size_t>(bin)] += 1.;
    }
  centres->SetNumberOfValues(bins);
  counts->SetNumberOfValues(bins);
  for (int i = 0; i < bins; i++)
    {
    centres->SetValue(i, minimum + (i + 0.5) * width);
    counts->SetValue(i, histogram[static_cast<size_t>(i)]);
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::ComputeClippedWindow(vtkMRMLVolumeNode* volume, double percentage, double window[2],
                                                     vtkIdType maxSamples)
{
  return vtkMRMLAstroVolumeNode::ComputeClippedWindow(volume ? volume->GetImageData() : nullptr, percentage, window,
                                                      maxSamples);
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::FitSlicesToVolume(vtkMRMLAstroVolumeNode* volume)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLApplicationLogic* appLogic = this->GetMRMLApplicationLogic();
  if (!scene || !appLogic || !volume || !volume->GetImageData() || !volume->GetAstroVolumeDisplayNode())
    {
    return;
    }
  std::vector<vtkMRMLNode*> sliceNodes;
  scene->GetNodesByClass("vtkMRMLSliceNode", sliceNodes);
  for (vtkMRMLNode* node : sliceNodes)
    {
    vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(node);
    vtkMRMLSliceLogic* sliceLogic = sliceNode ? appLogic->GetSliceLogic(sliceNode) : nullptr;
    if (!sliceLogic)
      {
      continue;
      }
    // the data of the view to fit: the volume, or another one fitted
    int dims[3] = {0, 0, 0};
    bool apply = false;
    vtkMRMLSliceLayerLogic* layers[2] = {sliceLogic->GetBackgroundLayer(), sliceLogic->GetForegroundLayer()};
    for (vtkMRMLSliceLayerLogic* layer : layers)
      {
      vtkMRMLAstroVolumeNode* shown = layer ? vtkMRMLAstroVolumeNode::SafeDownCast(layer->GetVolumeNode()) : nullptr;
      if (!shown || !shown->GetImageData() || !shown->GetAstroVolumeDisplayNode())
        {
        continue;
        }
      if ((shown == volume && volume->GetAstroVolumeDisplayNode()->GetFitSlices()) ||
          shown->GetAstroVolumeDisplayNode()->GetFitSlices())
        {
        shown->GetImageData()->GetDimensions(dims);
        apply = true;
        break;
        }
      }
    if (!apply)
      {
      sliceLogic->FitSliceToAll(true);
      continue;
      }
    double rasDimensions[3], rasCenter[3];
    sliceLogic->GetVolumeRASBox(volume, rasDimensions, rasCenter);
    double fieldOfView[3];
    sliceNode->GetFieldOfView(fieldOfView);
    const std::string orientation = sliceNode->GetOrientation();
    if (orientation == "PVMajor" || orientation == "PVMinor" || orientation == "Reformat")
      {
      fieldOfView[0] = std::sqrt(static_cast<double>(dims[0]) * dims[0] + static_cast<double>(dims[1]) * dims[1]);
      fieldOfView[1] = dims[2] * 1.2;
      }
    else if (orientation == "XY" || orientation == "PVDiagram")
      {
      fieldOfView[0] = dims[0];
      fieldOfView[1] = dims[1];
      }
    else if (orientation == "ZY")
      {
      fieldOfView[0] = dims[2];
      fieldOfView[1] = dims[1];
      }
    else if (orientation == "XZ")
      {
      fieldOfView[0] = dims[0];
      fieldOfView[1] = dims[2];
      }
    else
      {
      continue;
      }
    sliceNode->SetFieldOfView(fieldOfView[0], fieldOfView[1], fieldOfView[2]);
    // the origin at the centre of the volume
    vtkNew<vtkMatrix4x4> sliceToRAS;
    sliceToRAS->DeepCopy(sliceNode->GetSliceToRAS());
    sliceToRAS->SetElement(0, 3, rasCenter[0]);
    sliceToRAS->SetElement(1, 3, rasCenter[1]);
    sliceToRAS->SetElement(2, 3, rasCenter[2]);
    sliceNode->GetSliceToRAS()->DeepCopy(sliceToRAS);
    sliceNode->SetSliceOrigin(0, 0, 0);
    sliceLogic->SnapSliceOffsetToIJK();
    sliceNode->UpdateMatrices();
    sliceNode->Modified();
    }
}

//----------------------------------------------------------------------------
vtkMRMLSegmentationNode* vtkSlicerAstroVolumeLogic::CreateThresholdMask(vtkMRMLAstroVolumeNode* reference,
                                                                       vtkMRMLAstroVolumeNode* data,
                                                                       double threshold, const char* segmentName,
                                                                       const double color[3])
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene || !reference || !data || !data->GetImageData() || !segmentName)
    {
    vtkErrorMacro("CreateThresholdMask: data or scene not found");
    return nullptr;
    }
  vtkSmartPointer<vtkMRMLSegmentationNode> maskNode = vtkSmartPointer<vtkMRMLSegmentationNode>::Take(
    vtkMRMLSegmentationNode::SafeDownCast(scene->CreateNodeByClass("vtkMRMLSegmentationNode")));
  maskNode->SetName(vtkSlicerAstroVolumeLogic::GetFreeNodeName(scene, std::string(data->GetName()) + "_mask").c_str());
  maskNode->SetAttribute(vtkSlicerAstroVolumeLogic::GetThresholdMaskAttributeName(), data->GetID());
  scene->AddNode(maskNode);
  maskNode->CreateDefaultDisplayNodes();
  vtkSlicerAstroVolumeLogic::SetMaskReferenceVolume(maskNode, reference);
  double segmentColor[3] = {color[0], color[1], color[2]};
  const std::string segmentID = maskNode->GetSegmentation()->AddEmptySegment(segmentName, segmentName, segmentColor);
  vtkSegment* segment = maskNode->GetSegmentation()->GetSegment(segmentID);
  if (segment)
    {
    segment->SetTerminology(vtkMRMLAstroTerminology::GetMaskEntry());
    }
  vtkNew<vtkImageThreshold> imageThreshold;
  imageThreshold->SetInputData(data->GetImageData());
  imageThreshold->ThresholdBetween(threshold, std::max(threshold, StringToDouble(data->GetAttribute("SlicerAstro.DATAMAX"))));
  imageThreshold->SetInValue(1);
  imageThreshold->SetOutValue(0);
  imageThreshold->SetOutputScalarType(VTK_SHORT);
  imageThreshold->Update();
  vtkNew<vtkOrientedImageData> labelmap;
  labelmap->ShallowCopy(imageThreshold->GetOutput());
  vtkNew<vtkMatrix4x4> ijkToRAS;
  data->GetIJKToRASMatrix(ijkToRAS);
  labelmap->SetGeometryFromImageToWorldMatrix(ijkToRAS);
  if (segment)
    {
    segment->AddRepresentation(vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName(), labelmap);
    }
  vtkSlicerAstroVolumeLogic::SetBinaryLabelmapDisplay(maskNode);
  return maskNode;
}

//----------------------------------------------------------------------------
const char* vtkSlicerAstroVolumeLogic::GetMomentMapReferenceRole(int order)
{
  switch (order)
    {
    case 0: return "SlicerAstroMomentMap0";
    case 1: return "SlicerAstroMomentMap1";
    case 2: return "SlicerAstroMomentMap2";
    default: return nullptr;
    }
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroVolumeLogic::GetMomentMapName(vtkMRMLNode* cube, int order)
{
  if (!cube || !cube->GetName() || order < 0 || order > 2)
    {
    return std::string();
    }
  return std::string(cube->GetName()) + "_mom" + std::to_string(order);
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkSlicerAstroVolumeLogic::GetMomentMap(vtkMRMLAstroVolumeNode* cube, int order)
{
  const char* role = vtkSlicerAstroVolumeLogic::GetMomentMapReferenceRole(order);
  vtkMRMLAstroVolumeNode* map = (cube && role) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(cube->GetNodeReference(role)) : nullptr;
  return (map && map != cube && map->GetScene() && map->GetImageData()) ? map : nullptr;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SetMomentMap(vtkMRMLAstroVolumeNode* cube, int order, vtkMRMLAstroVolumeNode* map)
{
  const char* role = vtkSlicerAstroVolumeLogic::GetMomentMapReferenceRole(order);
  if (!cube || !role || map == cube)
    {
    return;
    }
  vtkMRMLAstroVolumeNode* previous = vtkMRMLAstroVolumeNode::SafeDownCast(cube->GetNodeReference(role));
  cube->SetNodeReferenceID(role, map ? map->GetID() : nullptr);
  vtkMRMLScene* scene = previous ? previous->GetScene() : nullptr;
  if (previous && previous != map && previous != cube && scene)
    {
    // display (2D and 3D) and storage nodes of the previous map
    std::vector<vtkMRMLNode*> owned;
    for (int i = 0; i < previous->GetNumberOfDisplayNodes(); i++)
      {
      owned.push_back(previous->GetNthDisplayNode(i));
      }
    owned.push_back(previous->GetStorageNode());
    scene->RemoveNode(previous);
    for (vtkMRMLNode* node : owned)
      {
      if (node && node->GetScene())
        {
        scene->RemoveNode(node);
        }
      }
    }
}

//----------------------------------------------------------------------------
vtkMRMLSegmentationNode* vtkSlicerAstroVolumeLogic::GetThresholdMaskOf(vtkMRMLScene* scene, vtkMRMLNode* data)
{
  if (!scene || !data || !data->GetID())
    {
    return nullptr;
    }
  std::vector<vtkMRMLNode*> segmentations;
  scene->GetNodesByClass("vtkMRMLSegmentationNode", segmentations);
  // the last mask made from the data
  for (std::vector<vtkMRMLNode*>::reverse_iterator it = segmentations.rbegin(); it != segmentations.rend(); ++it)
    {
    const char* source = (*it)->GetAttribute(vtkSlicerAstroVolumeLogic::GetThresholdMaskAttributeName());
    if (source && !strcmp(source, data->GetID()))
      {
      return vtkMRMLSegmentationNode::SafeDownCast(*it);
      }
    }
  return nullptr;
}

namespace
{
struct ContourFamily
{
  const char* Name;
  unsigned char Stops[5][3];
};
const ContourFamily ContourFamilies[5] = {
  {"Amber", {{255, 236, 196}, {246, 194, 108}, {242, 178, 76}, {217, 140, 36}, {184, 104, 16}}},
  {"Cyan", {{198, 244, 250}, {128, 226, 238}, {79, 209, 224}, {38, 166, 190}, {16, 120, 150}}},
  {"Deep blue", {{140, 170, 255}, {90, 130, 240}, {53, 80, 176}, {38, 56, 138}, {24, 36, 100}}},
  {"White", {{255, 255, 255}, {230, 234, 242}, {200, 208, 222}, {170, 180, 198}, {140, 150, 170}}},
  {"Observatory ramp", {{43, 63, 140}, {31, 143, 166}, {127, 211, 196}, {242, 178, 76}, {255, 243, 214}}}};
} // end of anonymous namespace

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::GetContourColorFamilies(vtkStringArray* names)
{
  if (!names)
    {
    return;
    }
  names->Initialize();
  for (const ContourFamily& family : ContourFamilies)
    {
    names->InsertNextValue(family.Name);
    }
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::GetContourFamilyColors(const char* name, int count, vtkDoubleArray* colors)
{
  if (!colors || count < 1)
    {
    return false;
    }
  const ContourFamily* family = &ContourFamilies[0];
  for (const ContourFamily& candidate : ContourFamilies)
    {
    if (name && !strcmp(candidate.Name, name))
      {
      family = &candidate;
      }
    }
  colors->Initialize();
  colors->SetNumberOfComponents(3);
  for (int index = 0; index < count; index++)
    {
    const double t = count == 1 ? 0. : static_cast<double>(index) / (count - 1);
    const double position = t * 4.;
    const int low = std::min(static_cast<int>(position), 3);
    const double u = position - low;
    double rgb[3];
    for (int c = 0; c < 3; c++)
      {
      rgb[c] = (family->Stops[low][c] + (family->Stops[low + 1][c] - family->Stops[low][c]) * u) / 255.;
      }
    colors->InsertNextTuple(rgb);
    }
  return true;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::GetContourSources(vtkMRMLSegmentationNode* contours, vtkStringArray* sourceIDs,
                                                 vtkIntArray* levels)
{
  if (!sourceIDs || !levels)
    {
    return;
    }
  sourceIDs->Initialize();
  levels->Initialize();
  vtkSegmentation* segmentation = contours ? contours->GetSegmentation() : nullptr;
  if (!segmentation)
    {
    return;
    }
  std::vector<std::string> segmentIDs;
  segmentation->GetSegmentIDs(segmentIDs);
  for (const std::string& segmentID : segmentIDs)
    {
    std::string sourceID;
    vtkSegment* segment = segmentation->GetSegment(segmentID);
    if (!segment || !segment->GetTag(GetContoursVolumeTagName(), sourceID) || sourceID.empty())
      {
      continue;
      }
    const vtkIdType index = sourceIDs->LookupValue(sourceID);
    if (index < 0)
      {
      sourceIDs->InsertNextValue(sourceID);
      levels->InsertNextValue(1);
      }
    else
      {
      levels->SetValue(index, levels->GetValue(index) + 1);
      }
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SetContourSourceVisibility(vtkMRMLSegmentationNode* contours, const char* sourceID,
                                                          bool visible)
{
  vtkMRMLSegmentationDisplayNode* displayNode = contours ?
    vtkMRMLSegmentationDisplayNode::SafeDownCast(contours->GetDisplayNode()) : nullptr;
  if (!displayNode || !sourceID)
    {
    return;
    }
  if (visible)
    {
    displayNode->SetVisibility(true);
    }
  std::vector<std::string> segmentIDs;
  contours->GetSegmentation()->GetSegmentIDs(segmentIDs);
  for (const std::string& segmentID : segmentIDs)
    {
    std::string tag;
    vtkSegment* segment = contours->GetSegmentation()->GetSegment(segmentID);
    if (segment && segment->GetTag(GetContoursVolumeTagName(), tag) && tag == sourceID)
      {
      displayNode->SetSegmentVisibility(segmentID, visible);
      }
    }
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::GetContourSourceVisibility(vtkMRMLSegmentationNode* contours, const char* sourceID)
{
  vtkMRMLSegmentationDisplayNode* displayNode = contours ?
    vtkMRMLSegmentationDisplayNode::SafeDownCast(contours->GetDisplayNode()) : nullptr;
  if (!displayNode || !sourceID || !displayNode->GetVisibility())
    {
    return false;
    }
  std::vector<std::string> segmentIDs;
  contours->GetSegmentation()->GetSegmentIDs(segmentIDs);
  for (const std::string& segmentID : segmentIDs)
    {
    std::string tag;
    vtkSegment* segment = contours->GetSegmentation()->GetSegment(segmentID);
    if (segment && segment->GetTag(GetContoursVolumeTagName(), tag) && tag == sourceID &&
        displayNode->GetSegmentVisibility(segmentID))
      {
      return true;
      }
    }
  return false;
}

//----------------------------------------------------------------------------
double vtkSlicerAstroVolumeLogic::GetHIColumnDensityFactor(vtkMRMLVolumeNode* mapNode)
{
  if (!mapNode)
    {
    return 0.;
    }
  const char* unit = mapNode->GetAttribute("SlicerAstro.BUNIT");
  std::string bunit = unit ? unit : "";
  std::transform(bunit.begin(), bunit.end(), bunit.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  bunit.erase(std::remove_if(bunit.begin(), bunit.end(),
    [](char c) { return c == ' ' || c == '*' || c == '.' || c == '(' || c == ')'; }), bunit.end());
  // brightness: Jy/beam or K; velocity: km/s or m/s
  std::string brightness, velocity;
  const char* brightnessUnits[] = {"JY/BEAM", "JY/B", "K"};
  for (const char* candidate : brightnessUnits)
    {
    if (bunit.compare(0, strlen(candidate), candidate) == 0)
      {
      brightness = candidate;
      velocity = bunit.substr(strlen(candidate));
      break;
      }
    }
  double velocityFactor = 0.;
  if (velocity == "KM/S" || velocity == "KMS-1" || velocity == "KMS^-1" || velocity == "KM/SEC")
    {
    velocityFactor = 1.;
    }
  else if (velocity == "M/S" || velocity == "MS-1" || velocity == "MS^-1")
    {
    velocityFactor = 0.001;
    }
  if (brightness.empty() || velocityFactor == 0.)
    {
    return 0.;
    }
  // N_HI = 1.8224e18 cm^-2 int(T_B dv / (K km/s)), optically thin
  const double columnDensityPerKelvinKms = 1.8224e18;
  if (brightness == "K")
    {
    return columnDensityPerKelvinKms * velocityFactor;
    }
  // T_B = c^2 S / (2 k nu^2 Omega_beam), Omega_beam = pi / (4 ln 2) BMAJ BMIN
  const char* keys[2] = {"SlicerAstro.BMAJ", "SlicerAstro.BMIN"};
  double beam[2] = {0., 0.};
  for (int index = 0; index < 2; index++)
    {
    const char* value = mapNode->GetAttribute(keys[index]);
    if (!value || !strcmp(value, "UNDEFINED"))
      {
      return 0.;
      }
    beam[index] = StringToDouble(value);
    }
  if (!(beam[0] > 0.) || !(beam[1] > 0.))
    {
    return 0.;
    }
  const double degree = vtkMath::Pi() / 180.;
  const double beamSolidAngle = vtkMath::Pi() / (4. * log(2.)) * beam[0] * degree * beam[1] * degree;
  const double speedOfLight = 299792458.;
  const double boltzmann = 1.380649e-23;
  const double frequencyHI = 1420405751.768;
  const double kelvinPerJyBeam = 1e-26 * speedOfLight * speedOfLight /
    (2. * boltzmann * frequencyHI * frequencyHI * beamSolidAngle);
  return columnDensityPerKelvinKms * kelvinPerJyBeam * velocityFactor;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroVolumeLogic::CreateContours(vtkMRMLAstroVolumeNode* target, vtkMRMLAstroVolumeNode* source,
                                              vtkDoubleArray* levels, bool bands, bool overlay,
                                              vtkDoubleArray* colors, bool visible3D)
{
  int created = 0;
  this->StartCreateContours(target, source, levels, bands, overlay, colors, visible3D,
                            [&created](int count) { created = count; }, true);
  return created;
}

//----------------------------------------------------------------------------
namespace
{
/// A contour level (or band) selected for the data
struct ContourLevel
{
  double Lower;
  double Upper;
  int Index;  // position of the level (of the band) in the input
};

/// The levels of a contours job, computed in the worker thread
struct ContoursJob
{
  vtkSmartPointer<vtkImageData> Data;
  std::vector<ContourLevel> Levels;
  std::vector<vtkSmartPointer<vtkImageData> > Labelmaps;
  std::atomic<bool> Cancel{false};
  std::atomic<int> Progress{0};
};
} // end of anonymous namespace

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::StartCreateContours(vtkMRMLAstroVolumeNode* target, vtkMRMLAstroVolumeNode* source,
                                                    vtkDoubleArray* levels, bool bands, bool overlay,
                                                    vtkDoubleArray* colors, bool visible3D,
                                                    const std::function<void(int created)>& done, bool synchronous)
{
  this->LastContoursMessage.clear();
  std::function<void(int)> finished = done ? done : std::function<void(int)>([](int) {});
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene || !target || !source || !source->GetImageData() || !target->GetImageData() ||
      !source->GetAstroVolumeDisplayNode() || !levels)
    {
    this->LastContoursMessage = "the data of the contours were not found";
    finished(0);
    return;
    }

  // own copies of the levels and colours (the arrays of the caller may be
  // gone at the end); the volumes may be removed meanwhile
  vtkSmartPointer<vtkDoubleArray> levelValues = vtkSmartPointer<vtkDoubleArray>::New();
  levelValues->DeepCopy(levels);
  vtkSmartPointer<vtkDoubleArray> colorValues;
  if (colors)
    {
    colorValues = vtkSmartPointer<vtkDoubleArray>::New();
    colorValues->DeepCopy(colors);
    }
  vtkWeakPointer<vtkMRMLAstroVolumeNode> weakTarget = target;
  vtkWeakPointer<vtkMRMLAstroVolumeNode> weakSource = source;
  const std::string sourceID = source->GetID();
  const std::string sourceName = source->GetName() ? source->GetName() : "";

  // the levels of the data (the source, or the source reprojected on the
  // grid of the target: temporary volume) in the background, the segments
  // in the GUI thread
  std::function<void(vtkMRMLAstroVolumeNode*, vtkMRMLAstroVolumeNode*)> contoursOfData =
    [this, weakTarget, weakSource, sourceID, sourceName, levelValues, colorValues, bands, overlay, visible3D,
     finished, synchronous](vtkMRMLAstroVolumeNode* data, vtkMRMLAstroVolumeNode* temporaryVolume)
    {
    vtkMRMLScene* scene = this->GetMRMLScene();
    vtkWeakPointer<vtkMRMLAstroVolumeNode> weakTemporary = temporaryVolume;
    std::function<void()> removeTemporary = [this, weakTemporary]()
      {
      vtkMRMLScene* currentScene = this->GetMRMLScene();
      if (weakTemporary && currentScene && currentScene->IsNodePresent(weakTemporary))
        {
        currentScene->RemoveNode(weakTemporary->GetAstroVolumeDisplayNode());
        currentScene->RemoveNode(weakTemporary);
        }
      };

    // levels within the data (bands: pairs of levels)
    const double dataMin = StringToDouble(data->GetAttribute("SlicerAstro.DATAMIN"));
    const double dataMax = StringToDouble(data->GetAttribute("SlicerAstro.DATAMAX"));
    std::shared_ptr<ContoursJob> job = std::make_shared<ContoursJob>();
    job->Data = data->GetImageData();
    int skipped = 0;
    const int step = bands ? 2 : 1;
    for (vtkIdType ii = 0; ii + step - 1 < levelValues->GetNumberOfValues(); ii += step)
      {
      const double level = levelValues->GetValue(ii);
      const double next = bands ? levelValues->GetValue(ii + 1) : 0.;
      if (level < dataMin || level > dataMax || (bands && (next < dataMin || next > dataMax)))
        {
        skipped++;
        continue;
        }
      ContourLevel entry;
      entry.Index = static_cast<int>(ii);
      if (!bands)
        {
        entry.Lower = level < 0. ? dataMin : level;
        entry.Upper = level < 0. ? level : dataMax;
        }
      else
        {
        entry.Lower = level < 0. ? next : level;
        entry.Upper = level < 0. ? level : next;
        }
      job->Levels.push_back(entry);
      }
    if (job->Levels.empty())
      {
      removeTemporary();
      std::ostringstream message;
      message << "all the contour levels are outside the range of the data (" << dataMin << ", " << dataMax << ")";
      this->LastContoursMessage = message.str();
      finished(0);
      return;
      }
    std::string skippedMessage;
    if (skipped > 0)
      {
      std::ostringstream message;
      message << skipped << " contour level" << (skipped > 1 ? "s are" : " is") << " outside the range of the data ("
              << dataMin << ", " << dataMax << ") and " << (skipped > 1 ? "are" : "is") << " ignored";
      skippedMessage = message.str();
      }
    vtkSmartPointer<vtkMatrix4x4> ijkToRAS = vtkSmartPointer<vtkMatrix4x4>::New();
    data->GetIJKToRASMatrix(ijkToRAS);

    // the binary labelmaps of the levels, in the worker thread (the voxels
    // held, read only)
    std::function<void()> work = [job]()
      {
      const int count = static_cast<int>(job->Levels.size());
      for (int index = 0; index < count && !job->Cancel; index++)
        {
        vtkNew<vtkImageThreshold> threshold;
        threshold->SetInputData(job->Data);
        threshold->ThresholdBetween(job->Levels[index].Lower, job->Levels[index].Upper);
        threshold->SetInValue(1);
        threshold->SetOutValue(0);
        threshold->SetOutputScalarType(VTK_SHORT);
        threshold->Update();
        vtkSmartPointer<vtkImageData> labelmap = threshold->GetOutput();
        job->Labelmaps.push_back(labelmap);
        job->Progress = static_cast<int>(100. * (index + 1) / count);
        }
      };

    // the segments of the contours, in the GUI thread
    std::function<void()> finish = [this, job, weakTarget, weakSource, sourceID, sourceName, colorValues, overlay,
                                    visible3D, ijkToRAS, removeTemporary, skippedMessage, finished]()
      {
      removeTemporary();
      vtkMRMLScene* scene = this->GetMRMLScene();
      if (job->Cancel)
        {
        this->LastContoursMessage = "cancelled";
        finished(0);
        return;
        }
      if (!scene || !weakTarget || !weakSource || !scene->IsNodePresent(weakTarget) ||
          !scene->IsNodePresent(weakSource) || job->Labelmaps.size() != job->Levels.size())
        {
        this->LastContoursMessage = "the data of the contours were removed";
        finished(0);
        return;
        }
      vtkMRMLAstroVolumeNode* target = weakTarget;
      vtkMRMLAstroVolumeNode* source = weakSource;
      vtkMRMLSegmentationNode* contours = this->GetContoursSegmentationForVolume(target, true);
      if (!contours || !contours->GetSegmentation())
        {
        this->LastContoursMessage = "the contours segmentation of the volume could not be created";
        finished(0);
        return;
        }
      if (!contours->GetDisplayNode())
        {
        contours->CreateDefaultDisplayNodes();
        }

      // the new contours replace the previous ones: all of them, or with
      // overlay only those of the same data (contours of other data stay)
      vtkSegmentation* segmentation = contours->GetSegmentation();
      std::vector<std::string> previousSegmentIDs;
      segmentation->GetSegmentIDs(previousSegmentIDs);
      for (const std::string& previousSegmentID : previousSegmentIDs)
        {
        vtkSegment* previousSegment = segmentation->GetSegment(previousSegmentID);
        std::string previousSourceID;
        if (!overlay || (previousSegment &&
            previousSegment->GetTag(vtkSlicerAstroVolumeLogic::GetContoursVolumeTagName(), previousSourceID) &&
            previousSourceID == sourceID))
          {
          segmentation->RemoveSegment(previousSegmentID);
          }
        }

      vtkDoubleArray* defaultColor = source->GetAstroVolumeDisplayNode() ?
        source->GetAstroVolumeDisplayNode()->GetContoursColor() : nullptr;
      const std::string contoursEntry = vtkSlicerTerminologiesModuleLogic::GetDefaultTerminologyEntryAsString(contours);
      std::vector<std::string> segmentIDs;
      for (size_t level = 0; level < job->Levels.size(); level++)
        {
        const std::string segmentID = sourceName + "Contour" + std::to_string(job->Levels[level].Index + 1);
        vtkNew<vtkOrientedImageData> labelmap;
        labelmap->ShallowCopy(job->Labelmaps[level]);
        labelmap->SetGeometryFromImageToWorldMatrix(ijkToRAS);

        vtkSlicerAstroVolumeLogic::PrepareContoursUpdate(contours);
        // each level has its own labelmap (own layer and geometry): contours
        // of data with different grids and overlapping levels are kept as
        // they are
        if (segmentation->GetSegment(segmentID))
          {
          segmentation->RemoveSegment(segmentID);
          }
        vtkNew<vtkSegment> segment;
        segment->SetName(segmentID.c_str());
        double rgb[3] = {defaultColor ? defaultColor->GetValue(0) : 1., defaultColor ? defaultColor->GetValue(1) : 1.,
                         defaultColor ? defaultColor->GetValue(2) : 1.};
        if (colorValues && colorValues->GetNumberOfComponents() == 3 &&
            static_cast<vtkIdType>(level) < colorValues->GetNumberOfTuples())
          {
          colorValues->GetTuple(static_cast<vtkIdType>(level), rgb);
          }
        segment->SetColor(rgb);
        segment->SetTerminology(contoursEntry.empty() ? vtkMRMLAstroTerminology::GetContourEntry() : contoursEntry);
        segment->SetTag(vtkSlicerAstroVolumeLogic::GetContoursVolumeTagName(), sourceID);
        segment->SetLabelValue(1);
        segment->AddRepresentation(vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName(), labelmap);
        if (segmentation->AddSegment(segment, segmentID))
          {
          segmentIDs.push_back(segmentID);
          }
        else
          {
          vtkErrorMacro("CreateContours: failed to add the contour segment " << segmentID);
          }
        }

      for (int ii = 0; ii < contours->GetNumberOfDisplayNodes(); ii++)
        {
        vtkMRMLSegmentationDisplayNode* displayNode =
          vtkMRMLSegmentationDisplayNode::SafeDownCast(contours->GetNthDisplayNode(ii));
        if (!displayNode)
          {
          continue;
          }
        if (!overlay)
          {
          displayNode->SetAllSegmentsVisibility(false);
          }
        for (const std::string& segmentID : segmentIDs)
          {
          displayNode->SetSegmentVisibility(segmentID, true);
          displayNode->SetSegmentVisibility3D(segmentID, visible3D);
          displayNode->SetSegmentVisibility2DFill(segmentID, false);
          displayNode->SetSegmentVisibility2DOutline(segmentID, true);
          }
        }

      // closed surfaces in 2D, binary labelmaps in 3D (not a mask)
      vtkSlicerAstroVolumeLogic::SetContoursDisplay(contours);
      if (contours->GetDisplayNode())
        {
        contours->GetDisplayNode()->SetVisibility(true);
        }
      this->LastContoursMessage = skippedMessage;
      finished(static_cast<int>(segmentIDs.size()));
      };
    (void)scene;
    this->StartInBackground("Contours of " + sourceName, work, [job]() { return job->Progress.load(); },
                            [job]() { job->Cancel = true; }, finish, synchronous);
    };

  // contours of other data: reprojected on the celestial grid of the target
  // (temporary volume with the channels of the other data). Rotated grids
  // and different celestial frames are handled by the reprojection.
  bool sameGrid = true;
  if (source != target)
    {
    sameGrid = target->GetImageData()->GetDimensions()[0] == source->GetImageData()->GetDimensions()[0] &&
      target->GetImageData()->GetDimensions()[1] == source->GetImageData()->GetDimensions()[1];
    const char* gridKeys[] = {"CTYPE1", "CTYPE2", "CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2", "CDELT1", "CDELT2",
                              "CROTA2", "PC1_1", "PC1_2", "PC2_1", "PC2_2", "EQUINOX", "RADESYS"};
    for (const char* key : gridKeys)
      {
      const std::string attribute = std::string("SlicerAstro.") + key;
      const char* targetValue = target->GetAttribute(attribute.c_str());
      const char* sourceValue = source->GetAttribute(attribute.c_str());
      if ((targetValue == nullptr) != (sourceValue == nullptr) || (targetValue && strcmp(targetValue, sourceValue)))
        {
        sameGrid = false;
        }
      }
    }
  if (sameGrid)
    {
    contoursOfData(source, nullptr);
    return;
    }

  vtkInfoMacro("Contours: the data of the contours (" << source->GetName() << ") are reprojected on the grid of "
               << target->GetName() << " (bilinear interpolation).");
  vtkSmartPointer<vtkMRMLAstroReprojectParametersNode> reprojectParameters =
    vtkSmartPointer<vtkMRMLAstroReprojectParametersNode>::New();
  reprojectParameters->SetInputVolumeNodeID(source->GetID());
  reprojectParameters->SetInputVolumeNodeIDs(source->GetID());
  reprojectParameters->SetReferenceVolumeNodeID(target->GetID());
  reprojectParameters->SetSpatialGridMode(vtkMRMLAstroReprojectParametersNode::SpatialReference);
  reprojectParameters->SetSpectralGridMode(vtkMRMLAstroReprojectParametersNode::SpectralInput);
  reprojectParameters->SetInterpolationOrder(vtkMRMLAstroReprojectParametersNode::Bilinear);
  const std::string targetName = target->GetName() ? target->GetName() : "";
  const bool started = this->StartReproject(reprojectParameters,
    [this, reprojectParameters, weakTarget, weakSource, sourceName, targetName, contoursOfData, finished]
    (bool success, const std::string& error)
    {
    vtkMRMLScene* scene = this->GetMRMLScene();
    vtkMRMLAstroVolumeNode* temporaryVolume = vtkMRMLAstroVolumeNode::SafeDownCast(
      success && scene && reprojectParameters->GetOutputVolumeNodeID() ?
      scene->GetNodeByID(reprojectParameters->GetOutputVolumeNodeID()) : nullptr);
    if (!success)
      {
      this->LastContoursMessage = error == "cancelled" ? std::string("cancelled") :
        "the reprojection of " + sourceName + " on the grid of " + targetName + " failed: " + error;
      finished(0);
      return;
      }
    if (!temporaryVolume || !temporaryVolume->GetImageData())
      {
      this->LastContoursMessage = "the reprojected data of the contours were not found";
      finished(0);
      return;
      }
    for (int ii = temporaryVolume->GetNumberOfDisplayNodes() - 1; ii >= 0; ii--)
      {
      if (vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(temporaryVolume->GetNthDisplayNode(ii)))
        {
        temporaryVolume->RemoveNthDisplayNodeID(ii);
        }
      }
    if (!weakTarget || !weakSource || !scene->IsNodePresent(weakTarget) || !scene->IsNodePresent(weakSource))
      {
      scene->RemoveNode(temporaryVolume->GetAstroVolumeDisplayNode());
      scene->RemoveNode(temporaryVolume);
      this->LastContoursMessage = "the data of the contours were removed";
      finished(0);
      return;
      }
    this->CenterVolume(weakTarget);
    this->CenterVolume(temporaryVolume);
    contoursOfData(temporaryVolume, temporaryVolume);
    }, synchronous);
  if (!started)
    {
    this->LastContoursMessage = "the reprojection of " + sourceName + " on the grid of " + targetName + " failed: " +
      this->LastReprojectError;
    finished(0);
    }
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::GetGridOffset(struct wcsprm* wcsA, const int dimensionsA[3], struct wcsprm* wcsB,
                                              int offset[3])
{
  offset[0] = offset[1] = offset[2] = 0;
  if (!wcsA || !wcsB || wcsA->naxis < 1 || wcsA->naxis > 3 || wcsB->naxis < 1 || wcsB->naxis > wcsA->naxis)
    {
    return false;
    }
  // the axes of B are compared (all, or the celestial ones of a 3D A and a 2D B)
  const int compared = wcsB->naxis;
  if (compared < wcsA->naxis && (compared != 2 || wcsA->lng != 0 || wcsA->lat != 1 ||
                                 wcsB->lng != 0 || wcsB->lat != 1))
    {
    return false;
    }
  // voxels of A: the corners and the center (FITS pixel coordinates, from 1)
  std::vector<std::array<double, 3> > samples;
  for (int corner = 0; corner < 8; corner++)
    {
    std::array<double, 3> pixel = {1., 1., 1.};
    for (int axis = 0; axis < 3; axis++)
      {
      const int size = (axis < wcsA->naxis) ? std::max(1, dimensionsA[axis]) : 1;
      pixel[axis] = ((corner >> axis) & 1) ? size : 1.;
      }
    samples.push_back(pixel);
    }
  samples.push_back({0.5 * (dimensionsA[0] + 1), 0.5 * (std::max(1, dimensionsA[1]) + 1),
                     0.5 * (std::max(1, dimensionsA[2]) + 1)});
  bool first = true;
  for (const std::array<double, 3>& sample : samples)
    {
    double pixelA[4] = {sample[0], sample[1], sample[2], 0.}, imgcrd[4] = {0.}, world[4] = {0.};
    double phi[1] = {0.}, theta[1] = {0.}, pixelB[4] = {0.};
    int status[1] = {0};
    if (wcsp2s(wcsA, 1, 4, pixelA, imgcrd, phi, theta, world, status))
      {
      return false;
      }
    double worldB[4] = {world[0], world[1], world[2], 0.};
    if (wcss2p(wcsB, 1, 4, worldB, phi, theta, imgcrd, pixelB, status))
      {
      return false;
      }
    for (int axis = 0; axis < compared; axis++)
      {
      const double difference = pixelB[axis] - pixelA[axis];
      const double rounded = std::round(difference);
      if (fabs(difference - rounded) > 1.e-3)
        {
        return false;
        }
      if (first)
        {
        offset[axis] = static_cast<int>(rounded);
        }
      else if (offset[axis] != static_cast<int>(rounded))
        {
        return false;
        }
      }
    first = false;
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::GetGridOffset(vtkMRMLAstroVolumeNode* volumeA, vtkMRMLAstroVolumeNode* volumeB,
                                              int offset[3])
{
  offset[0] = offset[1] = offset[2] = 0;
  vtkMRMLAstroVolumeDisplayNode* displayA = volumeA ? volumeA->GetAstroVolumeDisplayNode() : nullptr;
  vtkMRMLAstroVolumeDisplayNode* displayB = volumeB ? volumeB->GetAstroVolumeDisplayNode() : nullptr;
  if (!displayA || !displayB || !volumeA->GetImageData() ||
      strcmp(displayA->GetSpace(), "WCS") || strcmp(displayB->GetSpace(), "WCS"))
    {
    return false;
    }
  int dimensions[3] = {0, 0, 0};
  volumeA->GetImageData()->GetDimensions(dimensions);
  return vtkSlicerAstroVolumeLogic::GetGridOffset(displayA->GetWCSStruct(), dimensions,
                                                  displayB->GetWCSStruct(), offset);
}

namespace
{
//----------------------------------------------------------------------------
// Header (WCS) of the FITS file of a mask (nullptr if not a FITS mask with a file)
vtkSmartPointer<vtkFITSReader> ReadMaskHeader(vtkMRMLSegmentationNode* segmentationNode, int dimensions[3])
{
  vtkMRMLAstroSegmentationStorageNode* storageNode = segmentationNode ?
    vtkMRMLAstroSegmentationStorageNode::SafeDownCast(segmentationNode->GetStorageNode()) : nullptr;
  const char* fileName = storageNode ? storageNode->GetFileName() : nullptr;
  if (!fileName || !*fileName || !vtksys::SystemTools::FileExists(fileName, true))
    {
    return nullptr;
    }
  vtkSmartPointer<vtkFITSReader> reader = vtkSmartPointer<vtkFITSReader>::New();
  reader->SetFileName(fileName);
  if (!reader->CanReadFile(fileName))
    {
    return nullptr;
    }
  reader->UpdateInformation();
  if (!reader->GetWCSStruct() || reader->GetWCSStatus() != 0)
    {
    return nullptr;
    }
  for (int axis = 0; axis < 3; axis++)
    {
    const std::string key = "SlicerAstro.NAXIS" + std::to_string(axis + 1);
    const char* value = reader->GetHeaderValue(key.c_str());
    dimensions[axis] = (value && *value) ? std::max(1, atoi(value)) : 1;
    }
  return reader;
}

//----------------------------------------------------------------------------
// Labelmap moved to the grid of a volume: same voxels, extent shifted by offset
void MoveLabelmapToGrid(vtkOrientedImageData* labelmap, vtkMatrix4x4* ijkToRAS, const int offset[3])
{
  int extent[6] = {0, -1, 0, -1, 0, -1};
  labelmap->GetExtent(extent);
  for (int axis = 0; axis < 3; axis++)
    {
    extent[2 * axis] += offset[axis];
    extent[2 * axis + 1] += offset[axis];
    }
  labelmap->SetExtent(extent);
  labelmap->SetGeometryFromImageToWorldMatrix(ijkToRAS);
  labelmap->Modified();
}
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkSlicerAstroVolumeLogic::FindVolumeOfMask(vtkMRMLScene* scene,
                                                                   vtkMRMLSegmentationNode* segmentationNode,
                                                                   int offset[3])
{
  offset[0] = offset[1] = offset[2] = 0;
  if (!scene || !vtkSlicerAstroVolumeLogic::IsMaskSegmentation(segmentationNode) ||
      vtkMRMLAstroVolumeNode::SafeDownCast(segmentationNode->GetNodeReference(
        vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str())))
    {
    return nullptr;
    }
  int maskDimensions[3] = {1, 1, 1};
  vtkSmartPointer<vtkFITSReader> header = ReadMaskHeader(segmentationNode, maskDimensions);
  if (!header)
    {
    return nullptr;
    }
  // the active volume first, then the most recent ones
  std::vector<vtkMRMLNode*> volumes;
  scene->GetNodesByClass("vtkMRMLAstroVolumeNode", volumes);
  std::reverse(volumes.begin(), volumes.end());
  vtkMRMLSelectionNode* selectionNode = vtkMRMLSelectionNode::SafeDownCast(
    scene->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  vtkMRMLNode* active = (selectionNode && selectionNode->GetActiveVolumeID()) ?
    scene->GetNodeByID(selectionNode->GetActiveVolumeID()) : nullptr;
  std::vector<vtkMRMLNode*>::iterator activeIt = std::find(volumes.begin(), volumes.end(), active);
  if (activeIt != volumes.end())
    {
    std::rotate(volumes.begin(), activeIt, activeIt + 1);
    }
  for (vtkMRMLNode* node : volumes)
    {
    vtkMRMLAstroVolumeNode* volumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(node);
    vtkMRMLAstroVolumeDisplayNode* displayNode = volumeNode ? volumeNode->GetAstroVolumeDisplayNode() : nullptr;
    if (!displayNode || !volumeNode->GetImageData() || strcmp(displayNode->GetSpace(), "WCS") ||
        !displayNode->GetWCSStruct() || displayNode->GetWCSStruct()->naxis != header->GetWCSStruct()->naxis)
      {
      continue;
      }
    if (vtkSlicerAstroVolumeLogic::GetGridOffset(header->GetWCSStruct(), maskDimensions,
                                                 displayNode->GetWCSStruct(), offset))
      {
      return volumeNode;
      }
    }
  offset[0] = offset[1] = offset[2] = 0;
  return nullptr;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::AttachMaskToVolume(vtkMRMLSegmentationNode* segmentationNode,
                                                   vtkMRMLAstroVolumeNode* volumeNode, const int offset[3])
{
  vtkSegmentation* segmentation = segmentationNode ? segmentationNode->GetSegmentation() : nullptr;
  if (!segmentation || !volumeNode || !volumeNode->GetImageData())
    {
    return false;
    }
  vtkNew<vtkMatrix4x4> ijkToRAS;
  volumeNode->GetIJKToRASMatrix(ijkToRAS);
  MRMLNodeModifyBlocker blocker(segmentationNode);

  // labelmaps (shared by the segments of a layer: moved once)
  const std::string labelmapName = vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName();
  std::set<vtkOrientedImageData*> moved;
  std::vector<std::string> segmentIDs;
  segmentation->GetSegmentIDs(segmentIDs);
  for (const std::string& segmentID : segmentIDs)
    {
    vtkOrientedImageData* labelmap = vtkOrientedImageData::SafeDownCast(
      segmentation->GetSegment(segmentID)->GetRepresentation(labelmapName));
    if (labelmap && moved.insert(labelmap).second)
      {
      MoveLabelmapToGrid(labelmap, ijkToRAS, offset);
      }
    }

  // reference geometry: the grid of the mask (its own extent) in the voxel
  // grid of the volume
  const std::string geometryName = vtkSegmentationConverter::GetReferenceImageGeometryParameterName();
  vtkNew<vtkOrientedImageData> geometry;
  const std::string geometryString = segmentation->GetConversionParameter(geometryName);
  if (geometryString.empty() || !vtkSegmentationConverter::DeserializeImageGeometry(geometryString, geometry, false))
    {
    geometry->SetExtent(volumeNode->GetImageData()->GetExtent());
    geometry->SetGeometryFromImageToWorldMatrix(ijkToRAS);
    }
  else
    {
    MoveLabelmapToGrid(geometry, ijkToRAS, offset);
    }
  segmentation->SetConversionParameter(geometryName, vtkSegmentationConverter::SerializeImageGeometry(geometry));
  segmentationNode->SetNodeReferenceID(vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str(),
                                       volumeNode->GetID());
  segmentationNode->Modified();
  return true;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroVolumeLogic::AttachMasksToVolume(vtkMRMLAstroVolumeNode* volumeNode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLAstroVolumeDisplayNode* displayNode = volumeNode ? volumeNode->GetAstroVolumeDisplayNode() : nullptr;
  if (!scene || !displayNode || !displayNode->GetWCSStruct() || strcmp(displayNode->GetSpace(), "WCS"))
    {
    return 0;
    }
  int attached = 0;
  std::vector<vtkMRMLNode*> segmentationNodes;
  scene->GetNodesByClass("vtkMRMLSegmentationNode", segmentationNodes);
  for (vtkMRMLNode* node : segmentationNodes)
    {
    vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node);
    if (!vtkSlicerAstroVolumeLogic::IsMaskSegmentation(segmentationNode) ||
        vtkMRMLAstroVolumeNode::SafeDownCast(segmentationNode->GetNodeReference(
          vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str())))
      {
      continue;
      }
    int maskDimensions[3] = {1, 1, 1};
    vtkSmartPointer<vtkFITSReader> header = ReadMaskHeader(segmentationNode, maskDimensions);
    int offset[3] = {0, 0, 0};
    if (header && header->GetWCSStruct()->naxis == displayNode->GetWCSStruct()->naxis &&
        vtkSlicerAstroVolumeLogic::GetGridOffset(header->GetWCSStruct(), maskDimensions,
                                                 displayNode->GetWCSStruct(), offset) &&
        vtkSlicerAstroVolumeLogic::AttachMaskToVolume(segmentationNode, volumeNode, offset))
      {
      attached++;
      }
    }
  return attached;
}

//----------------------------------------------------------------------------
namespace
{
struct OpticalLine
{
  const char* Name;
  /// rest wavelength in vacuum, Angstrom (SDSS line list)
  double Wavelength;
};
const OpticalLine OpticalLines[] = {
  {"Ly\u03B1", 1215.67},
  {"[OII] 3727", 3727.09},
  {"[OII] 3730", 3729.88},
  {"H\u03B4", 4102.89},
  {"H\u03B3", 4341.68},
  {"H\u03B2", 4862.68},
  {"[OIII] 4960", 4960.30},
  {"[OIII] 5008", 5008.24},
  {"HeI 5877", 5877.29},
  {"[OI] 6302", 6302.05},
  {"[NII] 6550", 6549.86},
  {"H\u03B1", 6564.61},
  {"[NII] 6585", 6585.27},
  {"[SII] 6718", 6718.29},
  {"[SII] 6733", 6732.67},
  {"[SIII] 9071", 9071.10},
  {"[SIII] 9533", 9533.20},
  {"Pa\u03B2", 12821.59},
  {"Pa\u03B1", 18756.13},
  {"Br\u03B3", 21661.20},
};
} // end of anonymous namespace

//----------------------------------------------------------------------------
int vtkSlicerAstroVolumeLogic::GetNumberOfOpticalLines()
{
  return static_cast<int>(sizeof(OpticalLines) / sizeof(OpticalLines[0]));
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroVolumeLogic::GetOpticalLineName(int index)
{
  return index >= 0 && index < GetNumberOfOpticalLines() ? OpticalLines[index].Name : "";
}

//----------------------------------------------------------------------------
double vtkSlicerAstroVolumeLogic::GetOpticalLineWavelength(int index)
{
  return index >= 0 && index < GetNumberOfOpticalLines() ? OpticalLines[index].Wavelength * 1.e-10 : 0.;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::IsWavelengthAxis(vtkMRMLAstroVolumeNode* volume)
{
  vtkMRMLAstroVolumeDisplayNode* displayNode = volume ? volume->GetAstroVolumeDisplayNode() : nullptr;
  return displayNode && displayNode->IsWavelengthAxis();
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::SetRestWavelength(vtkMRMLAstroVolumeNode* volume, double restWavelength)
{
  vtkMRMLAstroVolumeDisplayNode* displayNode = volume ? volume->GetAstroVolumeDisplayNode() : nullptr;
  if (!displayNode || !displayNode->IsWavelengthAxis() || !(restWavelength > 0.))
    {
    return false;
    }
  struct wcsprm* wcs = displayNode->GetWCSStruct();
  const double speedOfLight = 299792458.;
  const double restFrequency = speedOfLight / restWavelength;
  // the line, then the optical velocity (as the reader for data with a rest
  // wavelength)
  vtkFITSReader::ConvertCDMatrixToPC(wcs);
  wcs->restwav = restWavelength;
  wcs->restfrq = 0.;
  wcs->flag = 0;
  if (wcsset(wcs) || !displayNode->SetOpticalVelocityDefinition(false))
    {
    vtkGenericWarningMacro("SetRestWavelength: the spectral axis of " << (volume->GetName() ? volume->GetName() : "")
                  << " could not be converted to optical velocities.");
    return false;
    }
  displayNode->SetSpaceQuantity(2, "velocity");
  // the attributes of the spectral axis, as after loading
  volume->SetAttribute("SlicerAstro.RESTFREQ", DoubleToString(restFrequency).c_str());
  volume->SetAttribute("SlicerAstro.CTYPE3", wcs->ctype[2]);
  volume->SetAttribute("SlicerAstro.CUNIT3", wcs->cunit[2]);
  volume->SetAttribute("SlicerAstro.CRVAL3", DoubleToString(wcs->crval[2]).c_str());
  volume->SetAttribute("SlicerAstro.CDELT3", DoubleToString(wcs->cdelt[2]).c_str());
  volume->SetAttribute("SlicerAstro.CRPIX3", DoubleToString(wcs->crpix[2]).c_str());
  if (wcs->naxis == 3)
    {
    volume->SetAttribute("SlicerAstro.PC3_3", DoubleToString(wcs->pc[8]).c_str());
    }
  // saved with the axis of the file and the line (RESTFRQ)
  vtkMRMLAstroVolumeStorageNode::RecordRestFrequency(volume, restFrequency);
  displayNode->Modified();
  volume->Modified();
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::GetFITSDataDescription(const char* fileName, int hduNumber, int dimensions[3],
                                                       int* bytesPerVoxel, vtkDoubleArray* spectralValues,
                                                       std::string* spectralUnit)
{
  dimensions[0] = dimensions[1] = dimensions[2] = 1;
  if (spectralValues)
    {
    spectralValues->Reset();
    }
  if (spectralUnit)
    {
    spectralUnit->clear();
    }
  if (!fileName || !*fileName)
    {
    return false;
    }
  // the header only (UpdateInformation reads no voxels)
  vtkNew<vtkFITSReader> reader;
  reader->SetFileName(fileName);
  reader->SetHDUNumber(hduNumber);
  if (!reader->CanReadFile(fileName))
    {
    return false;
    }
  reader->UpdateInformation();
  int extent[6] = {0, -1, 0, -1, 0, -1};
  reader->GetDataExtent(extent);
  for (int axis = 0; axis < 3; axis++)
    {
    dimensions[axis] = std::max(1, extent[2 * axis + 1] - extent[2 * axis] + 1);
    }
  if (bytesPerVoxel)
    {
    *bytesPerVoxel = vtkDataArray::GetDataTypeSize(reader->GetDataScalarType());
    }
  struct wcsprm* wcs = reader->GetWCSStruct();
  if (!spectralValues || !wcs || reader->GetWCSStatus() != 0 || wcs->naxis < 3 || dimensions[2] < 2)
    {
    return true;
    }
  // the spectral coordinate of each channel at the centre of the field
  const int count = dimensions[2];
  std::vector<double> pixels(static_cast<size_t>(count) * wcs->naxis, 0.);
  for (int channel = 0; channel < count; channel++)
    {
    pixels[channel * wcs->naxis + 0] = 0.5 * (dimensions[0] + 1);
    pixels[channel * wcs->naxis + 1] = 0.5 * (dimensions[1] + 1);
    pixels[channel * wcs->naxis + 2] = channel + 1;
    for (int axis = 3; axis < wcs->naxis; axis++)
      {
      pixels[channel * wcs->naxis + axis] = 1.;
      }
    }
  std::vector<double> intermediate(pixels.size()), world(pixels.size());
  std::vector<double> phi(count), theta(count);
  std::vector<int> status(count);
  if (wcsp2s(wcs, count, wcs->naxis, pixels.data(), intermediate.data(), phi.data(), theta.data(), world.data(),
             status.data()))
    {
    return true;
    }
  // velocities in km/s, wavelengths (m for WCSLIB) in Angstrom, other
  // spectral types in their units
  const std::string unit = wcs->cunit[2];
  const double scale = (unit == "m/s") ? 0.001 : (unit == "m" ? 1.e10 : 1.);
  for (int channel = 0; channel < count; channel++)
    {
    spectralValues->InsertNextValue(world[channel * wcs->naxis + 2] * scale);
    }
  if (spectralUnit)
    {
    *spectralUnit = unit == "m/s" ? std::string("km/s") : (unit == "m" ? std::string("\u00C5") : unit);
    }
  return true;
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroVolumeLogic::GetFreeNodeName(vtkMRMLScene* scene, const std::string& baseName,
                                                       vtkMRMLNode* except)
{
  if (!scene)
    {
    return baseName;
    }
  for (int index = 0; ; index++)
    {
    const std::string candidate = index == 0 ? baseName : baseName + "_" + std::to_string(index);
    bool used = false;
    vtkSmartPointer<vtkCollection> nodes = vtkSmartPointer<vtkCollection>::Take(
      scene->GetNodesByName(candidate.c_str()));
    for (int item = 0; nodes && item < nodes->GetNumberOfItems(); item++)
      {
      vtkMRMLNode* node = vtkMRMLNode::SafeDownCast(nodes->GetItemAsObject(item));
      if (node && node != except)
        {
        used = true;
        break;
        }
      }
    if (!used)
      {
      return candidate;
      }
    }
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroVolumeLogic::IsMaskCopyTarget(vtkMRMLAstroVolumeNode* sourceVolume,
                                                 vtkMRMLAstroVolumeNode* targetVolume)
{
  if (!sourceVolume || !targetVolume || sourceVolume == targetVolume || !sourceVolume->GetImageData() ||
      !targetVolume->GetImageData())
    {
    return false;
    }
  const int sourceAxes = StringToInt(sourceVolume->GetAttribute("SlicerAstro.NAXIS"));
  const int targetAxes = StringToInt(targetVolume->GetAttribute("SlicerAstro.NAXIS"));
  if (sourceAxes != targetAxes && !(sourceAxes == 3 && targetAxes == 2))
    {
    return false;
    }
  int offset[3] = {0, 0, 0};
  if (!vtkSlicerAstroVolumeLogic::GetGridOffset(sourceVolume, targetVolume, offset))
    {
    return false;
    }
  // the grids overlap (a projection on a 2D map ignores the channels)
  int sourceDimensions[3] = {0, 0, 0};
  int targetDimensions[3] = {0, 0, 0};
  sourceVolume->GetImageData()->GetDimensions(sourceDimensions);
  targetVolume->GetImageData()->GetDimensions(targetDimensions);
  const int axes = (sourceAxes == 3 && targetAxes == 2) ? 2 : 3;
  for (int axis = 0; axis < axes; axis++)
    {
    if (offset[axis] >= targetDimensions[axis] || offset[axis] + sourceDimensions[axis] <= 0)
      {
      return false;
      }
    }
  return true;
}

//----------------------------------------------------------------------------
vtkMRMLSegmentationNode* vtkSlicerAstroVolumeLogic::CopyMaskSegmentsToVolume(
  vtkMRMLSegmentationNode* segmentationNode, vtkStringArray* segmentIDsArray,
  vtkMRMLAstroVolumeNode* targetVolume, bool move)
{
  this->LastMaskCopyError.clear();
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkSegmentation* segmentation = segmentationNode ? segmentationNode->GetSegmentation() : nullptr;
  if (!scene || !segmentation || !targetVolume || !targetVolume->GetImageData())
    {
    this->LastMaskCopyError = "no mask or no data volume to copy it to.";
    return nullptr;
    }
  vtkMRMLAstroVolumeNode* sourceVolume = vtkMRMLAstroVolumeNode::SafeDownCast(segmentationNode->GetNodeReference(
    vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str()));
  if (!sourceVolume || !sourceVolume->GetImageData())
    {
    this->LastMaskCopyError = std::string("the mask ") + (segmentationNode->GetName() ? segmentationNode->GetName() : "") +
                              " is not on the grid of a data volume (no reference volume).";
    return nullptr;
    }
  std::vector<std::string> segmentIDs;
  if (segmentIDsArray && segmentIDsArray->GetNumberOfValues() > 0)
    {
    for (vtkIdType index = 0; index < segmentIDsArray->GetNumberOfValues(); index++)
      {
      segmentIDs.push_back(segmentIDsArray->GetValue(index));
      }
    }
  else
    {
    segmentation->GetSegmentIDs(segmentIDs);
    }
  for (const std::string& segmentID : segmentIDs)
    {
    if (!segmentation->GetSegment(segmentID))
      {
      this->LastMaskCopyError = "the segment " + segmentID + " is not in the mask.";
      return nullptr;
      }
    }
  if (segmentIDs.empty())
    {
    this->LastMaskCopyError = "the mask has no segments.";
    return nullptr;
    }
  if (!vtkSlicerAstroVolumeLogic::IsMaskCopyTarget(sourceVolume, targetVolume))
    {
    this->LastMaskCopyError = std::string(targetVolume->GetName()) + " is not on the grid of " +
      sourceVolume->GetName() + ": segments are copied only to data on the same voxel grid (reproject the data "
      "first).";
    return nullptr;
    }
  vtkMRMLSegmentationNode* targetMask = this->GetMaskSegmentationForVolume(targetVolume, true);
  if (!targetMask)
    {
    this->LastMaskCopyError = std::string("the mask of ") + targetVolume->GetName() + " could not be created.";
    return nullptr;
    }
  if (targetMask == segmentationNode)
    {
    this->LastMaskCopyError = std::string("the segments are already in the mask of ") + targetVolume->GetName() + ".";
    return nullptr;
    }

  int sourceDimensions[3] = {0, 0, 0};
  sourceVolume->GetImageData()->GetDimensions(sourceDimensions);
  int targetExtent[6] = {0, -1, 0, -1, 0, -1};
  targetVolume->GetImageData()->GetExtent(targetExtent);
  const int targetDimensions[3] = {targetExtent[1] - targetExtent[0] + 1, targetExtent[3] - targetExtent[2] + 1,
                                   targetExtent[5] - targetExtent[4] + 1};
  const vtkIdType targetSlice = static_cast<vtkIdType>(targetDimensions[0]) * targetDimensions[1];
  const vtkIdType targetSize = targetSlice * targetDimensions[2];
  const int sourceAxes = StringToInt(sourceVolume->GetAttribute("SlicerAstro.NAXIS"));
  const int targetAxes = StringToInt(targetVolume->GetAttribute("SlicerAstro.NAXIS"));

  // the masks of the segments on the grid of the target volume
  std::vector<std::vector<unsigned char> > targetMasks(segmentIDs.size());
  int offset[3] = {0, 0, 0};
  vtkSlicerAstroVolumeLogic::GetGridOffset(sourceVolume, targetVolume, offset);
  // same voxel lattice (offset), or projection along the channels on a 2D
  // image of the same sky grid (IsMaskCopyTarget)
  const bool projection = sourceAxes == 3 && targetAxes == 2;
  for (size_t index = 0; index < segmentIDs.size(); index++)
    {
    vtkNew<vtkImageData> sourceMask;
    std::string error;
    if (!vtkSlicerAstroVolumeLogic::CreateSegmentMask(segmentationNode, segmentIDs[index].c_str(), sourceVolume,
                                                      sourceMask, error))
      {
      this->LastMaskCopyError = error;
      return nullptr;
      }
    const unsigned char* source = static_cast<unsigned char*>(sourceMask->GetScalarPointer());
    std::vector<unsigned char>& target = targetMasks[index];
    target.assign(static_cast<size_t>(targetSize), 0);
    for (int k = 0; k < sourceDimensions[2]; k++)
      {
      const int tk = projection ? 0 : k + offset[2];
      if (tk < 0 || tk >= targetDimensions[2])
        {
        continue;
        }
      for (int j = 0; j < sourceDimensions[1]; j++)
        {
        const int tj = j + offset[1];
        if (tj < 0 || tj >= targetDimensions[1])
          {
          continue;
          }
        const unsigned char* row = source + (static_cast<vtkIdType>(k) * sourceDimensions[1] + j) * sourceDimensions[0];
        unsigned char* targetRow = target.data() + tk * targetSlice + static_cast<vtkIdType>(tj) * targetDimensions[0];
        for (int i = 0; i < sourceDimensions[0]; i++)
          {
          const int ti = i + offset[0];
          if (row[i] && ti >= 0 && ti < targetDimensions[0])
            {
            targetRow[ti] = 1;
            }
          }
        }
      }
    }

  // the segments in the mask of the target volume (names, colors, terminology)
  vtkNew<vtkMatrix4x4> targetIJKToRAS;
  targetVolume->GetIJKToRASMatrix(targetIJKToRAS);
  vtkSegmentation* targetSegmentation = targetMask->GetSegmentation();
  const std::string labelmapName = vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName();
  {
  MRMLNodeModifyBlocker blocker(targetMask);
  for (size_t index = 0; index < segmentIDs.size(); index++)
    {
    vtkNew<vtkOrientedImageData> labelmap;
    labelmap->SetExtent(targetExtent);
    labelmap->SetGeometryFromImageToWorldMatrix(targetIJKToRAS);
    labelmap->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
    std::copy(targetMasks[index].begin(), targetMasks[index].end(),
              static_cast<unsigned char*>(labelmap->GetScalarPointer()));
    vtkNew<vtkSegment> segment;
    segment->DeepCopyMetadata(segmentation->GetSegment(segmentIDs[index]));
    segment->RemoveAllRepresentations();
    segment->SetLabelValue(1);
    segment->AddRepresentation(labelmapName, labelmap);
    targetSegmentation->AddSegment(segment, targetSegmentation->GenerateUniqueSegmentID(segmentIDs[index]));
    }
  }
  if (move)
    {
    MRMLNodeModifyBlocker blocker(segmentationNode);
    for (const std::string& segmentID : segmentIDs)
      {
      segmentation->RemoveSegment(segmentID);
      }
    }
  return targetMask;
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
  // one mask per dataset, named "<data>_mask"
  const std::string name = currentMask->GetName() ? currentMask->GetName() : "";
  volumeNode->SetNodeReferenceID(maskRole, segmentationNode->GetID());
  scene->RemoveNode(currentMask);
  if (!name.empty())
    {
    segmentationNode->SetName(name.c_str());
    }
  return true;
}

//----------------------------------------------------------------------------
vtkMRMLSegmentationNode* vtkSlicerAstroVolumeLogic::GetConflictingMask(vtkMRMLSegmentationNode* loadedMask)
{
  if (!vtkSlicerAstroVolumeLogic::IsMaskSegmentation(loadedMask))
    {
    return nullptr;
    }
  vtkMRMLAstroVolumeNode* volumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(loadedMask->GetNodeReference(
    vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str()));
  vtkMRMLSegmentationNode* currentMask = volumeNode ? vtkMRMLSegmentationNode::SafeDownCast(
    volumeNode->GetNodeReference(vtkSlicerAstroVolumeLogic::GetMaskReferenceRole())) : nullptr;
  if (!currentMask || currentMask == loadedMask || !currentMask->GetSegmentation() ||
      currentMask->GetSegmentation()->GetNumberOfSegments() == 0)
    {
    return nullptr;
    }
  return currentMask;
}

//----------------------------------------------------------------------------
vtkMRMLSegmentationNode* vtkSlicerAstroVolumeLogic::ResolveMaskConflict(vtkMRMLSegmentationNode* loadedMask,
                                                                        int resolution)
{
  vtkMRMLScene* scene = loadedMask ? loadedMask->GetScene() : nullptr;
  vtkMRMLSegmentationNode* currentMask = vtkSlicerAstroVolumeLogic::GetConflictingMask(loadedMask);
  if (!scene || !currentMask)
    {
    return loadedMask;
    }
  vtkMRMLAstroVolumeNode* volumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(loadedMask->GetNodeReference(
    vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str()));
  if (resolution == MaskConflictReplace)
    {
    const std::string name = currentMask->GetName() ? currentMask->GetName() : "";
    volumeNode->SetNodeReferenceID(vtkSlicerAstroVolumeLogic::GetMaskReferenceRole(), loadedMask->GetID());
    scene->RemoveNode(currentMask);
    if (!name.empty())
      {
      loadedMask->SetName(name.c_str());
      }
    return loadedMask;
    }
  if (resolution == MaskConflictMerge)
    {
    vtkSegmentation* target = currentMask->GetSegmentation();
    vtkSegmentation* source = loadedMask->GetSegmentation();
    std::vector<std::string> segmentIDs;
    source->GetSegmentIDs(segmentIDs);
    {
    MRMLNodeModifyBlocker blocker(currentMask);
    for (const std::string& segmentID : segmentIDs)
      {
      vtkNew<vtkSegment> segment;
      segment->DeepCopy(source->GetSegment(segmentID));
      target->AddSegment(segment, target->GenerateUniqueSegmentID(segmentID));
      }
    }
    }
  // merged or cancelled: the loaded mask (and its storage) leaves the scene
  vtkMRMLStorageNode* storageNode = loadedMask->GetStorageNode();
  scene->RemoveNode(loadedMask);
  if (storageNode && storageNode->GetScene())
    {
    scene->RemoveNode(storageNode);
    }
  return currentMask;
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

//----------------------------------------------------------------------------
std::string vtkSlicerAstroVolumeLogic::GetSlicerAstroVersion()
{
  return std::string(SLICERASTRO_VERSION);
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroVolumeLogic::GetLibraryVersions()
{
  float cfitsioVersion = 0.f;
  fits_get_version(&cfitsioVersion);
  // CFITSIO reports 4.07 for 4.7.0
  const int major = static_cast<int>(cfitsioVersion);
  const int minor = static_cast<int>(std::lround((cfitsioVersion - major) * 100.));
  std::ostringstream versions;
  versions << "CFITSIO " << major << "." << minor << ";WCSLIB " << wcslib_version(nullptr);
  return versions.str();
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkMRMLAstroVolumeDisplayNode> vtkSlicerAstroVolumeLogic::CopyDisplayNodeForWorker(
  vtkMRMLAstroVolumeNode* volume)
{
  vtkMRMLAstroVolumeDisplayNode* display = volume ? volume->GetAstroVolumeDisplayNode() : nullptr;
  if (!display)
    {
    return nullptr;
    }
  vtkSmartPointer<vtkMRMLAstroVolumeDisplayNode> copy = vtkSmartPointer<vtkMRMLAstroVolumeDisplayNode>::New();
  copy->Copy(display);
  if (!copy->GetWCSStruct())
    {
    return nullptr;
    }
  return copy;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroVolumeLogic::AddPlotLayout(vtkMRMLLayoutNode* layoutNode)
{
  const int layoutID = 3502;
  if (layoutNode && !layoutNode->IsLayoutDescription(layoutID))
    {
    const char* description =
      "<layout type=\"vertical\" split=\"true\">"
      " <item splitSize=\"620\">"
      "  <layout type=\"horizontal\" split=\"true\">"
      "   <item>"
      "    <view class=\"vtkMRMLSliceNode\" singletontag=\"Red\">"
      "     <property name=\"viewlabel\" action=\"default\">R</property>"
      "     <property name=\"viewcolor\" action=\"default\">#F34A33</property>"
      "    </view>"
      "   </item>"
      "   <item>"
      "    <view class=\"vtkMRMLViewNode\" singletontag=\"1\">"
      "     <property name=\"viewlabel\" action=\"default\">1</property>"
      "    </view>"
      "   </item>"
      "  </layout>"
      " </item>"
      " <item splitSize=\"380\">"
      "  <view class=\"vtkMRMLPlotViewNode\" singletontag=\"PlotView1\">"
      "   <property name=\"viewlabel\" action=\"default\">P</property>"
      "  </view>"
      " </item>"
      "</layout>";
    layoutNode->AddLayoutDescription(layoutID, description);
    }
  return layoutID;
}
