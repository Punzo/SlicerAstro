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

// Qt includes
#include <QApplication>
#include <QWidget>

// VTK includes
#include <vtkCollection.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// Slicer includes
#include <qMRMLLayoutManager.h>
#include <qMRMLLayoutManager_p.h>
#include <qMRMLSliceAstroControllerWidget.h>
#include <qMRMLSliceAstroWidget.h>
#include <qMRMLSliceView.h>

// MRML includes
#include <vtkMRMLApplicationLogic.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSliceViewDisplayableManagerFactory.h>



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

  // Same initialization sequence as qMRMLLayoutSliceViewFactory:
  // set the slice logic before the slice node, so that displayable managers
  // can use the slice logic during initialization.
  sliceWidget->setSliceLogics(this->sliceLogics());
  this->sliceLogics()->AddItem(sliceWidget->sliceLogic());

  // Associate logic with its matching displayable manager group
  sliceWidget->sliceLogic()->SetDisplayableManagerGroup(sliceWidget->sliceView()->displayableManagerGroup());

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
