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
#include "vtkSlicerAstroPVSliceLogic.h"
#include "vtkSlicerAstroConfigure.h"

// MRML includes
#include <vtkMRMLAstroMomentMapsParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroPVSliceParametersNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLMarkupsDisplayNode.h>
#include <vtkMRMLCameraNode.h>
#include <vtkMRMLMarkupsLineNode.h>
#include <vtkMRMLMeasurement.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

// VTK includes
#include <vtkCacheManager.h>
#include <vtkGeneralTransform.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkVersion.h>

#include <algorithm>
#include <cmath>
#include <limits>

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

//----------------------------------------------------------------------------
class vtkSlicerAstroPVSliceLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkSlicerAstroMomentMapsLogic> AstroMomentMapsLogic;
};

//----------------------------------------------------------------------------
vtkSlicerAstroPVSliceLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = 0;
  this->AstroMomentMapsLogic = 0;
}

//---------------------------------------------------------------------------
vtkSlicerAstroPVSliceLogic::vtkInternal::~vtkInternal()
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

} // end namespace

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroPVSliceLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroPVSliceLogic::vtkSlicerAstroPVSliceLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerAstroPVSliceLogic::~vtkSlicerAstroPVSliceLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroPVSliceLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerAstroPVSliceLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroPVSliceLogic::SetAstroMomentMapsLogic(vtkSlicerAstroMomentMapsLogic *logic)
{
  this->Internal->AstroMomentMapsLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroMomentMapsLogic *vtkSlicerAstroPVSliceLogic::GetAstroMomentMapsLogic()
{
  return this->Internal->AstroMomentMapsLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroPVSliceLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerAstroPVSliceLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerAstroPVSliceLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  vtkMRMLAstroPVSliceParametersNode* pNode = vtkMRMLAstroPVSliceParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::Calculate0thMomentMap(vtkMRMLAstroPVSliceParametersNode *pnode)
{
  if (!this->GetMRMLScene() || !pnode)
    {
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::CalculateAndSet0thMomentMap :"
                  " inputVolume not found.");
    return false;
    }

  vtkMRMLAstroVolumeDisplayNode * astroMrmlDisplayNode = inputVolume->GetAstroVolumeDisplayNode();
  if (!astroMrmlDisplayNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::CalculateAndSet0thMomentMap :"
                  " astroMrmlDisplayNode not found.");
    return false;
    }

  // the moment-0 map of the cube shared by the modules ("<cube>_mom0")
  vtkMRMLAstroVolumeNode* sharedMap = vtkSlicerAstroVolumeLogic::GetMomentMap(inputVolume, 0);
  if (sharedMap)
    {
    pnode->SetMomentMapNodeID(sharedMap->GetID());
    return true;
    }
  std::string name = vtkSlicerAstroVolumeLogic::GetMomentMapName(inputVolume, 0);

  vtkMRMLAstroVolumeNode* MomentMapNode = vtkMRMLAstroVolumeNode::SafeDownCast
     (this->GetAstroVolumeLogic()->CloneVolumeWithoutImageData(this->GetMRMLScene(), inputVolume, name.c_str()));

  // Modify fits attributes
  MomentMapNode->SetAttribute("SlicerAstro.NAXIS", "2");
  MomentMapNode->GetAstroVolumeDisplayNode()->SetAttribute("SlicerAstro.NAXIS", "2");
  MomentMapNode->GetAstroVolumeDisplayNode()->CopyWCS(inputVolume->GetAstroVolumeDisplayNode());
  std::string Bunit = MomentMapNode->GetAttribute("SlicerAstro.BUNIT");
  Bunit += " km/s";
  MomentMapNode->SetAttribute("SlicerAstro.BUNIT", Bunit.c_str());
  std::string Btype = "";
  Btype = astroMrmlDisplayNode->AddVelocityInfoToDisplayStringZ(Btype);
  MomentMapNode->SetAttribute("SlicerAstro.BTYPE", Btype.c_str());
  MomentMapNode->RemoveAttribute("SlicerAstro.NAXIS3");
  MomentMapNode->RemoveAttribute("SlicerAstro.CROTA3");
  MomentMapNode->RemoveAttribute("SlicerAstro.CRPIX3");
  MomentMapNode->RemoveAttribute("SlicerAstro.CRVAL3");
  MomentMapNode->RemoveAttribute("SlicerAstro.CTYPE3");
  MomentMapNode->RemoveAttribute("SlicerAstro.CUNIT3");
  MomentMapNode->RemoveAttribute("SlicerAstro.DTYPE3");
  MomentMapNode->RemoveAttribute("SlicerAstro.DRVAL3");
  MomentMapNode->RemoveAttribute("SlicerAstro.DUNIT3");
  MomentMapNode->SetAttribute("SlicerAstro.DATAMODEL", "ZEROMOMENTMAP");

  // Remove old rendering Display
  int ndnodes = MomentMapNode->GetNumberOfDisplayNodes();
  for (int ii = 0; ii < ndnodes; ii++)
    {
    vtkMRMLVolumeRenderingDisplayNode *dnode =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(
        MomentMapNode->GetNthDisplayNode(ii));
    if (dnode)
      {
      MomentMapNode->RemoveNthDisplayNodeID(ii);
      }
    }

  // Copy a 2D image into the Astro Volume object
  int N1 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1"));
  int N2 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2"));
  vtkNew<vtkImageData> imageDataTemp;
  imageDataTemp->SetDimensions(N1, N2, 1);
  imageDataTemp->SetSpacing(1.,1.,1.);
  imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);

  MomentMapNode->SetAndObserveImageData(imageDataTemp.GetPointer());

  // Set the Origin
  double Origin[3];
  // (the origin of the cube, as the maps of the Moment maps module)
  inputVolume->GetOrigin(Origin);
  MomentMapNode->SetOrigin(Origin);

  vtkNew<vtkMRMLAstroMomentMapsParametersNode> momentMapParametersNode;
  momentMapParametersNode->SetInputVolumeNodeID(inputVolume->GetID());
  momentMapParametersNode->SetZeroMomentVolumeNodeID(MomentMapNode->GetID());
  momentMapParametersNode->SetMaskActive(false);
  momentMapParametersNode->SetGenerateZero(true);
  momentMapParametersNode->SetGenerateFirst(false);
  momentMapParametersNode->SetGenerateSecond(false);
  momentMapParametersNode->SetIntensityMin(StringToDouble
    (inputVolume->GetAttribute("SlicerAstro.DisplayThreshold")) * 2);
  momentMapParametersNode->SetIntensityMax(StringToDouble
    (inputVolume->GetAttribute("SlicerAstro.DATAMAX")));

  double ijk[3], worldOne[3], worldTwo[3];
  ijk[0] = N1 * 0.5;
  ijk[1] = N2 * 0.5;
  ijk[2] = 0.;
  astroMrmlDisplayNode->GetReferenceSpace(ijk, worldOne);
  struct wcsprm* WCS = astroMrmlDisplayNode->GetWCSStruct();
  if (!WCS)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::CalculateAndSet0thMomentMap :"
                  " WCS not found.");
    return false;
    }
  if(!strcmp(WCS->cunit[2], "m/s"))
    {
    worldOne[2] /= 1000.;
    }
  ijk[2] = StringToDouble(inputVolume->GetAttribute("SlicerAstro.NAXIS3"));
  if (ijk[2] < 2)
    {
    ijk[2] += 1;
    }
  astroMrmlDisplayNode->GetReferenceSpace(ijk, worldTwo);
  if(!strcmp(WCS->cunit[2], "m/s"))
    {
    worldTwo[2] /= 1000.;
    }

  double Vmin, Vmax;

  if (worldOne[2] < worldTwo[2])
    {
    Vmin = worldOne[2];
    Vmax = worldTwo[2];
    }
  else
    {
    Vmin = worldTwo[2];
    Vmax = worldOne[2];
    }
  momentMapParametersNode->SetVelocityMin(Vmin);
  momentMapParametersNode->SetVelocityMax(Vmax);

  momentMapParametersNode->SetStatus(0);
  momentMapParametersNode->SetCores(0);

  if (!this->GetAstroMomentMapsLogic()->CalculateMomentMaps(momentMapParametersNode.GetPointer()))
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::CalculateAndSet0thMomentMap :"
                  " MomentMaps calculations failed.");
    this->GetMRMLScene()->RemoveNode(MomentMapNode);
    return false;
    }

  vtkSlicerAstroVolumeLogic::SetMomentMap(inputVolume, 0, MomentMapNode);
  pnode->SetMomentMapNodeID(MomentMapNode->GetID());

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::SetMomentMapOnRedWidget(vtkMRMLAstroPVSliceParametersNode *pnode)
{
  if (!pnode || !this->GetMRMLScene())
    {
    return false;
    }

  // Setting the Layout for the Output
  qSlicerApplication* app = qSlicerApplication::application();
  if(!app)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::Set0thMomentMap : "
                   "app not found!");
    return false;
    }

  qSlicerLayoutManager* layoutManager = app->layoutManager();

  if(!layoutManager)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::Set0thMomentMap : "
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

  vtkMRMLSliceCompositeNode *greenSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
    this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeGreen"));
  greenSliceComposite->SetLabelVolumeID("");
  greenSliceComposite->SetForegroundVolumeID("");
  greenSliceComposite->SetForegroundOpacity(0.);
  greenSliceComposite->SetBackgroundVolumeID(pnode->GetInputVolumeNodeID());

  vtkSlicerApplicationLogic *appLogic = app->applicationLogic();
  if (!appLogic)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::Set0thMomentMap : "
                   "appLogic not found!");
    return false;
    }

  vtkMRMLSliceLogic* redSliceLogic = appLogic->GetSliceLogic(redSlice);
  if (!redSliceLogic)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::Set0thMomentMap : "
                   "redSliceLogic not found!");
    return false;
    }

  int *dims = redSlice->GetDimensions();
  if (dims)
    {
    redSliceLogic->FitSliceToAll(dims[0], dims[1]);
    }
  redSliceLogic->SnapSliceOffsetToIJK();

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::CreateAndSetLine(vtkMRMLAstroPVSliceParametersNode *pnode)
{
  if (!this->GetMRMLScene() || !pnode)
    {
    return false;
    }

  vtkMRMLAstroVolumeNode *PVMomentMap =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetMomentMapNodeID()));
  if(!PVMomentMap || !PVMomentMap->GetImageData())
    {
    return false;
    }

  // Compute the control points first, then add the line to the scene
  vtkNew<vtkMRMLMarkupsLineNode> LineNode;
  std::string name = this->GetMRMLScene()->GetUniqueNameByString("PVSliceLine");
  LineNode->SetName(name.c_str());
  // the length (mm) needs the scene for its units: not computed for the
  // points added before the line is in the scene (the AstroVolume logic sets
  // the measurements of the lines when they are added)
  if (vtkMRMLMeasurement* length = LineNode->GetMeasurement("length"))
    {
    length->SetEnabled(false);
    }

  // Set control points
  vtkImageData* image = PVMomentMap->GetImageData();
  double dimensions[2] = {0.};
  int* dims = image ? image->GetDimensions() : 0;
  if (dims)
    {
    dimensions[0] = dims[0];
    dimensions[1] = dims[1];
    }

  double* RASOrigin = PVMomentMap->GetOrigin();
  vtkNew<vtkGeneralTransform> RAStoIJKTransform;
  RAStoIJKTransform->Identity();
  RAStoIJKTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
  PVMomentMap->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());
  double IJKOrigin[3];
  RAStoIJKTransform->TransformPoint(RASOrigin, IJKOrigin);
  double position[3] = {0.};
  position[0] = dimensions[0] * 0.1;
  position[1] = dimensions[1] * 0.1;
  position[2] = IJKOrigin[2];

  vtkNew<vtkGeneralTransform> IJKtoRASTransform;
  IJKtoRASTransform->Identity();
  IJKtoRASTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> IJKtoRASMatrix;
  PVMomentMap->GetRASToIJKMatrix(IJKtoRASMatrix.GetPointer());
  IJKtoRASMatrix->Invert();
  IJKtoRASTransform->Concatenate(IJKtoRASMatrix.GetPointer());
  double RASposition[3] = {0.};
  IJKtoRASTransform->TransformPoint(position, RASposition);
  vtkVector3d point1(RASposition);
  LineNode->AddControlPointWorld(point1);

  position[0] = dimensions[0] - (dimensions[0] * 0.1);
  position[1] = dimensions[1] - (dimensions[1] * 0.1);
  position[2] = IJKOrigin[2];
  IJKtoRASTransform->TransformPoint(position, RASposition);
  vtkVector3d point2(RASposition);
  LineNode->AddControlPointWorld(point2);

  // Add to the scene
  this->GetMRMLScene()->AddNode(LineNode);

  pnode->SetLineNodeID(LineNode->GetID());

  // Set line dimension
  vtkMRMLMarkupsDisplayNode *displayNode =
    vtkMRMLMarkupsDisplayNode::SafeDownCast(LineNode->GetDisplayNode());
  if (!displayNode)
  {
    LineNode->CreateDefaultDisplayNodes();
    displayNode = vtkMRMLMarkupsDisplayNode::SafeDownCast(LineNode->GetDisplayNode());
  }

  displayNode->SetGlyphScale(2.5);

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::InitializeLine(vtkMRMLAstroPVSliceParametersNode *pnode)
{
  if (!this->GetMRMLScene() || !pnode)
    {
    return false;
    }

  vtkMRMLAstroVolumeNode *PVMomentMap =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetMomentMapNodeID()));
  if(!PVMomentMap || !PVMomentMap->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::InitializeLine :"
                  " PVMomentMap not found.");
    return false;
    }

  vtkMRMLMarkupsLineNode *LineNode =
    vtkMRMLMarkupsLineNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetLineNodeID()));
  if(!LineNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::InitializeLine :"
                  " LineNode not found.");
    return false;
    }

  // Set control points
  vtkImageData* image = PVMomentMap->GetImageData();
  double dimensions[2] = {0.};
  int* dims = image ? image->GetDimensions() : 0;
  if (dims)
    {
    dimensions[0] = dims[0];
    dimensions[1] = dims[1];
    }

  double* RASOrigin = PVMomentMap->GetOrigin();
  vtkNew<vtkGeneralTransform> RAStoIJKTransform;
  RAStoIJKTransform->Identity();
  RAStoIJKTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
  PVMomentMap->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());
  double IJKOrigin[3];
  RAStoIJKTransform->TransformPoint(RASOrigin, IJKOrigin);
  double position[3] = {0.};
  position[0] = dimensions[0] * 0.1;
  position[1] = dimensions[1] * 0.1;
  position[2] = IJKOrigin[2];

  vtkNew<vtkGeneralTransform> IJKtoRASTransform;
  IJKtoRASTransform->Identity();
  IJKtoRASTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> IJKtoRASMatrix;
  PVMomentMap->GetRASToIJKMatrix(IJKtoRASMatrix.GetPointer());
  IJKtoRASMatrix->Invert();
  IJKtoRASTransform->Concatenate(IJKtoRASMatrix.GetPointer());
  double RASposition[3] = {0.};
  IJKtoRASTransform->TransformPoint(position, RASposition);
  LineNode->SetLineStartPosition(RASposition);

  position[0] = dimensions[0] - (dimensions[0] * 0.1);
  position[1] = dimensions[1] - (dimensions[1] * 0.1);
  position[2] = IJKOrigin[2];
  IJKtoRASTransform->TransformPoint(position, RASposition);
  LineNode->SetLineEndPosition(RASposition);

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::UpdateLine(vtkMRMLAstroPVSliceParametersNode *pnode)
{
  if (!this->GetMRMLScene() || !pnode)
    {
    return false;
    }

  vtkMRMLAstroVolumeNode *PVMomentMap =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetMomentMapNodeID()));
  if(!PVMomentMap || !PVMomentMap->GetImageData())
    {
    return false;
    }

  vtkMRMLMarkupsLineNode *LineNode =
    vtkMRMLMarkupsLineNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetLineNodeID()));
  if(!LineNode)
    {
    return false;
    }

  if (fabs(pnode->GetLineAngle() - pnode->GetLineOldAngle()) < 0.01)
    {
    return false;
    }

  double* RASOrigin = PVMomentMap->GetOrigin();
  double position1[3] = {0};
  LineNode->GetNthControlPointPosition(0, position1);
  double position2[3] = {0};
  LineNode->GetNthControlPointPosition(1, position2);

  double LineCenter[3], Shift[3];
  for (int ii = 0; ii < 3; ii++)
    {
    LineCenter[ii] = (position1[ii] + position2[ii]) * 0.5;
    Shift[ii] = LineCenter[ii] - RASOrigin[ii];
    position1[ii] -= Shift[ii];
    position2[ii] -= Shift[ii];
    }

  vtkNew<vtkGeneralTransform> RAStoIJKTransform;
  RAStoIJKTransform->Identity();
  RAStoIJKTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
  PVMomentMap->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());

  RAStoIJKTransform->TransformPoint(position1,position1);
  RAStoIJKTransform->TransformPoint(position2,position2);
  RAStoIJKTransform->TransformPoint(LineCenter,LineCenter);

  vtkNew<vtkGeneralTransform> LineRotateTransform;
  LineRotateTransform->Identity();
  LineRotateTransform->PostMultiply();
  LineRotateTransform->RotateZ(pnode->GetLineOldAngle() -
                                pnode->GetLineAngle());

  LineRotateTransform->TransformPoint(position1, position1);
  LineRotateTransform->TransformPoint(position2, position2);

  RAStoIJKTransform->Identity();
  RAStoIJKMatrix->Invert();
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());

  RAStoIJKTransform->TransformPoint(position1,position1);
  RAStoIJKTransform->TransformPoint(position2,position2);

  for (int ii = 0; ii < 3; ii++)
    {
    position1[ii] += Shift[ii];
    position2[ii] += Shift[ii];
    }

  int wasModifying = LineNode->StartModify();
  LineNode->SetLineStartPosition(position1);
  LineNode->SetLineEndPosition(position2);
  LineNode->EndModify(wasModifying);

  pnode->DisableModifiedEventOn();
  pnode->SetLineOldAngle(pnode->GetLineAngle());
  pnode->DisableModifiedEventOff();

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::UpdateLineFromCenter(vtkMRMLAstroPVSliceParametersNode *pnode)
{
  if (!this->GetMRMLScene() || !pnode)
    {
    return false;
    }

  int NewIJKLineCenter[2];
  pnode->GetLineCenter(NewIJKLineCenter);

  vtkMRMLAstroVolumeNode *PVMomentMap =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetMomentMapNodeID()));
  if(!PVMomentMap || !PVMomentMap->GetImageData())
    {
    return false;
    }

  vtkMRMLMarkupsLineNode *LineNode =
    vtkMRMLMarkupsLineNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetLineNodeID()));
  if(!LineNode)
    {
    return false;
    }

  double position1[3] = {0};
  LineNode->GetNthControlPointPosition(0, position1);
  double position2[3] = {0};
  LineNode->GetNthControlPointPosition(1, position2);

  double LineCenter[3];
  for (int ii = 0; ii < 3; ii++)
    {
    LineCenter[ii] = (position1[ii] + position2[ii]) * 0.5;
    }

  vtkNew<vtkGeneralTransform> RAStoIJKTransform;
  RAStoIJKTransform->Identity();
  RAStoIJKTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
  PVMomentMap->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());

  RAStoIJKTransform->TransformPoint(position1,position1);
  RAStoIJKTransform->TransformPoint(position2,position2);
  RAStoIJKTransform->TransformPoint(LineCenter,LineCenter);

  vtkNew<vtkGeneralTransform> LineShiftTransform;
  LineShiftTransform->Identity();
  LineShiftTransform->PostMultiply();
  LineShiftTransform->Translate(NewIJKLineCenter[0] - LineCenter[0],
                                 NewIJKLineCenter[1] - LineCenter[1],
                                 0.);

  LineShiftTransform->TransformPoint(position1, position1);
  LineShiftTransform->TransformPoint(position2, position2);

  RAStoIJKTransform->Identity();
  RAStoIJKMatrix->Invert();
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());

  RAStoIJKTransform->TransformPoint(position1,position1);
  RAStoIJKTransform->TransformPoint(position2,position2);

  int wasModifying = LineNode->StartModify();
  LineNode->SetLineStartPosition(position1);
  LineNode->SetLineEndPosition(position2);
  LineNode->EndModify(wasModifying);

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::InitializePV(vtkMRMLAstroPVSliceParametersNode *pnode)
{
  if (!pnode || !this->GetMRMLScene())
    {
    return false;
    }

  // Calculate PVPhiMajor and Minor
  vtkMRMLMarkupsLineNode *LineNode =
    vtkMRMLMarkupsLineNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetLineNodeID()));
  if(!LineNode)
    {
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    return false;
    }

  double position1[3] = {0};
  LineNode->GetNthControlPointPosition(0, position1);
  double position2[3] = {0};
  LineNode->GetNthControlPointPosition(1, position2);

  // Get PV center
  int *dims = inputVolume->GetImageData()->GetDimensions();
  int Zcenter = dims[2] * 0.5;

  vtkNew<vtkGeneralTransform> RAStoIJKTransform;
  RAStoIJKTransform->Identity();
  RAStoIJKTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
  inputVolume->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());

  RAStoIJKTransform->TransformPoint(position1,position1);
  RAStoIJKTransform->TransformPoint(position2,position2);

  vtkNew<vtkGeneralTransform> IJKtoRASTransform;
  IJKtoRASTransform->Identity();
  IJKtoRASTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> IJKtoRASMatrix;
  inputVolume->GetIJKToRASMatrix(IJKtoRASMatrix.GetPointer());
  IJKtoRASTransform->Concatenate(IJKtoRASMatrix.GetPointer());
  double ijk[3] = {0.,0.,0.}, RAS[3] = {0.,0.,0.};
  ijk[0] = (position1[0] + position2[0]) * 0.5;
  ijk[1] = (position1[1] + position2[1]) * 0.5;
  ijk[2] = Zcenter;
  IJKtoRASTransform->TransformPoint(ijk, RAS);

  // Get PV inclination
  double distX = (position1[0] - position2[0]);
  double distY = (position1[1] - position2[1]);

  double PVPhiMajor = atan(distY / distX);
  if (PVPhiMajor < 0)
    {
    PVPhiMajor += 2 * PI;
    }
  else if (PVPhiMajor > 2 * PI)
    {
    PVPhiMajor -= 2 * PI;
    }
  PVPhiMajor *=  180. / PI;
  PVPhiMajor *= -1.;

  double PVPhiMinor = PVPhiMajor + 90.;

  // Create PV major
  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!yellowSliceNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::InitializePV :"
                  " yellowSliceNode not found.");
    return false;
    }

  yellowSliceNode->SetOrientation("XZ");
  vtkNew<vtkMatrix3x3> PVMajorMatrix;
  vtkNew<vtkTransform> transformMajor;
  transformMajor->SetMatrix(yellowSliceNode->GetSliceToRAS());
  transformMajor->RotateY(PVPhiMajor);

  PVMajorMatrix->SetElement(0, 0, transformMajor->GetMatrix()->GetElement(0,0));
  PVMajorMatrix->SetElement(0, 1, transformMajor->GetMatrix()->GetElement(0,1));
  PVMajorMatrix->SetElement(0, 2, transformMajor->GetMatrix()->GetElement(0,2));
  PVMajorMatrix->SetElement(1, 0, transformMajor->GetMatrix()->GetElement(1,0));
  PVMajorMatrix->SetElement(1, 1, transformMajor->GetMatrix()->GetElement(1,1));
  PVMajorMatrix->SetElement(1, 2, transformMajor->GetMatrix()->GetElement(1,2));
  PVMajorMatrix->SetElement(2, 0, transformMajor->GetMatrix()->GetElement(2,0));
  PVMajorMatrix->SetElement(2, 1, transformMajor->GetMatrix()->GetElement(2,1));
  PVMajorMatrix->SetElement(2, 2, transformMajor->GetMatrix()->GetElement(2,2));

  if (yellowSliceNode->HasSliceOrientationPreset("PVMajor"))
    {
    yellowSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix.GetPointer());
    }
  else
    {
    yellowSliceNode->AddSliceOrientationPreset("PVMajor", PVMajorMatrix.GetPointer());
    }

  yellowSliceNode->SetOrientation("PVMajor");

  // Translate to X and Y center
  yellowSliceNode->GetSliceToRAS()->SetElement(0, 3, RAS[0]);
  yellowSliceNode->GetSliceToRAS()->SetElement(1, 3, RAS[1]);
  yellowSliceNode->GetSliceToRAS()->SetElement(2, 3, RAS[2]);
  yellowSliceNode->UpdateMatrices();
  yellowSliceNode->SetSliceVisible(1);
  yellowSliceNode->SetRulerType(vtkMRMLSliceNode::RulerTypeThin);

  // Create PV minor
  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (!greenSliceNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::InitializePV :"
                  " greenSliceNode not found.");
    return false;
    }

  greenSliceNode->SetOrientation("XZ");
  vtkNew<vtkMatrix3x3> PVMinorMatrix;
  vtkNew<vtkTransform> transformMinor;
  transformMinor->SetMatrix(greenSliceNode->GetSliceToRAS());
  transformMinor->RotateY(PVPhiMinor);
  PVMinorMatrix->SetElement(0, 0, transformMinor->GetMatrix()->GetElement(0,0));
  PVMinorMatrix->SetElement(0, 1, transformMinor->GetMatrix()->GetElement(0,1));
  PVMinorMatrix->SetElement(0, 2, transformMinor->GetMatrix()->GetElement(0,2));
  PVMinorMatrix->SetElement(1, 0, transformMinor->GetMatrix()->GetElement(1,0));
  PVMinorMatrix->SetElement(1, 1, transformMinor->GetMatrix()->GetElement(1,1));
  PVMinorMatrix->SetElement(1, 2, transformMinor->GetMatrix()->GetElement(1,2));
  PVMinorMatrix->SetElement(2, 0, transformMinor->GetMatrix()->GetElement(2,0));
  PVMinorMatrix->SetElement(2, 1, transformMinor->GetMatrix()->GetElement(2,1));
  PVMinorMatrix->SetElement(2, 2, transformMinor->GetMatrix()->GetElement(2,2));

  if (greenSliceNode->HasSliceOrientationPreset("PVMinor"))
    {
    greenSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix.GetPointer());
    }
  else
    {
    greenSliceNode->AddSliceOrientationPreset("PVMinor", PVMinorMatrix.GetPointer());
    }
  greenSliceNode->SetOrientation("PVMinor");

  // Translate to X and Y center
  greenSliceNode->GetSliceToRAS()->SetElement(0, 3, RAS[0]);
  greenSliceNode->GetSliceToRAS()->SetElement(1, 3, RAS[1]);
  greenSliceNode->GetSliceToRAS()->SetElement(2, 3, RAS[2]);
  greenSliceNode->UpdateMatrices();
  greenSliceNode->SetRulerType(vtkMRMLSliceNode::RulerTypeThin);

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::UpdatePV(vtkMRMLAstroPVSliceParametersNode *pnode)
{
  if (!pnode || !this->GetMRMLScene())
    {
    return false;
    }

  // Calculate PVPhiMajor and Minor
  vtkMRMLMarkupsLineNode *LineNode =
    vtkMRMLMarkupsLineNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetLineNodeID()));
  if(!LineNode)
    {
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    return false;
    }

  double position1[3] = {0};
  LineNode->GetNthControlPointPosition(0, position1);
  double position2[3] = {0};
  LineNode->GetNthControlPointPosition(1, position2);

  // Get PV center
  int dims[3];
  inputVolume->GetImageData()->GetDimensions(dims);
  int Zcenter = dims[2] * 0.5;

  vtkNew<vtkGeneralTransform> RAStoIJKTransform;
  RAStoIJKTransform->Identity();
  RAStoIJKTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
  inputVolume->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());

  RAStoIJKTransform->TransformPoint(position1,position1);
  RAStoIJKTransform->TransformPoint(position2,position2);

  vtkNew<vtkGeneralTransform> IJKtoRASTransform;
  IJKtoRASTransform->Identity();
  IJKtoRASTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> IJKtoRASMatrix;
  inputVolume->GetIJKToRASMatrix(IJKtoRASMatrix.GetPointer());
  IJKtoRASTransform->Concatenate(IJKtoRASMatrix.GetPointer());
  double ijk[3] = {0.,0.,0.}, RAS[3] = {0.,0.,0.};
  ijk[0] = (position1[0] + position2[0]) * 0.5;
  ijk[1] = (position1[1] + position2[1]) * 0.5;
  ijk[2] = Zcenter;
  IJKtoRASTransform->TransformPoint(ijk, RAS);

  // Get PV inclination
  double LineLength = sqrt(((position1[0] - position2[0]) * (position1[0] - position2[0])) +
                            ((position1[1] - position2[1]) * (position1[1] - position2[1])));
  double distX = (position2[0] - position1[0]);
  double distY = (position2[1] - position1[1]);

  double PVPhiMajor = -atan(distY / distX);
  PVPhiMajor *=  180. / PI;
  if ((position1[0] - position2[0]) < 0.01)
    {
    PVPhiMajor += 180.;
    }

  double PVPhiMinor = PVPhiMajor + 90.;

  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!yellowSliceNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::UpdatePV :"
                  " yellowSliceNode not found.");
    return false;
    }

  vtkMatrix4x4* yellowSliceToRAS = yellowSliceNode->GetSliceToRAS();
  if (!yellowSliceToRAS)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::UpdatePV :"
                  " yellowSliceToRAS not found.");
    return false;
    }

  // XZ orientation (SlicerAstro name of the Axial preset), computed without
  // modifying the scene default slice node
  vtkNew<vtkMatrix3x3> xzOrientation;
  vtkMRMLSliceNode::GetAxialSliceToRASMatrix(xzOrientation);
  vtkNew<vtkMatrix4x4> xzSliceToRAS;
  for (int row = 0; row < 3; ++row)
    {
    for (int column = 0; column < 3; ++column)
      {
      xzSliceToRAS->SetElement(row, column, xzOrientation->GetElement(row, column));
      }
    }

  vtkNew<vtkTransform> yellowTransform;
  yellowTransform->SetMatrix(xzSliceToRAS);
  yellowTransform->RotateY(PVPhiMajor);
  yellowSliceToRAS->DeepCopy(yellowTransform->GetMatrix());
  yellowSliceToRAS->SetElement(0, 3, RAS[0]);
  yellowSliceToRAS->SetElement(1, 3, RAS[1]);
  yellowSliceToRAS->SetElement(2, 3, RAS[2]);

  vtkNew<vtkMatrix3x3> PVMajorMatrix;
  PVMajorMatrix->SetElement(0, 0, yellowTransform->GetMatrix()->GetElement(0,0));
  PVMajorMatrix->SetElement(0, 1, yellowTransform->GetMatrix()->GetElement(0,1));
  PVMajorMatrix->SetElement(0, 2, yellowTransform->GetMatrix()->GetElement(0,2));
  PVMajorMatrix->SetElement(1, 0, yellowTransform->GetMatrix()->GetElement(1,0));
  PVMajorMatrix->SetElement(1, 1, yellowTransform->GetMatrix()->GetElement(1,1));
  PVMajorMatrix->SetElement(1, 2, yellowTransform->GetMatrix()->GetElement(1,2));
  PVMajorMatrix->SetElement(2, 0, yellowTransform->GetMatrix()->GetElement(2,0));
  PVMajorMatrix->SetElement(2, 1, yellowTransform->GetMatrix()->GetElement(2,1));
  PVMajorMatrix->SetElement(2, 2, yellowTransform->GetMatrix()->GetElement(2,2));

  if (yellowSliceNode->HasSliceOrientationPreset("PVMajor"))
    {
    yellowSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix.GetPointer());
    }

  yellowSliceNode->UpdateMatrices();
  double FieldOfView[3];
  yellowSliceNode->GetFieldOfView(FieldOfView);
  FieldOfView[0] = LineLength;
  yellowSliceNode->SetFieldOfView(FieldOfView[0] + (FieldOfView[0] * 0.2),
                                  dims[2] + (dims[2] * 0.2),
                                  FieldOfView[2]);

  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (!greenSliceNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::UpdatePV :"
                  " greenSliceNode not found.");
    return false;
    }

  vtkMatrix4x4* greenSliceToRAS = greenSliceNode->GetSliceToRAS();
  if (!greenSliceToRAS)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::UpdatePV :"
                  " greenSliceToRAS not found.");
    return false;
    }

  vtkNew<vtkTransform> greenTransform;
  greenTransform->SetMatrix(xzSliceToRAS);
  greenTransform->RotateY(PVPhiMinor);
  greenSliceToRAS->DeepCopy(greenTransform->GetMatrix());
  greenSliceToRAS->SetElement(0, 3, RAS[0]);
  greenSliceToRAS->SetElement(1, 3, RAS[1]);
  greenSliceToRAS->SetElement(2, 3, RAS[2]);

  vtkNew<vtkMatrix3x3> PVMinorMatrix;
  PVMinorMatrix->SetElement(0, 0, greenTransform->GetMatrix()->GetElement(0,0));
  PVMinorMatrix->SetElement(0, 1, greenTransform->GetMatrix()->GetElement(0,1));
  PVMinorMatrix->SetElement(0, 2, greenTransform->GetMatrix()->GetElement(0,2));
  PVMinorMatrix->SetElement(1, 0, greenTransform->GetMatrix()->GetElement(1,0));
  PVMinorMatrix->SetElement(1, 1, greenTransform->GetMatrix()->GetElement(1,1));
  PVMinorMatrix->SetElement(1, 2, greenTransform->GetMatrix()->GetElement(1,2));
  PVMinorMatrix->SetElement(2, 0, greenTransform->GetMatrix()->GetElement(2,0));
  PVMinorMatrix->SetElement(2, 1, greenTransform->GetMatrix()->GetElement(2,1));
  PVMinorMatrix->SetElement(2, 2, greenTransform->GetMatrix()->GetElement(2,2));

  if (greenSliceNode->HasSliceOrientationPreset("PVMinor"))
    {
    greenSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix.GetPointer());
    }

  greenSliceNode->UpdateMatrices();
  greenSliceNode->GetFieldOfView(FieldOfView);
  FieldOfView[0] = LineLength;
  greenSliceNode->SetFieldOfView(FieldOfView[0] + (FieldOfView[0] * 0.2),
                                 dims[2] + (dims[2] * 0.2),
                                 FieldOfView[2]);

  return true;
}

