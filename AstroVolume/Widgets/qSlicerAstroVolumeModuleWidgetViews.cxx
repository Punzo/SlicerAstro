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

// Comparison views of the AstroVolume module (used by the Smoothing,
// Masking, Reproject and Modeling modules), messages to the user, work in
// the background and rocking 3D views: not the panel (see
// qSlicerAstroVolumeModuleWidget.cxx).

// Qt includes
#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QEventLoop>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QtDebug>

// VTK includes
#include <vtkCollection.h>
#include <vtkImageData.h>
#include <vtkImageThreshold.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkRenderer.h>
#include <vtkSegmentation.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>

// Slicer includes
#include <qMRMLThreeDView.h>
#include <qMRMLThreeDViewControllerWidget.h>
#include <qMRMLThreeDWidget.h>
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerTerminologyNavigatorWidget.h>
#include <vtkSlicerApplicationLogic.h>
#include <vtkSlicerSegmentationsModuleLogic.h>
#include <vtkSlicerTerminologiesModuleLogic.h>
#include <vtkSlicerTerminologyEntry.h>

// MRML includes
#include <vtkMRMLAstroTerminology.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLCameraNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLViewNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>
#include <vtkOrientedImageData.h>

// AstroVolume includes
#include <vtkSlicerAstroVolumeLogic.h>

#include "qSlicerAstroRunButton.h"
#include "qSlicerAstroVolumeModuleWidget.h"
#include "qSlicerAstroVolumeModuleWidget_p.h"

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
template <typename T> std::string NumberToString(T V)
{
  std::string stringValue;
  std::stringstream strstream;
  strstream << V;
  strstream >> stringValue;
  return stringValue;
}

//----------------------------------------------------------------------------
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
}

} // end namespace


