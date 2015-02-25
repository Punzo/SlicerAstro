/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Jean-Christophe Fillion-Robin, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

// FooBar Widgets includes
#include "qSlicerAstroVolumeFooBarWidget.h"
#include "ui_qSlicerAstroVolumeFooBarWidget.h"

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroVolume
class qSlicerAstroVolumeFooBarWidgetPrivate
  : public Ui_qSlicerAstroVolumeFooBarWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroVolumeFooBarWidget);
protected:
  qSlicerAstroVolumeFooBarWidget* const q_ptr;

public:
  qSlicerAstroVolumeFooBarWidgetPrivate(
    qSlicerAstroVolumeFooBarWidget& object);
  virtual void setupUi(qSlicerAstroVolumeFooBarWidget*);
};

// --------------------------------------------------------------------------
qSlicerAstroVolumeFooBarWidgetPrivate
::qSlicerAstroVolumeFooBarWidgetPrivate(
  qSlicerAstroVolumeFooBarWidget& object)
  : q_ptr(&object)
{
}

// --------------------------------------------------------------------------
void qSlicerAstroVolumeFooBarWidgetPrivate
::setupUi(qSlicerAstroVolumeFooBarWidget* widget)
{
  this->Ui_qSlicerAstroVolumeFooBarWidget::setupUi(widget);
}

//-----------------------------------------------------------------------------
// qSlicerAstroVolumeFooBarWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroVolumeFooBarWidget
::qSlicerAstroVolumeFooBarWidget(QWidget* parentWidget)
  : Superclass( parentWidget )
  , d_ptr( new qSlicerAstroVolumeFooBarWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroVolumeFooBarWidget);
  d->setupUi(this);
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeFooBarWidget
::~qSlicerAstroVolumeFooBarWidget()
{
}