//----------------------------------------------------------------------------
namespace
{
const double Degree = vtkMath::Pi() / 180.;

/// Moment map and line of the parameters node
bool PVNodes(vtkMRMLScene* scene, vtkMRMLAstroPVSliceParametersNode* pnode, vtkMRMLAstroVolumeNode*& map,
             vtkMRMLMarkupsLineNode*& line)
{
  map = (scene && pnode && pnode->GetMomentMapNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetMomentMapNodeID())) : nullptr;
  line = (scene && pnode && pnode->GetLineNodeID()) ?
    vtkMRMLMarkupsLineNode::SafeDownCast(scene->GetNodeByID(pnode->GetLineNodeID())) : nullptr;
  return map && map->GetImageData() && map->GetAstroVolumeDisplayNode() && line &&
    line->GetNumberOfDefinedControlPoints() >= 2;
}

/// Ends of the line in the IJK coordinates of the moment map
void LineEndsIJK(vtkMRMLAstroVolumeNode* map, vtkMRMLMarkupsLineNode* line, double ijk1[3], double ijk2[3])
{
  vtkNew<vtkMatrix4x4> rasToIJK;
  map->GetRASToIJKMatrix(rasToIJK);
  double ras[4] = {0., 0., 0., 1.};
  double ijk[4] = {0., 0., 0., 1.};
  line->GetLineStartPosition(ras);
  rasToIJK->MultiplyPoint(ras, ijk);
  std::copy(ijk, ijk + 3, ijk1);
  line->GetLineEndPosition(ras);
  ras[3] = 1.;
  rasToIJK->MultiplyPoint(ras, ijk);
  std::copy(ijk, ijk + 3, ijk2);
}

