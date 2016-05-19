// MRMLDisplayableManager includes
#include "vtkMRMLAstroTwoDAxesDisplayableManager.h"

// MRML includes
#include <vtkMRMLAbstractViewNode.h>
#include <vtkMRMLLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLViewNode.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceLayerLogic.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>

// VTK includes
#include <vtkActor2DCollection.h>
#include <vtkActor2D.h>
#include <vtkAxisActor2D.h>
#include <vtkCamera.h>
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
#include <vtkLine.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtksys/SystemTools.hxx>
#include <vtkCellArray.h>
#include <vtkGeneralTransform.h>
#include <vtkPolyDataMapper2D.h>

// STD includes
#include <sstream>

// SlicerQt includes
#include <qSlicerApplication.h>

// vtkSlicer includes
#include <vtkSlicerApplicationLogic.h>

// Constants
static const int RENDERER_LAYER = 1; // layer ID where the orientation marker will be displayed

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

  vtkMRMLAstroTwoDAxesDisplayableManager* External;

  qSlicerApplication* app;
};

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
  vtkActor2DCollection* actors = this->MarkerRenderer->GetActors2D() ;
  actors->InitTraversal();
  for (int i = 0; i < actors->GetNumberOfItems(); i++)
    {
    vtkActor2D* actor2D = actors->GetNextActor2D();
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

  int type = viewNode->GetRulerType();
  if (type == vtkMRMLAbstractViewNode::RulerTypeNone)
    {
    // ruler not visible, no updates are needed
    this->ShowActors(false);
    return;
    }

  int viewWidthPixel = 0;
  int viewHeightPixel = 0;

  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(viewNode);
  if (sliceNode)
    {
    viewWidthPixel = sliceNode->GetDimensions()[0];
    viewHeightPixel = sliceNode->GetDimensions()[1];

    }
  else
    {
    vtkErrorWithObjectMacro(this->External,
                            "vtkMRMLAstroTwoDAxesDisplayableManager::UpdateAxes()"
                            " failed: displayable node is invalid.");
    this->ShowActors(false);
    return;
    }

  // get the Logics
  this->app = qSlicerApplication::application();

  vtkMRMLSliceLogic* sliceLogic =
    this->app->applicationLogic()->GetSliceLogic(sliceNode);

  bool hasDisplay = false;

  if (!sliceLogic)
    {
    vtkErrorWithObjectMacro(this->External,
                            "vtkMRMLAstroTwoDAxesDisplayableManager::UpdateAxes()"
                            " failed: sliceLogic node is invalid.");
    }

  this->col->AddItem(sliceLogic->GetBackgroundLayer());
  this->col->AddItem(sliceLogic->GetForegroundLayer());
  this->col->AddItem(sliceLogic->GetLabelLayer());

  for (int i = 0; i < this->col->GetNumberOfItems(); i++)
    {
    vtkMRMLSliceLayerLogic* sliceLayerLogic =
      vtkMRMLSliceLayerLogic::SafeDownCast
        (this->col->GetItemAsObject(i));

    vtkMRMLAstroVolumeDisplayNode* displayNode =
      vtkMRMLAstroVolumeDisplayNode::SafeDownCast
        (sliceLayerLogic->GetVolumeDisplayNode());

    if (!displayNode)
      {
      continue;
      }
    else
      {
      hasDisplay = true;
      if (!strcmp(displayNode->GetSpace(), "WCS"))
        {

        sliceNode->UpdateMatrices();
        sliceLayerLogic->UpdateTransforms();

        vtkGeneralTransform* xyToIJK =
          sliceLayerLogic->GetXYToIJKTransform();

        int numberOfPointsHorizontal = (int) (viewWidthPixel / 120.) + 1;
        int numberOfPointsVertical = (int) (viewHeightPixel / 100.) + 1;
        double worldA[] = {0.,0.,0.}, worldB[] = {0.,0.,0.}, worldC[] = {0.,0.,0.};
        double xyz[] = {0.,0.,0.}, ijk[] = {0.,0.,0.};
        double axisCoord[] = {0.,0.}, wcsStep[] = {0.,0.};
        std::vector<std::vector<double> > world;
        std::vector<std::vector<double> > xyzDisplay;

        // calculate WCS coordinates of the view's corners
        xyToIJK->TransformPoint(xyz, ijk);
        displayNode->GetReferenceSpace(ijk, worldA);

        xyz[0] = viewWidthPixel;
        xyToIJK->TransformPoint(xyz, ijk);
        displayNode->GetReferenceSpace(ijk, worldB);

        xyz[0] = 0.;
        xyz[1] = viewHeightPixel;
        xyToIJK->TransformPoint(xyz, ijk);
        displayNode->GetReferenceSpace(ijk, worldC);

        // calculate the wcsSteps for the two axes
        if (!strcmp(sliceNode->GetOrientationString(), "Sagittal"))
          {
          wcsStep[0] = displayNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldB[2]), &numberOfPointsHorizontal);
          axisCoord[0] = displayNode->GetFirstWcsTickAxisZ(worldA[2], worldB[2], wcsStep[0]);
          wcsStep[1] = displayNode->GetWcsTickStepAxisY(fabs(worldA[1] - worldC[1]), &numberOfPointsVertical);
          axisCoord[1] = displayNode->GetFirstWcsTickAxisY(worldA[1], worldC[1], wcsStep[1]);
          }

        if (!strcmp(sliceNode->GetOrientationString(), "Coronal"))
          {
          wcsStep[0] = displayNode->GetWcsTickStepAxisX(fabs(worldA[0] - worldB[0]), &numberOfPointsHorizontal);
          axisCoord[0] = displayNode->GetFirstWcsTickAxisX(worldA[0], worldB[0], wcsStep[0]);
          wcsStep[1] = displayNode->GetWcsTickStepAxisY(fabs(worldA[1] - worldC[1]), &numberOfPointsVertical);
          axisCoord[1] = displayNode->GetFirstWcsTickAxisY(worldA[1], worldC[1], wcsStep[1]);
          }

        if (!strcmp(sliceNode->GetOrientationString(), "Axial"))
          {
          wcsStep[0] = displayNode->GetWcsTickStepAxisX(fabs(worldA[0] - worldB[0]), &numberOfPointsHorizontal);
          axisCoord[0] = displayNode->GetFirstWcsTickAxisX(worldA[0], worldB[0], wcsStep[0]);
          wcsStep[1] = displayNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldC[2]), &numberOfPointsVertical);
          axisCoord[1] = displayNode->GetFirstWcsTickAxisZ(worldA[2], worldC[2], wcsStep[1]);
          }

        // allocate point along the horizontal axes
        std::vector<double> temp;
        for (int i = 0; i < numberOfPointsHorizontal; i++)
          {
          int i8 = i * 8;
          if (!strcmp(sliceNode->GetOrientationString(), "Sagittal"))
            {
            temp.clear();
            temp.push_back(worldA[0]);
            temp.push_back(worldA[1]);
            temp.push_back(axisCoord[0] + wcsStep[0] * i);
            world.push_back(temp);
            }

          if (!strcmp(sliceNode->GetOrientationString(), "Coronal") ||
              !strcmp(sliceNode->GetOrientationString(), "Axial"))
            {
            temp.clear();
            temp.push_back(axisCoord[0] + wcsStep[0] * i);
            temp.push_back(worldA[1]);
            temp.push_back(worldA[2]);
            world.push_back(temp);
            }

          displayNode->GetIJKSpace(world[i], ijk);
          xyToIJK->Inverse();
          xyToIJK->TransformPoint(ijk, xyz);
          temp.clear();
          temp.push_back(xyz[0]);
          temp.push_back(xyz[1]);
          temp.push_back(xyz[2]);
          xyzDisplay.push_back(temp);
          this->twoDAxesPoints->InsertPoint(i8, xyz[0], 2, 0);
          this->twoDAxesPoints->InsertPoint(i8 + 1, xyz[0], 12, 0);

          if (!strcmp(sliceNode->GetOrientationString(), "Sagittal"))
            {
            xyz[0] = worldA[0];
            xyz[1] = worldA[1];
            xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 4.);
            displayNode->GetIJKSpace(xyz, ijk);
            }

          if (!strcmp(sliceNode->GetOrientationString(), "Coronal") ||
              !strcmp(sliceNode->GetOrientationString(), "Axial"))
            {
            xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 4.);
            xyz[1] = worldA[1];
            xyz[2] = worldA[2];
            displayNode->GetIJKSpace(xyz, ijk);
            }

          xyToIJK->TransformPoint(ijk, xyz);
          this->twoDAxesPoints->InsertPoint(i8 + 2, xyz[0], 2, 0);
          this->twoDAxesPoints->InsertPoint(i8 + 3, xyz[0], 7, 0);

          if (!strcmp(sliceNode->GetOrientationString(), "Sagittal"))
            {
            xyz[0] = worldA[0];
            xyz[1] = worldA[1];
            xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 2.);
            displayNode->GetIJKSpace(xyz, ijk);
            }

          if (!strcmp(sliceNode->GetOrientationString(), "Coronal") ||
              !strcmp(sliceNode->GetOrientationString(), "Axial"))
            {
            xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 2.);
            xyz[1] = worldA[1];
            xyz[2] = worldA[2];
            displayNode->GetIJKSpace(xyz, ijk);
            }

          xyToIJK->TransformPoint(ijk, xyz);
          this->twoDAxesPoints->InsertPoint(i8 + 4, xyz[0], 2, 0);
          this->twoDAxesPoints->InsertPoint(i8 + 5, xyz[0], 7, 0);

          if (!strcmp(sliceNode->GetOrientationString(), "Sagittal"))
            {
            xyz[0] = worldA[0];
            xyz[1] = worldA[1];
            xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] * 3. / 4.);
            displayNode->GetIJKSpace(xyz, ijk);
            }

          if (!strcmp(sliceNode->GetOrientationString(), "Coronal") ||
              !strcmp(sliceNode->GetOrientationString(), "Axial"))
            {
            xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] *3. / 4.);
            xyz[1] = worldA[1];
            xyz[2] = worldA[2];
            displayNode->GetIJKSpace(xyz, ijk);
            }

          xyToIJK->TransformPoint(ijk, xyz);
          this->twoDAxesPoints->InsertPoint(i8 + 6, xyz[0], 2, 0);
          this->twoDAxesPoints->InsertPoint(i8 + 7, xyz[0], 7, 0);

          xyToIJK->Inverse();
          }

        int nTot = numberOfPointsVertical + numberOfPointsHorizontal;

        // allocate point along the vertical axes
        for (int i = numberOfPointsHorizontal; i < nTot; i++)
          {
          int ii = i - numberOfPointsHorizontal;
          int i8 = i * 8;

          if (!strcmp(sliceNode->GetOrientationString(), "Sagittal") ||
              !strcmp(sliceNode->GetOrientationString(), "Coronal"))
            {
            temp.clear();
            temp.push_back(worldA[0]);
            temp.push_back(axisCoord[1] + wcsStep[1] * ii);
            temp.push_back(worldA[2]);
            world.push_back(temp);
            }

          if (!strcmp(sliceNode->GetOrientationString(), "Axial"))
            {
            temp.clear();
            temp.push_back(worldA[0]);
            temp.push_back(worldA[1]);
            temp.push_back(axisCoord[1] + wcsStep[1] * ii);
            world.push_back(temp);
            }

          displayNode->GetIJKSpace(world[i], ijk);
          xyToIJK->Inverse();
          xyToIJK->TransformPoint(ijk, xyz);
          temp.clear();
          temp.push_back(xyz[0]);
          temp.push_back(xyz[1]);
          temp.push_back(xyz[2]);
          xyzDisplay.push_back(temp);

          this->twoDAxesPoints->InsertPoint(i8, 2, xyz[1], 0);
          this->twoDAxesPoints->InsertPoint(i8 + 1, 12, xyz[1], 0);

          if (!strcmp(sliceNode->GetOrientationString(), "Sagittal") ||
              !strcmp(sliceNode->GetOrientationString(), "Coronal"))
            {
            xyz[0] = worldA[0];
            xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 4.);
            xyz[2] = worldA[2];
            displayNode->GetIJKSpace(xyz, ijk);
            }

          if (!strcmp(sliceNode->GetOrientationString(), "Axial"))
            {
            xyz[0] = worldA[0];
            xyz[1] = worldA[1];
            xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 4.);
            displayNode->GetIJKSpace(xyz, ijk);
            }

          xyToIJK->TransformPoint(ijk, xyz);
          this->twoDAxesPoints->InsertPoint(i8 + 2, 2, xyz[1], 0);
          this->twoDAxesPoints->InsertPoint(i8 + 3, 7, xyz[1], 0);

          if (!strcmp(sliceNode->GetOrientationString(), "Sagittal") ||
              !strcmp(sliceNode->GetOrientationString(), "Coronal"))
            {
            xyz[0] = worldA[0];
            xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 2.);
            xyz[2] = worldA[2];
            displayNode->GetIJKSpace(xyz, ijk);
            }

          if (!strcmp(sliceNode->GetOrientationString(), "Axial"))
            {
            xyz[0] = worldA[0];
            xyz[1] = worldA[1];
            xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 2.);
            displayNode->GetIJKSpace(xyz, ijk);
            }

          xyToIJK->TransformPoint(ijk, xyz);
          this->twoDAxesPoints->InsertPoint(i8 + 4, 2, xyz[1], 0);
          this->twoDAxesPoints->InsertPoint(i8 + 5, 7, xyz[1], 0);

          if (!strcmp(sliceNode->GetOrientationString(), "Sagittal") ||
              !strcmp(sliceNode->GetOrientationString(), "Coronal"))
            {
            xyz[0] = worldA[0];
            xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] * 3. / 4.);
            xyz[2] = worldA[2];
            displayNode->GetIJKSpace(xyz, ijk);
            }

          if (!strcmp(sliceNode->GetOrientationString(), "Axial"))
            {
            xyz[0] = worldA[0];
            xyz[1] = worldA[1];
            xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] * 3. / 4.);
            displayNode->GetIJKSpace(xyz, ijk);
            }

          xyToIJK->TransformPoint(ijk, xyz);
          this->twoDAxesPoints->InsertPoint(i8 + 6, 2, xyz[1], 0);
          this->twoDAxesPoints->InsertPoint(i8 + 7, 7, xyz[1], 0);

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

        // setup the mapper and acrtor
        this->twoDAxesPolyData->SetPoints(this->twoDAxesPoints);
        this->twoDAxesPolyData->SetLines(this->twoDAxesCellArray);
        this->twoDAxesMapper->SetInputData(this->twoDAxesPolyData);
        this->twoDAxesActor->SetMapper(this->twoDAxesMapper);
        int fontSize;
        switch (type)
        {
        case vtkMRMLAbstractViewNode::RulerTypeThin:
          this->twoDAxesActor->GetProperty()->SetLineWidth(1);
          fontSize = 12;
          break;
        case vtkMRMLAbstractViewNode::RulerTypeThick:
          this->twoDAxesActor->GetProperty()->SetLineWidth(3);
          fontSize = 18;
          break;
        default:
          break;
        }
        this->MarkerRenderer->AddActor2D(this->twoDAxesActor);


        std::string coord;
        // allocate 2DTextActors for the horizontal axes
        for (int i = 0; i < numberOfPointsHorizontal; i++)
          {
          if(xyzDisplay[i][0] < 50 || xyzDisplay[i][0] > viewWidthPixel - 50)
            {
            continue;
            }

          if (!strcmp(sliceNode->GetOrientationString(), "Sagittal"))
            {
            coord = displayNode->GetAxisDisplayStringFromValueZ(world[i][2]);
            }

          if (!strcmp(sliceNode->GetOrientationString(), "Coronal") ||
              !strcmp(sliceNode->GetOrientationString(), "Axial"))
            {
            coord = displayNode->GetAxisDisplayStringFromValueX(world[i][0]);
            }
          vtkSmartPointer<vtkTextActor> textActorHorizontal = vtkSmartPointer<vtkTextActor>::New();
          vtkTextProperty* textProperty = textActorHorizontal->GetTextProperty();
          textProperty->SetFontSize(fontSize);
          textProperty->SetFontFamilyToArial();
          textActorHorizontal->SetInput(coord.c_str());
          textActorHorizontal->SetDisplayPosition((int) (xyzDisplay[i][0]-40), 15);
          this->MarkerRenderer->AddActor2D(textActorHorizontal);
          }

        // allocate 2DTextActors for the vertical axes
        for (int i = numberOfPointsHorizontal; i < nTot; i++)
          {
          if(xyzDisplay[i][1] < 50 || xyzDisplay[i][1] > viewHeightPixel)
            {
            continue;
            }

          if (!strcmp(sliceNode->GetOrientationString(), "Sagittal") ||
              !strcmp(sliceNode->GetOrientationString(), "Coronal"))
            {
            coord = displayNode->GetAxisDisplayStringFromValueY(world[i][1]);
            }

          if (!strcmp(sliceNode->GetOrientationString(), "Axial"))
            {
            coord = displayNode->GetAxisDisplayStringFromValueZ(world[i][2]);
            }
          vtkSmartPointer<vtkTextActor> textActorVertical = vtkSmartPointer<vtkTextActor>::New();
          vtkTextProperty* textProperty = textActorVertical->GetTextProperty();
          textProperty->SetFontSize(fontSize);
          textProperty->SetFontFamilyToArial();
          textActorVertical->SetInput(coord.c_str());
          textActorVertical->SetDisplayPosition(20, (int) (xyzDisplay[i][1]-5));
          this->MarkerRenderer->AddActor2D(textActorVertical);
          }

        world.clear();
        xyzDisplay.clear();
        temp.clear();
        }
      else
        {
        vtkErrorWithObjectMacro(this->External,
                                "vtkMRMLAstroTwoDAxesDisplayableManager::UpdateAxes()"
                                " failed: display node has no valid WCS.");
        }
      break;
      }
    }

  if (!hasDisplay)
    {
    vtkErrorWithObjectMacro(this->External,
                            "vtkMRMLAstroTwoDAxesDisplayableManager::UpdateAxes()"
                            " failed: display node is invalid.");
    }

  this->ShowActors(true);
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
