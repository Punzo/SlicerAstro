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

  verticalLayout_2->removeWidget(SliceController);
  verticalLayout_2->removeWidget(frame);

  SliceController1 = new qMRMLSliceAstroControllerWidget(q);
  SliceController1->setObjectName(QString::fromUtf8("SliceController"));
  verticalLayout_2->addWidget(SliceController1);
  frame = new QFrame(q);
  frame->setObjectName(QString::fromUtf8("frame"));
  frame->setFrameShadow(QFrame::Raised);
  verticalLayout = new QVBoxLayout(frame);
  verticalLayout->setSpacing(0);
  verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
  verticalLayout->setContentsMargins(0, 0, 0, 0);
  SliceView = new qMRMLSliceView(frame);
  SliceView->setObjectName(QString::fromUtf8("SliceView"));
  SliceView->setProperty("renderEnabled", QVariant(true));

  verticalLayout->addWidget(SliceView);

  verticalLayout_2->addWidget(frame);

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)), SliceController1, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)), SliceView, SLOT(setMRMLScene(vtkMRMLScene*)));

  QMetaObject::connectSlotsByName(q);

  this->SliceView->sliceViewInteractorStyle()
    ->SetSliceLogic(this->SliceController1->sliceLogic());

  connect(this->SliceView, SIGNAL(resized(QSize)),
          this->SliceController1, SLOT(setSliceViewSize(QSize)));

#if (VTK_MAJOR_VERSION <= 5)
  connect(this->SliceController1, SIGNAL(imageDataChanged(vtkImageData*)),
          this, SLOT(setImageData(vtkImageData*)));
#else
  connect(this->SliceController1, SIGNAL(imageDataConnectionChanged(vtkAlgorithmOutput*)),
          this, SLOT(setImageDataConnection(vtkAlgorithmOutput*)));
#endif
  connect(this->SliceController1, SIGNAL(renderRequested()),
          this->SliceView, SLOT(scheduleRender()), Qt::QueuedConnection);
}

