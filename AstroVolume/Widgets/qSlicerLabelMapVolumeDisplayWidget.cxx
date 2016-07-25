// Qt includes

#include <qSlicerLabelMapVolumeDisplayWidget.h>
#include <ui_qSlicerLabelMapVolumeDisplayWidget.h>

// MRML includes
#include <vtkMRMLColorNode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>

// VTK includes

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_Volumes
class qSlicerLabelMapVolumeDisplayWidgetPrivate:
                                          public Ui_qSlicerLabelMapVolumeDisplayWidget
{
  Q_DECLARE_PUBLIC(qSlicerLabelMapVolumeDisplayWidget);
protected:
  qSlicerLabelMapVolumeDisplayWidget* const q_ptr;
public:
  qSlicerLabelMapVolumeDisplayWidgetPrivate(qSlicerLabelMapVolumeDisplayWidget& object);
  ~qSlicerLabelMapVolumeDisplayWidgetPrivate();
  void init();

  vtkMRMLAstroLabelMapVolumeNode* VolumeNode;
};

//-----------------------------------------------------------------------------
qSlicerLabelMapVolumeDisplayWidgetPrivate::qSlicerLabelMapVolumeDisplayWidgetPrivate(qSlicerLabelMapVolumeDisplayWidget& object)
  : q_ptr(&object)
{
  this->VolumeNode = 0;
}

//-----------------------------------------------------------------------------
qSlicerLabelMapVolumeDisplayWidgetPrivate::~qSlicerLabelMapVolumeDisplayWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerLabelMapVolumeDisplayWidgetPrivate::init()
{
  Q_Q(qSlicerLabelMapVolumeDisplayWidget);

  this->setupUi(q);
  QObject::connect(this->ColorTableComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setColorNode(vtkMRMLNode*)));
  QObject::connect(this->SliceIntersectionThicknessSpinBox, SIGNAL(valueChanged(int)),
                   q, SLOT(setSliceIntersectionThickness(int)));
  // disable as there is not MRML Node associated with the widget
  q->setEnabled(false);
}

// --------------------------------------------------------------------------
qSlicerLabelMapVolumeDisplayWidget::qSlicerLabelMapVolumeDisplayWidget(QWidget* _parent) : Superclass(_parent)
  , d_ptr(new qSlicerLabelMapVolumeDisplayWidgetPrivate(*this))
{
  Q_D(qSlicerLabelMapVolumeDisplayWidget);
  d->init();
}

// --------------------------------------------------------------------------
qSlicerLabelMapVolumeDisplayWidget::~qSlicerLabelMapVolumeDisplayWidget()
{
}

// --------------------------------------------------------------------------
vtkMRMLAstroLabelMapVolumeNode* qSlicerLabelMapVolumeDisplayWidget::volumeNode()const
{
  Q_D(const qSlicerLabelMapVolumeDisplayWidget);
  return d->VolumeNode;
}

// --------------------------------------------------------------------------
vtkMRMLAstroLabelMapVolumeDisplayNode* qSlicerLabelMapVolumeDisplayWidget::volumeDisplayNode()const
{
  Q_D(const qSlicerLabelMapVolumeDisplayWidget);
  return d->VolumeNode ? vtkMRMLAstroLabelMapVolumeDisplayNode::SafeDownCast(
    d->VolumeNode->GetAstroLabelMapVolumeDisplayNode()) : 0;
}

// --------------------------------------------------------------------------
void qSlicerLabelMapVolumeDisplayWidget::setMRMLVolumeNode(vtkMRMLNode* node)
{
  this->setMRMLVolumeNode(vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(node));
}

// --------------------------------------------------------------------------
void qSlicerLabelMapVolumeDisplayWidget::setMRMLVolumeNode(vtkMRMLAstroLabelMapVolumeNode* volumeNode)
{
  Q_D(qSlicerLabelMapVolumeDisplayWidget);
  vtkMRMLAstroLabelMapVolumeDisplayNode* oldVolumeDisplayNode = this->volumeDisplayNode();

  qvtkReconnect(oldVolumeDisplayNode, volumeNode ? volumeNode->GetDisplayNode() : 0,
                vtkCommand::ModifiedEvent,
                this, SLOT(updateWidgetFromMRML()));
  d->VolumeNode = volumeNode;
  this->setEnabled(volumeNode != 0);
  this->updateWidgetFromMRML();
}

// --------------------------------------------------------------------------
void qSlicerLabelMapVolumeDisplayWidget::updateWidgetFromMRML()
{
  Q_D(qSlicerLabelMapVolumeDisplayWidget);
  vtkMRMLAstroLabelMapVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (displayNode)
    {
    d->ColorTableComboBox->setCurrentNode(displayNode->GetColorNode());
    d->SliceIntersectionThicknessSpinBox->setValue(
       displayNode->GetSliceIntersectionThickness());
    }
}

// --------------------------------------------------------------------------
void qSlicerLabelMapVolumeDisplayWidget::setColorNode(vtkMRMLNode* colorNode)
{
  vtkMRMLAstroLabelMapVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (!displayNode || !colorNode)
    {
    return;
    }
  Q_ASSERT(vtkMRMLColorNode::SafeDownCast(colorNode));
  displayNode->SetAndObserveColorNodeID(colorNode->GetID());

}

// --------------------------------------------------------------------------
void qSlicerLabelMapVolumeDisplayWidget::setSliceIntersectionThickness(int thickness)
{
  vtkMRMLAstroLabelMapVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (!displayNode)
    {
    return;
    }
  displayNode->SetSliceIntersectionThickness(thickness);
}

//------------------------------------------------------------------------------
int qSlicerLabelMapVolumeDisplayWidget::sliceIntersectionThickness()const
{
  Q_D(const qSlicerLabelMapVolumeDisplayWidget);
  return d->SliceIntersectionThicknessSpinBox->value();
}

