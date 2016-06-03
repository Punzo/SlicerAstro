// Qt includes
#include <QDebug>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QWidgetAction>

// CTK includes
#include <ctkDoubleSlider.h>
#include <ctkDoubleSpinBox.h>
#include <ctkPopupWidget.h>
#include <ctkSignalMapper.h>

// qMRML includes
#include "qMRMLColors.h"
#include "qMRMLSliceAstroControllerWidget_p.h"
#include "qMRMLSliderWidget.h"

// MRMLLogic includes
#include <vtkMRMLSliceLayerLogic.h>

// MRML includes
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLScalarVolumeDisplayNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceLayerLogic.h>
#include <vtkMRMLSliceNode.h>

// AstroMRML includes
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>

// VTK includes
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtksys/SystemTools.hxx>

// SlicerQt includes
#include <qSlicerApplication.h>

// vtkSlicer includes
#include <vtkSlicerApplicationLogic.h>

//--------------------------------------------------------------------------
// qMRMLSliceAstroControllerWidgetPrivate methods

//---------------------------------------------------------------------------
qMRMLSliceAstroControllerWidgetPrivate::qMRMLSliceAstroControllerWidgetPrivate(qMRMLSliceAstroControllerWidget& object)
  : Superclass(object)
{
}

//---------------------------------------------------------------------------
qMRMLSliceAstroControllerWidgetPrivate::~qMRMLSliceAstroControllerWidgetPrivate()
{
}

//---------------------------------------------------------------------------
void qMRMLSliceAstroControllerWidgetPrivate::init()
{
  Q_Q(qMRMLSliceAstroControllerWidget);

  this->Superclass::init();

  this->SliceOrientationSelector->setToolTip(QApplication::translate("qMRMLAstroSliceControllerWidget", "Slice orientation (XY, XZ, ZY, Reformat).", 0, QApplication::UnicodeUTF8));
  qMRMLOrientation XZOrientation = {qMRMLSliceControllerWidget::tr("S: "), qMRMLSliceControllerWidget::tr("S <-----> N")};
  qMRMLOrientation ZYOrientation = {qMRMLSliceControllerWidget::tr("R: "), qMRMLSliceControllerWidget::tr("E <-----> W")};
  qMRMLOrientation XYOrientation = {qMRMLSliceControllerWidget::tr("A: "), qMRMLSliceControllerWidget::tr("z <-----> Z")};
  qMRMLOrientation obliqueOrientation = {qMRMLSliceControllerWidget::tr(""), qMRMLSliceControllerWidget::tr("Oblique")};

  this->SliceOrientationToDescription["XZ"] = XZOrientation;
  this->SliceOrientationToDescription["ZY"] = ZYOrientation;
  this->SliceOrientationToDescription["XY"] = XYOrientation;
  this->SliceOrientationToDescription["Reformat"] = obliqueOrientation;

  this->SliceOffsetSlider->setSpinBoxVisible(false);
  this->WCSDisplay = new QLabel();
  this->WCSDisplay->setObjectName(QString::fromUtf8("WCSDisplay"));
  this->WCSDisplay->setEnabled(true);
  this->WCSDisplay->setFixedWidth(110);
  this->WCSDisplay->setText("");
  this->BarLayout->addWidget(this->WCSDisplay);
  this->app = 0;
  this->col = vtkSmartPointer<vtkCollection>::New();
}

//---------------------------------------------------------------------------
void qMRMLSliceAstroControllerWidgetPrivate::setMRMLSliceNodeInternal(vtkMRMLSliceNode* newSliceNode)
{
  Q_Q(qMRMLSliceAstroControllerWidget);

  if (newSliceNode == this->MRMLSliceNode)
    {
    return;
    }

  this->qvtkReconnect(this->MRMLSliceNode, newSliceNode, vtkCommand::ModifiedEvent,
                      this, SLOT(updateWidgetFromMRMLSliceNode()));
  this->qvtkReconnect(this->MRMLSliceNode, newSliceNode, vtkCommand::ModifiedEvent,
                      this, SLOT(updateCoordinateWidgetFromMRMLSliceNode()));

  this->MRMLSliceNode = newSliceNode;

  // Update widget state given the new slice node
  this->updateWidgetFromMRMLSliceNode();
  this->updateCoordinateWidgetFromMRMLSliceNode();
  // Enable/disable widget
  q->setDisabled(newSliceNode == 0);
}

// --------------------------------------------------------------------------
void qMRMLSliceAstroControllerWidgetPrivate::updateCoordinateWidgetFromMRMLSliceNode()
{
  Q_Q(qMRMLSliceAstroControllerWidget);

  if (!this->MRMLSliceNode)
    {
    return;
    }

  q->setWCSDisplay();
}