// --------------------------------------------------------------------------
void qMRMLSliceAstroWidgetPrivate::endProcessing()
{
  // When a scene is closed, we need to reconfigure the SliceNode to
  // the size of the widget.
  QRect rect = this->SliceView->geometry();
  this->SliceController1->setSliceViewSize(QSize(rect.width(), rect.height()));
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

// --------------------------------------------------------------------------
qMRMLSliceAstroControllerWidget* qMRMLSliceAstroWidget::sliceController()const
{
  Q_D(const qMRMLSliceAstroWidget);
  return d->SliceController1;
}

//---------------------------------------------------------------------------
void qMRMLSliceAstroWidget::setMRMLSliceNode(vtkMRMLSliceNode* newSliceNode)
{
  Q_D(qMRMLSliceAstroWidget);

  d->SliceView->setMRMLSliceNode(newSliceNode);
  d->SliceController1->setMRMLSliceNode(newSliceNode);
}

//---------------------------------------------------------------------------
vtkMRMLSliceCompositeNode* qMRMLSliceAstroWidget::mrmlSliceCompositeNode()const
{
  Q_D(const qMRMLSliceAstroWidget);
  return d->SliceController1->mrmlSliceCompositeNode();
}

//---------------------------------------------------------------------------
void qMRMLSliceAstroWidget::setSliceViewName(const QString& newSliceViewName)
{
  Q_D(qMRMLSliceAstroWidget);
  d->SliceController1->setSliceViewName(newSliceViewName);

  // QColor sliceViewColor =
  //   qMRMLSliceController1Widget::sliceViewColor(newSliceViewName);

//Don't apply the color of the slice to the highlight box
//  double highlightedBoxColor[3];
//  highlightedBoxColor[0] = sliceViewColor.redF();
//  highlightedBoxColor[1] = sliceViewColor.greenF();
//  highlightedBoxColor[2] = sliceViewColor.blueF();
//  // Set the color associated with the highlightedBox
//  d->SliceView->setHighlightedBoxColor(highlightedBoxColor);
}

//---------------------------------------------------------------------------
QString qMRMLSliceAstroWidget::sliceViewName()const
{
  Q_D(const qMRMLSliceAstroWidget);
  return d->SliceController1->sliceViewName();
}

//---------------------------------------------------------------------------
void qMRMLSliceAstroWidget::setSliceViewLabel(const QString& newSliceViewLabel)
{
  Q_D(qMRMLSliceAstroWidget);
  d->SliceController1->setSliceViewLabel(newSliceViewLabel);
}

//---------------------------------------------------------------------------
QString qMRMLSliceAstroWidget::sliceViewLabel()const
{
  Q_D(const qMRMLSliceAstroWidget);
  return d->SliceController1->sliceViewLabel();
}

//---------------------------------------------------------------------------
void qMRMLSliceAstroWidget::setSliceViewColor(const QColor& newSliceViewColor)
{
  Q_D(qMRMLSliceAstroWidget);
  d->SliceController1->setSliceViewColor(newSliceViewColor);
}

//---------------------------------------------------------------------------
QColor qMRMLSliceAstroWidget::sliceViewColor()const
{
  Q_D(const qMRMLSliceAstroWidget);
  return d->SliceController1->sliceViewColor();
}

//---------------------------------------------------------------------------
void qMRMLSliceAstroWidget::setSliceOrientation(const QString& orientation)
{
  Q_D(qMRMLSliceAstroWidget);
  d->SliceController1->setSliceOrientation(orientation);
}

//---------------------------------------------------------------------------
QString qMRMLSliceAstroWidget::sliceOrientation()const
{
  Q_D(const qMRMLSliceAstroWidget);
  return d->SliceController1->sliceOrientation();
}

//---------------------------------------------------------------------------
#if (VTK_MAJOR_VERSION <= 5)
void qMRMLSliceAstroWidget::setImageData(vtkImageData* newImageData)
{
  Q_D(qMRMLSliceAstroWidget);
  d->SliceController1->setImageData(newImageData);
}
#else
void qMRMLSliceAstroWidget::setImageDataConnection(vtkAlgorithmOutput* newImageDataConnection)
{
  Q_D(qMRMLSliceAstroWidget);
  d->SliceController1->setImageDataConnection(newImageDataConnection);
}
#endif

//---------------------------------------------------------------------------
#if (VTK_MAJOR_VERSION <= 5)
vtkImageData* qMRMLSliceAstroWidget::imageData() const
{
  Q_D(const qMRMLSliceAstroWidget);
  return d->SliceController1->imageData();
}
#else
vtkAlgorithmOutput* qMRMLSliceAstroWidget::imageDataConnection() const
{
  Q_D(const qMRMLSliceAstroWidget);
  return d->SliceController1->imageDataConnection();
}
#endif

//---------------------------------------------------------------------------
vtkMRMLSliceNode* qMRMLSliceAstroWidget::mrmlSliceNode()const
{
  Q_D(const qMRMLSliceAstroWidget);
  return d->SliceController1->mrmlSliceNode();
}

//---------------------------------------------------------------------------
vtkMRMLSliceLogic* qMRMLSliceAstroWidget::sliceLogic()const
{
  Q_D(const qMRMLSliceAstroWidget);
  return d->SliceController1->sliceLogic();
}

// --------------------------------------------------------------------------
void qMRMLSliceAstroWidget::fitSliceToBackground()
{
  Q_D(qMRMLSliceAstroWidget);
  d->SliceController1->fitSliceToBackground();
}


// --------------------------------------------------------------------------
void qMRMLSliceAstroWidget::setSliceLogics(vtkCollection* logics)
{
  Q_D(qMRMLSliceAstroWidget);
  d->SliceController1->setSliceLogics(logics);
}


