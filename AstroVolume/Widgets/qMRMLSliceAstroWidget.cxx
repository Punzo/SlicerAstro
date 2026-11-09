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
#include <vtkMRMLSliceViewInteractorStyle.h>

// MRML includes
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLScene.h>

// MRML logic includes
#include <vtkMRMLSliceLogic.h>

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

  this->setupUi(q);

  // Replace the standard slice controller by the astro one (which displays
  // world coordinates), keeping the rest of the standard qMRMLSliceWidget
  // layout (slice view frame and vertical slice controller).
  qMRMLSliceControllerWidget* standardController = this->SliceController;
  this->SliceController = new qMRMLSliceAstroControllerWidget(q);
  this->SliceController->setObjectName(QLatin1String("SliceController"));
  delete this->verticalLayout_2->replaceWidget(standardController, this->SliceController);
  delete standardController;

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->SliceController, SLOT(setMRMLScene(vtkMRMLScene*)));

  vtkMRMLSliceLogic* sliceLogic = this->SliceController->sliceLogic();
  this->SliceVerticalController->setSliceLogic(sliceLogic);
  this->SliceView->interactorObserver()->SetSliceLogic(sliceLogic);

  connect(this->SliceView, SIGNAL(resized(QSize)),
          this, SLOT(astroResetSliceViewSize(QSize)));

  connect(this->SliceController, SIGNAL(imageDataConnectionChanged(vtkAlgorithmOutput*)),
          this, SLOT(setImageDataConnection(vtkAlgorithmOutput*)));
  connect(this->SliceController, SIGNAL(renderRequested()),
          this->SliceView, SLOT(scheduleRender()), Qt::QueuedConnection);
  connect(this->SliceVerticalController, SIGNAL(renderRequested()),
          this->SliceView, SLOT(scheduleRender()), Qt::QueuedConnection);
  connect(this->SliceController, SIGNAL(nodeAboutToBeEdited(vtkMRMLNode*)),
          q, SIGNAL(nodeAboutToBeEdited(vtkMRMLNode*)));

  this->updateSliceOffsetSliderOrientation();
}

//---------------------------------------------------------------------------
void qMRMLSliceAstroWidgetPrivate::astroResetSliceViewSize(const QSize &size)
{
  Q_Q(qMRMLSliceAstroWidget);
  Q_UNUSED(size);

  this->setSliceViewSize(this->SliceView->size());
  emit q->windowsResized();
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
