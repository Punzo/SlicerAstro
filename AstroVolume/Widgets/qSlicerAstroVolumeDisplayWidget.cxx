// QT includes

// SlicerQT includes
#include "qSlicerLabelMapVolumeDisplayWidget.h"
#include "qSlicerScalarVolumeDisplayWidget.h"
#include "qSlicerAstroVolumeDisplayWidget.h"

// MRML includes
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroVolume
class qSlicerAstroVolumeDisplayWidgetPrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroVolumeDisplayWidget);

protected:
  qSlicerAstroVolumeDisplayWidget* const q_ptr;

public:
  qSlicerAstroVolumeDisplayWidgetPrivate(qSlicerAstroVolumeDisplayWidget& object);
  void init();
  void setCurrentDisplayWidget(qSlicerWidget* displayWidget);

  qSlicerScalarVolumeDisplayWidget*            ScalarVolumeDisplayWidget;
  qSlicerLabelMapVolumeDisplayWidget*          LabelMapVolumeDisplayWidget;
};

// --------------------------------------------------------------------------
qSlicerAstroVolumeDisplayWidgetPrivate::qSlicerAstroVolumeDisplayWidgetPrivate(
  qSlicerAstroVolumeDisplayWidget& object)
  : q_ptr(&object)
{
  this->ScalarVolumeDisplayWidget = 0;
  this->LabelMapVolumeDisplayWidget = 0;
}

// --------------------------------------------------------------------------
void qSlicerAstroVolumeDisplayWidgetPrivate::init()
{
  Q_Q(qSlicerAstroVolumeDisplayWidget);
  this->ScalarVolumeDisplayWidget = new qSlicerScalarVolumeDisplayWidget(q);
  q->addWidget(this->ScalarVolumeDisplayWidget);

  this->LabelMapVolumeDisplayWidget = new qSlicerLabelMapVolumeDisplayWidget(q);
  q->addWidget(this->LabelMapVolumeDisplayWidget);
}

// --------------------------------------------------------------------------
void qSlicerAstroVolumeDisplayWidgetPrivate::setCurrentDisplayWidget(
  qSlicerWidget* displayWidget)
{
  Q_Q(qSlicerAstroVolumeDisplayWidget);
  qSlicerWidget* activeWidget = qobject_cast<qSlicerWidget*>(q->currentWidget());
  if (activeWidget == displayWidget)
    {
    return;
    }
  if (activeWidget)
    {
    // We must remove the node "before" the setting the scene to 0.
    // Because removing the scene could modify the observed node (e.g setting
    // the scene to 0 on a colortable combobox will set the color node of the
    // observed node to 0.
    vtkMRMLNode* emptyVolumeNode = 0;
    if (activeWidget == this->ScalarVolumeDisplayWidget)
      {
      this->ScalarVolumeDisplayWidget->setMRMLVolumeNode(emptyVolumeNode);
      }
    activeWidget->setMRMLScene(0);
    if (activeWidget == this->LabelMapVolumeDisplayWidget)
      {
      this->LabelMapVolumeDisplayWidget->setMRMLVolumeNode(emptyVolumeNode);
      }
  }
  // QStackWidget::setCurrentWidget(0) is not supported
  if (displayWidget)
    {
    q->setCurrentWidget(displayWidget);
    }
}

// --------------------------------------------------------------------------
// qSlicerAstroVolumeDisplayWidget
// --------------------------------------------------------------------------
qSlicerAstroVolumeDisplayWidget::qSlicerAstroVolumeDisplayWidget(QWidget* parentWidget)
  : Superclass(parentWidget)
  , d_ptr(new qSlicerAstroVolumeDisplayWidgetPrivate(*this))
{
  Q_D(qSlicerAstroVolumeDisplayWidget);
  d->init();
}

// --------------------------------------------------------------------------
qSlicerAstroVolumeDisplayWidget::~qSlicerAstroVolumeDisplayWidget()
{
}

// --------------------------------------------------------------------------
void qSlicerAstroVolumeDisplayWidget::setMRMLVolumeNode(vtkMRMLNode* VolumeNode)
{
   Q_D(qSlicerAstroVolumeDisplayWidget);
   qvtkDisconnect(0, vtkCommand::ModifiedEvent,
                  this, SLOT(updateFromMRML(vtkObject*)));

  if (VolumeNode == 0)
    {
    d->setCurrentDisplayWidget(0);
    return;
    }

  vtkMRMLScene* scene = VolumeNode->GetScene();
  vtkMRMLAstroLabelMapVolumeNode* labelMapVolumeNode =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(VolumeNode);
  vtkMRMLAstroVolumeNode* astroVolumeNode =
    vtkMRMLAstroVolumeNode::SafeDownCast(VolumeNode);
  if (astroVolumeNode)
    {
    qvtkConnect(VolumeNode, vtkCommand::ModifiedEvent,
              this, SLOT(updateFromMRML(vtkObject*)));
    d->ScalarVolumeDisplayWidget->setMRMLScene(scene);
    d->ScalarVolumeDisplayWidget->setMRMLVolumeNode(VolumeNode);
    d->setCurrentDisplayWidget(d->ScalarVolumeDisplayWidget);
    }
  else if (labelMapVolumeNode)
   {
   qvtkConnect(VolumeNode, vtkCommand::ModifiedEvent,
             this, SLOT(updateFromMRML(vtkObject*)));
   d->LabelMapVolumeDisplayWidget->setMRMLScene(scene);
   d->LabelMapVolumeDisplayWidget->setMRMLVolumeNode(VolumeNode);
   d->setCurrentDisplayWidget(d->LabelMapVolumeDisplayWidget);
   }
}

// --------------------------------------------------------------------------
void qSlicerAstroVolumeDisplayWidget::updateFromMRML(vtkObject* Volume)
{
  vtkMRMLVolumeNode* VolumeNode = vtkMRMLVolumeNode::SafeDownCast(Volume);
  this->setMRMLVolumeNode(VolumeNode);
}

