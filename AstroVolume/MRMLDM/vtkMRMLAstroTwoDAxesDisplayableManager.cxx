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

// MRMLDisplayableManager includes
#include "vtkMRMLAstroTwoDAxesDisplayableManager.h"

// Qt includes
#include <QColor>

// MRML includes
#include <vtkMRMLAbstractViewNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLLogic.h>
#include <vtkMRMLSliceLayerLogic.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLViewNode.h>

// VTK includes
#include <vtkActor2D.h>
#include <vtkActor2DCollection.h>
#include <vtkAxisActor2D.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkGeneralTransform.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
#include <vtkPoints.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkLine.h>
#include <vtksys/SystemTools.hxx>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>

// STD includes
#include <sstream>

// SlicerQt includes
#include <qSlicerApplication.h>

// vtkSlicer includes
#include <vtkSlicerApplicationLogic.h>

// Constants
static const int RENDERER_LAYER = 1; // layer ID where the orientation marker will be displayed
const double PI_2  = PI * 0.5;

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
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
}

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

}// end namespace

//---------------------------------------------------------------------------
class vtkAstroTwoDAxesRendererUpdateObserver : public vtkCommand
{
public:
  static vtkAstroTwoDAxesRendererUpdateObserver *New()
    {
    return new vtkAstroTwoDAxesRendererUpdateObserver;
    }
  vtkAstroTwoDAxesRendererUpdateObserver()
    {
    this->DisplayableManager = 0;
    }
  virtual void Execute(vtkObject* vtkNotUsed(wdg), unsigned long vtkNotUsed(event), void* vtkNotUsed(calldata))
    {
    if (this->DisplayableManager)
      {
      this->DisplayableManager->UpdateFromRenderer();
      }
  }
  vtkWeakPointer<vtkMRMLAstroTwoDAxesDisplayableManager> DisplayableManager;
};

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLAstroTwoDAxesDisplayableManager);

//---------------------------------------------------------------------------
class vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal
{
public:
  vtkInternal(vtkMRMLAstroTwoDAxesDisplayableManager * external);
  ~vtkInternal();

  void SetupMarkerRenderer();
  void AddRendererUpdateObserver(vtkRenderer* renderer);
  void RemoveRendererUpdateObserver();

  void SetupAxes();
  void UpdateAxes();
  void ShowActors(bool show);

  vtkSmartPointer<vtkRenderer> MarkerRenderer;
  vtkSmartPointer<vtkAstroTwoDAxesRendererUpdateObserver> RendererUpdateObserver;
  vtkSmartPointer<vtkPoints> twoDAxesPoints;
  vtkSmartPointer<vtkCellArray> twoDAxesCellArray;
  vtkSmartPointer<vtkPolyData> twoDAxesPolyData;
  vtkSmartPointer<vtkActor2D> twoDAxesActor;
  vtkSmartPointer<vtkPolyDataMapper2D> twoDAxesMapper;
  vtkSmartPointer<vtkCollection> col;

  int RendererUpdateObservationId;
  vtkWeakPointer<vtkRenderer> ObservedRenderer;

  bool ActorsAddedToRenderer;

  vtkSmartPointer<vtkDoubleArray> Color;
  static const double COLOR_INVALID[3];

  int fontSize;
  std::string fontStyle;

  vtkMRMLAstroTwoDAxesDisplayableManager* External;

  qSlicerApplication* app;
};

//----------------------------------------------------------------------------
const double vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::COLOR_INVALID[3] = {1., 0.731, 0.078};

//---------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::vtkInternal(vtkMRMLAstroTwoDAxesDisplayableManager * external)
{
  this->External = external;
  this->RendererUpdateObserver = vtkSmartPointer<vtkAstroTwoDAxesRendererUpdateObserver>::New();
  this->RendererUpdateObserver->DisplayableManager = this->External;
  this->RendererUpdateObservationId = 0;
  this->ActorsAddedToRenderer = false;
  this->MarkerRenderer = vtkSmartPointer<vtkRenderer>::New();
  this->twoDAxesPoints = vtkSmartPointer<vtkPoints>::New();
  this->twoDAxesCellArray = vtkSmartPointer<vtkCellArray>::New();
  this->twoDAxesPolyData = vtkSmartPointer<vtkPolyData>::New();
  this->twoDAxesActor = vtkSmartPointer<vtkActor2D>::New();
  this->twoDAxesMapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
  this->col = vtkSmartPointer<vtkCollection>::New();
  this->app = 0;
  this->Color = vtkSmartPointer<vtkDoubleArray>::New();
  this->Color->SetNumberOfValues(3);
  this->Color->SetValue(0, COLOR_INVALID[0]);
  this->Color->SetValue(1, COLOR_INVALID[1]);
  this->Color->SetValue(2, COLOR_INVALID[2]);
  this->fontSize = 12;
  this->fontStyle = "Arial";
}

