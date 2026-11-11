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
#include "vtkSlicerAstroConfigure.h"
#include <vtkSMPTools.h>

// STD includes
#include <algorithm>

// MRML includes
#include <vtkMRMLAstroMomentMapsParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroPVDiagramParametersNode.h>
#include <vtkMRMLColorTableNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLMarkupsCurveNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

// VTK includes
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCacheManager.h>
#include <vtkGeneralTransform.h>
#include <vtkImageData.h>
#include <vtkLine.h>
#include <vtkMath.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkVersion.h>

// SlicerQt includes
#include <qMRMLSliceWidget.h>
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerUtils.h>

// STD includes
#include <cassert>
#include <iostream>
#include <chrono>

// OpenMP includes


//----------------------------------------------------------------------------
class vtkSlicerAstroPVDiagramLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkSlicerAstroMomentMapsLogic> AstroMomentMapsLogic;
};

//----------------------------------------------------------------------------
vtkSlicerAstroPVDiagramLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = nullptr;
  this->AstroMomentMapsLogic = nullptr;
}

//---------------------------------------------------------------------------
vtkSlicerAstroPVDiagramLogic::vtkInternal::~vtkInternal()
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

//----------------------------------------------------------------------------
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
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
std::string IntToString(int Value)
{
  return NumberToString<int>(Value);
}

//----------------------------------------------------------------------------
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
}

