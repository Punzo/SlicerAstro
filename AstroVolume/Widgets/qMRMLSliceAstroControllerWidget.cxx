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

// VTK includes
#include <vtkNew.h>

//--------------------------------------------------------------------------
// qMRMLSliceViewPrivate methods

//---------------------------------------------------------------------------
qMRMLSliceAstroControllerWidgetPrivate::qMRMLSliceAstroControllerWidgetPrivate(qMRMLSliceAstroControllerWidget& object)
  : Superclass(object)
{
  qMRMLOrientation XZOrientation = {qMRMLSliceControllerWidget::tr(""), qMRMLSliceControllerWidget::tr("XZ plane")};
  qMRMLOrientation ZYOrientation = {qMRMLSliceControllerWidget::tr(""), qMRMLSliceControllerWidget::tr("ZY plane")};
  qMRMLOrientation XYOrientation = {qMRMLSliceControllerWidget::tr(""), qMRMLSliceControllerWidget::tr("XY plane")};
  qMRMLOrientation obliqueOrientation = {qMRMLSliceControllerWidget::tr(""), qMRMLSliceControllerWidget::tr("Oblique")};

  this->SliceOrientationToDescription["XZ"] = XZOrientation;
  this->SliceOrientationToDescription["ZY"] = ZYOrientation;
  this->SliceOrientationToDescription["XY"] = XYOrientation;
  this->SliceOrientationToDescription["Reformat"] = obliqueOrientation;

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

  SliceOrientationSelector->setItemText(0, "XY");
  SliceOrientationSelector->setItemText(1, "XZ");
  SliceOrientationSelector->setItemText(2, "ZY");
  SliceOrientationSelector->setItemText(3, "Reformat");

  /*SliceOrientationSelector->insertItems(4, QStringList()
   << QApplication::translate("qMRMLSliceControllerWidget", "XY", 0, QApplication::UnicodeUTF8)
   << QApplication::translate("qMRMLSliceControllerWidget", "XZ", 0, QApplication::UnicodeUTF8)
   << QApplication::translate("qMRMLSliceControllerWidget", "ZY", 0, QApplication::UnicodeUTF8)
   << QApplication::translate("qMRMLSliceControllerWidget", "Reformat", 0, QApplication::UnicodeUTF8)
  );*/

#ifndef QT_NO_TOOLTIP
  SliceOrientationSelector->setToolTip(QApplication::translate("qMRMLSliceControllerWidget", "Slice orientation (XY, XZ, ZY, Reformat).", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
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

  d->SliceLogic->StartSliceNodeInteraction(vtkMRMLSliceNode::OrientationFlag);
  d->MRMLSliceNode->SetOrientation(orientation.toLatin1());
  d->SliceLogic->EndSliceNodeInteraction();
}