//---------------------------------------------------------------------------
vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::~vtkInternal()
{
  RemoveRendererUpdateObserver();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::ShowActors(bool show)
{
  if (this->ActorsAddedToRenderer == show)
    {
    // no change
    return;
    }
  if (show)
    {
    this->MarkerRenderer->AddViewProp(this->twoDAxesActor);
    }
  else
    {
    this->MarkerRenderer->RemoveViewProp(this->twoDAxesActor);
    }
  this->ActorsAddedToRenderer = show;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::AddRendererUpdateObserver(vtkRenderer* renderer)
{
  RemoveRendererUpdateObserver();
  if (renderer)
    {
    this->ObservedRenderer = renderer;
    this->RendererUpdateObservationId = renderer->AddObserver(vtkCommand::StartEvent, this->RendererUpdateObserver);
    }
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::RemoveRendererUpdateObserver()
{
  if (this->ObservedRenderer)
    {
    this->ObservedRenderer->RemoveObserver(this->RendererUpdateObservationId);
    this->RendererUpdateObservationId = 0;
    this->ObservedRenderer = NULL;
    }
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::SetupMarkerRenderer()
{
  vtkRenderer* renderer = this->External->GetRenderer();
  if (renderer==NULL)
    {
    vtkErrorWithObjectMacro(this->External, "vtkMRMLAstroTwoDAxesDisplayableManager"
                                            "::vtkInternal::SetupMarkerRenderer() failed: renderer is invalid");
    return;
    }

  this->MarkerRenderer->InteractiveOff();

  vtkRenderWindow* renderWindow = renderer->GetRenderWindow();
  if (renderWindow->GetNumberOfLayers() < RENDERER_LAYER+1)
    {
    renderWindow->SetNumberOfLayers( RENDERER_LAYER+1 );
    }
  this->MarkerRenderer->SetLayer(RENDERER_LAYER);
  renderWindow->AddRenderer(this->MarkerRenderer);
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::SetupAxes()
{
  this->twoDAxesActor->PickableOff();
  this->twoDAxesActor->DragableOff();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::UpdateAxes()
{
  // clean the attributes before updating
  this->twoDAxesPoints->Initialize();
  this->twoDAxesPoints->Squeeze();
  this->twoDAxesCellArray->Initialize();
  this->twoDAxesCellArray->Squeeze();
  vtkActor2DCollection* actors = this->MarkerRenderer->GetActors2D();
  actors->InitTraversal();
  for (int actorIndex = 0; actorIndex < actors->GetNumberOfItems(); actorIndex++)
    {
    vtkActor2D* actor2D = actors->GetNextActor2D();
    if (!actor2D)
      {
      continue;
      }
    if (actor2D->IsA("vtkTextActor"))
      {
      this->MarkerRenderer->RemoveViewProp(actor2D);
      }
    }

  vtkMRMLAbstractViewNode* viewNode =
      vtkMRMLAbstractViewNode::SafeDownCast
      (this->External->GetMRMLDisplayableNode());
  if (!viewNode || !viewNode->GetRulerEnabled())
    {
    vtkErrorWithObjectMacro(this->External,
                            "vtkMRMLAstroTwoDAxesDisplayableManager::UpdateAxes()"
                            " failed: view node is invalid.");
    this->ShowActors(false);
    return;
    }

  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(viewNode);
  if (!sliceNode)
    {
    vtkErrorWithObjectMacro(this->External,
                            "vtkMRMLAstroTwoDAxesDisplayableManager::UpdateAxes()"
                            " failed: displayable node is invalid.");
    this->ShowActors(false);
    return;
    }

  if (sliceNode->GetOrientation().compare("XY") &&
      sliceNode->GetOrientation().compare("ZY") &&
      sliceNode->GetOrientation().compare("XZ") &&
      sliceNode->GetOrientation().compare("PVMajor") &&
      sliceNode->GetOrientation().compare("PVMinor") &&
      sliceNode->GetOrientation().compare("Reformat"))
    {
    this->ShowActors(false);
    return;
    }

  int type = viewNode->GetRulerType();
  if (type == vtkMRMLAbstractViewNode::RulerTypeNone)
    {
    // ruler not visible, no updates are needed
    this->ShowActors(false);
    return;
    }

  int viewWidthPixel = sliceNode->GetDimensions()[0];
  int viewHeightPixel = sliceNode->GetDimensions()[1];
  // get the Logics
  this->app = qSlicerApplication::application();

  vtkMRMLSliceLogic* sliceLogic =
    this->app->applicationLogic()->GetSliceLogic(sliceNode);

  if (!sliceLogic)
    {
    vtkErrorWithObjectMacro(this->External,
                            "vtkMRMLAstroTwoDAxesDisplayableManager::UpdateAxes()"
                            " failed: sliceLogic node is invalid.");
    this->ShowActors(false);
    return;
    }

  this->col->AddItem(sliceLogic->GetBackgroundLayer());
  this->col->AddItem(sliceLogic->GetForegroundLayer());
  this->col->AddItem(sliceLogic->GetLabelLayer());

  bool hasDisplay = false;

  for (int layer = 0; layer < this->col->GetNumberOfItems(); layer++)
    {
    vtkMRMLSliceLayerLogic* sliceLayerLogic =
      vtkMRMLSliceLayerLogic::SafeDownCast
        (this->col->GetItemAsObject(layer));

    if(!sliceLayerLogic)
      {
      return;
      }

    int MinorTickMin = 0, MinorTickMax = 0, MajorTickMin = 0, MajorTickMax = 0;
    int internalFontSize = this->fontSize;
    switch (type)
      {
      case vtkMRMLAbstractViewNode::RulerTypeThin:
        MinorTickMin = 2;
        MinorTickMax = 7;
        MajorTickMin = 2;
        MajorTickMax = 12;
        break;
      case vtkMRMLAbstractViewNode::RulerTypeThick:
        MinorTickMin = 3;
        MinorTickMax = 9;
        MajorTickMin = 3;
        MajorTickMax = 14;
        internalFontSize *= 1.5;
        break;
      }

    vtkMRMLAstroVolumeDisplayNode* displayNode =
      vtkMRMLAstroVolumeDisplayNode::SafeDownCast
        (sliceLayerLogic->GetVolumeDisplayNode());

    vtkMRMLAstroLabelMapVolumeDisplayNode* displayLabelNode =
      vtkMRMLAstroLabelMapVolumeDisplayNode::SafeDownCast
        (sliceLayerLogic->GetVolumeDisplayNode());

    if (displayNode)
      {
      vtkMRMLAstroVolumeNode* astroVolume =
        vtkMRMLAstroVolumeNode::SafeDownCast
          (sliceLayerLogic->GetVolumeNode());

      if (astroVolume)
        {
        double CROTA1 = 0., CROTA2 = 0., CROTA3 = 0.;
        int N = StringToInt(astroVolume->GetAttribute("SlicerAstro.NAXIS"));
        if (N < 2)
          {
          CROTA1 = StringToDouble(astroVolume->GetAttribute("SlicerAstro.CROTA1"));
          }
        else if (N < 3)
          {
          CROTA1 = StringToDouble(astroVolume->GetAttribute("SlicerAstro.CROTA1"));
          CROTA2 = StringToDouble(astroVolume->GetAttribute("SlicerAstro.CROTA2"));
          }
        else
          {
          CROTA1 = StringToDouble(astroVolume->GetAttribute("SlicerAstro.CROTA1"));
          CROTA2 = StringToDouble(astroVolume->GetAttribute("SlicerAstro.CROTA2"));
          CROTA3 = StringToDouble(astroVolume->GetAttribute("SlicerAstro.CROTA3"));
          }

        if (fabs(CROTA1) > 1.E-6 || fabs(CROTA2) > 1.E-6 || fabs(CROTA3) > 1.E-6)
          {
          vtkWarningWithObjectMacro(this->External,
                                    "vtkMRMLAstroTwoDAxesDisplayableManager::UpdateAxes() : "
                                    "it is not possible to display WCS axes for rotated data (i.e., CROTAi != 0).");
          return;
          }
        }

      if (strcmp(displayNode->GetSpace(), "WCS") != 0)
        {
        continue;
        }

      hasDisplay = true;

      sliceNode->UpdateMatrices();
      sliceLayerLogic->UpdateTransforms();

      vtkGeneralTransform* xyToIJK =
          sliceLayerLogic->GetXYToIJKTransform();

      int numberOfPointsHorizontal = (int) (viewWidthPixel / 150.) + 1;
      if (numberOfPointsHorizontal < 5)
        {
        numberOfPointsHorizontal = 5;
        }
      int numberOfPointsVertical = (int) (viewHeightPixel / 150.) + 1;
      if (numberOfPointsVertical < 5)
        {
        numberOfPointsVertical = 5;
        }
      double worldA[] = {0.,0.,0.}, worldB[] = {0.,0.,0.}, worldC[] = {0.,0.,0.}, worldD[] = {0.,0.,0.};
      double xyz[] = {0.,0.,0.}, ijk[] = {0.,0.,0.};
      double axisCoord[] = {0.,0.}, wcsStep[] = {0.,0.};
      double PVwcsStepCos = 0., PVwcsStepSin = 0., PVAngle = 0.;
      std::vector<std::vector<double> > *world = new std::vector<std::vector<double> >();
      std::vector<std::vector<double> > *xyzDisplay = new std::vector<std::vector<double> >();
      std::vector<double> *temp = new std::vector<double>();

      // calculate WCS coordinates of the view's corners
      xyToIJK->TransformPoint(xyz, ijk);
      int controlY1 = ijk[1];
      displayNode->GetReferenceSpace(ijk, worldA);

      xyz[0] = viewWidthPixel;
      xyToIJK->TransformPoint(xyz, ijk);
      displayNode->GetReferenceSpace(ijk, worldB);

      xyz[0] = 0.;
      xyz[1] = viewHeightPixel;
      xyToIJK->TransformPoint(xyz, ijk);
      int controlY2 = ijk[1];
      // check if the reformat plane is parallel to the velocity axes
      bool showReformat = false;
      if (controlY1 == controlY2 && !sliceNode->GetOrientation().compare("Reformat"))
        {
        showReformat = true;
        }
      else if(!sliceNode->GetOrientation().compare("Reformat")    )
        {
        this->ShowActors(false);
        return;
        }

      displayNode->GetReferenceSpace(ijk, worldC);

      xyz[0] = viewWidthPixel / 2.;
      xyz[1] = 0.;
      xyToIJK->TransformPoint(xyz, ijk);
      displayNode->GetReferenceSpace(ijk, worldD);

      // calculate the wcsSteps for the two axes
      if (!sliceNode->GetOrientation().compare("ZY"))
        {
        wcsStep[0] = displayNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldB[2]), &numberOfPointsHorizontal);
        axisCoord[0] = displayNode->GetFirstWcsTickAxisZ(worldA[2], worldB[2], wcsStep[0]);
        wcsStep[1] = displayNode->GetWcsTickStepAxisY(fabs(worldA[1] - worldC[1]), &numberOfPointsVertical);
        axisCoord[1] = displayNode->GetFirstWcsTickAxisY(worldA[1], worldC[1], wcsStep[1]);
        }

      if (!sliceNode->GetOrientation().compare("XY"))
        {
        wcsStep[0] = displayNode->GetWcsTickStepAxisX(fabs(worldA[0] - worldB[0]), &numberOfPointsHorizontal);
        axisCoord[0] = displayNode->GetFirstWcsTickAxisX(worldA[0], worldB[0], wcsStep[0]);
        wcsStep[1] = displayNode->GetWcsTickStepAxisY(fabs(worldA[1] - worldC[1]), &numberOfPointsVertical);
        axisCoord[1] = displayNode->GetFirstWcsTickAxisY(worldA[1], worldC[1], wcsStep[1]);
        }

      if (!sliceNode->GetOrientation().compare("XZ"))
        {
        wcsStep[0] = displayNode->GetWcsTickStepAxisX(fabs(worldA[0] - worldB[0]), &numberOfPointsHorizontal);
        axisCoord[0] = displayNode->GetFirstWcsTickAxisX(worldA[0], worldB[0], wcsStep[0]);
        wcsStep[1] = displayNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldC[2]), &numberOfPointsVertical);
        axisCoord[1] = displayNode->GetFirstWcsTickAxisZ(worldA[2], worldC[2], wcsStep[1]);
        }

      if (!sliceNode->GetOrientation().compare("PVMajor") ||
          !sliceNode->GetOrientation().compare("PVMinor") ||
          showReformat)
        {
        double distX = (worldA[0] - worldB[0]);
        double distY = (worldA[1] - worldB[1]);
        double dist = sqrt((distX * distX) + (distY * distY));
        wcsStep[0] = displayNode->GetWcsTickStepAxisY(dist, &numberOfPointsHorizontal);

        PVAngle = atan(distY / distX);
        PVwcsStepCos = wcsStep[0] * cos(PVAngle);
        PVwcsStepSin = wcsStep[0] * sin(PVAngle);

        if (numberOfPointsHorizontal % 2 != 0)
          {
          numberOfPointsHorizontal += 1;
          }
        wcsStep[1] = displayNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldC[2]), &numberOfPointsVertical);
        axisCoord[1] = displayNode->GetFirstWcsTickAxisZ(worldA[2], worldC[2], wcsStep[1]);
        }

      // allocate point along the horizontal axes
      for (int i = 0; i < numberOfPointsHorizontal; i++)
        {
        int i8 = i * 8;
        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          temp->clear();
          temp->push_back(worldA[0]);
          temp->push_back(worldA[1]);
          temp->push_back(axisCoord[0] + wcsStep[0] * i);
          world->push_back((*temp));
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          temp->clear();
          temp->push_back(axisCoord[0] + wcsStep[0] * i);
          temp->push_back(worldA[1]);
          temp->push_back(worldA[2]);
          world->push_back((*temp));
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          temp->clear();
          temp->push_back(worldD[0] + PVwcsStepCos  * (i - (numberOfPointsHorizontal / 2.)));
          temp->push_back(worldD[1] + PVwcsStepSin  * (i - (numberOfPointsHorizontal / 2.)));
          temp->push_back(worldD[2]);
          world->push_back((*temp));
          }

        displayNode->GetIJKSpace((*world)[i], ijk);
        xyToIJK->Inverse();
        xyToIJK->TransformPoint(ijk, xyz);
        temp->clear();
        temp->push_back(xyz[0]);
        temp->push_back(xyz[1]);
        temp->push_back(xyz[2]);
        xyzDisplay->push_back((*temp));
        this->twoDAxesPoints->InsertPoint(i8, xyz[0], MajorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 1, xyz[0], MajorTickMax, 0);

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 4.);
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 4.);
          xyz[1] = worldA[1];
          xyz[2] = worldA[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldD[0] + PVwcsStepCos * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepCos / 4.;
          xyz[1] = worldD[1] + PVwcsStepSin * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepSin / 4.;
          xyz[2] = worldD[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 2, xyz[0], MinorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 3, xyz[0], MinorTickMax, 0);

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 2.);
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 2.);
          xyz[1] = worldA[1];
          xyz[2] = worldA[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldD[0] + PVwcsStepCos * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepCos / 2.;
          xyz[1] = worldD[1] + PVwcsStepSin * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepSin / 2.;
          xyz[2] = worldD[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 4, xyz[0], MinorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 5, xyz[0], MinorTickMax, 0);

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] * 3. / 4.);
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] * 3. / 4.);
          xyz[1] = worldA[1];
          xyz[2] = worldA[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldD[0] + PVwcsStepCos * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepCos * 3. / 4.;
          xyz[1] = worldD[1] + PVwcsStepSin * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepSin * 3. / 4.;
          xyz[2] = worldD[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 6, xyz[0], MinorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 7, xyz[0], MinorTickMax, 0);

        xyToIJK->Inverse();
        }

      int nTot = numberOfPointsVertical + numberOfPointsHorizontal;

      // allocate point along the vertical axes
      for (int i = numberOfPointsHorizontal; i < nTot; i++)
        {
        int ii = i - numberOfPointsHorizontal;
        int i8 = i * 8;

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          temp->clear();
          temp->push_back(worldA[0]);
          temp->push_back(axisCoord[1] + wcsStep[1] * ii);
          temp->push_back(worldA[2]);
          world->push_back((*temp));
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          temp->clear();
          temp->push_back(worldA[0]);
          temp->push_back(worldA[1]);
          temp->push_back(axisCoord[1] + wcsStep[1] * ii);
          world->push_back((*temp));
          }

        displayNode->GetIJKSpace((*world)[i], ijk);
        xyToIJK->Inverse();
        xyToIJK->TransformPoint(ijk, xyz);
        temp->clear();
        temp->push_back(xyz[0]);
        temp->push_back(xyz[1]);
        temp->push_back(xyz[2]);
        xyzDisplay->push_back((*temp));

        this->twoDAxesPoints->InsertPoint(i8, MajorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 1, MajorTickMax, xyz[1], 0);

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 4.);
          xyz[2] = worldA[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 4.);
          displayNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 2, MinorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 3, MinorTickMax, xyz[1], 0);

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 2.);
          xyz[2] = worldA[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 2.);
          displayNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 4, MinorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 5, MinorTickMax, xyz[1], 0);

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] * 3. / 4.);
          xyz[2] = worldA[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] * 3. / 4.);
          displayNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 6, MinorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 7, MinorTickMax, xyz[1], 0);

        xyToIJK->Inverse();
        }

      int n = this->twoDAxesPoints->GetNumberOfPoints();

      // unify the points with lines
      std::vector<vtkSmartPointer<vtkLine> > lines;
      for (int i = 0; i < n - 1; i++)
        {
        vtkSmartPointer<vtkLine> line = vtkSmartPointer<vtkLine>::New();
        lines.push_back(line);
        }

      int nHori8 = numberOfPointsHorizontal * 8;
      for (int i = 0; i < nHori8 - 1; i++)
        {
        if (i%2 == 0)
          {
          lines[i]->GetPointIds()->SetId(0, i);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        else
          {
          lines[i]->GetPointIds()->SetId(0, i - 1);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        }

      int nVert8 = numberOfPointsVertical * 8;
      int nTot8 = nHori8 + nVert8;
      for (int i = nHori8; i < nTot8 - 1; i++)
        {
        if (i%2 == 0)
          {
          lines[i]->GetPointIds()->SetId(0, i);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        else
          {
          lines[i]->GetPointIds()->SetId(0, i - 1);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        }

      // create the cellArray
      for (int i = 0; i < n - 1; i++)
        {
        this->twoDAxesCellArray->InsertNextCell(lines[i]);
        }

      // setup the mapper and actor
      this->twoDAxesPolyData->SetPoints(this->twoDAxesPoints);
      this->twoDAxesPolyData->SetLines(this->twoDAxesCellArray);
      this->twoDAxesMapper->SetInputData(this->twoDAxesPolyData);
      this->twoDAxesActor->SetMapper(this->twoDAxesMapper);
      switch (type)
        {
        case vtkMRMLAbstractViewNode::RulerTypeThin:
          this->twoDAxesActor->GetProperty()->SetLineWidth(1);
          break;
        case vtkMRMLAbstractViewNode::RulerTypeThick:
          this->twoDAxesActor->GetProperty()->SetLineWidth(3);
          break;
        default:
          break;
        }
      this->twoDAxesActor->GetProperty()->SetColor(this->Color->GetValue(0),
                                                   this->Color->GetValue(1),
                                                   this->Color->GetValue(2));
      this->MarkerRenderer->AddActor2D(this->twoDAxesActor);


      std::string coord;
      double outputHorizontalValues[3] = {0.}, oldOutputHorizontalValues[3] = {0.};

      // allocate 2DTextActors for the horizontal axes
      for (int i = 0; i < numberOfPointsHorizontal; i++)
        {
        if((*xyzDisplay)[i][0] < 30 || (*xyzDisplay)[i][0] > viewWidthPixel - 30)
          {
          continue;
          }

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          coord = displayNode->GetDisplayStringFromValueZ((*world)[i][2],
                                                           oldOutputHorizontalValues,
                                                           outputHorizontalValues, 0, true);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          coord = displayNode->GetDisplayStringFromValueX((*world)[i][0],
                                                           oldOutputHorizontalValues,
                                                           outputHorizontalValues, 0, true);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          double dist = sqrt((((*world)[i][0] - worldD[0]) * ((*world)[i][0] - worldD[0])) +
                            (((*world)[i][1] - worldD[1]) * ((*world)[i][1] - worldD[1])));
          coord = displayNode->GetDisplayStringFromValueY(dist,
                                                          oldOutputHorizontalValues,
                                                          outputHorizontalValues, 0, true);
          }

        oldOutputHorizontalValues[0] = outputHorizontalValues[0];
        oldOutputHorizontalValues[1] = outputHorizontalValues[1];
        oldOutputHorizontalValues[2] = outputHorizontalValues[2];

        vtkSmartPointer<vtkTextActor> textActorHorizontal = vtkSmartPointer<vtkTextActor>::New();
        vtkTextProperty* textProperty = textActorHorizontal->GetTextProperty();
        textProperty->SetFontSize(internalFontSize);

        if (!fontStyle.compare("Arial"))
          {
          textProperty->SetFontFamilyToArial();
          }
        else if (!fontStyle.compare("Courier"))
          {
          textProperty->SetFontFamilyToCourier();
          }
        else if (!fontStyle.compare("Times"))
          {
          textProperty->SetFontFamilyToTimes();
          }
        else
          {
          textProperty->SetFontFamilyToArial();
          }

        textActorHorizontal->GetProperty()->SetColor(this->Color->GetValue(0),
                                                     this->Color->GetValue(1),
                                                     this->Color->GetValue(2));
        textActorHorizontal->SetInput(coord.c_str());

        textActorHorizontal->SetDisplayPosition((int) ((*xyzDisplay)[i][0] - (internalFontSize * 2)), 15);

        this->MarkerRenderer->AddActor2D(textActorHorizontal);
        }

      double outputVerticalValues[3] = {0.}, oldOutputVerticalValues[3] = {0.};

      // allocate 2DTextActors for the vertical axes
      for (int i = numberOfPointsHorizontal; i < nTot; i++)
        {
        if((*xyzDisplay)[i][1] < 50 || (*xyzDisplay)[i][1] > viewHeightPixel)
          {
          continue;
          }

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          coord = displayNode->GetDisplayStringFromValueY((*world)[i][1],
                                                           oldOutputVerticalValues,
                                                           outputVerticalValues, 0);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          coord = displayNode->GetDisplayStringFromValueZ((*world)[i][2],
                                                           oldOutputVerticalValues,
                                                           outputVerticalValues, 0);
          }

        oldOutputVerticalValues[0] = outputVerticalValues[0];
        oldOutputVerticalValues[1] = outputVerticalValues[1];
        oldOutputVerticalValues[2] = outputVerticalValues[2];

        vtkSmartPointer<vtkTextActor> textActorVertical = vtkSmartPointer<vtkTextActor>::New();
        vtkTextProperty* textProperty = textActorVertical->GetTextProperty();
        textProperty->SetFontSize(internalFontSize);

        if (!fontStyle.compare("Arial"))
          {
          textProperty->SetFontFamilyToArial();
          }
        else if (!fontStyle.compare("Courier"))
          {
          textProperty->SetFontFamilyToCourier();
          }
        else if (!fontStyle.compare("Times"))
          {
          textProperty->SetFontFamilyToTimes();
          }
        else
          {
          textProperty->SetFontFamilyToArial();
          }

        textActorVertical->GetProperty()->SetColor(this->Color->GetValue(0),
                                                   this->Color->GetValue(1),
                                                   this->Color->GetValue(2));
        textActorVertical->SetInput(coord.c_str());
        textActorVertical->SetDisplayPosition(20, (int) ((*xyzDisplay)[i][1]- (internalFontSize * 0.5)));
        this->MarkerRenderer->AddActor2D(textActorVertical);
        }

      world->clear();
      xyzDisplay->clear();
      temp->clear();
      delete world;
      delete xyzDisplay;
      delete temp;

      this->ShowActors(true);
      break;
      }
    else if (displayLabelNode)
      {
      if (strcmp(displayLabelNode->GetSpace(), "WCS") != 0)
        {
        continue;
        }

      vtkMRMLAstroLabelMapVolumeNode* astroLabelVolume =
        vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
          (sliceLayerLogic->GetVolumeNode());

      if (astroLabelVolume)
        {
        double CROTA1 = 0., CROTA2 = 0., CROTA3 = 0.;
        int N = StringToInt(astroLabelVolume->GetAttribute("SlicerAstro.NAXIS"));
        if (N < 2)
          {
          CROTA1 = StringToDouble(astroLabelVolume->GetAttribute("SlicerAstro.CROTA1"));
          }
        else if (N < 3)
          {
          CROTA1 = StringToDouble(astroLabelVolume->GetAttribute("SlicerAstro.CROTA1"));
          CROTA2 = StringToDouble(astroLabelVolume->GetAttribute("SlicerAstro.CROTA2"));
          }
        else
          {
          CROTA1 = StringToDouble(astroLabelVolume->GetAttribute("SlicerAstro.CROTA1"));
          CROTA2 = StringToDouble(astroLabelVolume->GetAttribute("SlicerAstro.CROTA2"));
          CROTA3 = StringToDouble(astroLabelVolume->GetAttribute("SlicerAstro.CROTA3"));
          }

        if (fabs(CROTA1) > 1.E-6 || fabs(CROTA2) > 1.E-6 || fabs(CROTA3) > 1.E-6)
          {
          vtkWarningWithObjectMacro(this->External,
                                    "vtkMRMLAstroTwoDAxesDisplayableManager::UpdateAxes() : "
                                    "it is not possible to display WCS axes for rotated data (i.e., CROTAi != 0).");
          return;
          }
        }

      hasDisplay = true;

      sliceNode->UpdateMatrices();
      sliceLayerLogic->UpdateTransforms();

      vtkGeneralTransform* xyToIJK =
          sliceLayerLogic->GetXYToIJKTransform();

      int numberOfPointsHorizontal = (int) (viewWidthPixel / 150.) + 1;
      if (numberOfPointsHorizontal < 5)
        {
        numberOfPointsHorizontal = 5;
        }
      int numberOfPointsVertical = (int) (viewHeightPixel / 150.) + 1;
      if (numberOfPointsVertical < 5)
        {
        numberOfPointsVertical = 5;
        }
      double worldA[] = {0.,0.,0.}, worldB[] = {0.,0.,0.}, worldC[] = {0.,0.,0.}, worldD[] = {0.,0.,0.};
      double xyz[] = {0.,0.,0.}, ijk[] = {0.,0.,0.};
      double axisCoord[] = {0.,0.}, wcsStep[] = {0.,0.};
      double PVwcsStepCos = 0., PVwcsStepSin = 0., PVAngle = 0.;
      std::vector<std::vector<double> > *world = new std::vector<std::vector<double> >();
      std::vector<std::vector<double> > *xyzDisplay = new std::vector<std::vector<double> >();
      std::vector<double> *temp = new std::vector<double>();

      // calculate WCS coordinates of the view's corners
      xyToIJK->TransformPoint(xyz, ijk);
      int controlY1 = ijk[1];
      displayLabelNode->GetReferenceSpace(ijk, worldA);

      xyz[0] = viewWidthPixel;
      xyToIJK->TransformPoint(xyz, ijk);
      displayLabelNode->GetReferenceSpace(ijk, worldB);

      xyz[0] = 0.;
      xyz[1] = viewHeightPixel;
      xyToIJK->TransformPoint(xyz, ijk);
      int controlY2 = ijk[1];
      // check if the reformat plane is parallel to the velocity axes
      bool showReformat = false;
      if (controlY1 == controlY2 && !sliceNode->GetOrientation().compare("Reformat"))
        {
        showReformat = true;
        }
      else if(!sliceNode->GetOrientation().compare("Reformat")    )
        {
        this->ShowActors(false);
        return;
        }

      displayLabelNode->GetReferenceSpace(ijk, worldC);

      xyz[0] = viewWidthPixel / 2.;
      xyz[1] = 0.;
      xyToIJK->TransformPoint(xyz, ijk);
      displayLabelNode->GetReferenceSpace(ijk, worldD);

      // calculate the wcsSteps for the two axes
      if (!sliceNode->GetOrientation().compare("ZY"))
        {
        wcsStep[0] = displayLabelNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldB[2]), &numberOfPointsHorizontal);
        axisCoord[0] = displayLabelNode->GetFirstWcsTickAxisZ(worldA[2], worldB[2], wcsStep[0]);
        wcsStep[1] = displayLabelNode->GetWcsTickStepAxisY(fabs(worldA[1] - worldC[1]), &numberOfPointsVertical);
        axisCoord[1] = displayLabelNode->GetFirstWcsTickAxisY(worldA[1], worldC[1], wcsStep[1]);
        }

      if (!sliceNode->GetOrientation().compare("XY"))
        {
        wcsStep[0] = displayLabelNode->GetWcsTickStepAxisX(fabs(worldA[0] - worldB[0]), &numberOfPointsHorizontal);
        axisCoord[0] = displayLabelNode->GetFirstWcsTickAxisX(worldA[0], worldB[0], wcsStep[0]);
        wcsStep[1] = displayLabelNode->GetWcsTickStepAxisY(fabs(worldA[1] - worldC[1]), &numberOfPointsVertical);
        axisCoord[1] = displayLabelNode->GetFirstWcsTickAxisY(worldA[1], worldC[1], wcsStep[1]);
        }

      if (!sliceNode->GetOrientation().compare("XZ"))
        {
        wcsStep[0] = displayLabelNode->GetWcsTickStepAxisX(fabs(worldA[0] - worldB[0]), &numberOfPointsHorizontal);
        axisCoord[0] = displayLabelNode->GetFirstWcsTickAxisX(worldA[0], worldB[0], wcsStep[0]);
        wcsStep[1] = displayLabelNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldC[2]), &numberOfPointsVertical);
        axisCoord[1] = displayLabelNode->GetFirstWcsTickAxisZ(worldA[2], worldC[2], wcsStep[1]);
        }

      if (!sliceNode->GetOrientation().compare("PVMajor") ||
          !sliceNode->GetOrientation().compare("PVMinor") ||
          showReformat)
        {
        double distX = (worldA[0] - worldB[0]);
        double distY = (worldA[1] - worldB[1]);
        double dist = sqrt((distX * distX) + (distY * distY));
        wcsStep[0] = displayLabelNode->GetWcsTickStepAxisY(dist, &numberOfPointsHorizontal);

        PVAngle = atan(distY / distX);
        PVwcsStepCos = wcsStep[0] * cos(PVAngle);
        PVwcsStepSin = wcsStep[0] * sin(PVAngle);

        if (numberOfPointsHorizontal % 2 != 0)
          {
          numberOfPointsHorizontal += 1;
          }
        wcsStep[1] = displayLabelNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldC[2]), &numberOfPointsVertical);
        axisCoord[1] = displayLabelNode->GetFirstWcsTickAxisZ(worldA[2], worldC[2], wcsStep[1]);
        }

      // allocate point along the horizontal axes
      for (int i = 0; i < numberOfPointsHorizontal; i++)
        {
        int i8 = i * 8;
        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          temp->clear();
          temp->push_back(worldA[0]);
          temp->push_back(worldA[1]);
          temp->push_back(axisCoord[0] + wcsStep[0] * i);
          world->push_back((*temp));
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          temp->clear();
          temp->push_back(axisCoord[0] + wcsStep[0] * i);
          temp->push_back(worldA[1]);
          temp->push_back(worldA[2]);
          world->push_back((*temp));
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          temp->clear();
          temp->push_back(worldD[0] + PVwcsStepCos  * (i - (numberOfPointsHorizontal / 2.)));
          temp->push_back(worldD[1] + PVwcsStepSin  * (i - (numberOfPointsHorizontal / 2.)));
          temp->push_back(worldD[2]);
          world->push_back((*temp));
          }

        displayLabelNode->GetIJKSpace((*world)[i], ijk);
        xyToIJK->Inverse();
        xyToIJK->TransformPoint(ijk, xyz);
        temp->clear();
        temp->push_back(xyz[0]);
        temp->push_back(xyz[1]);
        temp->push_back(xyz[2]);
        xyzDisplay->push_back((*temp));
        this->twoDAxesPoints->InsertPoint(i8, xyz[0], MajorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 1, xyz[0], MajorTickMax, 0);

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 4.);
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 4.);
          xyz[1] = worldA[1];
          xyz[2] = worldA[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldD[0] + PVwcsStepCos * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepCos / 4.;
          xyz[1] = worldD[1] + PVwcsStepSin * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepSin / 4.;
          xyz[2] = worldD[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 2, xyz[0], MinorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 3, xyz[0], MinorTickMax, 0);

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 2.);
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 2.);
          xyz[1] = worldA[1];
          xyz[2] = worldA[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldD[0] + PVwcsStepCos * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepCos / 2.;
          xyz[1] = worldD[1] + PVwcsStepSin * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepSin / 2.;
          xyz[2] = worldD[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 4, xyz[0], MinorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 5, xyz[0], MinorTickMax, 0);

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] * 3. / 4.);
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] * 3. / 4.);
          xyz[1] = worldA[1];
          xyz[2] = worldA[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldD[0] + PVwcsStepCos * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepCos * 3. / 4.;
          xyz[1] = worldD[1] + PVwcsStepSin * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepSin * 3. / 4.;
          xyz[2] = worldD[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 6, xyz[0], MinorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 7, xyz[0], MinorTickMax, 0);

        xyToIJK->Inverse();
        }

      int nTot = numberOfPointsVertical + numberOfPointsHorizontal;

      // allocate point along the vertical axes
      for (int i = numberOfPointsHorizontal; i < nTot; i++)
        {
        int ii = i - numberOfPointsHorizontal;
        int i8 = i * 8;

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          temp->clear();
          temp->push_back(worldA[0]);
          temp->push_back(axisCoord[1] + wcsStep[1] * ii);
          temp->push_back(worldA[2]);
          world->push_back((*temp));
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          temp->clear();
          temp->push_back(worldA[0]);
          temp->push_back(worldA[1]);
          temp->push_back(axisCoord[1] + wcsStep[1] * ii);
          world->push_back((*temp));
          }

        displayLabelNode->GetIJKSpace((*world)[i], ijk);
        xyToIJK->Inverse();
        xyToIJK->TransformPoint(ijk, xyz);
        temp->clear();
        temp->push_back(xyz[0]);
        temp->push_back(xyz[1]);
        temp->push_back(xyz[2]);
        xyzDisplay->push_back((*temp));

        this->twoDAxesPoints->InsertPoint(i8, MajorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 1, MajorTickMax, xyz[1], 0);

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 4.);
          xyz[2] = worldA[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 4.);
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 2, MinorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 3, MinorTickMax, xyz[1], 0);

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 2.);
          xyz[2] = worldA[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 2.);
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 4, MinorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 5, MinorTickMax, xyz[1], 0);

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] * 3. / 4.);
          xyz[2] = worldA[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] * 3. / 4.);
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 6, MinorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 7, MinorTickMax, xyz[1], 0);

        xyToIJK->Inverse();
        }

      int n = this->twoDAxesPoints->GetNumberOfPoints();

      // unify the points with lines
      std::vector<vtkSmartPointer<vtkLine> > lines;
      for (int i = 0; i < n - 1; i++)
        {
        vtkSmartPointer<vtkLine> line = vtkSmartPointer<vtkLine>::New();
        lines.push_back(line);
        }

      int nHori8 = numberOfPointsHorizontal * 8;
      for (int i = 0; i < nHori8 - 1; i++)
        {
        if (i%2 == 0)
          {
          lines[i]->GetPointIds()->SetId(0, i);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        else
          {
          lines[i]->GetPointIds()->SetId(0, i - 1);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        }

      int nVert8 = numberOfPointsVertical * 8;
      int nTot8 = nHori8 + nVert8;
      for (int i = nHori8; i < nTot8 - 1; i++)
        {
        if (i%2 == 0)
          {
          lines[i]->GetPointIds()->SetId(0, i);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        else
          {
          lines[i]->GetPointIds()->SetId(0, i - 1);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        }

      // create the cellArray
      for (int i = 0; i < n - 1; i++)
        {
        this->twoDAxesCellArray->InsertNextCell(lines[i]);
        }

      // setup the mapper and actor
      this->twoDAxesPolyData->SetPoints(this->twoDAxesPoints);
      this->twoDAxesPolyData->SetLines(this->twoDAxesCellArray);
      this->twoDAxesMapper->SetInputData(this->twoDAxesPolyData);
      this->twoDAxesActor->SetMapper(this->twoDAxesMapper);
      switch (type)
        {
        case vtkMRMLAbstractViewNode::RulerTypeThin:
          this->twoDAxesActor->GetProperty()->SetLineWidth(1);
          break;
        case vtkMRMLAbstractViewNode::RulerTypeThick:
          this->twoDAxesActor->GetProperty()->SetLineWidth(3);
          break;
        default:
          break;
        }
      this->twoDAxesActor->GetProperty()->SetColor(this->Color->GetValue(0),
                                                   this->Color->GetValue(1),
                                                   this->Color->GetValue(2));
      this->MarkerRenderer->AddActor2D(this->twoDAxesActor);


      std::string coord;
      double outputHorizontalValues[3] = {0.}, oldOutputHorizontalValues[3] = {0.};

      // allocate 2DTextActors for the horizontal axes
      for (int i = 0; i < numberOfPointsHorizontal; i++)
        {
        if((*xyzDisplay)[i][0] < 30 || (*xyzDisplay)[i][0] > viewWidthPixel - 30)
          {
          continue;
          }

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          coord = displayLabelNode->GetDisplayStringFromValueZ((*world)[i][2],
                                                           oldOutputHorizontalValues,
                                                           outputHorizontalValues, 0, true);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          coord = displayLabelNode->GetDisplayStringFromValueX((*world)[i][0],
                                                           oldOutputHorizontalValues,
                                                           outputHorizontalValues, 0, true);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          double dist = sqrt((((*world)[i][0] - worldD[0]) * ((*world)[i][0] - worldD[0])) +
                            (((*world)[i][1] - worldD[1]) * ((*world)[i][1] - worldD[1])));
          coord = displayLabelNode->GetDisplayStringFromValueY(dist,
                                                          oldOutputHorizontalValues,
                                                          outputHorizontalValues, 0, true);
          }

        oldOutputHorizontalValues[0] = outputHorizontalValues[0];
        oldOutputHorizontalValues[1] = outputHorizontalValues[1];
        oldOutputHorizontalValues[2] = outputHorizontalValues[2];

        vtkSmartPointer<vtkTextActor> textActorHorizontal = vtkSmartPointer<vtkTextActor>::New();
        vtkTextProperty* textProperty = textActorHorizontal->GetTextProperty();
        textProperty->SetFontSize(internalFontSize);

        if (!fontStyle.compare("Arial"))
          {
          textProperty->SetFontFamilyToArial();
          }
        else if (!fontStyle.compare("Courier"))
          {
          textProperty->SetFontFamilyToCourier();
          }
        else if (!fontStyle.compare("Times"))
          {
          textProperty->SetFontFamilyToTimes();
          }
        else
          {
          textProperty->SetFontFamilyToArial();
          }

        textActorHorizontal->GetProperty()->SetColor(this->Color->GetValue(0),
                                                     this->Color->GetValue(1),
                                                     this->Color->GetValue(2));
        textActorHorizontal->SetInput(coord.c_str());

        textActorHorizontal->SetDisplayPosition((int) ((*xyzDisplay)[i][0] - (internalFontSize * 2)), 15);

        this->MarkerRenderer->AddActor2D(textActorHorizontal);
        }

      double outputVerticalValues[3] = {0.}, oldOutputVerticalValues[3] = {0.};

      // allocate 2DTextActors for the vertical axes
      for (int i = numberOfPointsHorizontal; i < nTot; i++)
        {
        if((*xyzDisplay)[i][1] < 50 || (*xyzDisplay)[i][1] > viewHeightPixel)
          {
          continue;
          }

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          coord = displayLabelNode->GetDisplayStringFromValueY((*world)[i][1],
                                                           oldOutputVerticalValues,
                                                           outputVerticalValues, 0);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          coord = displayLabelNode->GetDisplayStringFromValueZ((*world)[i][2],
                                                           oldOutputVerticalValues,
                                                           outputVerticalValues, 0);
          }

        oldOutputVerticalValues[0] = outputVerticalValues[0];
        oldOutputVerticalValues[1] = outputVerticalValues[1];
        oldOutputVerticalValues[2] = outputVerticalValues[2];

        vtkSmartPointer<vtkTextActor> textActorVertical = vtkSmartPointer<vtkTextActor>::New();
        vtkTextProperty* textProperty = textActorVertical->GetTextProperty();
        textProperty->SetFontSize(internalFontSize);

        if (!fontStyle.compare("Arial"))
          {
          textProperty->SetFontFamilyToArial();
          }
        else if (!fontStyle.compare("Courier"))
          {
          textProperty->SetFontFamilyToCourier();
          }
        else if (!fontStyle.compare("Times"))
          {
          textProperty->SetFontFamilyToTimes();
          }
        else
          {
          textProperty->SetFontFamilyToArial();
          }

        textActorVertical->GetProperty()->SetColor(this->Color->GetValue(0),
                                                   this->Color->GetValue(1),
                                                   this->Color->GetValue(2));
        textActorVertical->SetInput(coord.c_str());
        textActorVertical->SetDisplayPosition(20, (int) ((*xyzDisplay)[i][1]- (internalFontSize * 0.5)));
        this->MarkerRenderer->AddActor2D(textActorVertical);
        }

      world->clear();
      xyzDisplay->clear();
      temp->clear();
      delete world;
      delete xyzDisplay;
      delete temp;

      this->ShowActors(true);
      break;
      }
    }

  this->col->RemoveAllItems();

  if (!hasDisplay)
    {
    this->ShowActors(false);
    }
}

