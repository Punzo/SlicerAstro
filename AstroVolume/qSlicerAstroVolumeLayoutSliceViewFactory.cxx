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

// SlicerQt includes
#include "qSlicerAstroVolumeLayoutSliceViewFactory.h"

// STD includes
#include <vector>

// Qt includes
#include <QApplication>
#include <QWidget>

// VTK includes
#include <vtkCollection.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkObjectFactory.h>
#include <vtkPropCollection.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>

// Slicer includes
#include <qMRMLLayoutManager.h>
#include <qMRMLLayoutManager_p.h>
#include <qMRMLSliceAstroControllerWidget.h>
#include <qMRMLSliceAstroWidget.h>
#include <qMRMLSliceView.h>

// MRML includes
#include <vtkMRMLApplicationLogic.h>
#include <vtkMRMLDisplayableManagerGroup.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSliceViewDisplayableManagerFactory.h>



namespace
{
//-----------------------------------------------------------------------------
// qMRMLSliceView::displayableManagerGroup() is protected (only the core slice
// view factory is a friend class): expose it through a pointer to member.
struct SliceViewAccessor : public qMRMLSliceView
{
  using qMRMLSliceView::displayableManagerGroup;
};
} // namespace

//-----------------------------------------------------------------------------
qSlicerAstroVolumeLayoutSliceViewFactory::qSlicerAstroVolumeLayoutSliceViewFactory(QObject* _parent)
  : Superclass(_parent)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeLayoutSliceViewFactory::~qSlicerAstroVolumeLayoutSliceViewFactory()
{
}

//-----------------------------------------------------------------------------
QWidget* qSlicerAstroVolumeLayoutSliceViewFactory::createViewFromNode(vtkMRMLAbstractViewNode* viewNode)
{
  if (!this->layoutManager() || !viewNode  || !this->layoutManager()->viewport())
    {// can't create a slice widget if there is no parent widget
    Q_ASSERT(viewNode);
    return 0;
    }

  // there is a unique slice widget per node
  Q_ASSERT(!this->viewWidget(viewNode));

  qMRMLSliceAstroWidget * sliceWidget = new qMRMLSliceAstroWidget(this->layoutManager()->viewport());

  // The new slice view has registered the Slicer ruler displayable manager again
  qSlicerAstroVolumeLayoutSliceViewFactory::removeSlicerRulers(this->layoutManager());
  qSlicerAstroVolumeLayoutSliceViewFactory::removeSlicerRulerRenderers(sliceWidget->sliceView());

  // Same initialization sequence as qMRMLLayoutSliceViewFactory:
  // set the slice logic before the slice node, so that displayable managers
  // can use the slice logic during initialization.
  sliceWidget->setSliceLogics(this->sliceLogics());
  this->sliceLogics()->AddItem(sliceWidget->sliceLogic());

  // Associate the slice logic with the displayable manager group of the view
  // (as qMRMLLayoutSliceViewFactory does): it is used by
  // vtkSlicerApplicationLogic::GetViewDisplayableManagerByClassName.
  vtkMRMLDisplayableManagerGroup* (qMRMLSliceView::*displayableManagerGroup)() const =
    &SliceViewAccessor::displayableManagerGroup;
  sliceWidget->sliceLogic()->SetDisplayableManagerGroup(
    (sliceWidget->sliceView()->*displayableManagerGroup)());

  sliceWidget->sliceController()->setControllerButtonGroup(this->SliceControllerButtonGroup);
  sliceWidget->setObjectName(QString("qMRMLSliceWidget%1").arg(viewNode->GetLayoutName()));
  // set slice node before setting the scene to allow using slice node names
  // in the slice transform, display, and model nodes
  sliceWidget->setMRMLSliceNode(vtkMRMLSliceNode::SafeDownCast(viewNode));
  sliceWidget->setMRMLScene(this->mrmlScene());

  QObject::connect(sliceWidget, SIGNAL(nodeAboutToBeEdited(vtkMRMLNode*)),
                   this->layoutManager(), SIGNAL(nodeAboutToBeEdited(vtkMRMLNode*)));

  return sliceWidget;
}

void qSlicerAstroVolumeLayoutSliceViewFactory::removeSlicerRulers(qMRMLLayoutManager* layoutManager)
{
  vtkMRMLSliceViewDisplayableManagerFactory* factory = vtkMRMLSliceViewDisplayableManagerFactory::GetInstance();
  if (factory->IsDisplayableManagerRegistered("vtkMRMLRulerDisplayableManager"))
    {
    // the displayable manager groups of the slice views delete their instances
    factory->UnRegisterDisplayableManager("vtkMRMLRulerDisplayableManager");
    }
  if (!layoutManager)
    {
    return;
    }
  foreach (const QString& sliceViewName, layoutManager->sliceViewNames())
    {
    qMRMLSliceWidget* sliceWidget = layoutManager->sliceWidget(sliceViewName);
    if (sliceWidget)
      {
      qSlicerAstroVolumeLayoutSliceViewFactory::removeSlicerRulerRenderers(sliceWidget->sliceView());
      }
    }
}

//------------------------------------------------------------------------------
void qSlicerAstroVolumeLayoutSliceViewFactory::removeSlicerRulerRenderers(qMRMLSliceView* sliceView)
{
  if (!sliceView || !sliceView->renderWindow())
    {
    return;
    }
  // vtkMRMLRulerDisplayableManager does not remove its marker renderer (with
  // the ruler actors, possibly visible) from the render window when it is
  // deleted. It is the only displayable manager using a vtkAxisActor2D.
  vtkRenderWindow* renderWindow = sliceView->renderWindow();
  vtkRendererCollection* renderers = renderWindow->GetRenderers();
  std::vector<vtkSmartPointer<vtkRenderer>> rulerRenderers;
  vtkCollectionSimpleIterator rendererIt;
  renderers->InitTraversal(rendererIt);
  while (vtkRenderer* renderer = renderers->GetNextRenderer(rendererIt))
    {
    vtkPropCollection* props = renderer->GetViewProps();
    vtkCollectionSimpleIterator propIt;
    props->InitTraversal(propIt);
    while (vtkProp* prop = props->GetNextProp(propIt))
      {
      if (prop->IsA("vtkAxisActor2D"))
        {
        rulerRenderers.push_back(renderer);
        break;
        }
      }
    }
  for (vtkRenderer* renderer : rulerRenderers)
    {
    renderWindow->RemoveRenderer(renderer);
    }
}

//------------------------------------------------------------------------------
void qSlicerAstroVolumeLayoutSliceViewFactory::deleteView(vtkMRMLAbstractViewNode *viewNode)
{
  qMRMLSliceAstroWidget* sliceWidget =
    qobject_cast<qMRMLSliceAstroWidget*>(this->viewWidget(viewNode));
  if (sliceWidget)
    {
    this->sliceLogics()->RemoveItem(sliceWidget->sliceLogic());
    }
  this->Superclass::deleteView(viewNode);
}

QString qSlicerAstroVolumeLayoutSliceViewFactory::viewClassName() const
{
  return "vtkMRMLSliceNode";
}