/// The ends of the line at IJK positions of the moment map (on its plane)
void SetLineEndsIJK(vtkMRMLAstroVolumeNode* map, vtkMRMLMarkupsLineNode* line, const double ijk1[3],
                    const double ijk2[3])
{
  vtkNew<vtkMatrix4x4> ijkToRAS;
  map->GetIJKToRASMatrix(ijkToRAS);
  double ijk[4] = {ijk1[0], ijk1[1], 0., 1.};
  double ras1[4] = {0., 0., 0., 1.};
  double ras2[4] = {0., 0., 0., 1.};
  ijkToRAS->MultiplyPoint(ijk, ras1);
  ijk[0] = ijk2[0];
  ijk[1] = ijk2[1];
  ijkToRAS->MultiplyPoint(ijk, ras2);
  const int wasModifying = line->StartModify();
  line->SetLineStartPosition(ras1);
  line->SetLineEndPosition(ras2);
  line->EndModify(wasModifying);
}

/// Position angle (degrees from North to East, 0-180) and separation
/// (degrees) of two sky positions (degrees)
void SkyGeometry(const double world1[3], const double world2[3], double* positionAngle, double* separation)
{
  const double a1 = world1[0] * Degree;
  const double d1 = world1[1] * Degree;
  const double a2 = world2[0] * Degree;
  const double d2 = world2[1] * Degree;
  const double cosine = std::sin(d1) * std::sin(d2) + std::cos(d1) * std::cos(d2) * std::cos(a2 - a1);
  *separation = std::acos(std::max(-1., std::min(1., cosine))) / Degree;
  // from the first end to the second: East is increasing right ascension
  double angle = std::atan2(std::sin(a2 - a1) * std::cos(d2),
                            std::cos(d1) * std::sin(d2) - std::sin(d1) * std::cos(d2) * std::cos(a2 - a1)) / Degree;
  angle = std::fmod(angle + 360., 180.);
  *positionAngle = angle;
}