//----------------------------------------------------------------------------
std::string ZeroPadNumber(int num)
{
  std::stringstream ss;

  // the number is converted to string with the help of stringstream
  ss << num;
  std::string ret;
  ss >> ret;

  // Append zero chars
  int str_length = ret.length();
  for (int ii = 0; ii < 7 - str_length; ii++)
    ret = "0" + ret;
  return ret;
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
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerAstroPVDiagramLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerAstroPVDiagramLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  vtkMRMLAstroPVDiagramParametersNode* pNode = vtkMRMLAstroPVDiagramParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

//----------------------------------------------------------------------------
const char* vtkSlicerAstroPVDiagramLogic::GetSecondMomentMapReferenceRole()
{
  return "secondMomentMap";
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkSlicerAstroPVDiagramLogic::CreateMomentMapVolume(vtkMRMLAstroVolumeNode* inputVolume,
                                                                            const std::string& nameSuffix,
                                                                            const char* bunit,
                                                                            const char* dataModel)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene || !inputVolume || !inputVolume->GetImageData())
    {
    return nullptr;
    }

  std::string name = inputVolume->GetName();
  name += "_";
  name += scene->GetUniqueNameByString(nameSuffix.c_str());

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

  // Remove old rendering Display
  int ndnodes = momentMapNode->GetNumberOfDisplayNodes();
  for (int ii = ndnodes - 1; ii >= 0; ii--)
    {
    vtkMRMLVolumeRenderingDisplayNode *dnode =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(momentMapNode->GetNthDisplayNode(ii));
    if (dnode)
      {
      momentMapNode->RemoveNthDisplayNodeID(ii);
      }
    }

  // Copy a 2D image into the Astro Volume object
  int N1 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1"));
  int N2 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2"));
  vtkNew<vtkImageData> imageDataTemp;
  imageDataTemp->SetDimensions(N1, N2, 1);
  imageDataTemp->SetSpacing(1.,1.,1.);
  imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);
  momentMapNode->SetAndObserveImageData(imageDataTemp.GetPointer());

  // Set the Origin
  double Origin[3];
  inputVolume->GetOrigin(Origin);
  Origin[1] = -Origin[1];
  momentMapNode->SetOrigin(Origin);

  return momentMapNode;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::CalculateMomentMap(vtkMRMLAstroPVDiagramParametersNode *pnode, int order)
{
  if (!this->GetMRMLScene() || !pnode || (order != 0 && order != 2))
    {
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::CalculateMomentMap :"
                  " inputVolume not found.");
    return false;
    }

  vtkMRMLAstroVolumeDisplayNode * astroMrmlDisplayNode = inputVolume->GetAstroVolumeDisplayNode();
  struct wcsprm* WCS = astroMrmlDisplayNode ? astroMrmlDisplayNode->GetWCSStruct() : nullptr;
  if (!WCS)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::CalculateMomentMap :"
                  " WCS of the input volume not found.");
    return false;
    }

  // 0th moment map (Red view) or 2nd moment map (velocity dispersion, Green view)
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
      (this->GetMRMLScene()->GetFirstNodeByName("Rainbow"));
    if (momentMapNode && rainbowColorTableNode)
      {
      momentMapNode->GetAstroVolumeDisplayNode()->SetAndObserveColorNodeID(rainbowColorTableNode->GetID());
      }
    }
  if (!momentMapNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::CalculateMomentMap :"
                  " the moment map volume could not be created.");
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
  int N1 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1"));
  int N2 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2"));
  double ijk[3], worldOne[3], worldTwo[3];
  ijk[0] = N1 * 0.5;
  ijk[1] = N2 * 0.5;
  ijk[2] = 0.;
  astroMrmlDisplayNode->GetReferenceSpace(ijk, worldOne);
  ijk[2] = StringToDouble(inputVolume->GetAttribute("SlicerAstro.NAXIS3")) - 1.;
  if (ijk[2] < 1)
    {
    ijk[2] = 1;
    }
  astroMrmlDisplayNode->GetReferenceSpace(ijk, worldTwo);
  if(!strcmp(WCS->cunit[2], "m/s"))
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
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::CalculateMomentMap :"
                  " MomentMaps calculations failed.");
    return false;
    }

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
bool vtkSlicerAstroPVDiagramLogic::SetMomentMapsOnViews(vtkMRMLAstroPVDiagramParametersNode *pnode)
{
  if (!pnode || !this->GetMRMLScene())
    {
    return false;
    }

  // Setting the Layout for the Output
  qSlicerApplication* app = qSlicerApplication::application();
  if(!app)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::SetMomentMapsOnViews : "
                   "app not found!");
    return false;
    }

  qSlicerLayoutManager* layoutManager = app->layoutManager();

  if(!layoutManager)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::SetMomentMapsOnViews : "
                   "layoutManager not found!");
    return false;
    }

  int viewArra = layoutManager->layoutLogic()->GetLayoutNode()->GetViewArrangement();
  if (viewArra != vtkMRMLLayoutNode::SlicerLayoutFourUpView)
    {
    layoutManager->layoutLogic()->GetLayoutNode()->SetViewArrangement(vtkMRMLLayoutNode::SlicerLayoutFourUpView);
    }

  vtkMRMLSliceCompositeNode *redSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
    this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeRed"));
  redSliceComposite->SetLabelVolumeID("");
  redSliceComposite->SetForegroundVolumeID("");
  redSliceComposite->SetForegroundOpacity(0.);
  redSliceComposite->SetBackgroundVolumeID(pnode->GetMomentMapNodeID());

  vtkMRMLSliceNode *redSlice = vtkMRMLSliceNode::SafeDownCast(
    this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
  // setting to the XZ orientation is needed in order to force the refresh
  redSlice->SetOrientation("XZ");
  redSlice->SetOrientation("XY");

  vtkMRMLSliceCompositeNode *yellowSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
    this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow"));
  yellowSliceComposite->SetLabelVolumeID("");
  yellowSliceComposite->SetForegroundVolumeID("");
  yellowSliceComposite->SetForegroundOpacity(0.);
  yellowSliceComposite->SetBackgroundVolumeID(pnode->GetInputVolumeNodeID());

  // 2nd moment map (velocity dispersion) in the Green view
  vtkMRMLNode* secondMomentMapNode =
    pnode->GetNodeReference(vtkSlicerAstroPVDiagramLogic::GetSecondMomentMapReferenceRole());
  vtkMRMLSliceCompositeNode *greenSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
    this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeGreen"));
  greenSliceComposite->SetLabelVolumeID("");
  greenSliceComposite->SetForegroundVolumeID("");
  greenSliceComposite->SetForegroundOpacity(0.);
  greenSliceComposite->SetBackgroundVolumeID(secondMomentMapNode ? secondMomentMapNode->GetID() : "");

  vtkSlicerApplicationLogic *appLogic = app->applicationLogic();
  if (!appLogic)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::SetMomentMapsOnViews : "
                   "appLogic not found!");
    return false;
    }

  vtkMRMLSliceLogic* redSliceLogic = appLogic->GetSliceLogic(redSlice);
  if (!redSliceLogic)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::SetMomentMapsOnViews : "
                   "redSliceLogic not found!");
    return false;
    }

  int *dims = redSlice->GetDimensions();
  if (dims)
    {
    redSliceLogic->FitSliceToAll(dims[0], dims[1]);
    }
  redSliceLogic->SnapSliceOffsetToIJK();

  vtkMRMLSliceNode *greenSlice = vtkMRMLSliceNode::SafeDownCast(
    this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  vtkMRMLSliceLogic* greenSliceLogic = greenSlice ? appLogic->GetSliceLogic(greenSlice) : nullptr;
  if (greenSliceLogic)
    {
    // setting to the XZ orientation is needed in order to force the refresh
    greenSlice->SetOrientation("XZ");
    greenSlice->SetOrientation("XY");
    int *greenDims = greenSlice->GetDimensions();
    if (greenDims)
      {
      greenSliceLogic->FitSliceToAll(greenDims[0], greenDims[1]);
      }
    greenSliceLogic->SnapSliceOffsetToIJK();
    }

  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!yellowSliceNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::InitializePV :"
                  " yellowSliceNode not found.");
    return false;
    }

  yellowSliceNode->SetOrientation("XY");
  vtkNew<vtkMatrix3x3> PVDiagramMatrix;

  PVDiagramMatrix->SetElement(0, 0, yellowSliceNode->GetSliceToRAS()->GetElement(0,0) + 0.001);
  PVDiagramMatrix->SetElement(0, 1, yellowSliceNode->GetSliceToRAS()->GetElement(0,1));
  PVDiagramMatrix->SetElement(0, 2, yellowSliceNode->GetSliceToRAS()->GetElement(0,2));
  PVDiagramMatrix->SetElement(1, 0, yellowSliceNode->GetSliceToRAS()->GetElement(1,0));
  PVDiagramMatrix->SetElement(1, 1, yellowSliceNode->GetSliceToRAS()->GetElement(1,1));
  PVDiagramMatrix->SetElement(1, 2, yellowSliceNode->GetSliceToRAS()->GetElement(1,2));
  PVDiagramMatrix->SetElement(2, 0, yellowSliceNode->GetSliceToRAS()->GetElement(2,0));
  PVDiagramMatrix->SetElement(2, 1, yellowSliceNode->GetSliceToRAS()->GetElement(2,1));
  PVDiagramMatrix->SetElement(2, 2, yellowSliceNode->GetSliceToRAS()->GetElement(2,2));

  if (yellowSliceNode->HasSliceOrientationPreset("PVDiagram"))
    {
    yellowSliceNode->GetSliceOrientationPreset("PVDiagram")->DeepCopy(PVDiagramMatrix.GetPointer());
    }
  else
    {
    yellowSliceNode->AddSliceOrientationPreset("PVDiagram", PVDiagramMatrix.GetPointer());
    }

  yellowSliceNode->SetOrientation("PVDiagram");

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::GenerateAndSetPVDiagram(vtkMRMLAstroPVDiagramParametersNode *pnode)
{

  if (!this->GetMRMLScene() || !pnode)
    {
    return false;
    }

  // line of the PV diagram: Markups open curve (linear or spline)
  vtkMRMLMarkupsCurveNode *curveNode =
    vtkMRMLMarkupsCurveNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetFiducialsMarkupsID()));
  if(!curveNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::GenerateAndSetPVDiagram :"
                  " line (curve) not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *PVMomentMap =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetMomentMapNodeID()));
  if(!PVMomentMap || !PVMomentMap->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::GenerateAndSetPVDiagram :"
                  " PVMomentMap not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::GenerateAndSetPVDiagram :"
                  " inputVolume not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *PVDiagramVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if(!PVDiagramVolume || !PVDiagramVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::GenerateAndSetPVDiagram :"
                  " PVDiagramVolume not found.");
    return false;
    }

  vtkPoints* curvePoints = curveNode->GetCurvePointsWorld();
  if (curveNode->GetNumberOfDefinedControlPoints() < 2 || !curvePoints || curvePoints->GetNumberOfPoints() < 2)
    {
    return false;
    }

  vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
  PVMomentMap->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());

  // The interpolated points of the curve in the pixels of the moment map,
  // resampled every pixel along the line (positions of the PV diagram columns)
  vtkNew<vtkPoints> points;
  double previousIJK[3] = {0., 0., 0.};
  double lastSampleIJK[3] = {0., 0., 0.};
  double toNextSample = 1.;
  for (vtkIdType pointIndex = 0; pointIndex < curvePoints->GetNumberOfPoints(); pointIndex++)
    {
    double posRAS[4] = {0., 0., 0., 1.}, posIJK[4] = {0., 0., 0., 1.};
    curvePoints->GetPoint(pointIndex, posRAS);
    RAStoIJKMatrix->MultiplyPoint(posRAS, posIJK);
    posIJK[2] = 0.;
    if (pointIndex == 0)
      {
      points->InsertNextPoint(posIJK);
      std::copy(posIJK, posIJK + 3, previousIJK);
      std::copy(posIJK, posIJK + 3, lastSampleIJK);
      continue;
      }
    const double deltaX = posIJK[0] - previousIJK[0];
    const double deltaY = posIJK[1] - previousIJK[1];
    const double segmentLength = sqrt(deltaX * deltaX + deltaY * deltaY);
    double alongSegment = 0.;
    while (segmentLength - alongSegment >= toNextSample)
      {
      alongSegment += toNextSample;
      const double fraction = alongSegment / segmentLength;
      lastSampleIJK[0] = previousIJK[0] + deltaX * fraction;
      lastSampleIJK[1] = previousIJK[1] + deltaY * fraction;
      lastSampleIJK[2] = 0.;
      points->InsertNextPoint(lastSampleIJK);
      toNextSample = 1.;
      }
    toNextSample -= segmentLength - alongSegment;
    std::copy(posIJK, posIJK + 3, previousIJK);
    }
  // the end of the line (if not just sampled)
  if (sqrt(vtkMath::Distance2BetweenPoints(previousIJK, lastSampleIJK)) > 0.5)
    {
    points->InsertNextPoint(previousIJK);
    }
  if (points->GetNumberOfPoints() < 2)
    {
    return false;
    }

  // Get dimensions
  int N1 = points->GetNumberOfPoints();
  int N2 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS3"));

  // Create an empty 2D image
  vtkNew<vtkImageData> imageDataTemp;
  imageDataTemp->SetDimensions(N1, N2, 1);
  imageDataTemp->SetSpacing(1.,1.,1.);
  imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);

  int wasModifying = PVDiagramVolume->StartModify();
  PVDiagramVolume->SetAttribute("SlicerAstro.NAXIS1", IntToString(N1).c_str());
  PVDiagramVolume->SetAndObserveImageData(imageDataTemp.GetPointer());

  float *inFPixel = nullptr;
  float *PVDiagramFPixel = nullptr;
  double *inDPixel = nullptr;
  double *PVDiagramDPixel = nullptr;
  const int DataType = inputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      PVDiagramFPixel = static_cast<float*> (PVDiagramVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      PVDiagramDPixel = static_cast<double*> (PVDiagramVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }

  const int *dims = inputVolume->GetImageData()->GetDimensions();
  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numSlice = dims[0] * dims[1] * numComponents;

  // 2D nearest-neighbour interpolation.
  // Fiducials are restrained on the Moment Map,
  // therefore there are no indexes checks.
  vtkSMPTools::For(0, points->GetNumberOfPoints(), [&](vtkIdType firstPoint, vtkIdType lastPoint)
    {
    for (vtkIdType PointIndex = firstPoint; PointIndex < lastPoint; PointIndex++)
    {
    double posIJK[3];
    points->GetPoint(PointIndex, posIJK);

    // Bilinear interpolation weights. Neighbours are left/left+1 and
    // bottom/bottom+1 (clamped to the image), so that positions on integer
    // coordinates get weight 1 (floor/ceil gave zero weights there).
    const int iiLeft = std::clamp(static_cast<int>(floor(posIJK[0])), 0, dims[0] - 1);
    const int iiRight = std::min(iiLeft + 1, dims[0] - 1);
    const double iiInterpolatedLeft = (iiRight == iiLeft) ? 0. : posIJK[0] - iiLeft; // weight of the right pixels
    const double iiInterpolatedRight = 1. - iiInterpolatedLeft;                        // weight of the left pixels

    const int jjBottom = std::clamp(static_cast<int>(floor(posIJK[1])), 0, dims[1] - 1);
    const int jjTop = std::min(jjBottom + 1, dims[1] - 1);
    const double jjInterpolatedBottom = (jjTop == jjBottom) ? 0. : posIJK[1] - jjBottom; // weight of the top pixels
    const double jjInterpolatedTop = 1. - jjInterpolatedBottom;                            // weight of the bottom pixels

    int IndexLB = iiLeft + jjBottom * dims[0];
    int IndexLT = iiLeft + jjTop * dims[0];
    int IndexRB = iiRight + jjBottom * dims[0];
    int IndexRT = iiRight + jjTop * dims[0];

    for (int kk = 0; kk < N2; kk++)
      {
      const vtkIdType IndexZ = static_cast<vtkIdType>(kk) * numSlice;
      const vtkIdType IndexZLB = IndexLB + IndexZ;
      const vtkIdType IndexZLT = IndexLT + IndexZ;
      const vtkIdType IndexZRB = IndexRB + IndexZ;
      const vtkIdType IndexZRT = IndexRT + IndexZ;
      double ITop, IBottom;
      switch (DataType)
        {
        case VTK_FLOAT:
          ITop = *(inFPixel + IndexZLT) * (iiInterpolatedRight) + *(inFPixel + IndexZRT) * (iiInterpolatedLeft);
          IBottom = *(inFPixel + IndexZLB) * (iiInterpolatedRight) + *(inFPixel + IndexZRB) * (iiInterpolatedLeft);
          *(PVDiagramFPixel + PointIndex + static_cast<vtkIdType>(kk) * N1) = ITop * jjInterpolatedBottom + IBottom * jjInterpolatedTop;
          break;
        case VTK_DOUBLE:
          ITop = *(inDPixel + IndexZLT) * (iiInterpolatedRight) + *(inDPixel + IndexZRT) * (iiInterpolatedLeft);
          IBottom = *(inDPixel + IndexZLB) * (iiInterpolatedRight) + *(inDPixel + IndexZRB) * (iiInterpolatedLeft);
          *(PVDiagramDPixel + PointIndex + static_cast<vtkIdType>(kk) * N1) = ITop * jjInterpolatedBottom + IBottom * jjInterpolatedTop;
          break;
        }
      }
    }
    });

  inFPixel = nullptr;
  PVDiagramFPixel = nullptr;
  inDPixel = nullptr;
  PVDiagramDPixel = nullptr;

  delete inFPixel;
  delete PVDiagramFPixel;
  delete inDPixel;
  delete PVDiagramDPixel;

  PVDiagramVolume->UpdateRangeAttributes();

  int HistoryIndex = 0;
  for (int index = 1; index < 1000000; index++)
    {
    std::string keywordName = "SlicerAstro._HISTORY";
    keywordName += ZeroPadNumber(index);
    if (!PVDiagramVolume->GetAttribute(keywordName.c_str()))
      {
      HistoryIndex = index + 1;
      break;
      }
    }

  vtkMRMLAstroVolumeDisplayNode* astroInputDisplay = inputVolume->GetAstroVolumeDisplayNode();
  if (!astroInputDisplay || !astroInputDisplay->GetWCSStruct())
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::GenerateAndSetPVDiagram :"
                  " astroInputDisplay not found.");
    return false;
    }

  double CDELT1 = 0.;
  std::string unit = astroInputDisplay->GetWCSStruct()->cunit[0];
  for (int PointIndex = 0; PointIndex < points->GetNumberOfPoints(); PointIndex++)
    {
    double posIJK[3], posWCS[3];
    points->GetPoint(PointIndex, posIJK);
    astroInputDisplay->GetReferenceSpace(posIJK, posWCS);
    std::string keywordName = "SlicerAstro._HISTORY";
    keywordName += ZeroPadNumber(HistoryIndex + PointIndex);
    std::string keywordString;
    keywordString = "PVPoint " + IntToString(PointIndex) + " : " +
                    "ij = " + DoubleToString(posIJK[0]) + " , " + DoubleToString(posIJK[1]) + " ; " +
                    "WCS = " + DoubleToString(posWCS[0]) + " , " + DoubleToString(posWCS[1]) + " " + unit;
    PVDiagramVolume->SetAttribute(keywordName.c_str(), keywordString.c_str());
    if (PointIndex != 0)
      {
      double oldPosIJK[3], oldPosWCS[3];
      points->GetPoint(PointIndex - 1, oldPosIJK);
      astroInputDisplay->GetReferenceSpace(oldPosIJK, oldPosWCS);
      // angular distance on the sky (degrees): RA differences shrink with cos(Dec)
      const double deg2rad = vtkMath::Pi() / 180.;
      const double cosDec = cos(0.5 * (posWCS[1] + oldPosWCS[1]) * deg2rad);
      const double deltaRA = (posWCS[0] - oldPosWCS[0]) * cosDec;
      const double deltaDec = posWCS[1] - oldPosWCS[1];
      CDELT1 += sqrt((deltaRA * deltaRA) + (deltaDec * deltaDec));
      }
    }
  CDELT1 /= points->GetNumberOfPoints() - 1;
  PVDiagramVolume->SetAttribute("SlicerAstro.CDELT1", DoubleToString(CDELT1).c_str());
  PVDiagramVolume->EndModify(wasModifying);

  vtkMRMLAstroVolumeDisplayNode * astroPVDisplayNode = PVDiagramVolume->GetAstroVolumeDisplayNode();
  if (!astroPVDisplayNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::GenerateAndSetPVDiagram :"
                  " astroPVDisplayNode not found.");
    return false;
    }

  astroPVDisplayNode->SetFitSlices(true);

  // Setting the Layout for the Output
  qSlicerApplication* app = qSlicerApplication::application();
  if(!app)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::GenerateAndSetPVDiagram : "
                   "app not found!");
    return false;
    }

  qSlicerLayoutManager* layoutManager = app->layoutManager();

  if(!layoutManager)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::GenerateAndSetPVDiagram : "
                   "layoutManager not found!");
    return false;
    }

  int viewArra = layoutManager->layoutLogic()->GetLayoutNode()->GetViewArrangement();
  if (viewArra != vtkMRMLLayoutNode::SlicerLayoutFourUpView)
    {
    layoutManager->layoutLogic()->GetLayoutNode()->SetViewArrangement(vtkMRMLLayoutNode::SlicerLayoutFourUpView);

    vtkMRMLSliceNode *redSlice = vtkMRMLSliceNode::SafeDownCast(
      this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
    // setting to the XZ orientation is needed in order to force the refresh
    redSlice->SetOrientation("XZ");
    redSlice->SetOrientation("XY");

    // Green: 2nd moment map (XY)
    vtkMRMLNode* secondMomentMapNode =
      pnode->GetNodeReference(vtkSlicerAstroPVDiagramLogic::GetSecondMomentMapReferenceRole());
    vtkMRMLSliceCompositeNode *greenSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
      this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeGreen"));
    greenSliceComposite->SetLabelVolumeID("");
    greenSliceComposite->SetForegroundVolumeID("");
    greenSliceComposite->SetForegroundOpacity(0.);
    greenSliceComposite->SetBackgroundVolumeID(secondMomentMapNode ? secondMomentMapNode->GetID() : "");
    vtkMRMLSliceNode *greenSlice = vtkMRMLSliceNode::SafeDownCast(
      this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
    if (greenSlice)
      {
      greenSlice->SetOrientation("XY");
      }
    }

  vtkMRMLSliceCompositeNode *yellowSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
    this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow"));
  yellowSliceComposite->SetLabelVolumeID("");
  yellowSliceComposite->SetForegroundVolumeID("");
  yellowSliceComposite->SetForegroundOpacity(0.);
  yellowSliceComposite->SetBackgroundVolumeID(pnode->GetOutputVolumeNodeID());

  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!yellowSliceNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::GenerateAndSetPVDiagram :"
                  " yellowSliceNode not found.");
    return false;
    }

  yellowSliceNode->SetOrientation("PVDiagram");
  yellowSliceNode->SetSliceOffset(0.);

  vtkSlicerApplicationLogic *appLogic = app->applicationLogic();
  if (!appLogic)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::GenerateAndSetPVDiagram :"
                   " appLogic not found!");
    return false;
    }

  vtkMRMLSliceLogic* yellowSliceLogic = appLogic->GetSliceLogic(yellowSliceNode);
  if (yellowSliceLogic)
    {
    yellowSliceLogic->FitSliceToAll(dims[0], dims[1]);
    yellowSliceLogic->SnapSliceOffsetToIJK();
    }

  double FieldOfView[3];
  yellowSliceNode->GetFieldOfView(FieldOfView);
  yellowSliceNode->SetFieldOfView(N1, dims[2], FieldOfView[2]);

  return true;
}
