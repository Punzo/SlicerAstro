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
#include <vtkCollection.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
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
  this->SliceController->setObjectName(QLatin1String("SliceController"));
  this->verticalLayout_2->addWidget(this->SliceController);
  this->frame = new QFrame(q);
  this->frame->setObjectName(QLatin1String("frame"));
  QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  sizePolicy.setHorizontalStretch(0);
  sizePolicy.setVerticalStretch(0);
  sizePolicy.setHeightForWidth(frame->sizePolicy().hasHeightForWidth());
  frame->setSizePolicy(sizePolicy);
  this->frame->setFrameShadow(QFrame::Raised);
  this->verticalLayout = new QVBoxLayout(this->frame);
  this->verticalLayout->setSpacing(0);
  this->verticalLayout->setObjectName(QLatin1String("verticalLayout"));
  this->verticalLayout->setContentsMargins(0, 0, 0, 0);
  this->SliceView = new qMRMLSliceView(this->frame);
  this->SliceView->setObjectName(QLatin1String("SliceView"));
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
