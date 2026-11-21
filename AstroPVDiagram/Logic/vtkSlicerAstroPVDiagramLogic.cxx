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

// Logic includes
#include "vtkSlicerAstroMomentMapsLogic.h"
#include "vtkSlicerAstroVolumeLogic.h"
#include "vtkSlicerAstroPVDiagramLogic.h"

// MRML includes
#include <vtkMRMLAstroMomentMapsParametersNode.h>
#include <vtkMRMLAstroPVDiagramParametersNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLColorTableNode.h>
#include <vtkCurveGenerator.h>
#include <vtkMRMLMarkupsCurveNode.h>
#include <vtkMRMLMarkupsDisplayNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

// VTK includes
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkSMPTools.h>
#include <vtkSmartPointer.h>

// SlicerQt includes
#include <qSlicerApplication.h>
#include <vtkSlicerApplicationLogic.h>

// WCS includes
#include "wcslib.h"

// STD includes
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <vector>

//----------------------------------------------------------------------------
class vtkSlicerAstroPVDiagramLogic::vtkInternal
{
public:
  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkSlicerAstroMomentMapsLogic> AstroMomentMapsLogic;
};

namespace
{
//----------------------------------------------------------------------------
template <typename T> T StringToNumber(const char* num)
{
  std::stringstream ss;
  ss << (num ? num : "");
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
  return StringToNumber<int>(str);
}

//----------------------------------------------------------------------------
std::string DoubleToString(double value)
{
  std::ostringstream stream;
  stream.precision(12);
  stream << value;
  return stream.str();
}

//----------------------------------------------------------------------------
std::string ZeroPadNumber(int number)
{
  std::string text = std::to_string(number);
  return std::string(text.size() < 7 ? 7 - text.size() : 0, '0') + text;
}

//----------------------------------------------------------------------------
/// Angle between two sky positions (degrees), haversine formula
double AngularDistance(const double a[2], const double b[2])
{
  const double toRadians = vtkMath::Pi() / 180.;
  const double sinDec = std::sin(0.5 * (b[1] - a[1]) * toRadians);
  const double sinRA = std::sin(0.5 * (b[0] - a[0]) * toRadians);
  const double h = sinDec * sinDec + std::cos(a[1] * toRadians) * std::cos(b[1] * toRadians) * sinRA * sinRA;
  return 2. * std::asin(std::min(1., std::sqrt(h))) / toRadians;
}

//----------------------------------------------------------------------------
/// Samples of the path one pixel apart along it (and its end), as (i, j)
/// pixel coordinates of the map
std::vector<std::array<double, 2> > SamplePath(vtkMRMLMarkupsCurveNode* path, vtkMRMLVolumeNode* map)
{
  std::vector<std::array<double, 2> > samples;
  vtkPoints* curvePoints = path ? path->GetCurvePointsWorld() : nullptr;
  if (!map || !curvePoints || path->GetNumberOfDefinedControlPoints() < 2 ||
      curvePoints->GetNumberOfPoints() < 2)
    {
    return samples;
    }
  vtkNew<vtkMatrix4x4> rasToIJK;
  map->GetRASToIJKMatrix(rasToIJK);
  std::array<double, 2> previous = {0., 0.};
  std::array<double, 2> lastSample = {0., 0.};
  double toNextSample = 1.;
  for (vtkIdType pointIndex = 0; pointIndex < curvePoints->GetNumberOfPoints(); pointIndex++)
    {
    double ras[4] = {0., 0., 0., 1.};
    double ijk[4] = {0., 0., 0., 1.};
    curvePoints->GetPoint(pointIndex, ras);
    rasToIJK->MultiplyPoint(ras, ijk);
    const std::array<double, 2> position = {ijk[0], ijk[1]};
    if (pointIndex == 0)
      {
      samples.push_back(position);
      previous = position;
      lastSample = position;
      continue;
      }
    const double deltaX = position[0] - previous[0];
    const double deltaY = position[1] - previous[1];
    const double segmentLength = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    double alongSegment = 0.;
    while (segmentLength - alongSegment >= toNextSample)
      {
      alongSegment += toNextSample;
      const double fraction = alongSegment / segmentLength;
      lastSample = {previous[0] + deltaX * fraction, previous[1] + deltaY * fraction};
      samples.push_back(lastSample);
      toNextSample = 1.;
      }
    toNextSample -= segmentLength - alongSegment;
    previous = position;
    }
  // the end of the path (if not just sampled)
  if (std::hypot(previous[0] - lastSample[0], previous[1] - lastSample[1]) > 0.5)
    {
    samples.push_back(previous);
    }
  return samples;
}

//----------------------------------------------------------------------------
/// Bilinear interpolation stencil of a position in a plane of the cube:
/// the neighbours with a weight (zero weights left out, so that a blank
/// neighbour that does not contribute does not blank the value)
struct Stencil
{
  vtkIdType Index[4];
  double Weight[4];
  int Count{0};
};

//----------------------------------------------------------------------------
bool MakeStencil(double x, double y, const int dims[3], Stencil& stencil)
{
  const double tolerance = 1e-6;
  if (x < -tolerance || y < -tolerance || x > dims[0] - 1. + tolerance || y > dims[1] - 1. + tolerance)
    {
    return false;
    }
  x = std::clamp(x, 0., dims[0] - 1.);
  y = std::clamp(y, 0., dims[1] - 1.);
  const int left = std::min(static_cast<int>(std::floor(x)), dims[0] - 1);
  const int bottom = std::min(static_cast<int>(std::floor(y)), dims[1] - 1);
  const int right = std::min(left + 1, dims[0] - 1);
  const int top = std::min(bottom + 1, dims[1] - 1);
  const double weightRight = right == left ? 0. : x - left;
  const double weightTop = top == bottom ? 0. : y - bottom;
  const int columns[4] = {left, right, left, right};
  const int rows[4] = {bottom, bottom, top, top};
  const double weights[4] = {(1. - weightRight) * (1. - weightTop), weightRight * (1. - weightTop),
                             (1. - weightRight) * weightTop, weightRight * weightTop};
  stencil.Count = 0;
  for (int neighbour = 0; neighbour < 4; neighbour++)
    {
    if (weights[neighbour] <= 0.)
      {
      continue;
      }
    stencil.Index[stencil.Count] = columns[neighbour] + static_cast<vtkIdType>(rows[neighbour]) * dims[0];
    stencil.Weight[stencil.Count] = weights[neighbour];
    stencil.Count++;
    }
  return stencil.Count > 0;
}

//----------------------------------------------------------------------------
/// PV diagram (columns x channels): per column, the mean of the finite
/// interpolated values at the positions across the path
template <typename T>
void ComputePVDiagram(const T* cube, T* pvDiagram, const int dims[3],
                      const std::vector<std::vector<Stencil> >& columns)
{
  const vtkIdType numberOfColumns = static_cast<vtkIdType>(columns.size());
  const vtkIdType sliceSize = static_cast<vtkIdType>(dims[0]) * dims[1];
  const double blank = std::numeric_limits<double>::quiet_NaN();
  vtkSMPTools::For(0, dims[2], [&](vtkIdType firstChannel, vtkIdType lastChannel)
    {
    for (vtkIdType channel = firstChannel; channel < lastChannel; channel++)
      {
      const T* plane = cube + channel * sliceSize;
      T* row = pvDiagram + channel * numberOfColumns;
      for (vtkIdType column = 0; column < numberOfColumns; column++)
        {
        double sum = 0.;
        int count = 0;
        for (const Stencil& stencil : columns[column])
          {
          double value = 0.;
          for (int neighbour = 0; neighbour < stencil.Count; neighbour++)
            {
            value += static_cast<double>(plane[stencil.Index[neighbour]]) * stencil.Weight[neighbour];
            }
          if (std::isfinite(value))
            {
            sum += value;
            count++;
            }
          }
        row[column] = static_cast<T>(count > 0 ? sum / count : blank);
        }
      }
    });
}

//----------------------------------------------------------------------------
vtkMRMLSliceNode* SliceNode(vtkMRMLScene* scene, const char* name)
{
  return scene ? vtkMRMLSliceNode::SafeDownCast(scene->GetNodeByID((std::string("vtkMRMLSliceNode") + name).c_str())) :
                 nullptr;
}

//----------------------------------------------------------------------------
/// The volume alone in the background of a slice view
void SetBackground(vtkMRMLScene* scene, const char* name, const char* volumeID)
{
  vtkMRMLSliceCompositeNode* composite = scene ? vtkMRMLSliceCompositeNode::SafeDownCast(
    scene->GetNodeByID((std::string("vtkMRMLSliceCompositeNode") + name).c_str())) : nullptr;
  if (!composite)
    {
    return;
    }
  const int wasModifying = composite->StartModify();
  composite->SetLabelVolumeID("");
  composite->SetForegroundVolumeID("");
  composite->SetForegroundOpacity(0.);
  composite->SetBackgroundVolumeID(volumeID ? volumeID : "");
  composite->EndModify(wasModifying);
}

//----------------------------------------------------------------------------
/// Orientation preset "PVDiagram" of a slice node: XY, slightly rotated so
/// that it differs from the XY preset (the PV diagram is a 2D image in its
/// IJ plane, without the XY views of the cube)
bool AddPVDiagramOrientation(vtkMRMLSliceNode* sliceNode)
{
  if (!sliceNode)
    {
    return false;
    }
  const std::string current = sliceNode->GetOrientation();
  sliceNode->SetOrientation("XY");
  vtkNew<vtkMatrix3x3> orientation;
  for (int row = 0; row < 3; row++)
    {
    for (int column = 0; column < 3; column++)
      {
      orientation->SetElement(row, column, sliceNode->GetSliceToRAS()->GetElement(row, column));
      }
    }
  orientation->SetElement(0, 0, orientation->GetElement(0, 0) + 0.001);
  if (sliceNode->HasSliceOrientationPreset("PVDiagram"))
    {
    sliceNode->GetSliceOrientationPreset("PVDiagram")->DeepCopy(orientation);
    }
  else
    {
    sliceNode->AddSliceOrientationPreset("PVDiagram", orientation);
    }
  // the orientation of the view kept
  if (!current.empty() && current != "XY")
    {
    sliceNode->SetOrientation(current.c_str());
    }
  return true;
}

//----------------------------------------------------------------------------
vtkMRMLSliceLogic* SliceLogic(vtkMRMLSliceNode* sliceNode)
{
  qSlicerApplication* app = qSlicerApplication::application();
  return (app && app->applicationLogic() && sliceNode) ? app->applicationLogic()->GetSliceLogic(sliceNode) : nullptr;
}
} // end namespace

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroPVDiagramLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroPVDiagramLogic::vtkSlicerAstroPVDiagramLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerAstroPVDiagramLogic::~vtkSlicerAstroPVDiagramLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroPVDiagramLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerAstroPVDiagramLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroPVDiagramLogic::SetAstroMomentMapsLogic(vtkSlicerAstroMomentMapsLogic *logic)
{
  this->Internal->AstroMomentMapsLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroMomentMapsLogic *vtkSlicerAstroPVDiagramLogic::GetAstroMomentMapsLogic()
{
  return this->Internal->AstroMomentMapsLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroPVDiagramLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkSlicerAstroPVDiagramLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }
  vtkNew<vtkMRMLAstroPVDiagramParametersNode> parametersNode;
  this->GetMRMLScene()->RegisterNodeClass(parametersNode);
}

