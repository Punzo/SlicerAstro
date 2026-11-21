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
// Masking and Reproject modules), messages to the user, work in
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
  // through the render window (its OpenGL context current), not the
  // renderer alone: rendering a renderer with another context current
  // corrupts the OpenGL state cache of VTK (GL_ACTIVE_TEXTURE ... errors)
  ThreeDView1->scheduleRender();

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
  // through the render window (its OpenGL context current), not the
  // renderer alone: rendering a renderer with another context current
  // corrupts the OpenGL state cache of VTK (GL_ACTIVE_TEXTURE ... errors)
  ThreeDView2->scheduleRender();

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
  // through the render window (its OpenGL context current), not the
  // renderer alone: rendering a renderer with another context current
  // corrupts the OpenGL state cache of VTK (GL_ACTIVE_TEXTURE ... errors)
  ThreeDView->scheduleRender();

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