//---------------------------------------------------------------------------
// vtkMRMLRulerDisplayableManager methods

//---------------------------------------------------------------------------
vtkMRMLAstroTwoDAxesDisplayableManager::vtkMRMLAstroTwoDAxesDisplayableManager()
{
  this->Internal = new vtkInternal(this);
}

//---------------------------------------------------------------------------
vtkMRMLAstroTwoDAxesDisplayableManager::~vtkMRMLAstroTwoDAxesDisplayableManager()
{
  delete this->Internal;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
vtkRenderer *vtkMRMLAstroTwoDAxesDisplayableManager::vtkMarkerRenderer()
{
  return this->Internal->MarkerRenderer;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::SetAnnotationsColor(double red,
                                                                 double green,
                                                                 double blue)
{
  this->Internal->Color->SetValue(0, red);
  this->Internal->Color->SetValue(1, green);
  this->Internal->Color->SetValue(2, blue);
  this->Internal->UpdateAxes();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::SetAnnotationsFontStyle(const char *font)
{
  if (!font)
    {
    return;
    }

  this->Internal->fontStyle = font;
  this->Internal->UpdateAxes();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::SetAnnotationsFontSize(int size)
{
  this->Internal->fontSize = size;
  this->Internal->UpdateAxes();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::Create()
{
  this->Internal->SetupMarkerRenderer();
  this->Internal->SetupAxes();
  this->Superclass::Create();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::UpdateFromViewNode()
{
  // View node is changed, which may mean that either the marker type (visibility), size, or orientation is changed
  this->Internal->UpdateAxes();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::OnMRMLDisplayableNodeModifiedEvent(vtkObject* vtkNotUsed(caller))
{
  // view node is changed
  this->UpdateFromViewNode();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::UpdateFromRenderer()
{
  // Rendering is performed, so let's re-render the marker with up-to-date orientation
  this->Internal->UpdateAxes();
}
