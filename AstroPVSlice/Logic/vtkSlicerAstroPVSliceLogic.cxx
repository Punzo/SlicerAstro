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
#include <vtkMRMLAnnotationRulerNode.h>
#include <vtkMRMLAnnotationTextDisplayNode.h>
#include <vtkMRMLAnnotationPointDisplayNode.h>
#include <vtkMRMLAnnotationLineDisplayNode.h>
#include <vtkMRMLAstroMomentMapsParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroPVSliceParametersNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

// VTK includes
#include <vtkCacheManager.h>
#include <vtkGeneralTransform.h>
#include <vtkImageData.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
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

// Qt includes
#include <QtDebug>

#include <iostream>
#include <sys/time.h>

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

  // Create Astro Volume for the moment map
  std::string name = inputVolume->GetName();
  name += "_";
  name += this->GetMRMLScene()->GetUniqueNameByString("PVSliceMomentMap");

  vtkMRMLAstroVolumeNode* MomentMapNode = vtkMRMLAstroVolumeNode::SafeDownCast
     (this->GetAstroVolumeLogic()->CloneVolumeWithoutImageData(this->GetMRMLScene(), inputVolume, name.c_str()));

  // Modify fits attributes
  MomentMapNode->SetAttribute("SlicerAstro.NAXIS", "2");
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
  inputVolume->GetOrigin(Origin);
  Origin[1] = -Origin[1];
  MomentMapNode->SetOrigin(Origin);

  vtkNew<vtkMRMLAstroMomentMapsParametersNode> momentMapParametersNode;
  momentMapParametersNode->SetInputVolumeNodeID(inputVolume->GetID());
  momentMapParametersNode->SetZeroMomentVolumeNodeID(MomentMapNode->GetID());
  momentMapParametersNode->SetMaskActive(false);
  momentMapParametersNode->SetGenerateZero(true);
  momentMapParametersNode->SetGenerateFirst(false);
  momentMapParametersNode->SetGenerateSecond(false);
  momentMapParametersNode->SetIntensityMin(StringToDouble
    (inputVolume->GetAttribute("SlicerAstro.3DDisplayThreshold")) * 2);
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
    return false;
    }

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

  layoutManager->layoutLogic()->GetLayoutNode()->SetViewArrangement(2);

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
bool vtkSlicerAstroPVSliceLogic::CreateAndSetRuler(vtkMRMLAstroPVSliceParametersNode *pnode)
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

  vtkSmartPointer<vtkMRMLAnnotationRulerNode> RulerNode;
  vtkMRMLNode *foo = this->GetMRMLScene()->CreateNodeByClass("vtkMRMLAnnotationRulerNode");
  RulerNode.TakeReference(vtkMRMLAnnotationRulerNode::SafeDownCast(foo));

  // Set name
  std::string name = this->GetMRMLScene()->GetUniqueNameByString("PVSliceRuler");
  RulerNode->SetName(name.c_str());

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
  RulerNode->SetPosition1(RASposition);

  position[0] = dimensions[0] - (dimensions[0] * 0.1);
  position[1] = dimensions[1] - (dimensions[1] * 0.1);
  position[2] = IJKOrigin[2];
  IJKtoRASTransform->TransformPoint(position, RASposition);
  RulerNode->SetPosition2(RASposition);

  // Add to the scene
  this->GetMRMLScene()->AddNode(RulerNode);

  // Change the look of the ruler
  vtkMRMLAnnotationTextDisplayNode* TextDisplayNode =
    RulerNode->GetAnnotationTextDisplayNode();
  if (!TextDisplayNode)
    {
    RulerNode->CreateAnnotationTextDisplayNode();
    }
  TextDisplayNode->SetVisibility(0);

  vtkMRMLAnnotationPointDisplayNode* PointDisplayNode =
    RulerNode->GetAnnotationPointDisplayNode();
  if (!PointDisplayNode)
    {
    RulerNode->CreateAnnotationPointDisplayNode();
    }
  PointDisplayNode->SetGlyphScale(15.);

  vtkMRMLAnnotationLineDisplayNode* LineDisplayNode =
    RulerNode->GetAnnotationLineDisplayNode();
  if (!LineDisplayNode)
    {
    RulerNode->CreateAnnotationLineDisplayNode();
    }
  LineDisplayNode->SetLabelVisibility(0);
  LineDisplayNode->SetMaxTicks(0);
  LineDisplayNode->SetLineWidth(3);
  LineDisplayNode->SetLineThickness(3);

  pnode->SetRulerNodeID(RulerNode->GetID());

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::InitializeRuler(vtkMRMLAstroPVSliceParametersNode *pnode)
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
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::InitializeRuler :"
                  " PVMomentMap not found.");
    return false;
    }

  vtkMRMLAnnotationRulerNode *RulerNode =
    vtkMRMLAnnotationRulerNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetRulerNodeID()));
  if(!RulerNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::InitializeRuler :"
                  " RulerNode not found.");
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
  RulerNode->SetPosition1(RASposition);

  position[0] = dimensions[0] - (dimensions[0] * 0.1);
  position[1] = dimensions[1] - (dimensions[1] * 0.1);
  position[2] = IJKOrigin[2];
  IJKtoRASTransform->TransformPoint(position, RASposition);
  RulerNode->SetPosition2(RASposition);

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::UpdateRuler(vtkMRMLAstroPVSliceParametersNode *pnode)
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

  vtkMRMLAnnotationRulerNode *RulerNode =
    vtkMRMLAnnotationRulerNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetRulerNodeID()));
  if(!RulerNode)
    {
    return false;
    }

  if (fabs(pnode->GetRulerAngle() - pnode->GetRulerOldAngle()) < 0.01 &&
      fabs(pnode->GetRulerShiftX() - pnode->GetRulerOldShiftX()) < 0.01 &&
      fabs(pnode->GetRulerShiftY() - pnode->GetRulerOldShiftY()) < 0.01)
    {
    return false;
    }

  double* RASOrigin = PVMomentMap->GetOrigin();
  double position1[3] = {0};
  RulerNode->GetPosition1(position1);
  double position2[3] = {0};
  RulerNode->GetPosition2(position2);

  double RulerCenter[3], Shift[3];
  for (int ii = 0; ii < 3; ii++)
    {
    RulerCenter[ii] = (position1[ii] + position2[ii]) * 0.5;
    Shift[ii] = RulerCenter[ii] - RASOrigin[ii];
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
  RAStoIJKTransform->TransformPoint(RulerCenter,RulerCenter);

  vtkNew<vtkGeneralTransform> RulerRotateTransform;
  RulerRotateTransform->Identity();
  RulerRotateTransform->PostMultiply();
  RulerRotateTransform->RotateZ(pnode->GetRulerOldAngle() -
                                pnode->GetRulerAngle());

  RulerRotateTransform->TransformPoint(position1, position1);
  RulerRotateTransform->TransformPoint(position2, position2);

  vtkNew<vtkGeneralTransform> RulerShiftTransform;
  RulerShiftTransform->Identity();
  RulerShiftTransform->PostMultiply();
  RulerShiftTransform->Translate(pnode->GetRulerShiftX() - pnode->GetRulerOldShiftX(),
                                 pnode->GetRulerShiftY() - pnode->GetRulerOldShiftY(),
                                 0.);

  RulerShiftTransform->TransformPoint(position1, position1);
  RulerShiftTransform->TransformPoint(position2, position2);

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

  int wasModifying = RulerNode->StartModify();
  RulerNode->SetPosition1(position1);
  RulerNode->SetPosition2(position2);
  RulerNode->EndModify(wasModifying);

  pnode->DisableModifiedEventOn();
  pnode->SetRulerOldAngle(pnode->GetRulerAngle());
  pnode->SetRulerOldShiftX(pnode->GetRulerShiftX());
  pnode->SetRulerOldShiftY(pnode->GetRulerShiftY());
  int IJKRulerCenter[2];
  IJKRulerCenter[0] = RulerCenter[0];
  IJKRulerCenter[1] = RulerCenter[1];
  pnode->SetRulerCenter(IJKRulerCenter);
  pnode->DisableModifiedEventOff();

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVSliceLogic::UpdateRulerFromCenter(vtkMRMLAstroPVSliceParametersNode *pnode)
{
  if (!this->GetMRMLScene() || !pnode)
    {
    return false;
    }

  int NewIJKRulerCenter[2];
  pnode->GetRulerCenter(NewIJKRulerCenter);

  vtkMRMLAstroVolumeNode *PVMomentMap =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetMomentMapNodeID()));
  if(!PVMomentMap || !PVMomentMap->GetImageData())
    {
    return false;
    }

  vtkMRMLAnnotationRulerNode *RulerNode =
    vtkMRMLAnnotationRulerNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetRulerNodeID()));
  if(!RulerNode)
    {
    return false;
    }

  double position1[3] = {0};
  RulerNode->GetPosition1(position1);
  double position2[3] = {0};
  RulerNode->GetPosition2(position2);

  double RulerCenter[3];
  for (int ii = 0; ii < 3; ii++)
    {
    RulerCenter[ii] = (position1[ii] + position2[ii]) * 0.5;
    }

  vtkNew<vtkGeneralTransform> RAStoIJKTransform;
  RAStoIJKTransform->Identity();
  RAStoIJKTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
  PVMomentMap->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());

  RAStoIJKTransform->TransformPoint(position1,position1);
  RAStoIJKTransform->TransformPoint(position2,position2);
  RAStoIJKTransform->TransformPoint(RulerCenter,RulerCenter);

  vtkNew<vtkGeneralTransform> RulerShiftTransform;
  RulerShiftTransform->Identity();
  RulerShiftTransform->PostMultiply();
  RulerShiftTransform->Translate(NewIJKRulerCenter[0] - RulerCenter[0],
                                 NewIJKRulerCenter[1] - RulerCenter[1],
                                 0.);

  RulerShiftTransform->TransformPoint(position1, position1);
  RulerShiftTransform->TransformPoint(position2, position2);

  RAStoIJKTransform->Identity();
  RAStoIJKMatrix->Invert();
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());

  RAStoIJKTransform->TransformPoint(position1,position1);
  RAStoIJKTransform->TransformPoint(position2,position2);

  int wasModifying = RulerNode->StartModify();
  RulerNode->SetPosition1(position1);
  RulerNode->SetPosition2(position2);
  RulerNode->EndModify(wasModifying);

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
  vtkMRMLAnnotationRulerNode *RulerNode =
    vtkMRMLAnnotationRulerNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetRulerNodeID()));
  if(!RulerNode)
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
  RulerNode->GetPosition1(position1);
  double position2[3] = {0};
  RulerNode->GetPosition2(position2);

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
  vtkMRMLAnnotationRulerNode *RulerNode =
    vtkMRMLAnnotationRulerNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetRulerNodeID()));
  if(!RulerNode)
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
  RulerNode->GetPosition1(position1);
  double position2[3] = {0};
  RulerNode->GetPosition2(position2);

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
  double RulerLength = sqrt(((position1[0] - position2[0]) * (position1[0] - position2[0])) +
                            ((position1[1] - position2[1]) * (position1[1] - position2[1])));
  double distX = (position2[0] - position1[0]);
  double distY = (position2[1] - position1[1]);

  double PVPhiMajor = -atan(distY / distX);
  PVPhiMajor *=  180. / PI;
  if ((position1[0] - position2[0]) < 0.)
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

  vtkSmartPointer<vtkMRMLNode> defaultNode = vtkMRMLSliceNode::SafeDownCast
      (this->GetMRMLScene()->GetDefaultNodeByClass("vtkMRMLSliceNode"));
  vtkMRMLSliceNode *defaultSliceNode = vtkMRMLSliceNode::SafeDownCast(defaultNode);
  if (!defaultSliceNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVSliceLogic::UpdatePV :"
                  " defaultSliceNode not found.");
    return false;
    }
  defaultSliceNode->SetOrientation("XZ");

  vtkNew<vtkTransform> yellowTransform;
  yellowTransform->SetMatrix(defaultSliceNode->GetSliceToRAS());
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
  FieldOfView[0] = RulerLength;
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
  greenTransform->SetMatrix(defaultSliceNode->GetSliceToRAS());
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
  FieldOfView[0] = RulerLength;
  greenSliceNode->SetFieldOfView(FieldOfView[0] + (FieldOfView[0] * 0.2),
                                 dims[2] + (dims[2] * 0.2),
                                 FieldOfView[2]);

  return true;
}
