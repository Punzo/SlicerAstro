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

// MRML includes
#include <vtkMRMLApplicationLogic.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>



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
  if (!this->layoutManager() || !viewNode)
    {// can't create a slice widget if there is no parent widget
    Q_ASSERT(viewNode);
    return 0;
    }

  // there is a unique slice widget per node
  Q_ASSERT(!this->viewWidget(viewNode));

  qMRMLSliceAstroWidget * sliceWidget = new qMRMLSliceAstroWidget(this->layoutManager()->viewport());
  sliceWidget->sliceController()->setControllerButtonGroup(this->SliceControllerButtonGroup);
  QString sliceLayoutName(viewNode->GetLayoutName());
  QString sliceViewLabel(viewNode->GetLayoutLabel());
  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(viewNode);
  QColor sliceLayoutColor = QColor::fromRgbF(sliceNode->GetLayoutColor()[0],
                                             sliceNode->GetLayoutColor()[1],
                                             sliceNode->GetLayoutColor()[2]);
  sliceWidget->setSliceViewName(sliceLayoutName);
  sliceWidget->setObjectName(QString("qMRMLSliceWidget" + sliceLayoutName));
  sliceWidget->setSliceViewLabel(sliceViewLabel);
  sliceWidget->setSliceViewColor(sliceLayoutColor);
  sliceWidget->setMRMLScene(this->mrmlScene());
  sliceWidget->setMRMLSliceNode(sliceNode);
  sliceWidget->setSliceLogics(this->sliceLogics());

  this->sliceLogics()->AddItem(sliceWidget->sliceLogic());

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
