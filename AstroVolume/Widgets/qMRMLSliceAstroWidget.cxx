// Qt includes
#include <QDebug>
#include <QVBoxLayout>
#include <QFrame>

// qMRML includes
#include "qMRMLSliceAstroWidget_p.h"
#include "qMRMLSliceAstroControllerWidget.h"
#include "qMRMLSliceControllerWidget.h"
#include "qMRMLSliceView.h"

// MRMLDisplayableManager includes
#include <vtkSliceViewInteractorStyle.h>

// MRML includes
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkCollection.h>
#include <vtkWeakPointer.h>


//--------------------------------------------------------------------------
// qMRMLSliceAstroWidgetPrivate methods

//---------------------------------------------------------------------------
qMRMLSliceAstroWidgetPrivate::qMRMLSliceAstroWidgetPrivate(qMRMLSliceAstroWidget& object)
  : Superclass(object)
{
}

//---------------------------------------------------------------------------
qMRMLSliceAstroWidgetPrivate::~qMRMLSliceAstroWidgetPrivate()
{
}

//---------------------------------------------------------------------------
void qMRMLSliceAstroWidgetPrivate::init()
{
  Q_Q(qMRMLSliceAstroWidget);

  this->Superclass::init();

  this->verticalLayout_2->removeWidget(this->SliceController);
  this->verticalLayout_2->removeWidget(this->frame);

  delete this->SliceController;
  delete this->frame;

  this->SliceController = new qMRMLSliceAstroControllerWidget(q);
  this->SliceController->setObjectName(QString::fromUtf8("SliceController"));
  this->verticalLayout_2->addWidget(this->SliceController);
  this->frame = new QFrame(q);
  this->frame->setObjectName(QString::fromUtf8("frame"));
  this->frame->setFrameShadow(QFrame::Raised);
  this->verticalLayout = new QVBoxLayout(this->frame);
  this->verticalLayout->setSpacing(0);
  this->verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
  this->verticalLayout->setContentsMargins(0, 0, 0, 0);
  this->SliceView = new qMRMLSliceView(this->frame);
  this->SliceView->setObjectName(QString::fromUtf8("SliceView"));
  this->SliceView->setProperty("renderEnabled", QVariant(true));

  this->verticalLayout->addWidget(this->SliceView);

  this->verticalLayout_2->addWidget(this->frame);

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->SliceController, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->SliceView, SLOT(setMRMLScene(vtkMRMLScene*)));

  QMetaObject::connectSlotsByName(q);

  this->SliceView->sliceViewInteractorStyle()
    ->SetSliceLogic(this->SliceController->sliceLogic());


  connect(this->SliceView, SIGNAL(resized(QSize)),
          this->SliceController, SLOT(setSliceViewSize(QSize)));

  connect(this->SliceController, SIGNAL(imageDataConnectionChanged(vtkAlgorithmOutput*)),
          this, SLOT(setImageDataConnection(vtkAlgorithmOutput*)));

  connect(this->SliceController, SIGNAL(renderRequested()),
          this->SliceView, SLOT(scheduleRender()), Qt::QueuedConnection);
}

// --------------------------------------------------------------------------
// qMRMLSliceView methods

// --------------------------------------------------------------------------
qMRMLSliceAstroWidget::qMRMLSliceAstroWidget(QWidget* _parent)
  : Superclass(new qMRMLSliceAstroWidgetPrivate(*this), _parent)
{
  Q_D(qMRMLSliceAstroWidget);
  d->init();
}

qMRMLSliceAstroWidget::qMRMLSliceAstroWidget(qMRMLSliceAstroWidgetPrivate *pimpl, QWidget *parent)
  : Superclass(pimpl, parent)
{
  // init() should be called in the subclass constructor
}

// --------------------------------------------------------------------------
qMRMLSliceAstroWidget::~qMRMLSliceAstroWidget()
{
}