// --------------------------------------------------------------------------
// qMRMLSliceView methods

// --------------------------------------------------------------------------
qMRMLSliceAstroControllerWidget::qMRMLSliceAstroControllerWidget(QWidget* _parent)
  : Superclass(new qMRMLSliceAstroControllerWidgetPrivate(*this), _parent)
{
  Q_D(qMRMLSliceAstroControllerWidget);
  d->init();
}

qMRMLSliceAstroControllerWidget::qMRMLSliceAstroControllerWidget(qMRMLSliceAstroControllerWidgetPrivate *pimpl, QWidget *parentWidget)
  : Superclass(pimpl, parentWidget)
{
  // init() should be called in the subclass constructor
}

// --------------------------------------------------------------------------
qMRMLSliceAstroControllerWidget::~qMRMLSliceAstroControllerWidget()
{
}

// --------------------------------------------------------------------------
void qMRMLSliceAstroControllerWidget::setWCSDisplay()
{
  Q_D(qMRMLSliceAstroControllerWidget);

  std::string orientation = this->mrmlSliceNode()->GetOrientation();

  if (orientation.compare("Reformat"))
    {
    d->WCSDisplay->setText("");
    return;
    }

  // get the Logics
  d->app = qSlicerApplication::application();

  vtkMRMLSliceLogic* sliceLogic =
    d->app->applicationLogic()->GetSliceLogic
      (this->mrmlSliceNode());

  bool hasDisplay = false;

  if (!sliceLogic)
    {
    d->WCSDisplay->setText("");
    return;
    }

  if (sliceLogic->GetBackgroundLayer())
    {
    d->col->AddItem(sliceLogic->GetBackgroundLayer());
    }
  if (sliceLogic->GetForegroundLayer())
    {
    d->col->AddItem(sliceLogic->GetForegroundLayer());
    }
  if (sliceLogic->GetLabelLayer())
    {
    d->col->AddItem(sliceLogic->GetLabelLayer());
    }

  if (d->col->GetNumberOfItems() == 0)
    {
    d->WCSDisplay->setText("");
    return;
    }

  for (int i = 0; i < d->col->GetNumberOfItems(); i++)
    {
    vtkMRMLSliceLayerLogic* sliceLayerLogic =
      vtkMRMLSliceLayerLogic::SafeDownCast
        (d->col->GetItemAsObject(i));

    vtkMRMLAstroVolumeNode* astroVolume =
      vtkMRMLAstroVolumeNode::SafeDownCast
        (sliceLayerLogic->GetVolumeNode());

    vtkMRMLAstroVolumeDisplayNode* displayNode =
      vtkMRMLAstroVolumeDisplayNode::SafeDownCast
        (sliceLayerLogic->GetVolumeDisplayNode());

    if (!displayNode || !astroVolume)
      {
      continue;
      }
    else
      {

      hasDisplay = true;
      if (!strcmp(displayNode->GetSpace(), "WCS"))
        {

        double offset = this->mrmlSliceNode()->GetSliceOffset();
        double world [] = {0., 0., 0.};
        int extent[6];
        double ijk [] = {0., 0., 0.};
        astroVolume->GetImageData()->GetExtent(extent);

        ijk[0] = extent[1] / 2;
        ijk[1] = extent[5] / 2;
        ijk[2] = extent[3] / 2;

        if(!orientation.compare("XZ"))
          {
          ijk[1] += offset;
          }
        else if(!orientation.compare("XY"))
          {
          ijk[2] += offset;
          }
        else if(!orientation.compare("ZY"))
          {
          ijk[0] -= offset;
          }
        else
          {
          d->WCSDisplay->setText("");
          break;
          }

        displayNode->GetReferenceSpace(ijk, world);

        if(!orientation.compare("XZ"))
          {
          d->WCSDisplay->setText((astroVolume->GetAstroVolumeDisplayNode()
                                 ->GetDisplayStringFromValueY(world[1])).c_str());
          }
        else if(!orientation.compare("XY"))
          {
          d->WCSDisplay->setText((astroVolume->GetAstroVolumeDisplayNode()
                                 ->GetDisplayStringFromValueZ(world[2])).c_str());
          }
        else if(!orientation.compare("ZY"))
          {
          d->WCSDisplay->setText((astroVolume->GetAstroVolumeDisplayNode()
                                 ->GetDisplayStringFromValueX(world[0])).c_str());
          }
        }
      break;
      }
    }

    if (!hasDisplay)
      {
      d->WCSDisplay->setText("");
      }
}