/// Bearing (position angle, degrees from North to East, 0-360) of world2
/// seen from world1 (degrees)
double Bearing(const double world1[3], const double world2[3])
{
  const double a1 = world1[0] * Degree;
  const double d1 = world1[1] * Degree;
  const double a2 = world2[0] * Degree;
  const double d2 = world2[1] * Degree;
  const double angle = std::atan2(std::sin(a2 - a1) * std::cos(d2),
                                  std::cos(d1) * std::sin(d2) - std::sin(d1) * std::cos(d2) * std::cos(a2 - a1));
  return std::fmod(angle / Degree + 360., 360.);
}

/// Sky position (degrees) at an angular distance (degrees) from a centre
/// along a great circle with the bearing (degrees from North to East)
void Destination(const double center[2], double bearing, double distance, double world[3])
{
  const double a0 = center[0] * Degree;
  const double d0 = center[1] * Degree;
  const double b = bearing * Degree;
  const double r = distance * Degree;
  const double sinD = std::sin(d0) * std::cos(r) + std::cos(d0) * std::sin(r) * std::cos(b);
  const double d = std::asin(std::max(-1., std::min(1., sinD)));
  const double a = a0 + std::atan2(std::sin(b) * std::sin(r) * std::cos(d0), std::cos(r) - std::sin(d0) * sinD);
  world[0] = std::fmod(a / Degree + 360., 360.);
  world[1] = d / Degree;
  world[2] = 0.;
}
} // end of anonymous namespace

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::GetLineSky(vtkMRMLAstroPVSliceParametersNode* pnode, double center[2],
                                            double* positionAngle, double* length)
{
  vtkMRMLAstroVolumeNode* map = nullptr;
  vtkMRMLMarkupsLineNode* line = nullptr;
  if (!PVNodes(this->GetMRMLScene(), pnode, map, line))
    {
    return false;
    }
  double ijk1[3] = {0., 0., 0.};
  double ijk2[3] = {0., 0., 0.};
  LineEndsIJK(map, line, ijk1, ijk2);
  const double ijkCenter[3] = {0.5 * (ijk1[0] + ijk2[0]), 0.5 * (ijk1[1] + ijk2[1]), 0.};
  ijk1[2] = ijk2[2] = 0.;
  double world1[3] = {0., 0., 0.};
  double world2[3] = {0., 0., 0.};
  double worldCenter[3] = {0., 0., 0.};
  vtkMRMLAstroVolumeDisplayNode* display = map->GetAstroVolumeDisplayNode();
  if (!display->GetReferenceSpace(ijk1, world1) || !display->GetReferenceSpace(ijk2, world2) ||
      !display->GetReferenceSpace(ijkCenter, worldCenter))
    {
    return false;
    }
  double separation = 0.;
  double angle = 0.;
  SkyGeometry(world1, world2, &angle, &separation);
  // the position angle at the centre of the line (the meridians converge:
  // the bearing changes along the line)
  angle = std::fmod(Bearing(worldCenter, world2), 180.);
  center[0] = worldCenter[0];
  center[1] = worldCenter[1];
  if (positionAngle)
    {
    *positionAngle = angle;
    }
  if (length)
    {
    *length = separation * 60.;
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::SetLineSky(vtkMRMLAstroPVSliceParametersNode* pnode, const double center[2],
                                            double positionAngle, double length)
{
  vtkMRMLAstroVolumeNode* map = nullptr;
  vtkMRMLMarkupsLineNode* line = nullptr;
  if (!PVNodes(this->GetMRMLScene(), pnode, map, line) || !(length > 0.) || !std::isfinite(center[0]) ||
      !std::isfinite(center[1]) || !std::isfinite(positionAngle))
    {
    return false;
    }
  // the ends at half the length from the centre, on the great circle with
  // the position angle at the centre
  const double half = 0.5 * length / 60.;
  double world1[3] = {0., 0., 0.};
  double world2[3] = {0., 0., 0.};
  Destination(center, positionAngle + 180., half, world1);
  Destination(center, positionAngle, half, world2);
  double ijk1[3] = {0., 0., 0.};
  double ijk2[3] = {0., 0., 0.};
  vtkMRMLAstroVolumeDisplayNode* display = map->GetAstroVolumeDisplayNode();
  if (!display->GetIJKSpace(world1, ijk1) || !display->GetIJKSpace(world2, ijk2))
    {
    return false;
    }
  SetLineEndsIJK(map, line, ijk1, ijk2);
  const int wasModifying = pnode->StartModify();
  pnode->SetLineCenter(static_cast<int>(std::lround(0.5 * (ijk1[0] + ijk2[0]))),
                       static_cast<int>(std::lround(0.5 * (ijk1[1] + ijk2[1]))));
  pnode->SetLineAngle(positionAngle);
  pnode->SetLineOldAngle(positionAngle);
  pnode->EndModify(wasModifying);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::CenterLineOnPeak(vtkMRMLAstroPVSliceParametersNode* pnode)
{
  vtkMRMLAstroVolumeNode* map = nullptr;
  vtkMRMLMarkupsLineNode* line = nullptr;
  double center[2] = {0., 0.};
  double angle = 0.;
  double length = 0.;
  if (!PVNodes(this->GetMRMLScene(), pnode, map, line) || !this->GetLineSky(pnode, center, &angle, &length))
    {
    return false;
    }
  vtkImageData* image = map->GetImageData();
  int dims[3] = {0, 0, 0};
  image->GetDimensions(dims);
  double peak = -std::numeric_limits<double>::infinity();
  int peakIJK[2] = {-1, -1};
  for (int j = 0; j < dims[1]; j++)
    {
    for (int i = 0; i < dims[0]; i++)
      {
      const double value = image->GetScalarComponentAsDouble(i, j, 0, 0);
      if (std::isfinite(value) && value > peak)
        {
        peak = value;
        peakIJK[0] = i;
        peakIJK[1] = j;
        }
      }
    }
  const double ijk[3] = {static_cast<double>(peakIJK[0]), static_cast<double>(peakIJK[1]), 0.};
  double world[3] = {0., 0., 0.};
  if (peakIJK[0] < 0 || !map->GetAstroVolumeDisplayNode()->GetReferenceSpace(ijk, world))
    {
    return false;
    }
  const double peakCenter[2] = {world[0], world[1]};
  return this->SetLineSky(pnode, peakCenter, angle, length);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::AlignLineWithMajorAxis(vtkMRMLAstroPVSliceParametersNode* pnode)
{
  vtkMRMLAstroVolumeNode* map = nullptr;
  vtkMRMLMarkupsLineNode* line = nullptr;
  double center[2] = {0., 0.};
  double angle = 0.;
  double length = 0.;
  if (!PVNodes(this->GetMRMLScene(), pnode, map, line) || !this->GetLineSky(pnode, center, &angle, &length))
    {
    return false;
    }
  vtkImageData* image = map->GetImageData();
  int dims[3] = {0, 0, 0};
  image->GetDimensions(dims);
  double peak = 0.;
  for (int j = 0; j < dims[1]; j++)
    {
    for (int i = 0; i < dims[0]; i++)
      {
      const double value = image->GetScalarComponentAsDouble(i, j, 0, 0);
      if (std::isfinite(value))
        {
        peak = std::max(peak, value);
        }
      }
    }
  if (!(peak > 0.))
    {
    return false;
    }
  // intensity-weighted centroid and second moments of the emission
  double weight = 0.;
  double mi = 0.;
  double mj = 0.;
  for (int j = 0; j < dims[1]; j++)
    {
    for (int i = 0; i < dims[0]; i++)
      {
      const double value = image->GetScalarComponentAsDouble(i, j, 0, 0);
      if (std::isfinite(value) && value > 0.2 * peak)
        {
        weight += value;
        mi += value * i;
        mj += value * j;
        }
      }
    }
  mi /= weight;
  mj /= weight;
  double sii = 0.;
  double sjj = 0.;
  double sij = 0.;
  for (int j = 0; j < dims[1]; j++)
    {
    for (int i = 0; i < dims[0]; i++)
      {
      const double value = image->GetScalarComponentAsDouble(i, j, 0, 0);
      if (std::isfinite(value) && value > 0.2 * peak)
        {
        sii += value * (i - mi) * (i - mi);
        sjj += value * (j - mj) * (j - mj);
        sij += value * (i - mi) * (j - mj);
        }
      }
    }
  // direction of the largest eigenvalue, in pixels
  const double theta = 0.5 * std::atan2(2. * sij, sii - sjj);
  const double ijkCentroid[3] = {mi, mj, 0.};
  const double ijkAlong[3] = {mi + 10. * std::cos(theta), mj + 10. * std::sin(theta), 0.};
  double worldCentroid[3] = {0., 0., 0.};
  double worldAlong[3] = {0., 0., 0.};
  vtkMRMLAstroVolumeDisplayNode* display = map->GetAstroVolumeDisplayNode();
  if (!display->GetReferenceSpace(ijkCentroid, worldCentroid) || !display->GetReferenceSpace(ijkAlong, worldAlong))
    {
    return false;
    }
  double majorAngle = 0.;
  double separation = 0.;
  SkyGeometry(worldCentroid, worldAlong, &majorAngle, &separation);
  const double centroid[2] = {worldCentroid[0], worldCentroid[1]};
  return this->SetLineSky(pnode, centroid, majorAngle, length);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::ConstrainLine(vtkMRMLAstroPVSliceParametersNode* pnode)
{
  vtkMRMLAstroVolumeNode* map = nullptr;
  vtkMRMLMarkupsLineNode* line = nullptr;
  if (this->ConstrainingLine || !PVNodes(this->GetMRMLScene(), pnode, map, line))
    {
    return false;
    }
  // moving the points fires point events again (Markups ignore
  // DisableModifiedEvent for them): the points move only when needed
  this->ConstrainingLine = true;
  double ijk1[3] = {0., 0., 0.};
  double ijk2[3] = {0., 0., 0.};
  LineEndsIJK(map, line, ijk1, ijk2);
  if (std::fabs(ijk1[2]) > 1.e-3 || std::fabs(ijk2[2]) > 1.e-3)
    {
    SetLineEndsIJK(map, line, ijk1, ijk2);
    }
  pnode->SetLineCenter(static_cast<int>(0.5 * (ijk1[0] + ijk2[0])), static_cast<int>(0.5 * (ijk1[1] + ijk2[1])));
  this->ConstrainingLine = false;
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::SetPVCamera(vtkMRMLAstroPVSliceParametersNode* pnode, int view)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLAstroVolumeNode* volume = (scene && pnode && pnode->GetInputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetInputVolumeNodeID())) : nullptr;
  vtkMRMLCameraNode* camera = scene ? vtkSlicerAstroVolumeLogic::GetThreeDViewCameraNode(scene, "vtkMRMLViewNode1") :
    nullptr;
  if (!volume || !volume->GetImageData() || !camera)
    {
    return false;
    }
  if (view == SkyView)
    {
    // looking along the velocity axis (A in RAS), north up
    const double directionOfProjection[3] = {0., -1., 0.};
    const double viewUp[3] = {0., 0., 1.};
    return vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(camera, volume, directionOfProjection, viewUp);
    }
  vtkMRMLMarkupsLineNode* line = pnode->GetLineNodeID() ?
    vtkMRMLMarkupsLineNode::SafeDownCast(scene->GetNodeByID(pnode->GetLineNodeID())) : nullptr;
  if (!line || line->GetNumberOfDefinedControlPoints() < 2)
    {
    return false;
    }
  double position1[3] = {0., 0., 0.};
  double position2[3] = {0., 0., 0.};
  line->GetLineStartPosition(position1);
  line->GetLineEndPosition(position2);
  // the line in the sky plane (R, S in RAS), velocity up (A)
  double along[3] = {position2[0] - position1[0], 0., position2[2] - position1[2]};
  const double norm = std::sqrt(along[0] * along[0] + along[2] * along[2]);
  if (!(norm > 0.))
    {
    return false;
    }
  along[0] /= norm;
  along[2] /= norm;
  double bounds[6] = {0., 0., 0., 0., 0., 0.};
  volume->GetRASBounds(bounds);
  const double focalPoint[3] = {0.5 * (position1[0] + position2[0]), 0.5 * (bounds[2] + bounds[3]),
                                0.5 * (position1[2] + position2[2])};
  const double viewUp[3] = {0., 1., 0.};
  // along the line, or facing the PV plane (perpendicular to the line)
  const double directionOfProjection[3] = {view == AlongLineView ? along[0] : -along[2], 0.,
                                           view == AlongLineView ? along[2] : along[0]};
  return vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(camera, volume, directionOfProjection, viewUp, focalPoint);
}