//----------------------------------------------------------------------------
const char* vtkSlicerAstroPVDiagramLogic::GetSecondMomentMapReferenceRole()
{
  return "secondMomentMap";
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkSlicerAstroPVDiagramLogic::GetPVDiagram(vtkMRMLAstroVolumeNode* cube)
{
  vtkMRMLAstroVolumeNode* pvDiagram = cube ?
    vtkMRMLAstroVolumeNode::SafeDownCast(cube->GetNodeReference(GetPVDiagramReferenceRole())) : nullptr;
  return (pvDiagram && pvDiagram != cube && pvDiagram->GetScene()) ? pvDiagram : nullptr;
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkSlicerAstroPVDiagramLogic::GetInputVolume(vtkMRMLAstroPVDiagramParametersNode* pnode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  return (scene && pnode && pnode->GetInputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetInputVolumeNodeID())) : nullptr;
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkSlicerAstroPVDiagramLogic::GetMomentMap(vtkMRMLAstroPVDiagramParametersNode* pnode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLAstroVolumeNode* map = (scene && pnode && pnode->GetMomentMapNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetMomentMapNodeID())) : nullptr;
  return (map && map->GetImageData()) ? map : nullptr;
}

//----------------------------------------------------------------------------
vtkMRMLMarkupsCurveNode* vtkSlicerAstroPVDiagramLogic::GetPath(vtkMRMLAstroPVDiagramParametersNode* pnode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  return (scene && pnode && pnode->GetFiducialsMarkupsID()) ?
    vtkMRMLMarkupsCurveNode::SafeDownCast(scene->GetNodeByID(pnode->GetFiducialsMarkupsID())) : nullptr;
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkSlicerAstroPVDiagramLogic::CreateMomentMapVolume(vtkMRMLAstroVolumeNode* inputVolume,
                                                                            const std::string& nameSuffix,
                                                                            const char* bunit,
                                                                            const char* dataModel)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene || !inputVolume || !inputVolume->GetImageData() || !this->GetAstroVolumeLogic())
    {
    return nullptr;
    }

  const std::string name = vtkSlicerAstroVolumeLogic::GetFreeNodeName(
    scene, std::string(inputVolume->GetName() ? inputVolume->GetName() : "") + "_" + nameSuffix);
  vtkMRMLAstroVolumeNode* momentMapNode = vtkMRMLAstroVolumeNode::SafeDownCast
     (this->GetAstroVolumeLogic()->CloneVolumeWithoutImageData(scene, inputVolume, name.c_str()));
  if (!momentMapNode)
    {
    return nullptr;
    }

  // Modify fits attributes
  momentMapNode->SetAttribute("SlicerAstro.NAXIS", "2");
  momentMapNode->GetAstroVolumeDisplayNode()->SetAttribute("SlicerAstro.NAXIS", "2");
  momentMapNode->GetAstroVolumeDisplayNode()->CopyWCS(inputVolume->GetAstroVolumeDisplayNode());
  momentMapNode->SetAttribute("SlicerAstro.BUNIT", bunit);
  std::string btype = "";
  btype = inputVolume->GetAstroVolumeDisplayNode()->AddVelocityInfoToDisplayStringZ(btype);
  momentMapNode->SetAttribute("SlicerAstro.BTYPE", btype.c_str());
  for (const char* keyword : { "NAXIS3", "CDELT3", "CROTA3", "CRPIX3", "CRVAL3", "CTYPE3", "CUNIT3",
                               "DTYPE3", "DRVAL3", "DUNIT3", "PC1_3", "PC2_3", "PC3_1", "PC3_2", "PC3_3" })
    {
    momentMapNode->RemoveAttribute((std::string("SlicerAstro.") + keyword).c_str());
    }
  momentMapNode->SetAttribute("SlicerAstro.DATAMODEL", dataModel);

  // no volume rendering
  for (int index = momentMapNode->GetNumberOfDisplayNodes() - 1; index >= 0; index--)
    {
    if (vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(momentMapNode->GetNthDisplayNode(index)))
      {
      momentMapNode->RemoveNthDisplayNodeID(index);
      }
    }

  int dims[3] = {0, 0, 0};
  inputVolume->GetImageData()->GetDimensions(dims);
  vtkNew<vtkImageData> imageData;
  imageData->SetDimensions(dims[0], dims[1], 1);
  imageData->SetSpacing(1., 1., 1.);
  imageData->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);
  momentMapNode->SetAndObserveImageData(imageData);

  // the origin of the cube (as the maps of the Moment maps module)
  double origin[3] = {0., 0., 0.};
  inputVolume->GetOrigin(origin);
  momentMapNode->SetOrigin(origin);
  return momentMapNode;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::CalculateMomentMap(vtkMRMLAstroPVDiagramParametersNode *pnode, int order)
{
  if (!this->GetMRMLScene() || !pnode || (order != 0 && order != 2))
    {
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume = this->GetInputVolume(pnode);
  if (!inputVolume || !inputVolume->GetImageData())
    {
    this->LastError = "No data cube.";
    return false;
    }

  // the moment map of the cube shared by the modules ("<cube>_mom0",
  // "<cube>_mom2"), computed once
  vtkMRMLAstroVolumeNode* sharedMap = vtkSlicerAstroVolumeLogic::GetMomentMap(inputVolume, order);
  if (sharedMap)
    {
    if (order == 0)
      {
      pnode->SetMomentMapNodeID(sharedMap->GetID());
      }
    else
      {
      pnode->SetNodeReferenceID(vtkSlicerAstroPVDiagramLogic::GetSecondMomentMapReferenceRole(), sharedMap->GetID());
      }
    return true;
    }

  vtkMRMLAstroVolumeDisplayNode * astroMrmlDisplayNode = inputVolume->GetAstroVolumeDisplayNode();
  struct wcsprm* WCS = astroMrmlDisplayNode ? astroMrmlDisplayNode->GetWCSStruct() : nullptr;
  if (!WCS || !this->GetAstroMomentMapsLogic())
    {
    this->LastError = "The cube has no WCS.";
    return false;
    }

  // 0th moment map or 2nd moment map (velocity dispersion)
  vtkMRMLAstroVolumeNode* momentMapNode = nullptr;
  if (order == 0)
    {
    std::string bunit = inputVolume->GetAttribute("SlicerAstro.BUNIT") ?
      inputVolume->GetAttribute("SlicerAstro.BUNIT") : "";
    bunit += " km/s";
    momentMapNode = this->CreateMomentMapVolume(inputVolume, "PVDiagramMomentMap", bunit.c_str(), "ZEROMOMENTMAP");
    }
  else
    {
    momentMapNode = this->CreateMomentMapVolume(inputVolume, "PVDiagramSecondMomentMap", "km/s", "SECONDMOMENTMAP");
    vtkMRMLColorTableNode* rainbowColorTableNode = vtkMRMLColorTableNode::SafeDownCast
      (this->GetMRMLScene()->GetFirstNodeByName(vtkSlicerAstroVolumeLogic::GetMomentMapColorTableName(2)));
    if (momentMapNode && rainbowColorTableNode)
      {
      momentMapNode->GetAstroVolumeDisplayNode()->SetAndObserveColorNodeID(rainbowColorTableNode->GetID());
      }
    }
  if (!momentMapNode)
    {
    this->LastError = "The moment map volume could not be created.";
    return false;
    }

  vtkNew<vtkMRMLAstroMomentMapsParametersNode> momentMapParametersNode;
  momentMapParametersNode->SetInputVolumeNodeID(inputVolume->GetID());
  momentMapParametersNode->SetMaskActive(false);
  momentMapParametersNode->SetGenerateZero(order == 0);
  momentMapParametersNode->SetGenerateFirst(false);
  momentMapParametersNode->SetGenerateSecond(order == 2);
  if (order == 0)
    {
    momentMapParametersNode->SetZeroMomentVolumeNodeID(momentMapNode->GetID());
    }
  else
    {
    momentMapParametersNode->SetSecondMomentVolumeNodeID(momentMapNode->GetID());
    }
  // the velocity dispersion is noisy at low signal to noise: higher threshold
  const double displayThreshold = StringToDouble(inputVolume->GetAttribute("SlicerAstro.DisplayThreshold"));
  momentMapParametersNode->SetIntensityMin(displayThreshold * (order == 0 ? 2. : 4.));
  momentMapParametersNode->SetIntensityMax(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMAX")));

  // the full velocity range of the cube
  int dims[3] = {0, 0, 0};
  inputVolume->GetImageData()->GetDimensions(dims);
  double ijk[3] = {dims[0] * 0.5, dims[1] * 0.5, 0.};
  double worldOne[3] = {0., 0., 0.}, worldTwo[3] = {0., 0., 0.};
  astroMrmlDisplayNode->GetReferenceSpace(ijk, worldOne);
  ijk[2] = std::max(1., dims[2] - 1.);
  astroMrmlDisplayNode->GetReferenceSpace(ijk, worldTwo);
  if (!strcmp(WCS->cunit[2], "m/s"))
    {
    worldOne[2] /= 1000.;
    worldTwo[2] /= 1000.;
    }
  momentMapParametersNode->SetVelocityMin(std::min(worldOne[2], worldTwo[2]));
  momentMapParametersNode->SetVelocityMax(std::max(worldOne[2], worldTwo[2]));
  momentMapParametersNode->SetStatus(0);
  momentMapParametersNode->SetCores(0);

  if (!this->GetAstroMomentMapsLogic()->CalculateMomentMaps(momentMapParametersNode.GetPointer()))
    {
    this->LastError = "The moment map could not be computed (see the error log).";
    this->GetMRMLScene()->RemoveNode(momentMapNode);
    return false;
    }

  momentMapNode->SetName(vtkSlicerAstroVolumeLogic::GetMomentMapName(inputVolume, order).c_str());
  vtkSlicerAstroVolumeLogic::SetMomentMap(inputVolume, order, momentMapNode);
  if (order == 0)
    {
    pnode->SetMomentMapNodeID(momentMapNode->GetID());
    }
  else
    {
    pnode->SetNodeReferenceID(vtkSlicerAstroPVDiagramLogic::GetSecondMomentMapReferenceRole(),
                              momentMapNode->GetID());
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::PreparePath(vtkMRMLAstroPVDiagramParametersNode *pnode)
{
  this->LastError.clear();
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLAstroVolumeNode* cube = this->GetInputVolume(pnode);
  if (!scene || !cube || !cube->GetImageData())
    {
    this->LastError = "No data cube.";
    return false;
    }
  if (cube->GetImageData()->GetDimensions()[2] < 2)
    {
    this->LastError = "A PV diagram needs a data cube (two axes on the sky and the spectral axis).";
    return false;
    }

  const int wasModifying = pnode->StartModify();
  // the moment-0 map shared by the modules
  vtkMRMLAstroVolumeNode* map = vtkSlicerAstroVolumeLogic::GetMomentMap(cube, 0);
  if (map)
    {
    pnode->SetMomentMapNodeID(map->GetID());
    }
  else if (!this->CalculateMomentMap(pnode, 0))
    {
    pnode->EndModify(wasModifying);
    return false;
    }
  // the PV diagram of the cube
  vtkMRMLAstroVolumeNode* pvDiagram = vtkSlicerAstroPVDiagramLogic::GetPVDiagram(cube);
  pnode->SetOutputVolumeNodeID(pvDiagram ? pvDiagram->GetID() : nullptr);

  // the path: the referenced curve, or the one of the scene (the one with
  // points if several, e.g. after a scene import), or a new one
  vtkMRMLMarkupsCurveNode* path = this->GetPath(pnode);
  std::vector<vtkMRMLNode*> curves;
  scene->GetNodesByClass("vtkMRMLMarkupsCurveNode", curves);
  for (vtkMRMLNode* node : curves)
    {
    vtkMRMLMarkupsCurveNode* curve = vtkMRMLMarkupsCurveNode::SafeDownCast(node);
    if (curve && curve != path && curve->GetName() && !strcmp(curve->GetName(), GetPathName()) &&
        (!path || (path->GetNumberOfControlPoints() == 0 && curve->GetNumberOfControlPoints() > 0)))
      {
      path = curve;
      }
    }
  if (!path)
    {
    path = vtkMRMLMarkupsCurveNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLMarkupsCurveNode", GetPathName()));
    }
  if (!path)
    {
    pnode->EndModify(wasModifying);
    this->LastError = "The path could not be created.";
    return false;
    }
  // empty duplicates (a path created before a scene with its own was loaded)
  for (vtkMRMLNode* node : curves)
    {
    vtkMRMLMarkupsCurveNode* curve = vtkMRMLMarkupsCurveNode::SafeDownCast(node);
    if (curve && curve != path && curve->GetScene() && curve->GetName() &&
        !strcmp(curve->GetName(), GetPathName()) && curve->GetNumberOfControlPoints() == 0)
      {
      scene->RemoveNode(curve);
      }
    }
  pnode->SetFiducialsMarkupsID(path->GetID());
  pnode->EndModify(wasModifying);

  // shown on the moment maps (Red, Green) and in 3D, not on the PV diagram
  if (!path->GetDisplayNode())
    {
    path->CreateDefaultDisplayNodes();
    }
  vtkMRMLMarkupsDisplayNode* display = path->GetMarkupsDisplayNode();
  if (display)
    {
    const int wasModifyingDisplay = display->StartModify();
    display->RemoveAllViewNodeIDs();
    display->AddViewNodeID("vtkMRMLSliceNodeRed");
    display->AddViewNodeID("vtkMRMLSliceNodeGreen");
    display->AddViewNodeID("vtkMRMLViewNode1");
    display->SetGlyphType(vtkMRMLMarkupsDisplayNode::Sphere3D);
    display->SetColor(0., 0.66, 1.);
    display->SetSelectedColor(0., 0.66, 1.);
    display->EndModify(wasModifyingDisplay);
    }
  this->UpdatePathCurveType(pnode);
  this->ConstrainPath(pnode);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::UpdatePathCurveType(vtkMRMLAstroPVDiagramParametersNode *pnode)
{
  vtkMRMLMarkupsCurveNode* path = this->GetPath(pnode);
  if (!path)
    {
    return false;
    }
  const int curveType = pnode->GetInterpolation() ? vtkCurveGenerator::CURVE_TYPE_CARDINAL_SPLINE :
                                                    vtkCurveGenerator::CURVE_TYPE_LINEAR_SPLINE;
  if (path->GetCurveType() != curveType)
    {
    path->SetCurveType(curveType);
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::ConstrainPath(vtkMRMLAstroPVDiagramParametersNode *pnode)
{
  vtkMRMLMarkupsCurveNode* path = this->GetPath(pnode);
  vtkMRMLAstroVolumeNode* map = this->GetMomentMap(pnode);
  if (!path || !map)
    {
    return false;
    }
  int dims[3] = {0, 0, 0};
  map->GetImageData()->GetDimensions(dims);
  vtkNew<vtkMatrix4x4> rasToIJK;
  map->GetRASToIJKMatrix(rasToIJK);
  vtkNew<vtkMatrix4x4> ijkToRAS;
  map->GetIJKToRASMatrix(ijkToRAS);
  bool moved = false;
  for (int index = 0; index < path->GetNumberOfControlPoints(); index++)
    {
    // a point being placed (preview) or undefined is left alone
    if (path->GetNthControlPointPositionStatus(index) != vtkMRMLMarkupsNode::PositionDefined)
      {
      continue;
      }
    double ras[4] = {0., 0., 0., 1.};
    double ijk[4] = {0., 0., 0., 1.};
    path->GetNthControlPointPosition(index, ras);
    rasToIJK->MultiplyPoint(ras, ijk);
    const double clamped[4] = {std::clamp(ijk[0], 0., dims[0] - 1.), std::clamp(ijk[1], 0., dims[1] - 1.), 0., 1.};
    if (std::fabs(clamped[0] - ijk[0]) > 1e-6 || std::fabs(clamped[1] - ijk[1]) > 1e-6 || std::fabs(ijk[2]) > 1e-6)
      {
      double clampedRAS[4] = {0., 0., 0., 1.};
      ijkToRAS->MultiplyPoint(clamped, clampedRAS);
      path->SetNthControlPointPosition(index, clampedRAS[0], clampedRAS[1], clampedRAS[2]);
      moved = true;
      }
    }
  return moved;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::ClearPath(vtkMRMLAstroPVDiagramParametersNode *pnode)
{
  vtkMRMLMarkupsCurveNode* path = this->GetPath(pnode);
  if (!path)
    {
    return false;
    }
  path->RemoveAllControlPoints();
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::GetPathInfo(vtkMRMLAstroPVDiagramParametersNode *pnode,
                                              double* lengthArcmin, int* columns)
{
  vtkMRMLAstroVolumeNode* cube = this->GetInputVolume(pnode);
  const std::vector<std::array<double, 2> > samples = SamplePath(this->GetPath(pnode), this->GetMomentMap(pnode));
  vtkMRMLAstroVolumeDisplayNode* display = cube ? cube->GetAstroVolumeDisplayNode() : nullptr;
  if (samples.size() < 2 || !display)
    {
    return false;
    }
  double length = 0.;
  double previous[3] = {0., 0., 0.};
  for (size_t index = 0; index < samples.size(); index++)
    {
    double ijk[3] = {samples[index][0], samples[index][1], 0.};
    double world[3] = {0., 0., 0.};
    display->GetReferenceSpace(ijk, world);
    if (index > 0)
      {
      length += AngularDistance(previous, world);
      }
    std::copy(world, world + 3, previous);
    }
  if (lengthArcmin)
    {
    *lengthArcmin = length * 60.;
    }
  if (columns)
    {
    *columns = static_cast<int>(samples.size());
    }
  return true;
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkSlicerAstroPVDiagramLogic::CreatePVDiagramVolume(vtkMRMLAstroVolumeNode* cube)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene || !cube || !this->GetAstroVolumeLogic())
    {
    return nullptr;
    }
  const std::string name = vtkSlicerAstroVolumeLogic::GetFreeNodeName(
    scene, std::string(cube->GetName() ? cube->GetName() : "") + "_PVDiagram");
  vtkMRMLAstroVolumeNode* pvDiagram = vtkMRMLAstroVolumeNode::SafeDownCast(
    this->GetAstroVolumeLogic()->CloneVolumeWithoutImageData(scene, cube, name.c_str()));
  if (!pvDiagram)
    {
    return nullptr;
    }
  const int wasModifying = pvDiagram->StartModify();
  pvDiagram->SetAttribute("SlicerAstro.DATAMODEL", "PVDIAGRAM");
  pvDiagram->SetAttribute("SlicerAstro.NAXIS", "2");
  pvDiagram->GetAstroVolumeDisplayNode()->SetAttribute("SlicerAstro.NAXIS", "2");
  pvDiagram->GetAstroVolumeDisplayNode()->CopyWCS(cube->GetAstroVolumeDisplayNode());

  // axis 1: offset along the path (CDELT1, degrees, set by UpdatePVDiagram)
  pvDiagram->SetAttribute("SlicerAstro.NAXIS1", "1");
  pvDiagram->SetAttribute("SlicerAstro.CROTA1", "0");
  pvDiagram->SetAttribute("SlicerAstro.CRPIX1", "1");
  pvDiagram->SetAttribute("SlicerAstro.CRVAL1", "0");
  pvDiagram->SetAttribute("SlicerAstro.CTYPE1", "OFFSET");
  pvDiagram->SetAttribute("SlicerAstro.CUNIT1", "deg");
  // axis 2: the spectral axis of the cube
  const char* spectralStep = cube->GetAttribute("SlicerAstro.CDELT3") ? cube->GetAttribute("SlicerAstro.CDELT3") :
                                                                       cube->GetAttribute("SlicerAstro.CD3_3");
  if (spectralStep)
    {
    pvDiagram->SetAttribute("SlicerAstro.CDELT2", spectralStep);
    }
  for (const char* keyword : {"NAXIS", "CROTA", "CRPIX", "CRVAL", "CTYPE", "CUNIT", "DTYPE", "DRVAL", "DUNIT"})
    {
    const std::string from = std::string("SlicerAstro.") + keyword + "3";
    const std::string to = std::string("SlicerAstro.") + keyword + "2";
    const char* value = cube->GetAttribute(from.c_str());
    if (value)
      {
      pvDiagram->SetAttribute(to.c_str(), value);
      }
    else
      {
      pvDiagram->RemoveAttribute(to.c_str());
      }
    pvDiagram->RemoveAttribute(from.c_str());
    }
  pvDiagram->RemoveAttribute("SlicerAstro.CDELT3");
  for (int row = 1; row <= 3; row++)
    {
    for (int column = 1; column <= 3; column++)
      {
      const std::string suffix = std::to_string(row) + "_" + std::to_string(column);
      pvDiagram->RemoveAttribute(("SlicerAstro.PC" + suffix).c_str());
      pvDiagram->RemoveAttribute(("SlicerAstro.CD" + suffix).c_str());
      }
    }

  // no volume rendering, no WCS in the views (the PV axes of the 2D axes)
  for (int index = pvDiagram->GetNumberOfDisplayNodes() - 1; index >= 0; index--)
    {
    if (vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(pvDiagram->GetNthDisplayNode(index)))
      {
      pvDiagram->RemoveNthDisplayNodeID(index);
      }
    }
  vtkMRMLAstroVolumeDisplayNode* display = pvDiagram->GetAstroVolumeDisplayNode();
  display->SetSpace("IJK");
  vtkMRMLNode* noPreset = nullptr;
  pvDiagram->SetPresetNode(noPreset);
  // the 2D colours of the cube (same intensities): stable while the path
  // changes
  vtkMRMLAstroVolumeDisplayNode* cubeDisplay = cube->GetAstroVolumeDisplayNode();
  if (cubeDisplay)
    {
    const int wasModifyingDisplay = display->StartModify();
    display->SetAutoWindowLevel(0);
    display->SetAndObserveColorNodeID(cubeDisplay->GetColorNodeID());
    display->SetWindowLevel(cubeDisplay->GetWindow(), cubeDisplay->GetLevel());
    display->EndModify(wasModifyingDisplay);
    }
  pvDiagram->EndModify(wasModifying);

  // the origin of the PV diagram at the first column and channel
  double origin[3] = {0., 0., 0.};
  cube->GetOrigin(origin);
  origin[1] = 0.;
  pvDiagram->SetOrigin(origin);
  return pvDiagram;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::UpdatePVDiagram(vtkMRMLAstroPVDiagramParametersNode *pnode)
{
  this->LastError.clear();
  vtkMRMLAstroVolumeNode* cube = this->GetInputVolume(pnode);
  vtkMRMLAstroVolumeNode* map = this->GetMomentMap(pnode);
  vtkMRMLMarkupsCurveNode* path = this->GetPath(pnode);
  vtkImageData* cubeImage = cube ? cube->GetImageData() : nullptr;
  if (!cubeImage || !map || !path)
    {
    this->LastError = "No data cube, moment map or path (PreparePath first).";
    return false;
    }
  const int scalarType = cubeImage->GetScalarType();
  if ((scalarType != VTK_FLOAT && scalarType != VTK_DOUBLE) || cubeImage->GetNumberOfScalarComponents() != 1)
    {
    this->LastError = "The data cube must have float or double voxels.";
    return false;
    }
  vtkMRMLAstroVolumeDisplayNode* cubeDisplay = cube->GetAstroVolumeDisplayNode();
  if (!cubeDisplay)
    {
    this->LastError = "The data cube has no display node.";
    return false;
    }
  const std::vector<std::array<double, 2> > samples = SamplePath(path, map);
  if (samples.size() < 2)
    {
    this->LastError = "The path needs at least two points.";
    return false;
    }
  int dims[3] = {0, 0, 0};
  cubeImage->GetDimensions(dims);

  // interpolation stencils of the columns: positions across the path, one
  // pixel apart, along the normal of the path
  const int width = pnode->GetWidth();
  const int numberOfColumns = static_cast<int>(samples.size());
  std::vector<std::vector<Stencil> > columns(numberOfColumns);
  for (int column = 0; column < numberOfColumns; column++)
    {
    const std::array<double, 2>& before = samples[std::max(column - 1, 0)];
    const std::array<double, 2>& after = samples[std::min(column + 1, numberOfColumns - 1)];
    double tangent[2] = {after[0] - before[0], after[1] - before[1]};
    const double norm = std::hypot(tangent[0], tangent[1]);
    if (norm > 0.)
      {
      tangent[0] /= norm;
      tangent[1] /= norm;
      }
    const double normal[2] = {-tangent[1], tangent[0]};
    for (int across = 0; across < width; across++)
      {
      const double offset = across - 0.5 * (width - 1);
      Stencil stencil;
      if (MakeStencil(samples[column][0] + offset * normal[0], samples[column][1] + offset * normal[1], dims, stencil))
        {
        columns[column].push_back(stencil);
        }
      }
    }

  vtkNew<vtkImageData> image;
  image->SetDimensions(numberOfColumns, dims[2], 1);
  image->SetSpacing(1., 1., 1.);
  image->AllocateScalars(scalarType, 1);
  if (scalarType == VTK_FLOAT)
    {
    ComputePVDiagram<float>(static_cast<const float*>(cubeImage->GetScalarPointer()),
                            static_cast<float*>(image->GetScalarPointer()), dims, columns);
    }
  else
    {
    ComputePVDiagram<double>(static_cast<const double*>(cubeImage->GetScalarPointer()),
                             static_cast<double*>(image->GetScalarPointer()), dims, columns);
    }

  // length of the path on the sky: offset step of the columns (degrees)
  double length = 0.;
  double previous[3] = {0., 0., 0.};
  for (int column = 0; column < numberOfColumns; column++)
    {
    double ijk[3] = {samples[column][0], samples[column][1], 0.};
    double world[3] = {0., 0., 0.};
    cubeDisplay->GetReferenceSpace(ijk, world);
    if (column > 0)
      {
      length += AngularDistance(previous, world);
      }
    std::copy(world, world + 3, previous);
    }

  vtkMRMLAstroVolumeNode* pvDiagram = vtkSlicerAstroPVDiagramLogic::GetPVDiagram(cube);
  if (!pvDiagram)
    {
    pvDiagram = this->CreatePVDiagramVolume(cube);
    if (!pvDiagram)
      {
      this->LastError = "The PV diagram volume could not be created.";
      return false;
      }
    cube->SetNodeReferenceID(GetPVDiagramReferenceRole(), pvDiagram->GetID());
    }

  const int wasModifying = pvDiagram->StartModify();
  pvDiagram->SetAttribute("SlicerAstro.NAXIS1", std::to_string(numberOfColumns).c_str());
  pvDiagram->SetAttribute("SlicerAstro.CDELT1", DoubleToString(length / (numberOfColumns - 1)).c_str());
  pvDiagram->SetAndObserveImageData(image);
  pvDiagram->UpdateRangeAttributes();

  // the path in the header (HISTORY cards, replaced at each update)
  std::vector<std::string> history;
  std::ostringstream summary;
  summary << "PV diagram of " << (cube->GetName() ? cube->GetName() : "") << " along a path of "
          << path->GetNumberOfDefinedControlPoints() << " points ("
          << (pnode->GetInterpolation() ? "spline" : "straight segments") << "), width " << width << " pixels";
  history.push_back(summary.str());
  vtkNew<vtkMatrix4x4> rasToIJK;
  map->GetRASToIJKMatrix(rasToIJK);
  for (int index = 0; index < path->GetNumberOfControlPoints(); index++)
    {
    if (path->GetNthControlPointPositionStatus(index) != vtkMRMLMarkupsNode::PositionDefined)
      {
      continue;
      }
    double ras[4] = {0., 0., 0., 1.};
    double ijk[4] = {0., 0., 0., 1.};
    path->GetNthControlPointPosition(index, ras);
    rasToIJK->MultiplyPoint(ras, ijk);
    double world[3] = {0., 0., 0.};
    ijk[2] = 0.;
    cubeDisplay->GetReferenceSpace(ijk, world);
    std::ostringstream point;
    point.precision(9);
    point << "PV diagram path point " << index + 1 << ": " << world[0] << ", " << world[1] << " deg";
    history.push_back(point.str());
    }
  std::vector<std::string> attributeNames = pvDiagram->GetAttributeNames();
  int lastHistory = 0;
  for (const std::string& attributeName : attributeNames)
    {
    if (attributeName.compare(0, 20, "SlicerAstro._HISTORY") != 0)
      {
      continue;
      }
    const char* value = pvDiagram->GetAttribute(attributeName.c_str());
    if (value && !strncmp(value, "PV diagram", 10))
      {
      pvDiagram->RemoveAttribute(attributeName.c_str());
      continue;
      }
    lastHistory = std::max(lastHistory, StringToInt(attributeName.substr(20).c_str()));
    }
  for (const std::string& card : history)
    {
    pvDiagram->SetAttribute(("SlicerAstro._HISTORY" + ZeroPadNumber(++lastHistory)).c_str(), card.c_str());
    }
  pvDiagram->EndModify(wasModifying);

  vtkMRMLAstroVolumeDisplayNode* display = pvDiagram->GetAstroVolumeDisplayNode();
  if (display)
    {
    display->SetFitSlices(true);
    }
  pnode->SetOutputVolumeNodeID(pvDiagram->GetID());
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::SetMomentMapsOnViews(vtkMRMLAstroPVDiagramParametersNode *pnode,
                                                       bool secondMomentMap)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!pnode || !scene || !this->GetMomentMap(pnode))
    {
    return false;
    }

  // Red: the moment-0 map (XY; the XZ orientation set first forces the
  // refresh of the view)
  SetBackground(scene, "Red", pnode->GetMomentMapNodeID());
  vtkMRMLSliceNode* red = SliceNode(scene, "Red");
  if (red)
    {
    red->SetOrientation("XZ");
    red->SetOrientation("XY");
    vtkMRMLSliceLogic* redLogic = SliceLogic(red);
    if (redLogic)
      {
      redLogic->FitSliceToAll(red->GetDimensions()[0], red->GetDimensions()[1]);
      redLogic->SnapSliceOffsetToIJK();
      }
    }

  // Green: the 2nd moment map (velocity dispersion)
  if (secondMomentMap)
    {
    if (!pnode->GetNodeReference(GetSecondMomentMapReferenceRole()))
      {
      this->CalculateMomentMap(pnode, 2);
      }
    vtkMRMLNode* second = pnode->GetNodeReference(GetSecondMomentMapReferenceRole());
    SetBackground(scene, "Green", second ? second->GetID() : nullptr);
    vtkMRMLSliceNode* green = SliceNode(scene, "Green");
    if (green)
      {
      green->SetOrientation("XZ");
      green->SetOrientation("XY");
      vtkMRMLSliceLogic* greenLogic = SliceLogic(green);
      if (greenLogic)
        {
        greenLogic->FitSliceToAll(green->GetDimensions()[0], green->GetDimensions()[1]);
        greenLogic->SnapSliceOffsetToIJK();
        }
      }
    }

  // Yellow: prepared for the PV diagram
  return AddPVDiagramOrientation(SliceNode(scene, "Yellow"));
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::SetPVDiagramOnViews(vtkMRMLAstroPVDiagramParametersNode *pnode, bool fitView)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLAstroVolumeNode* pvDiagram = (scene && pnode && pnode->GetOutputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetOutputVolumeNodeID())) : nullptr;
  vtkMRMLSliceNode* yellow = SliceNode(scene, "Yellow");
  if (!pvDiagram || !pvDiagram->GetImageData() || !yellow)
    {
    return false;
    }
  if (!yellow->HasSliceOrientationPreset("PVDiagram"))
    {
    AddPVDiagramOrientation(yellow);
    }
  SetBackground(scene, "Yellow", pvDiagram->GetID());
  yellow->SetOrientation("PVDiagram");
  yellow->SetSliceOffset(0.);
  if (!fitView)
    {
    return true;
    }
  vtkMRMLSliceLogic* yellowLogic = SliceLogic(yellow);
  if (yellowLogic)
    {
    yellowLogic->FitSliceToAll(yellow->GetDimensions()[0], yellow->GetDimensions()[1]);
    yellowLogic->SnapSliceOffsetToIJK();
    }
  int dims[3] = {0, 0, 0};
  pvDiagram->GetImageData()->GetDimensions(dims);
  double fieldOfView[3] = {0., 0., 0.};
  yellow->GetFieldOfView(fieldOfView);
  yellow->SetFieldOfView(dims[0], dims[1], fieldOfView[2]);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::GenerateAndSetPVDiagram(vtkMRMLAstroPVDiagramParametersNode *pnode)
{
  return this->UpdatePVDiagram(pnode) && this->SetPVDiagramOnViews(pnode, true);
}
