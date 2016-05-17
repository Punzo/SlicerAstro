// Qt includes
#include <QDebug>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QWidgetAction>
#include <QString>

// CTK includes
#include <ctkDoubleSlider.h>
#include <ctkPopupWidget.h>
#include <ctkSignalMapper.h>
#include <ctkDoubleSpinBox.h>

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
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>

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

  this->SliceOrientationSelector->setItemText(0, "XY");
  this->SliceOrientationSelector->setItemText(1, "XZ");
  this->SliceOrientationSelector->setItemText(2, "ZY");
  this->SliceOrientationSelector->setItemText(3, "Reformat");

  this->SliceOrientationSelector->setToolTip(QApplication::translate("qMRMLAstroSliceControllerWidget", "Slice orientation (XY, XZ, ZY, Reformat).", 0, QApplication::UnicodeUTF8));
  qMRMLOrientation axialOrientation = {qMRMLSliceControllerWidget::tr("S: "), qMRMLSliceControllerWidget::tr("S <-----> N")};
  qMRMLOrientation sagittalOrientation = {qMRMLSliceControllerWidget::tr("R: "), qMRMLSliceControllerWidget::tr("E <-----> W")};
  qMRMLOrientation coronalOrientation = {qMRMLSliceControllerWidget::tr("A: "), qMRMLSliceControllerWidget::tr("z <-----> Z")};
  qMRMLOrientation obliqueOrientation = {qMRMLSliceControllerWidget::tr(""), qMRMLSliceControllerWidget::tr("Oblique")};

  this->SliceOrientationToDescription["Axial"] = axialOrientation;
  this->SliceOrientationToDescription["Sagittal"] = sagittalOrientation;
  this->SliceOrientationToDescription["Coronal"] = coronalOrientation;
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

