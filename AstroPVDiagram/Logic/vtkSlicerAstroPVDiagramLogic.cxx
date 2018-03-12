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

// MRML includes
#include <vtkMRMLAnnotationRulerNode.h>
#include <vtkMRMLAnnotationTextDisplayNode.h>
#include <vtkMRMLAnnotationPointDisplayNode.h>
#include <vtkMRMLAnnotationLineDisplayNode.h>
#include <vtkMRMLAstroMomentMapsParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroPVDiagramParametersNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

// VTK includes
#include <vtkCardinalSpline.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCacheManager.h>
#include <vtkGeneralTransform.h>
#include <vtkImageData.h>
#include <vtkLine.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkTubeFilter.h>
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

// OpenMP includes
#ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
#include <omp.h>
#endif

#include <iostream>
#include <sys/time.h>

//----------------------------------------------------------------------------
class vtkSlicerAstroPVDiagramLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkSlicerAstroMomentMapsLogic> AstroMomentMapsLogic;
  vtkSmartPointer<vtkPolyData> CurvePoly;
};

//----------------------------------------------------------------------------
vtkSlicerAstroPVDiagramLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = 0;
  this->AstroMomentMapsLogic = 0;
  this->CurvePoly = vtkSmartPointer<vtkPolyData>::New();
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
bool vtkSlicerAstroPVDiagramLogic::Calculate0thMomentMap(vtkMRMLAstroPVDiagramParametersNode *pnode)
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
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::CalculateAndSet0thMomentMap :"
                  " inputVolume not found.");
    return false;
    }

  vtkMRMLAstroVolumeDisplayNode * astroMrmlDisplayNode = inputVolume->GetAstroVolumeDisplayNode();
  if (!astroMrmlDisplayNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::CalculateAndSet0thMomentMap :"
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
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::CalculateAndSet0thMomentMap :"
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
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::CalculateAndSet0thMomentMap :"
                  " MomentMaps calculations failed.");
    return false;
    }

  pnode->SetMomentMapNodeID(MomentMapNode->GetID());

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::SetMomentMapOnRedWidget(vtkMRMLAstroPVDiagramParametersNode *pnode)
{
  if (!pnode || !this->GetMRMLScene())
    {
    return false;
    }

  // Setting the Layout for the Output
  qSlicerApplication* app = qSlicerApplication::application();
  if(!app)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::Set0thMomentMap : "
                   "app not found!");
    return false;
    }

  qSlicerLayoutManager* layoutManager = app->layoutManager();

  if(!layoutManager)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::Set0thMomentMap : "
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
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::Set0thMomentMap : "
                   "appLogic not found!");
    return false;
    }

  vtkMRMLSliceLogic* redSliceLogic = appLogic->GetSliceLogic(redSlice);
  if (!redSliceLogic)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::Set0thMomentMap : "
                   "redSliceLogic not found!");
    return false;
    }

  int *dims = redSlice->GetDimensions();
  if (dims)
    {
    redSliceLogic->FitSliceToAll(dims[0], dims[1]);
    }
  redSliceLogic->SnapSliceOffsetToIJK();

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

  PVDiagramMatrix->SetElement(0, 0, yellowSliceNode->GetSliceToRAS()->GetElement(0,0));
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
bool vtkSlicerAstroPVDiagramLogic::UpdateSliceSelection(vtkMRMLAstroPVDiagramParametersNode *pnode)
{
  if (!this->GetMRMLScene() || !pnode)
    {
    return false;
    }

  vtkMRMLModelNode *LineSelectionModelNode =
    vtkMRMLModelNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetModelID()));
  if(!LineSelectionModelNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::UpdateSliceSelection :"
                  " LineSelectionModelNode not found.");
    return false;
    }

  vtkMRMLMarkupsFiducialNode *PointsSourceNode =
    vtkMRMLMarkupsFiducialNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetFiducialsMarkupsID()));
  if(!PointsSourceNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::UpdateSliceSelection :"
                  " PointsSourceNode not found.");
    return false;
    }

  int nOfControlPoints = PointsSourceNode->GetNumberOfFiducials();
  if (nOfControlPoints < 2)
    {
    this->Internal->CurvePoly->Initialize();
    LineSelectionModelNode->SetAndObservePolyData(this->Internal->CurvePoly);
    LineSelectionModelNode->Modified();
    return false;
    }

  vtkNew<vtkPoints> points;
  vtkNew<vtkCellArray> cellArray;
  int nInterpolatedPoints = (10 * (nOfControlPoints - 1)) + 1;

  if (!pnode->GetInterpolation() || nOfControlPoints == 2)
    {
    points->SetNumberOfPoints(nInterpolatedPoints);
    cellArray->InsertNextCell(nInterpolatedPoints);

    double pos[3] = {0.}, oldPos[3] = {0.};
    for (int ii = 0; ii < nOfControlPoints; ii++)
      {
      PointsSourceNode->GetNthFiducialPosition(ii, pos);
      if (ii == 0)
        {
        points->SetPoint(ii, pos);
        cellArray->InsertCellPoint(ii);
        }
      else
        {
        PointsSourceNode->GetNthFiducialPosition(ii - 1, oldPos);
        double deltaX = (pos[0] - oldPos[0]) * 0.1;
        double deltaY = (pos[1] - oldPos[1]) * 0.1;
        double deltaZ = (pos[2] - oldPos[2]) * 0.1;
        for (int jj = 0; jj < 10; jj++)
          {
          int pointIndex = ((ii - 1) * 10) + jj + 1;
          oldPos[0] += deltaX;
          oldPos[1] += deltaY;
          oldPos[2] += deltaZ;
          points->SetPoint(pointIndex, oldPos);
          cellArray->InsertCellPoint(pointIndex);
          }
        }
      }
    }
  else
    {
    double pos[3] = {0.};

    vtkNew<vtkCardinalSpline> aSplineX;
    vtkNew<vtkCardinalSpline> aSplineY;
    vtkNew<vtkCardinalSpline> aSplineZ;

    aSplineX->ClosedOff();
    aSplineY->ClosedOff();
    aSplineZ->ClosedOff();

    for (int ii = 0; ii < nOfControlPoints; ii++)
      {
      PointsSourceNode->GetNthFiducialPosition(ii, pos);
      aSplineX->AddPoint(ii, pos[0]);
      aSplineY->AddPoint(ii, pos[1]);
      aSplineZ->AddPoint(ii, pos[2]);
      }

    double r[2] = {0.};
    aSplineX->GetParametricRange(r);
    double t = r[0];
    int p = 0;
    double tStep = (nOfControlPoints - 1.) / (nInterpolatedPoints - 1.);
    int nOutputPoints = 0;

    while (t < r[1])
      {
      points->InsertPoint(p, aSplineX->Evaluate(t), aSplineY->Evaluate(t), aSplineZ->Evaluate(t));
      t += tStep;
      p++;
      }
    t += tStep;
    points->InsertPoint(p, aSplineX->Evaluate(t), aSplineY->Evaluate(t), aSplineZ->Evaluate(t));
    p++;
    nOutputPoints = p;

    cellArray->InsertNextCell(nOutputPoints);
    for (int ii = 0; ii < nOutputPoints; ii++)
      {
      cellArray->InsertCellPoint(ii);
      }
    }

  this->Internal->CurvePoly->Initialize();
  this->Internal->CurvePoly->SetPoints(points.GetPointer());
  this->Internal->CurvePoly->SetLines(cellArray.GetPointer());

  vtkNew<vtkTubeFilter> tubeFilter;

  tubeFilter->SetInputData(this->Internal->CurvePoly);
  tubeFilter->SetRadius(0.5);
  tubeFilter->SetNumberOfSides(20);
  tubeFilter->CappingOn();
  tubeFilter->Update();

  LineSelectionModelNode->SetAndObservePolyData(tubeFilter->GetOutput());
  LineSelectionModelNode->Modified();

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroPVDiagramLogic::GenerateAndSetPVDiagram(vtkMRMLAstroPVDiagramParametersNode *pnode)
{
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics : "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results that "
                  "the AstroPVDiagram algorithm may show poor performance.")
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  if (!this->GetMRMLScene() || !pnode)
    {
    return false;
    }

  vtkMRMLMarkupsFiducialNode *PointsSourceNode =
    vtkMRMLMarkupsFiducialNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetFiducialsMarkupsID()));
  if(!PointsSourceNode)
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::UpdateSliceSelection :"
                  " PointsSourceNode not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *PVMomentMap =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetMomentMapNodeID()));
  if(!PVMomentMap || !PVMomentMap->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::UpdateSliceSelection :"
                  " PVMomentMap not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::UpdateSliceSelection :"
                  " inputVolume not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *PVDiagramVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->
      GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if(!PVDiagramVolume || !PVDiagramVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroPVDiagramLogic::UpdateSliceSelection :"
                  " PVDiagramVolume not found.");
    return false;
    }

  int nOfControlPoints = PointsSourceNode->GetNumberOfFiducials();
  if (nOfControlPoints < 2)
    {
    return false;
    }

  vtkNew<vtkGeneralTransform> RAStoIJKTransform;
  RAStoIJKTransform->Identity();
  RAStoIJKTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
  PVMomentMap->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());

  vtkNew<vtkPoints> points;
  double posRAS[3] = {0.}, oldPosRAS[3] = {0.};
  double posIJK[3] = {0.}, oldPosIJK[3] = {0.};
  if (!pnode->GetInterpolation() || nOfControlPoints == 2)
    {
    for (int ii = 0; ii < nOfControlPoints; ii++)
      {
      PointsSourceNode->GetNthFiducialPosition(ii, posRAS);
      RAStoIJKTransform->TransformPoint(posRAS, posIJK);
      if (ii == 0)
        {
        points->InsertNextPoint(posIJK);
        }
      else
        {
        PointsSourceNode->GetNthFiducialPosition(ii - 1, oldPosRAS);
        RAStoIJKTransform->TransformPoint(oldPosRAS, oldPosIJK);
        double step = 0.5;
        double deltaX = (posIJK[0] - oldPosIJK[0]) * step;
        double deltaY = (posIJK[1] - oldPosIJK[1]) * step;
        while (fabs(deltaX) > 0.95 || fabs(deltaY) > 0.95)
          {
          step *= 0.75;
          deltaX = (posIJK[0] - oldPosIJK[0]) * step;
          deltaY = (posIJK[1] - oldPosIJK[1]) * step;
          }

        int numberOfSteps = (int) (1. / step);
        for (int jj = 0; jj < numberOfSteps; jj++)
          {
          oldPosIJK[0] += deltaX;
          oldPosIJK[1] += deltaY;
          points->InsertNextPoint(oldPosIJK);
          }
        }
      }
    }
  else
    {
    vtkNew<vtkCardinalSpline> aSplineX;
    vtkNew<vtkCardinalSpline> aSplineY;
    vtkNew<vtkCardinalSpline> aSplineZ;

    aSplineX->ClosedOff();
    aSplineY->ClosedOff();
    aSplineZ->ClosedOff();

    for (int ii = 0; ii < nOfControlPoints; ii++)
      {
      PointsSourceNode->GetNthFiducialPosition(ii, posRAS);
      RAStoIJKTransform->TransformPoint(posRAS, posIJK);
      if (ii == 0)
        {
        RAStoIJKTransform->TransformPoint(posRAS, oldPosIJK);
        }
      aSplineX->AddPoint(ii, posIJK[0]);
      aSplineY->AddPoint(ii, posIJK[1]);
      aSplineZ->AddPoint(ii, posIJK[2]);
      }

    double r[2] = {0.};
    aSplineX->GetParametricRange(r);
    double t = r[0], newT;
    double tStep = 1. / (3 * nOfControlPoints);
    double valueX, valueY, valueZ, deltaX, deltaY;
    double oldValueX = oldPosIJK[0];
    double oldValueY = oldPosIJK[1];

    valueX = aSplineX->Evaluate(t);
    valueY = aSplineY->Evaluate(t);
    valueZ = aSplineZ->Evaluate(t);
    points->InsertNextPoint(valueX, valueY, valueZ);

    while (t < r[1])
      {
      newT = t + tStep;
      valueX = aSplineX->Evaluate(newT);
      valueY = aSplineY->Evaluate(newT);

      deltaX = valueX - oldValueX;
      deltaY = valueY - oldValueY;

      while (fabs(deltaX) > 0.95 || fabs(deltaY) > 0.95)
        {
        tStep *= 0.75;
        newT = t + tStep;
        valueX = aSplineX->Evaluate(newT);
        valueY = aSplineY->Evaluate(newT);

        deltaX = valueX - oldValueX;
        deltaY = valueY - oldValueY;
        }

      t = newT;
      tStep = 1. / (2 * nOfControlPoints);

      oldValueX = valueX;
      oldValueY = valueY;

      points->InsertNextPoint(valueX, valueY, valueZ);
      }
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

  float *inFPixel = NULL;
  float *PVDiagramFPixel = NULL;
  double *inDPixel = NULL;
  double *PVDiagramDPixel = NULL;
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
  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  omp_set_num_threads(omp_get_num_procs());
  #pragma omp parallel for schedule(static) shared(pnode, points, PVDiagramFPixel, inFPixel, PVDiagramDPixel, inDPixel)
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
  for (int PointIndex = 0; PointIndex < points->GetNumberOfPoints(); PointIndex++)
    {
    double posIJK[3];
    points->GetPoint(PointIndex, posIJK);

    int iiLeft = floor(posIJK[0]);
    int iiRight = ceil(posIJK[0]);
    double iiInterpolatedRight = iiRight - posIJK[0];
    double iiInterpolatedLeft = posIJK[0] - iiLeft;

    int jjBottom = floor(posIJK[1]);
    int jjTop = ceil(posIJK[1]);
    double jjInterpolatedTop = jjTop - posIJK[1];
    double jjInterpolatedBottom = posIJK[1] - jjBottom;

    int IndexLB = iiLeft + jjBottom * dims[0];
    int IndexLT = iiLeft + jjTop * dims[0];
    int IndexRB = iiRight + jjBottom * dims[0];
    int IndexRT = iiRight + jjTop * dims[0];

    for (int kk = 0; kk < N2; kk++)
      {
      double IndexZ = kk * numSlice;
      int IndexZLB = IndexLB + IndexZ;
      int IndexZLT = IndexLT + IndexZ;
      int IndexZRB = IndexRB + IndexZ;
      int IndexZRT = IndexRT + IndexZ;
      double ITop, IBottom;
      switch (DataType)
        {
        case VTK_FLOAT:
          ITop = *(inFPixel + IndexZLT) * (iiInterpolatedRight) + *(inFPixel + IndexZRT) * (iiInterpolatedLeft);
          IBottom = *(inFPixel + IndexZLB) * (iiInterpolatedRight) + *(inFPixel + IndexZRB) * (iiInterpolatedLeft);
          *(PVDiagramFPixel + PointIndex + kk * N1) = ITop * jjInterpolatedBottom + IBottom * jjInterpolatedTop;
          break;
        case VTK_DOUBLE:
          ITop = *(inDPixel + IndexZLT) * (iiInterpolatedRight) + *(inDPixel + IndexZRT) * (iiInterpolatedLeft);
          IBottom = *(inDPixel + IndexZLB) * (iiInterpolatedRight) + *(inDPixel + IndexZRB) * (iiInterpolatedLeft);
          *(PVDiagramDPixel + PointIndex + kk * N1) = ITop * jjInterpolatedBottom + IBottom * jjInterpolatedTop;
          break;
        }
      }
    }

  inFPixel = NULL;
  PVDiagramFPixel = NULL;
  inDPixel = NULL;
  PVDiagramDPixel = NULL;

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
      CDELT1 += sqrt(((posWCS[0] - oldPosWCS[0]) * (posWCS[0] - oldPosWCS[0])) +
                     ((posWCS[1] - oldPosWCS[1]) * (posWCS[1] - oldPosWCS[1])));
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

  layoutManager->layoutLogic()->GetLayoutNode()->SetViewArrangement(2);

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
  yellowSliceComposite->SetBackgroundVolumeID(pnode->GetOutputVolumeNodeID());

  vtkMRMLSliceCompositeNode *greenSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
    this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeGreen"));
  greenSliceComposite->SetLabelVolumeID("");
  greenSliceComposite->SetForegroundVolumeID("");
  greenSliceComposite->SetForegroundOpacity(0.);
  greenSliceComposite->SetBackgroundVolumeID("");

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
    int *dimsSlice = yellowSliceNode->GetDimensions();
    if (dimsSlice)
      {
      yellowSliceLogic->FitSliceToAll(dims[0], dims[1]);
      }
    yellowSliceLogic->SnapSliceOffsetToIJK();
    }

  double FieldOfView[3];
  yellowSliceNode->GetFieldOfView(FieldOfView);
  yellowSliceNode->SetFieldOfView(N1, dims[2], FieldOfView[2]);

  return true;
}
