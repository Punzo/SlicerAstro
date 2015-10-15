// SlicerQt includes
#include "qSlicerAstroVolumeLayoutSliceViewFactory.h"

// Qt includes
#include <QApplication>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

// VTK includes
#include <vtkCollection.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// Slicer includes
#include "qMRMLLayoutManager.h"
#include "qMRMLLayoutManager_p.h"
#include "qMRMLSliceControllerWidget.h"
#include "qMRMLSliceWidget.h"

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

  qMRMLSliceWidget * sliceWidget = new qMRMLSliceWidget(this->layoutManager()->viewport());
  sliceWidget->sliceController()->setControllerButtonGroup(this->SliceControllerButtonGroup);
  QString sliceLayoutName(viewNode->GetLayoutName());
  QString sliceViewLabel(viewNode->GetLayoutLabel());
  //cout<<"bella"<<endl;
  //cout<<viewNode->GetClassName()<<endl;
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
  qMRMLSliceWidget* sliceWidget =
    qobject_cast<qMRMLSliceWidget*>(this->viewWidget(viewNode));
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