// --------------------------------------------------------------------------
void qMRMLSliceAstroControllerWidgetPrivate::updateWidgetFromMRMLSliceNode()
{
  Q_Q(qMRMLSliceAstroControllerWidget);

  if (!this->MRMLSliceNode)
    {
    return;
    }

  std::string orient = this->MRMLSliceNode->GetOrientationString();
  std::string sysOrient = this->MRMLSliceNode->GetOrientationDisplayString();

  if(!orient.compare("Axial"))
    {
    this->SliceOffsetSlider->setToolTip(qMRMLSliceControllerWidget::tr("S <-----> N"));
    this->WCSDisplay->setToolTip("Declination Coordinate");
    }
  if(!orient.compare("Coronal"))
    {
    this->SliceOffsetSlider->setToolTip(qMRMLSliceControllerWidget::tr("z <-----> Z"));
    this->WCSDisplay->setToolTip("Frequency/velocity Coordinate");
    }
  if(!orient.compare("Sagittal"))
    {
    this->SliceOffsetSlider->setToolTip(qMRMLSliceControllerWidget::tr("E <-----> W"));
    this->WCSDisplay->setToolTip("Right Ascension Coordinate");
    }
  if(!orient.compare("Reformat"))
    {
    this->SliceOffsetSlider->setToolTip(qMRMLSliceControllerWidget::tr("Oblique"));
    }

  q->setWCSDisplay();


  bool wasBlocked;

  // Update abbreviated slice view name
  this->ViewLabel->setText(this->MRMLSliceNode->GetLayoutLabel());

  // Update orientation selector state
  int index = this->SliceOrientationSelector->findText(
      QString::fromStdString(sysOrient));
  Q_ASSERT(index>=0 && index <=4);

  // We block the signal to avoid calling setSliceOrientation from the MRMLNode
  wasBlocked = this->SliceOrientationSelector->blockSignals(true);
  this->SliceOrientationSelector->setCurrentIndex(index);
  this->SliceOrientationSelector->blockSignals(wasBlocked);

  // Update slice visibility toggle
  this->actionShow_in_3D->setChecked(this->MRMLSliceNode->GetSliceVisible());
  this->actionLockNormalToCamera->setChecked(
    this->MRMLSliceNode->GetWidgetNormalLockedToCamera());

  // Label Outline
  bool showOutline = this->MRMLSliceNode->GetUseLabelOutline();
  this->actionLabelMapOutline->setChecked(showOutline);
  this->actionLabelMapOutline->setText(showOutline ?
    tr("Hide label volume outlines") : tr("Show label volume outlines"));
  // Reformat
  bool showReformat = this->MRMLSliceNode->GetWidgetVisible();
  this->actionShow_reformat_widget->setChecked(showReformat);
  this->actionShow_reformat_widget->setText(
    showReformat ? tr("Hide reformat widget"): tr("Show reformat widget"));
  // Slice spacing mode
  this->SliceSpacingButton->setIcon(
    this->MRMLSliceNode->GetSliceSpacingMode() == vtkMRMLSliceNode::AutomaticSliceSpacingMode ?
      QIcon(":/Icons/SlicerAutomaticSliceSpacing.png") :
      QIcon(":/Icons/SlicerManualSliceSpacing.png"));
  this->actionSliceSpacingModeAutomatic->setChecked(
    this->MRMLSliceNode->GetSliceSpacingMode() == vtkMRMLSliceNode::AutomaticSliceSpacingMode);
  // Prescribed slice spacing
  double spacing[3] = {0.0, 0.0, 0.0};
  this->MRMLSliceNode->GetPrescribedSliceSpacing(spacing);
  this->SliceSpacingSpinBox->setValue(spacing[2]);
  // Field of view
  double fov[3]  = {0.0, 0.0, 0.0};
  this->MRMLSliceNode->GetFieldOfView(fov);
  wasBlocked = this->SliceFOVSpinBox->blockSignals(true);
  this->SliceFOVSpinBox->setValue(fov[0] < fov[1] ? fov[0] : fov[1]);
  this->SliceFOVSpinBox->blockSignals(wasBlocked);
  // Lightbox
  int rows = this->MRMLSliceNode->GetLayoutGridRows();
  int columns = this->MRMLSliceNode->GetLayoutGridColumns();
  this->actionLightbox1x1_view->setChecked(rows == 1 && columns == 1);
  this->actionLightbox1x2_view->setChecked(rows == 1 && columns == 2);
  this->actionLightbox1x3_view->setChecked(rows == 1 && columns == 3);
  this->actionLightbox1x4_view->setChecked(rows == 1 && columns == 4);
  this->actionLightbox1x6_view->setChecked(rows == 1 && columns == 6);
  this->actionLightbox1x8_view->setChecked(rows == 1 && columns == 8);
  this->actionLightbox2x2_view->setChecked(rows == 2 && columns == 2);
  this->actionLightbox3x3_view->setChecked(rows == 3 && columns == 3);
  this->actionLightbox6x6_view->setChecked(rows == 6 && columns == 6);

  this->actionSliceModelModeVolumes->setChecked(this->MRMLSliceNode->GetSliceResolutionMode() ==
                                                vtkMRMLSliceNode::SliceResolutionMatchVolumes);
  this->actionSliceModelMode2D->setChecked(this->MRMLSliceNode->GetSliceResolutionMode() ==
                                                vtkMRMLSliceNode::SliceResolutionMatch2DView);
  this->actionSliceModelMode2D_Volumes->setChecked(this->MRMLSliceNode->GetSliceResolutionMode() ==
                                                vtkMRMLSliceNode::SliceFOVMatch2DViewSpacingMatchVolumes);
  this->actionSliceModelModeVolumes_2D->setChecked(this->MRMLSliceNode->GetSliceResolutionMode() ==
                                                vtkMRMLSliceNode::SliceFOVMatchVolumesSpacingMatch2DView);
  //this->actionSliceModelModeCustom->setChecked(this->MRMLSliceNode->GetSliceResolutionMode() ==
  //                                              vtkMRMLSliceNode::SliceResolutionCustom);

  double UVWExtents[] = {256,256,256};
  double UVWOrigin[] = {0,0,0};
  int UVWDimensions[] = {256,256,256};

  this->MRMLSliceNode->GetUVWExtents(UVWExtents);
  this->MRMLSliceNode->GetUVWOrigin(UVWOrigin);
  this->MRMLSliceNode->GetUVWDimensions(UVWDimensions);

  wasBlocked = this->SliceModelFOVXSpinBox->blockSignals(true);
  this->SliceModelFOVXSpinBox->setValue(UVWExtents[0]);
  this->SliceModelFOVXSpinBox->blockSignals(wasBlocked);

  wasBlocked = this->SliceModelFOVYSpinBox->blockSignals(true);
  this->SliceModelFOVYSpinBox->setValue(UVWExtents[1]);
  this->SliceModelFOVYSpinBox->blockSignals(wasBlocked);

  wasBlocked = this->SliceModelDimensionXSpinBox->blockSignals(true);
  this->SliceModelDimensionXSpinBox->setValue(UVWDimensions[0]);
  this->SliceModelDimensionXSpinBox->blockSignals(wasBlocked);

  wasBlocked = this->SliceModelDimensionYSpinBox->blockSignals(true);
  this->SliceModelDimensionYSpinBox->setValue(UVWDimensions[1]);
  this->SliceModelDimensionYSpinBox->blockSignals(wasBlocked);

  wasBlocked = this->SliceModelOriginXSpinBox->blockSignals(true);
  this->SliceModelOriginXSpinBox->setValue(UVWOrigin[0]);
  this->SliceModelOriginXSpinBox->blockSignals(wasBlocked);

  wasBlocked = this->SliceModelOriginYSpinBox->blockSignals(true);
  this->SliceModelOriginYSpinBox->setValue(UVWOrigin[1]);
  this->SliceModelOriginYSpinBox->blockSignals(wasBlocked);

  // OrientationMarker (check the selected option)
  QAction* action = qobject_cast<QAction*>(this->OrientationMarkerTypesMapper->mapping(this->MRMLSliceNode->GetOrientationMarkerType()));
  if (action)
    {
    action->setChecked(true);
    }
  action = qobject_cast<QAction*>(this->OrientationMarkerSizesMapper->mapping(this->MRMLSliceNode->GetOrientationMarkerSize()));
  if (action)
    {
    action->setChecked(true);
    }

  // Ruler (check the selected option)
  action = qobject_cast<QAction*>(this->RulerTypesMapper->mapping(this->MRMLSliceNode->GetRulerType()));
  if (action)
    {
    action->setChecked(true);
    }


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

  // get the Logics
  d->app = qSlicerApplication::application();

  vtkMRMLSliceLogic* sliceLogic =
    d->app->applicationLogic()->GetSliceLogic
      (this->mrmlSliceNode());

  bool hasDisplay = false;

  if (!sliceLogic)
    {
    d->WCSDisplay->setText("");
    }

  d->col->AddItem(sliceLogic->GetBackgroundLayer());
  d->col->AddItem(sliceLogic->GetForegroundLayer());
  d->col->AddItem(sliceLogic->GetLabelLayer());

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
        double ijk [] = {0., 0., 0.};
        astroVolume->GetOrigin(world);

        ijk[0] = fabs(world[0]);
        ijk[1] = fabs(world[2]);
        ijk[2] = fabs(world[1]);

        std::string orientation = this->mrmlSliceNode()->GetOrientationString();

        if(!orientation.compare("Axial"))
          {
          ijk[1] += offset;
          }
        if(!orientation.compare("Coronal"))
          {
          ijk[2] += offset;
          }
        if(!orientation.compare("Sagittal"))
          {
          ijk[0] -= offset;
          }
        if(!orientation.compare("Reformat"))
          {
          d->WCSDisplay->setText("");
          break;
          }

        displayNode->GetReferenceSpace(ijk, world);

        if(!orientation.compare("Axial"))
          {
          d->WCSDisplay->setText((astroVolume->GetAstroVolumeDisplayNode()
                                 ->GetDisplayStringFromValueY(world[1])).c_str());
          }
        if(!orientation.compare("Coronal"))
          {
          d->WCSDisplay->setText((astroVolume->GetAstroVolumeDisplayNode()
                                 ->GetDisplayStringFromValueZ(world[2])).c_str());
          }
        if(!orientation.compare("Sagittal"))
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

void qMRMLSliceAstroControllerWidget::setSliceOrientation(const QString &orientation)
{
  Q_D(qMRMLSliceAstroControllerWidget);

#ifndef QT_NO_DEBUG
  QStringList expectedOrientation;
  expectedOrientation << "XY" << "XZ" << "ZY" << "Reformat";
  Q_ASSERT(expectedOrientation.contains(orientation));
#endif

  if (!d->MRMLSliceNode || !d->MRMLSliceCompositeNode)
    {
    return;
    }

  QString sysOrientation = "Reformat";

  if(!orientation.compare("XY"))
    {
    sysOrientation = "Coronal";
    }

  if(!orientation.compare("XZ"))
    {
    sysOrientation = "Axial";
    }

  if(!orientation.compare("ZY"))
    {
    sysOrientation = "Sagittal";
    }

  d->SliceLogic->StartSliceNodeInteraction(vtkMRMLSliceNode::OrientationFlag);
  d->MRMLSliceNode->SetOrientation(sysOrientation.toLatin1());
  this->fitSliceToBackground();
  d->SliceLogic->EndSliceNodeInteraction();
}