//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setComparative3DViews(const char* volumeNodeOneID,
                                                           const char* volumeNodeTwoID,
                                                           bool generateMasks /* = false */,
                                                           bool overlay2D /* = true*/)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app || !app->layoutManager() || !app->layoutManager()->layoutLogic()
     || !app->layoutManager()->layoutLogic()->GetLayoutNode())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "qSlicerApplication not found.";
    return;
    }

  app->layoutManager()->layoutLogic()->GetLayoutNode()->SetViewArrangement
          (vtkMRMLLayoutNode::SlicerLayoutDual3DView);

  if (!this->mrmlScene())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews :"
                   " scene not found.";
    return;
    }

  const bool autoPropagate = this->autoShowActiveVolume();
  this->setAutoShowActiveVolume(false);

  vtkMRMLAstroVolumeNode *volumeOne = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeOneID));
  vtkMRMLAstroVolumeNode *volumeTwo = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeTwoID));

  if(!volumeOne || !volumeTwo ||
     !volumeOne->GetImageData() || !volumeTwo->GetImageData())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews :"
                   " volumes not valid.";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setComparative3DViews : appLogic not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  if (!d->selectionNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setComparative3DViews : selectionNode not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  vtkSmartPointer<vtkCollection> col = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLViewNode"));

  d->selectionNode->SetActiveLabelVolumeID("");
  d->selectionNode->SetActiveVolumeID(volumeOne->GetID());

  unsigned int numViewNodes = col->GetNumberOfItems();

  vtkMRMLVolumeRenderingDisplayNode *volumeOneRenderingDisplay =
    volumeOne->GetAstroVolumeRenderingDisplayNode();
  if (volumeOneRenderingDisplay)
    {
    volumeOneRenderingDisplay->RemoveAllViewNodeIDs();

    for (unsigned int viewIndex = 0; viewIndex < numViewNodes; viewIndex++)
      {
      vtkMRMLViewNode *viewNodeIter =
          vtkMRMLViewNode::SafeDownCast(col->GetItemAsObject(viewIndex));
      if (!viewNodeIter)
        {
        continue;
        }

      volumeOneRenderingDisplay->AddViewNodeID(viewNodeIter->GetID());
      break;
      }
    }

  this->prepareVolumeRendering(volumeOne);

  d->selectionNode->SetActiveVolumeID(volumeTwo->GetID());

  vtkMRMLVolumeRenderingDisplayNode *volumeTwoRenderingDisplay =
    volumeTwo->GetAstroVolumeRenderingDisplayNode();
  if (volumeTwoRenderingDisplay)
    {
    volumeTwoRenderingDisplay->RemoveAllViewNodeIDs();

    bool second = false;
    for (unsigned int viewIndex = 0; viewIndex < numViewNodes; viewIndex++)
      {
      vtkMRMLViewNode *viewNodeIter =
          vtkMRMLViewNode::SafeDownCast(col->GetItemAsObject(viewIndex));
      if (!viewNodeIter)
        {
        continue;
        }

      if(second)
        {
        volumeTwoRenderingDisplay->AddViewNodeID(viewNodeIter->GetID());
        break;
        }
      second = true;
      }

    volumeTwoRenderingDisplay->SetVisibility(true);
    }

  this->prepareVolumeRendering(volumeTwo);

  d->selectionNode->SetActiveVolumeID(volumeOne->GetID());

  if (volumeOneRenderingDisplay)
    {
    volumeOneRenderingDisplay->SetVisibility(true);
    }

  if (overlay2D)
    {
    d->selectionNode->SetSecondaryVolumeID(volumeTwo->GetID());
    }
  else
    {
    d->selectionNode->SetSecondaryVolumeID("");
    }

  vtkSmartPointer<vtkCollection> cameraNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode"));
  int numCameraNodes = cameraNodes->GetNumberOfItems();

  vtkMRMLViewNode *View1 = vtkMRMLViewNode::SafeDownCast
    (mrmlScene()->GetNodeByID("vtkMRMLViewNode1"));
  if (View1)
    {
    for (int cameraIndex = 0; cameraIndex < numCameraNodes; cameraIndex++)
      {
      vtkMRMLCameraNode *cameraNode =
        vtkMRMLCameraNode::SafeDownCast(cameraNodes->GetItemAsObject(cameraIndex));
      if (!cameraNode)
        {
        continue;
        }

      if (cameraNode->GetLayoutName() && View1->GetLayoutName() &&
          !strcmp(cameraNode->GetLayoutName(), View1->GetLayoutName()))
        {
        // looking along the velocity axis (A in RAS), north up, centered on the cube
        const double directionOfProjection[3] = {0., -1., 0.};
        const double viewUp[3] = {0., 0., 1.};
        vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(cameraNode, volumeOne,
                                                        directionOfProjection, viewUp);
        break;
        }
      }
    }

  vtkMRMLViewNode *View2 = vtkMRMLViewNode::SafeDownCast
    (mrmlScene()->GetNodeByID("vtkMRMLViewNode2"));
  if (View2)
    {
    for (int cameraIndex = 0; cameraIndex < numCameraNodes; cameraIndex++)
      {
      vtkMRMLCameraNode *cameraNode =
        vtkMRMLCameraNode::SafeDownCast(cameraNodes->GetItemAsObject(cameraIndex));
      if (!cameraNode)
        {
        continue;
        }

      if (cameraNode->GetLayoutName() && View2->GetLayoutName() &&
          !strcmp(cameraNode->GetLayoutName(), View2->GetLayoutName()))
        {
        // looking along the velocity axis (A in RAS), north up, centered on the cube
        const double directionOfProjection[3] = {0., -1., 0.};
        const double viewUp[3] = {0., 0., 1.};
        vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(cameraNode, volumeTwo,
                                                        directionOfProjection, viewUp);
        break;
        }
      }
    }

  if (overlay2D)
    {
    vtkMRMLSliceCompositeNode *yellowSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow"));
    yellowSliceComposite->SetForegroundOpacity(1.);

    vtkMRMLSliceNode *yellowSlice = vtkMRMLSliceNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
    // setting to the XZ orientation is needed in order to force the refresh
    yellowSlice->SetOrientation("XZ");
    yellowSlice->SetOrientation("XY");
    yellowSlice->SetSliceOffset(0.);
    }

  appLogic->PropagateVolumeSelection();

  // reset the 3D rendering boundaries
  qMRMLThreeDWidget* ThreeDWidget1 = app->layoutManager()->threeDWidget(0);
  if(!ThreeDWidget1)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "ThreeDWidget1 not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  qMRMLThreeDView* ThreeDView1 = ThreeDWidget1->threeDView();
  if(!ThreeDView1 || !ThreeDView1->renderer())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "ThreeDView1 not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  ThreeDView1->renderer()->ResetCameraClippingRange();
  ThreeDView1->renderer()->Render();

  qMRMLThreeDWidget* ThreeDWidget2 = app->layoutManager()->threeDWidget(1);
  if(!ThreeDWidget2)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "ThreeDWidget2 not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  qMRMLThreeDView* ThreeDView2 = ThreeDWidget2->threeDView();
  if(!ThreeDView2 || !ThreeDView2->renderer())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "ThreeDView2 not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  ThreeDView2->renderer()->ResetCameraClippingRange();
  ThreeDView2->renderer()->Render();

  if (!generateMasks)
    {
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  // Create Segmentations
  if (!d->segmentEditorNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                  "segmentEditorNode not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  // Smoothing makes a mask only (no contours): the contours segmentation of
  // the Smoothing module of previous runs or scenes is removed
  vtkSmartPointer<vtkCollection> segNodes = vtkSmartPointer<vtkCollection>::Take
    (this->mrmlScene()->GetNodesByClassByName("vtkMRMLSegmentationNode", "AstroSmoothingContoursSeg"));
  for (int ii = 0; ii < segNodes->GetNumberOfItems(); ii++)
    {
    this->mrmlScene()->RemoveNode(vtkMRMLNode::SafeDownCast(segNodes->GetItemAsObject(ii)));
    }

  // Mask of the smoothed data at 3 RMS: a mask (binary labelmap in 2D and 3D,
  // FITS mask) on the grid of the input data, e.g. for the other modules
  // recommended color of the Mask type of the astronomical terminology
  double maskColor[3] = {0.31, 0.82, 0.88};
  vtkNew<vtkSlicerTerminologyEntry> maskEntry;
  if (vtkSlicerTerminologiesModuleLogic::DeserializeTerminologyEntry(vtkMRMLAstroTerminology::GetMaskEntry(), maskEntry))
    {
    const QColor recommendedColor = qSlicerTerminologyNavigatorWidget::recommendedColorFromTerminology(maskEntry);
    if (recommendedColor.isValid())
      {
      maskColor[0] = recommendedColor.redF();
      maskColor[1] = recommendedColor.greenF();
      maskColor[2] = recommendedColor.blueF();
      }
    }
  vtkSlicerAstroVolumeLogic* astroVolumeLogic = vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
  vtkMRMLSegmentationNode* maskNode = astroVolumeLogic ? astroVolumeLogic->CreateThresholdMask(volumeOne, volumeTwo,
    StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DisplayThreshold")) * 3.,
    (std::string(volumeTwo->GetName()) + "_3RMS").c_str(), maskColor) : nullptr;
  if (!maskNode)
    {
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }
  vtkMRMLSegmentationDisplayNode* maskDisplayNode =
    vtkMRMLSegmentationDisplayNode::SafeDownCast(maskNode->GetDisplayNode());
  if (maskDisplayNode)
    {
    // shown with the input data (first 3D view) and in the slice views; not
    // with the smoothed data (second 3D view). Slicer's default opacities
    // (the ones shown and edited in the Segment Editor)
    maskDisplayNode->RemoveAllViewNodeIDs();
    vtkMRMLViewNode* inputDataView = col->GetNumberOfItems() > 0 ?
      vtkMRMLViewNode::SafeDownCast(col->GetItemAsObject(0)) : nullptr;
    if (inputDataView)
      {
      maskDisplayNode->AddViewNodeID(inputDataView->GetID());
      }
    std::vector<vtkMRMLNode*> sliceNodes;
    this->mrmlScene()->GetNodesByClass("vtkMRMLSliceNode", sliceNodes);
    for (vtkMRMLNode* sliceNode : sliceNodes)
      {
      maskDisplayNode->AddViewNodeID(sliceNode->GetID());
      }
    }
  // the mask is the segmentation of the Segment Editor (and of the mask
  // selectors of the modules)
  d->segmentEditorNode->SetAndObserveSegmentationNode(maskNode);

  this->setAutoShowActiveVolume(autoPropagate);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setThreeComparativeView(const char *volumeNodeOneID,
                                                             const char *volumeNodeTwoID,
                                                             const char *volumeNodeThreeID)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app || !app->layoutManager() || !app->layoutManager()->layoutLogic()
     || !app->layoutManager()->layoutLogic()->GetLayoutNode())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "qSlicerApplication not found.";
    return;
    }

  app->layoutManager()->layoutLogic()->GetLayoutNode()->SetViewArrangement
          (vtkMRMLLayoutNode::SlicerLayoutFourUpView);

  if (!this->mrmlScene())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews :"
                   " scene not found.";
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews :"
                   " appLogic not found!";
    return;
    }

  const bool autoPropagate = this->autoShowActiveVolume();
  this->setAutoShowActiveVolume(false);

  vtkMRMLAstroVolumeNode *volumeOne = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeOneID));
  vtkMRMLAstroVolumeNode *volumeTwo = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeTwoID));
  vtkMRMLAstroVolumeNode *volumeThree = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeThreeID));

  if(!volumeOne || !volumeTwo || !volumeThree ||
     !volumeOne->GetImageData() || !volumeTwo->GetImageData() ||
     !volumeThree->GetImageData())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : volumes not valid.";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode *volumeOneRenderingDisplay =
    volumeOne->GetAstroVolumeRenderingDisplayNode();
  if (volumeOneRenderingDisplay)
    {
    volumeOneRenderingDisplay->SetVisibility(false);
    }

  vtkMRMLVolumeRenderingDisplayNode *volumeTwoRenderingDisplay =
    volumeTwo->GetAstroVolumeRenderingDisplayNode();
  if (volumeTwoRenderingDisplay)
    {
    volumeTwoRenderingDisplay->SetVisibility(false);
    }

  vtkMRMLVolumeRenderingDisplayNode *volumeThreeRenderingDisplay =
    volumeThree->GetAstroVolumeRenderingDisplayNode();
  if (volumeThreeRenderingDisplay)
    {
    volumeThreeRenderingDisplay->SetVisibility(false);
    }

  volumeTwo->SetDisplayVisibility(0);
  volumeThree->SetDisplayVisibility(0);
  volumeOne->SetDisplayVisibility(0);

  if (!d->selectionNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : selectionNode not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  // Update 3D Renderings
  d->selectionNode->SetActiveLabelVolumeID("");
  d->selectionNode->SetActiveVolumeID(volumeTwo->GetID());
  this->prepareVolumeRendering(volumeTwo);

  d->selectionNode->SetActiveVolumeID(volumeThree->GetID());
  this->prepareVolumeRendering(volumeThree);

  d->selectionNode->SetSecondaryVolumeID(volumeTwo->GetID());
  d->selectionNode->SetActiveVolumeID(volumeOne->GetID());
  this->prepareVolumeRendering(volumeOne);

  vtkSmartPointer<vtkCollection> cameraNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode"));
  int numCameraNodes = cameraNodes->GetNumberOfItems();

  vtkMRMLViewNode *View1 = vtkMRMLViewNode::SafeDownCast
    (mrmlScene()->GetNodeByID("vtkMRMLViewNode1"));
  if (View1)
    {
    for (int cameraIndex = 0; cameraIndex < numCameraNodes; cameraIndex++)
      {
      vtkMRMLCameraNode *cameraNode =
        vtkMRMLCameraNode::SafeDownCast(cameraNodes->GetItemAsObject(cameraIndex));
      if (!cameraNode)
        {
        continue;
        }

      if (cameraNode->GetLayoutName() && View1->GetLayoutName() &&
          !strcmp(cameraNode->GetLayoutName(), View1->GetLayoutName()))
        {
        // looking along the velocity axis (A in RAS), north up, centered on the cube
        const double directionOfProjection[3] = {0., -1., 0.};
        const double viewUp[3] = {0., 0., 1.};
        vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(cameraNode, volumeOne,
                                                        directionOfProjection, viewUp);
        break;
        }
      }
    }

  if (volumeOneRenderingDisplay)
    {
    volumeOneRenderingDisplay->SetVisibility(true);
    }

  qMRMLThreeDWidget* ThreeDWidget = app->layoutManager()->threeDWidget(0);
  if(!ThreeDWidget)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                   "ThreeDWidget not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  qMRMLThreeDView* ThreeDView = ThreeDWidget->threeDView();
  if(!ThreeDView || !ThreeDView->renderer())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                   "ThreeDView not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  ThreeDView->renderer()->ResetCameraClippingRange();
  ThreeDView->renderer()->Render();

  vtkMRMLSliceCompositeNode* redSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeRed"));
  if (redSliceComposite)
    {
    redSliceComposite->SetForegroundVolumeID("");
    redSliceComposite->SetForegroundOpacity(0.);
    redSliceComposite->SetBackgroundVolumeID(volumeNodeOneID);
    redSliceComposite->SetLabelVolumeID("");
    }

  vtkMRMLSliceNode* redSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
  if (redSliceNode)
    {
    // setting to the XZ orientation is needed in order to force the refresh
    redSliceNode->SetOrientation("XZ");
    redSliceNode->SetOrientation("XY");
    redSliceNode->SetSliceOffset(0.);
    redSliceNode->SetRulerType(redSliceNode->RulerTypeThin);
    }

  vtkMRMLSliceLogic* redSliceLogic = appLogic->GetSliceLogic(redSliceNode);
  if (redSliceLogic)
    {
    int *dimsSliceRed = redSliceNode->GetDimensions();
    redSliceLogic->FitSliceToAll(dimsSliceRed[0], dimsSliceRed[1]);
    redSliceLogic->SnapSliceOffsetToIJK();
    }

  vtkMRMLSliceCompositeNode* yellowSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow"));
  if (yellowSliceComposite)
    {
    yellowSliceComposite->SetForegroundVolumeID("");
    yellowSliceComposite->SetForegroundOpacity(0.);
    yellowSliceComposite->SetBackgroundVolumeID(volumeNodeTwoID);
    yellowSliceComposite->SetLabelVolumeID("");
    }

  vtkMRMLSliceNode* yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (yellowSliceNode)
    {
    // setting to the XZ orientation is needed in order to force the refresh
    yellowSliceNode->SetOrientation("XZ");
    yellowSliceNode->SetOrientation("XY");
    yellowSliceNode->SetSliceOffset(0.);
    yellowSliceNode->SetRulerType(yellowSliceNode->RulerTypeThin);
    }

  vtkMRMLSliceLogic* yellowSliceLogic = appLogic->GetSliceLogic(yellowSliceNode);
  if (yellowSliceLogic)
    {
    int *dimsSliceYellow = yellowSliceNode->GetDimensions();
    yellowSliceLogic->FitSliceToAll(dimsSliceYellow[0], dimsSliceYellow[1]);
    yellowSliceLogic->SnapSliceOffsetToIJK();
    }

  vtkMRMLSliceCompositeNode* greenSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeGreen"));
  if (greenSliceComposite)
    {
    greenSliceComposite->SetForegroundVolumeID("");
    greenSliceComposite->SetForegroundOpacity(0.);
    greenSliceComposite->SetBackgroundVolumeID(volumeNodeThreeID);
    greenSliceComposite->SetLabelVolumeID("");
    }

  vtkMRMLSliceNode* greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (greenSliceNode)
    {
    // setting to the XZ orientation is needed in order to force the refresh
    greenSliceNode->SetOrientation("XZ");
    greenSliceNode->SetOrientation("XY");
    greenSliceNode->SetSliceOffset(0.);
    greenSliceNode->SetRulerType(greenSliceNode->RulerTypeThin);
    }

  vtkMRMLSliceLogic* greenSliceLogic = appLogic->GetSliceLogic(greenSliceNode);
  if (greenSliceLogic)
    {
    int *dimsSliceGreen = greenSliceNode->GetDimensions();
    greenSliceLogic->FitSliceToAll(dimsSliceGreen[0], dimsSliceGreen[1]);
    greenSliceLogic->SnapSliceOffsetToIJK();
    }

  this->setAutoShowActiveVolume(autoPropagate);
}

//---------------------------------------------------------------------------
bool qSlicerAstroVolumeModuleWidget::askUser(QWidget* parent, const QString& title, const QString& question)
{
  qInfo().noquote() << title << ":" << question;
  qSlicerApplication* application = qSlicerApplication::application();
  if (!application || application->testAttribute(qSlicerCoreApplication::AA_EnableTesting))
    {
    return true;
    }
  return QMessageBox::question(parent, title, question, QMessageBox::Yes | QMessageBox::No,
                               QMessageBox::Yes) == QMessageBox::Yes;
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::informUser(QWidget* parent, QMessageBox::Icon icon,
                                                const QString& title, const QString& message)
{
  if (icon == QMessageBox::Critical)
    {
    qCritical().noquote() << title << ":" << message;
    }
  else if (icon == QMessageBox::Warning)
    {
    qWarning().noquote() << title << ":" << message;
    }
  else
    {
    qInfo().noquote() << title << ":" << message;
    }
  qSlicerApplication* application = qSlicerApplication::application();
  if (!application || application->testAttribute(qSlicerCoreApplication::AA_EnableTesting))
    {
    return;
    }
  QMessageBox messageBox(icon, title, message, QMessageBox::Ok, parent);
  messageBox.exec();
}

//---------------------------------------------------------------------------
namespace
{
// While a background computation runs: user input (mouse, keyboard) goes only
// to the progress dialog (no other operation can start, e.g. before the
// dialog is shown)
class BackgroundInputBlocker : public QObject
{
public:
  explicit BackgroundInputBlocker(QWidget* allowed)
    : Allowed(allowed)
    {
    }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override
    {
    switch (event->type())
      {
      case QEvent::MouseButtonPress:
      case QEvent::MouseButtonRelease:
      case QEvent::MouseButtonDblClick:
      case QEvent::KeyPress:
      case QEvent::KeyRelease:
      case QEvent::Wheel:
      case QEvent::Shortcut:
      case QEvent::ShortcutOverride:
      case QEvent::ContextMenu:
        {
        QWidget* widget = qobject_cast<QWidget*>(watched);
        return !(widget && this->Allowed && (widget == this->Allowed || this->Allowed->isAncestorOf(widget)));
        }
      default:
        return false;
      }
    }

private:
  QWidget* Allowed;
};
} // end of anonymous namespace

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::runInBackground(QWidget* parent, const QString& label,
                                                     const std::function<void()>& work,
                                                     const std::function<int()>& progress,
                                                     const std::function<void()>& cancel)
{
  // started by a run button of a panel: the progress is shown in the button
  // (no dialog), only its Cancel button accepts input
  qSlicerAstroRunButton* runButton = qSlicerAstroRunButton::activeButton();
  if (runButton && runButton->isVisible())
    {
    QThread* buttonThread = QThread::create(work);
    runButton->setProgressText(label.endsWith("...") ? label.left(label.size() - 3) : label);
    runButton->setRunning(true);
    runButton->setProgress(0);
    const QMetaObject::Connection buttonCancel =
      QObject::connect(runButton, &qSlicerAstroRunButton::cancelRequested, [cancel]() { cancel(); });
    QEventLoop buttonLoop;
    QTimer buttonTimer;
    buttonTimer.setInterval(100);
    QObject::connect(&buttonTimer, &QTimer::timeout, [runButton, &buttonLoop, buttonThread, progress]()
      {
      const int value = progress();
      if (value >= 0 && value <= 100)
        {
        runButton->setProgress(value);
        }
      if (buttonThread->isFinished())
        {
        buttonLoop.quit();
        }
      });
    BackgroundInputBlocker buttonInputBlocker(runButton->cancelButton());
    qApp->installEventFilter(&buttonInputBlocker);
    buttonThread->start();
    buttonTimer.start();
    buttonLoop.exec();
    qApp->removeEventFilter(&buttonInputBlocker);
    buttonTimer.stop();
    buttonThread->wait();
    delete buttonThread;
    QObject::disconnect(buttonCancel);
    runButton->setRunning(false);
    return;
    }

  QThread* thread = QThread::create(work);
  QProgressDialog dialog(label, tr("Cancel"), 0, 100, parent);
  dialog.setWindowTitle(tr("SlicerAstro"));
  dialog.setWindowModality(Qt::WindowModal);
  dialog.setMinimumDuration(500);
  dialog.setAutoClose(false);
  dialog.setAutoReset(false);
  dialog.setValue(0);
  const QMetaObject::Connection cancelConnection =
    QObject::connect(&dialog, &QProgressDialog::canceled, [&dialog, cancel]()
    {
    dialog.setLabelText(tr("Cancelling..."));
    cancel();
    });
  QEventLoop loop;
  QTimer timer;
  timer.setInterval(100);
  // the end of the work is polled (no lost signal before the loop starts)
  QObject::connect(&timer, &QTimer::timeout, [&dialog, &loop, thread, progress]()
    {
    const int value = progress();
    if (value >= 0 && value <= 100 && !dialog.wasCanceled())
      {
      dialog.setValue(value);
      }
    if (thread->isFinished())
      {
      loop.quit();
      }
    });
  BackgroundInputBlocker inputBlocker(&dialog);
  qApp->installEventFilter(&inputBlocker);
  thread->start();
  timer.start();
  loop.exec();
  qApp->removeEventFilter(&inputBlocker);
  timer.stop();
  thread->wait();
  delete thread;
  // closing a QProgressDialog emits canceled(): not a cancel request
  QObject::disconnect(cancelConnection);
  dialog.close();
}

//---------------------------------------------------------------------------
const char* qSlicerAstroVolumeModuleWidget::modelingDataContoursSegmentID()
{
  return "ModelingDataContours";
}

//---------------------------------------------------------------------------
const char* qSlicerAstroVolumeModuleWidget::modelingModelContoursSegmentID()
{
  return "ModelingModelContours";
}

//---------------------------------------------------------------------------
const char* qSlicerAstroVolumeModuleWidget::modelingModelMaskSegmentID()
{
  return "ModelingModelMask";
}

//---------------------------------------------------------------------------
bool qSlicerAstroVolumeModuleWidget::updateModelingContourSegment(const char* segmentID,
                                                                  const std::string& name,
                                                                  const double color[3],
                                                                  vtkMRMLAstroVolumeNode* volume,
                                                                  double lower, double upper)
{
  if (!this->contoursSegNodeModeling || !this->contoursSegNodeModeling->GetSegmentation() ||
      !volume || !volume->GetImageData())
    {
    return false;
    }
  // one segment per kind of contours, recomputed in place (the labelmap of a
  // segment has the size of the cube)
  vtkSegmentation* segmentation = this->contoursSegNodeModeling->GetSegmentation();
  vtkSegment* segment = segmentation->GetSegment(segmentID);
  if (!segment)
    {
    double segmentColor[3] = {color[0], color[1], color[2]};
    segmentation->AddEmptySegment(segmentID, name, segmentColor);
    // astronomical terminology: contours of the data, or of the model
    segment = segmentation->GetSegment(segmentID);
    const bool model = !strcmp(segmentID, qSlicerAstroVolumeModuleWidget::modelingModelContoursSegmentID()) ||
                       !strcmp(segmentID, qSlicerAstroVolumeModuleWidget::modelingModelMaskSegmentID());
    if (segment)
      {
      segment->SetTerminology(model ? vtkMRMLAstroTerminology::GetModelEntry() :
                                      vtkMRMLAstroTerminology::GetContourEntry());
      }
    }
  else
    {
    segment->SetName(name.c_str());
    }

  vtkNew<vtkImageThreshold> imageThreshold;
  imageThreshold->SetInputData(volume->GetImageData());
  imageThreshold->ThresholdBetween(lower, upper);
  imageThreshold->SetInValue(1);
  imageThreshold->SetOutValue(0);
  imageThreshold->SetOutputScalarType(VTK_SHORT);
  imageThreshold->Update();
  vtkNew<vtkOrientedImageData> modifierLabelmap;
  modifierLabelmap->ShallowCopy(imageThreshold->GetOutput());
  vtkNew<vtkMatrix4x4> IJKToRASMatrix;
  volume->GetIJKToRASMatrix(IJKToRASMatrix.GetPointer());
  modifierLabelmap->SetGeometryFromImageToWorldMatrix(IJKToRASMatrix.GetPointer());

  vtkSlicerAstroVolumeLogic::PrepareContoursUpdate(this->contoursSegNodeModeling);
  if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
      modifierLabelmap.GetPointer(), this->contoursSegNodeModeling, segmentID,
      vtkSlicerSegmentationsModuleLogic::MODE_REPLACE))
    {
    qCritical() << Q_FUNC_INFO << ": failed to update the segment" << segmentID;
    return false;
    }
  return true;
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::removeObsoleteModelingContourSegments()
{
  if (!this->contoursSegNodeModeling || !this->contoursSegNodeModeling->GetSegmentation())
    {
    return;
    }
  // e.g. one segment per contour level or per model (scenes of SlicerAstro 1)
  vtkSegmentation* segmentation = this->contoursSegNodeModeling->GetSegmentation();
  std::vector<std::string> segmentIDs;
  segmentation->GetSegmentIDs(segmentIDs);
  for (const std::string& segmentID : segmentIDs)
    {
    if (segmentID != modelingDataContoursSegmentID() &&
        segmentID != modelingModelContoursSegmentID() &&
        segmentID != modelingModelMaskSegmentID())
      {
      segmentation->RemoveSegment(segmentID);
      }
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setQuantitative3DView(const char *volumeNodeOneID,
                                                           const char *volumeNodeTwoID,
                                                           const char *volumeNodeThreeID,
                                                           double ContourLevel,
                                                           double PVPhiMajor,
                                                           double PVPhiMinor,
                                                           double RAS[3])
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!this->mrmlScene())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView :"
                   " scene not found.";
    return;
    }

  const bool autoPropagate = this->autoShowActiveVolume();
  this->setAutoShowActiveVolume(false);

  // Set a Plot Layout
  vtkMRMLLayoutNode* layoutNode = vtkMRMLLayoutNode::SafeDownCast(
    this->mrmlScene()->GetFirstNodeByClass("vtkMRMLLayoutNode"));
  if (!layoutNode)
    {
    qWarning() << Q_FUNC_INFO << ": Unable to get layout node!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }
  int viewArra = layoutNode->GetViewArrangement();
  if (viewArra != vtkMRMLLayoutNode::SlicerLayoutConventionalPlotView  &&
      viewArra != vtkMRMLLayoutNode::SlicerLayoutFourUpPlotView        &&
      viewArra != vtkMRMLLayoutNode::SlicerLayoutFourUpPlotTableView   &&
      viewArra != vtkMRMLLayoutNode::SlicerLayoutOneUpPlotView         &&
      viewArra != vtkMRMLLayoutNode::SlicerLayoutThreeOverThreePlotView)
    {
    layoutNode->SetViewArrangement(vtkMRMLLayoutNode::SlicerLayoutConventionalPlotView);
    }

  vtkMRMLAstroVolumeNode *volumeOne = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeOneID));
  vtkMRMLAstroVolumeNode *volumeTwo = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeTwoID));
  vtkMRMLAstroVolumeNode *volumeThree = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeThreeID));

  if(!volumeOne || !volumeTwo || !volumeThree ||
     !volumeOne->GetImageData() || !volumeTwo->GetImageData() ||
     !volumeThree->GetImageData())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : volumes not valid.";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode *volumeOneRenderingDisplay =
    volumeOne->GetAstroVolumeRenderingDisplayNode();
  if (volumeOneRenderingDisplay)
    {
    volumeOneRenderingDisplay->SetVisibility(false);
    }

  vtkMRMLVolumeRenderingDisplayNode *volumeTwoRenderingDisplay =
    volumeTwo->GetAstroVolumeRenderingDisplayNode();
  if (volumeTwoRenderingDisplay)
    {
    volumeTwoRenderingDisplay->SetVisibility(false);
    }

  vtkMRMLVolumeRenderingDisplayNode *volumeThreeRenderingDisplay =
    volumeThree->GetAstroVolumeRenderingDisplayNode();
  if (volumeThreeRenderingDisplay)
    {
    volumeThreeRenderingDisplay->SetVisibility(false);
    }

  volumeTwo->SetDisplayVisibility(0);
  volumeThree->SetDisplayVisibility(0);
  volumeOne->SetDisplayVisibility(0);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : appLogic not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  if (!d->selectionNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : selectionNode not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  // Update 3D Renderings
  d->selectionNode->SetActiveLabelVolumeID("");
  d->selectionNode->SetActiveVolumeID(volumeTwo->GetID());
  this->prepareVolumeRendering(volumeTwo);

  d->selectionNode->SetActiveVolumeID(volumeThree->GetID());
  this->prepareVolumeRendering(volumeThree);

  d->selectionNode->SetSecondaryVolumeID(volumeTwo->GetID());
  d->selectionNode->SetActiveVolumeID(volumeOne->GetID());
  this->prepareVolumeRendering(volumeOne);

  vtkSmartPointer<vtkCollection> cameraNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode"));
  int numCameraNodes = cameraNodes->GetNumberOfItems();

  vtkMRMLViewNode *View1 = vtkMRMLViewNode::SafeDownCast
    (mrmlScene()->GetNodeByID("vtkMRMLViewNode1"));
  if (View1)
    {
    for (int cameraIndex = 0; cameraIndex < numCameraNodes; cameraIndex++)
      {
      vtkMRMLCameraNode *cameraNode =
        vtkMRMLCameraNode::SafeDownCast(cameraNodes->GetItemAsObject(cameraIndex));
      if (!cameraNode)
        {
        continue;
        }

      if (cameraNode->GetLayoutName() && View1->GetLayoutName() &&
          !strcmp(cameraNode->GetLayoutName(), View1->GetLayoutName()))
        {
        // looking along the velocity axis (A in RAS), north up, centered on the cube
        const double directionOfProjection[3] = {0., -1., 0.};
        const double viewUp[3] = {0., 0., 1.};
        vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(cameraNode, volumeOne,
                                                        directionOfProjection, viewUp);
        break;
        }
      }
    }

  if (volumeOneRenderingDisplay)
    {
    volumeOneRenderingDisplay->SetVisibility(true);
    }

  // reset the 3D rendering boundaries
  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                   "qSlicerApplication not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  qMRMLThreeDWidget* ThreeDWidget = app->layoutManager()->threeDWidget(0);
  if(!ThreeDWidget)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                   "ThreeDWidget not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  qMRMLThreeDView* ThreeDView = ThreeDWidget->threeDView();
  if(!ThreeDView || !ThreeDView->renderer())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                   "ThreeDView not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  ThreeDView->renderer()->ResetCameraClippingRange();
  ThreeDView->renderer()->Render();

  // Create Segmentations
  if (!d->segmentEditorNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                  "segmentEditorNode not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  vtkSmartPointer<vtkCollection> segNodes = vtkSmartPointer<vtkCollection>::Take
    (this->mrmlScene()->GetNodesByClassByName("vtkMRMLSegmentationNode", "AstroModelingContoursSeg"));
  this->contoursSegNodeModeling = vtkMRMLSegmentationNode::SafeDownCast(segNodes->GetItemAsObject(0));
  if (!this->contoursSegNodeModeling)
    {
    // owned by the scene: the widget keeps a weak pointer (a new scene, e.g.
    // after a scene close, gets a new node)
    vtkSmartPointer<vtkMRMLSegmentationNode> contours = vtkSmartPointer<vtkMRMLSegmentationNode>::Take(
      vtkMRMLSegmentationNode::SafeDownCast(this->mrmlScene()->CreateNodeByClass("vtkMRMLSegmentationNode")));
    contours->SetName("AstroModelingContoursSeg");
    contours->SetAttribute(vtkSlicerAstroVolumeLogic::GetContoursAttributeName(), "true");
    this->mrmlScene()->AddNode(contours);
    this->contoursSegNodeModeling = contours;
    }

  // the contours are not edited: the Segment Editor keeps its segmentation (mask)

  if (!this->contoursSegNodeModeling->GetDisplayNode())
    {
    this->contoursSegNodeModeling->CreateDefaultDisplayNodes();
    }

  // Create empty segment in current segmentation

  const double contourThreshold =
    StringToDouble(volumeOne->GetAttribute("SlicerAstro.DisplayThreshold")) * ContourLevel;
  const std::string levelSuffix = "_" + DoubleToString(ContourLevel) + "RMS";
  const double dataColor[3] = {0.5, 0.68, 0.5};
  const double modelColor[3] = {1., 0.9, 0.13};
  const double maskColor[3] = {0.73, 0.06, 0.59};
  const std::string SegmentOneID = modelingDataContoursSegmentID();
  const std::string SegmentTwoID = modelingModelContoursSegmentID();
  const std::string SegmentThreeID = modelingModelMaskSegmentID();
  this->removeObsoleteModelingContourSegments();
  this->updateModelingContourSegment(SegmentOneID.c_str(), std::string(volumeOne->GetName()) + levelSuffix,
    dataColor, volumeOne, contourThreshold,
    StringToDouble(volumeOne->GetAttribute("SlicerAstro.DATAMAX")));
  this->updateModelingContourSegment(SegmentTwoID.c_str(), std::string(volumeTwo->GetName()) + levelSuffix,
    modelColor, volumeTwo, contourThreshold,
    StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DATAMAX")));
  // region of the model (independent of the contour level)
  this->updateModelingContourSegment(SegmentThreeID.c_str(), std::string(volumeTwo->GetName()) + "_asMask",
    maskColor, volumeTwo, 1.E-6, StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DATAMAX")));

  if (this->contoursSegNodeModeling->GetNumberOfDisplayNodes() < 1)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView error :"
                   " SegmentationNode has no displayNodes!";
    }

  for (int ii = 0; ii < this->contoursSegNodeModeling->GetNumberOfDisplayNodes(); ii++)
    {
    vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(this->contoursSegNodeModeling->GetNthDisplayNode(ii));
    if (!SegmentationDisplayNode)
      {
      continue;
      }
    SegmentationDisplayNode->SetAllSegmentsVisibility(false);
    SegmentationDisplayNode->SetSegmentVisibility(SegmentOneID, true);
    SegmentationDisplayNode->SetSegmentVisibility(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility(SegmentThreeID, true);
    SegmentationDisplayNode->SetSegmentVisibility3D(SegmentOneID, false);
    SegmentationDisplayNode->SetSegmentVisibility3D(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility3D(SegmentThreeID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentOneID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentThreeID,false);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentOneID, false);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentTwoID, false);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentThreeID, false);
    }

  // data and model contours of the Modeling module: closed surfaces in 2D
  // (contour lines), binary labelmaps in 3D
  vtkSlicerAstroVolumeLogic::SetContoursDisplay(this->contoursSegNodeModeling);

  // Create PV major
  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!yellowSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                  "yellowSliceNode not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  yellowSliceNode->SetOrientation("XZ");
  vtkSmartPointer<vtkMatrix3x3> PVMajorMatrix = vtkSmartPointer<vtkMatrix3x3>::New();
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
    yellowSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix);
    }
  else
    {
    yellowSliceNode->AddSliceOrientationPreset("PVMajor", PVMajorMatrix);
    }
  yellowSliceNode->SetOrientation("PVMajor");

  // Translate to X and Y center
  yellowSliceNode->GetSliceToRAS()->SetElement(0, 3, RAS[0]);
  yellowSliceNode->GetSliceToRAS()->SetElement(1, 3, RAS[1]);
  yellowSliceNode->GetSliceToRAS()->SetElement(2, 3, RAS[2]);
  yellowSliceNode->UpdateMatrices();

  // Create PV minor
  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (!greenSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                  "greenSliceNode not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  greenSliceNode->SetOrientation("XZ");
  vtkSmartPointer<vtkMatrix3x3> PVMinorMatrix = vtkSmartPointer<vtkMatrix3x3>::New();
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
    greenSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix);
    }
  else
    {
    greenSliceNode->AddSliceOrientationPreset("PVMinor", PVMinorMatrix);
    }
  greenSliceNode->SetOrientation("PVMinor");
  // Translate to X and Y center
  greenSliceNode->GetSliceToRAS()->SetElement(0, 3, RAS[0]);
  greenSliceNode->GetSliceToRAS()->SetElement(1, 3, RAS[1]);
  greenSliceNode->GetSliceToRAS()->SetElement(2, 3, RAS[2]);
  greenSliceNode->UpdateMatrices();

  // Add Presents PV to Red, Yellow and Green slices
  vtkMRMLSliceNode *redSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
  if (!redSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                  "redSliceNode not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  if (redSliceNode->HasSliceOrientationPreset("PVMajor"))
    {
    redSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix);
    }
  else
    {
    redSliceNode->AddSliceOrientationPreset("PVMajor", PVMajorMatrix);
    }

  if (redSliceNode->HasSliceOrientationPreset("PVMinor"))
    {
    redSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix);
    }
  else
    {
    redSliceNode->AddSliceOrientationPreset("PVMinor", PVMinorMatrix);
    }
  redSliceNode->Modified();

  if (greenSliceNode->HasSliceOrientationPreset("PVMajor"))
    {
    greenSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix);
    }
  else
    {
    greenSliceNode->AddSliceOrientationPreset("PVMajor", PVMajorMatrix);
    }

  if (yellowSliceNode->HasSliceOrientationPreset("PVMinor"))
    {
    yellowSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix);
    }
  else
    {
    yellowSliceNode->AddSliceOrientationPreset("PVMinor", PVMinorMatrix);
    }

  this->setAutoShowActiveVolume(autoPropagate);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::updateQuantitative3DView(const char *volumeNodeOneID,
                                                              const char *volumeNodeTwoID,
                                                              double ContourLevel,
                                                              double PVPhiMajor,
                                                              double PVPhiMinor,
                                                              double yellowRAS[3],
                                                              double greenRAS[3],
                                                              bool overrideSegments/*=false*/)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!this->mrmlScene())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView :"
                   " scene not found.";
    return;
    }

  const bool autoPropagate = this->autoShowActiveVolume();
  this->setAutoShowActiveVolume(false);

  // Set a Plot Layout
  vtkMRMLLayoutNode* layoutNode = vtkMRMLLayoutNode::SafeDownCast(
    this->mrmlScene()->GetFirstNodeByClass("vtkMRMLLayoutNode"));
  if (!layoutNode)
    {
    qWarning() << Q_FUNC_INFO << ": Unable to get layout node!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }
  int viewArra = layoutNode->GetViewArrangement();
  if (viewArra != vtkMRMLLayoutNode::SlicerLayoutConventionalPlotView  &&
      viewArra != vtkMRMLLayoutNode::SlicerLayoutFourUpPlotView        &&
      viewArra != vtkMRMLLayoutNode::SlicerLayoutFourUpPlotTableView   &&
      viewArra != vtkMRMLLayoutNode::SlicerLayoutOneUpPlotView         &&
      viewArra != vtkMRMLLayoutNode::SlicerLayoutThreeOverThreePlotView)
    {
    layoutNode->SetViewArrangement(vtkMRMLLayoutNode::SlicerLayoutConventionalPlotView);
    }

  vtkMRMLAstroVolumeNode *volumeOne = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeOneID));
  vtkMRMLAstroVolumeNode *volumeTwo = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeTwoID));

  if(!volumeOne || !volumeTwo || !volumeOne->GetImageData() || !volumeTwo->GetImageData())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView() : volumes not valid.";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  // the lowest surface of the 3D color function (3 sigma) at the contour level
  double rms = StringToDouble(volumeOne->GetAttribute("SlicerAstro.DisplayThreshold"));
  vtkSlicerAstroVolumeLogic* astroVolumeLogic = vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
  if (astroVolumeLogic)
    {
    astroVolumeLogic->SetColorFunctionShift(volumeOne, (rms * ContourLevel) - (rms * 3.));
    }

  if (!d->segmentEditorNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView() : segmentEditorNode not valid.";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  vtkSmartPointer<vtkCollection> segNodes = vtkSmartPointer<vtkCollection>::Take
    (this->mrmlScene()->GetNodesByClassByName("vtkMRMLSegmentationNode", "AstroModelingContoursSeg"));
  this->contoursSegNodeModeling = vtkMRMLSegmentationNode::SafeDownCast(segNodes->GetItemAsObject(0));
  if (!this->contoursSegNodeModeling)
    {
    // owned by the scene (weak pointer in the widget)
    vtkSmartPointer<vtkMRMLSegmentationNode> contours = vtkSmartPointer<vtkMRMLSegmentationNode>::Take(
      vtkMRMLSegmentationNode::SafeDownCast(this->mrmlScene()->CreateNodeByClass("vtkMRMLSegmentationNode")));
    contours->SetName("AstroModelingContoursSeg");
    contours->SetAttribute(vtkSlicerAstroVolumeLogic::GetContoursAttributeName(), "true");
    this->mrmlScene()->AddNode(contours);
    this->contoursSegNodeModeling = contours;
    }

  // the contours are not edited: the Segment Editor keeps its segmentation (mask)

  if (!this->contoursSegNodeModeling->GetDisplayNode())
    {
    this->contoursSegNodeModeling->CreateDefaultDisplayNodes();
    }

  // Create empty segment in current segmentation

  const double contourThreshold =
    StringToDouble(volumeOne->GetAttribute("SlicerAstro.DisplayThreshold")) * ContourLevel;
  const std::string levelSuffix = "_" + DoubleToString(ContourLevel) + "RMS";
  const double dataColor[3] = {0.5, 0.68, 0.5};
  const double modelColor[3] = {1., 0.9, 0.13};
  const double maskColor[3] = {0.73, 0.06, 0.59};
  const std::string SegmentOneID = modelingDataContoursSegmentID();
  const std::string SegmentTwoID = modelingModelContoursSegmentID();
  const std::string SegmentThreeID = modelingModelMaskSegmentID();
  this->removeObsoleteModelingContourSegments();
  this->updateModelingContourSegment(SegmentOneID.c_str(), std::string(volumeOne->GetName()) + levelSuffix,
    dataColor, volumeOne, contourThreshold,
    StringToDouble(volumeOne->GetAttribute("SlicerAstro.DATAMAX")));
  this->updateModelingContourSegment(SegmentTwoID.c_str(), std::string(volumeTwo->GetName()) + levelSuffix,
    modelColor, volumeTwo, contourThreshold,
    StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DATAMAX")));
  // region of the model: recomputed when the model changes (independent of
  // the contour level)
  if (overrideSegments || !this->contoursSegNodeModeling->GetSegmentation()->GetSegment(SegmentThreeID))
    {
    this->updateModelingContourSegment(SegmentThreeID.c_str(), std::string(volumeTwo->GetName()) + "_asMask",
      maskColor, volumeTwo, 1.E-6, StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DATAMAX")));
    }

  if (this->contoursSegNodeModeling->GetNumberOfDisplayNodes() < 1)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView error :"
                   " SegmentationNode has no displayNodes!";
    }

  for (int ii = 0; ii < this->contoursSegNodeModeling->GetNumberOfDisplayNodes(); ii++)
    {
    vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(this->contoursSegNodeModeling->GetNthDisplayNode(ii));
    if (!SegmentationDisplayNode)
      {
      continue;
      }
    SegmentationDisplayNode->SetAllSegmentsVisibility(false);
    SegmentationDisplayNode->SetSegmentVisibility(SegmentOneID, true);
    SegmentationDisplayNode->SetSegmentVisibility(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility3D(SegmentOneID, false);
    SegmentationDisplayNode->SetSegmentVisibility3D(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentOneID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentOneID, false);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentTwoID, false);
    }

  // data and model contours of the Modeling module: closed surfaces in 2D
  // (contour lines), binary labelmaps in 3D
  vtkSlicerAstroVolumeLogic::SetContoursDisplay(this->contoursSegNodeModeling);

  // Create PV major
  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!yellowSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::updateQuantitative3DView : "
                  "yellowSliceNode not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  if (!yellowSliceNode->GetSliceToRAS())
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::updateQuantitative3DView : "
                  "SliceToRAS matrix not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  yellowSliceNode->SetOrientation("XZ");
  vtkSmartPointer<vtkMatrix3x3> PVMajorMatrix = vtkSmartPointer<vtkMatrix3x3>::New();
  vtkNew<vtkTransform> transformMajor;
  transformMajor->SetMatrix(yellowSliceNode->GetSliceToRAS());
  transformMajor->RotateY(PVPhiMajor);
  for (int ii = 0; ii < 3; ii++)
    {
    for (int jj = 0; jj < 3; jj++)
      {
      PVMajorMatrix->SetElement(ii,jj, transformMajor->GetMatrix()->GetElement(ii,jj));
      }
    }

  if (yellowSliceNode->HasSliceOrientationPreset("PVMajor"))
    {
    yellowSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix);
    }
  else
    {
    yellowSliceNode->AddSliceOrientationPreset("PVMajor", PVMajorMatrix);
    }
  yellowSliceNode->SetOrientation("PVMajor");
  // Translate to X and Y center
  yellowSliceNode->GetSliceToRAS()->SetElement(0, 3, yellowRAS[0]);
  yellowSliceNode->GetSliceToRAS()->SetElement(1, 3, yellowRAS[1]);
  yellowSliceNode->GetSliceToRAS()->SetElement(2, 3, yellowRAS[2]);
  yellowSliceNode->UpdateMatrices();

  // Create PV minor
  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (!greenSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::updateQuantitative3DView : "
                  "greenSliceNode not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  greenSliceNode->SetOrientation("XZ");
  vtkSmartPointer<vtkMatrix3x3> PVMinorMatrix = vtkSmartPointer<vtkMatrix3x3>::New();
  vtkNew<vtkTransform> transformMinor;
  transformMinor->SetMatrix(greenSliceNode->GetSliceToRAS());
  transformMinor->RotateY(PVPhiMinor);
  for (int ii = 0; ii < 3; ii++)
    {
    for (int jj = 0; jj < 3; jj++)
      {
      PVMinorMatrix->SetElement(ii,jj, transformMinor->GetMatrix()->GetElement(ii,jj));
      }
    }

  if (greenSliceNode->HasSliceOrientationPreset("PVMinor"))
    {
    greenSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix);
    }
  else
    {
    greenSliceNode->AddSliceOrientationPreset("PVMinor", PVMinorMatrix);
    }
  greenSliceNode->SetOrientation("PVMinor");
  // Translate to X and Y center
  greenSliceNode->GetSliceToRAS()->SetElement(0, 3, greenRAS[0]);
  greenSliceNode->GetSliceToRAS()->SetElement(1, 3, greenRAS[1]);
  greenSliceNode->GetSliceToRAS()->SetElement(2, 3, greenRAS[2]);
  greenSliceNode->UpdateMatrices();

  // Add Presents PV to Red, Yellow and Green slices
  vtkMRMLSliceNode *redSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
  if (!redSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                  "redSliceNode not found!";
    this->setAutoShowActiveVolume(autoPropagate);
    return;
    }

  if (redSliceNode->HasSliceOrientationPreset("PVMajor"))
    {
    redSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix);
    }
  else
    {
    redSliceNode->AddSliceOrientationPreset("PVMajor", PVMajorMatrix);
    }

  if (redSliceNode->HasSliceOrientationPreset("PVMinor"))
    {
    redSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix);
    }
  else
    {
    redSliceNode->AddSliceOrientationPreset("PVMinor", PVMinorMatrix);
    }
  redSliceNode->Modified();

  if (greenSliceNode->HasSliceOrientationPreset("PVMajor"))
    {
    greenSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix);
    }
  else
    {
    greenSliceNode->AddSliceOrientationPreset("PVMajor", PVMajorMatrix);
    }

  if (yellowSliceNode->HasSliceOrientationPreset("PVMinor"))
    {
    yellowSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix);
    }
  else
    {
    yellowSliceNode->AddSliceOrientationPreset("PVMinor", PVMinorMatrix);
    }

  this->setAutoShowActiveVolume(autoPropagate);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::stopRockView()
{
  qSlicerApplication* app = qSlicerApplication::application();
  for (int i = 0; i < app->layoutManager()->threeDViewCount(); i++)
    {
    app->layoutManager()->threeDWidget(i)->threeDController()->rockView(false);
  }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::startRockView()
{
  qSlicerApplication* app = qSlicerApplication::application();
  for (int i = 0; i < app->layoutManager()->threeDViewCount(); i++)
    {
    app->layoutManager()->threeDWidget(i)->threeDController()->rockView(true);
    }
}
