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
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtksys/SystemTools.hxx>

// SlicerQt includes
#include <qSlicerApplication.h>

// vtkSlicer includes
#include <vtkSlicerApplicationLogic.h>

//---------------------------------------------------------------------------
// qMRMLSliceAstroControllerWidgetPrivate methods

namespace
{
//---------------------------------------------------------------------------
template <typename T> T StringToNumber(const char* num)
{
  std::stringstream ss;
  ss << num;
  T result;
  return ss >> result ? result : 0;
}

//---------------------------------------------------------------------------
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
}
}// end namespace

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

  this->SliceOrientationSelector->setToolTip(QApplication::translate("qMRMLAstroSliceControllerWidget", "Slice orientation (XY, XZ, ZY, Reformat).", 0));
  // To Do: update To Slicer5
  //qMRMLOrientation XZOrientation = {qMRMLSliceControllerWidget::tr("S: "), qMRMLSliceControllerWidget::tr("S <-----> N")};
  //qMRMLOrientation ZYOrientation = {qMRMLSliceControllerWidget::tr("R: "), qMRMLSliceControllerWidget::tr("W <-----> E")};
  //qMRMLOrientation XYOrientation = {qMRMLSliceControllerWidget::tr("A: "), qMRMLSliceControllerWidget::tr("z <-----> Z")};
  //qMRMLOrientation obliqueOrientation = {qMRMLSliceControllerWidget::tr(""), qMRMLSliceControllerWidget::tr("Oblique")};

  //this->SliceOrientationToDescription["XZ"] = XZOrientation;
  //this->SliceOrientationToDescription["ZY"] = ZYOrientation;
  //this->SliceOrientationToDescription["XY"] = XYOrientation;
  //this->SliceOrientationToDescription["Reformat"] = obliqueOrientation;

  this->SliceOffsetSlider->setSpinBoxVisible(false);
  this->WCSDisplay = new QLabel();
  this->WCSDisplay->setObjectName(QLatin1String("WCSDisplay"));
  this->WCSDisplay->setEnabled(true);
  this->WCSDisplay->setFixedWidth(10);
  this->WCSDisplay->setText("");
  this->BarLayout->addWidget(this->WCSDisplay);
  this->app = 0;
  this->col = vtkSmartPointer<vtkCollection>::New();

  BeamButton = new QToolButton(SliceFrame);
  BeamButton->setObjectName(QLatin1String("BeamButton"));
  QIcon icon;
  icon.addFile(QLatin1String(":/Icons/Beam.png"), QSize(), QIcon::Normal, QIcon::Off);
  BeamButton->setIcon(icon);
  BeamButton->setAutoRaise(true);
  BeamButton->setToolTip("Show/hide the beam in the 2D XY view.");
  BeamButton->setVisible(false);
  BeamButton->setChecked(false);
  BeamButton->setCheckable(true);
  this->horizontalLayout_2->addWidget(BeamButton,0,Qt::AlignLeft);
  this->horizontalLayout_2->addStretch(1);
  QObject::connect(MoreButton, SIGNAL(toggled(bool)),
                   BeamButton, SLOT(setVisible(bool)));
  QObject::connect(BeamButton, SIGNAL(toggled(bool)),
                   this, SLOT(onBeamToggled(bool)));

  RulerButton->setToolTip("Show/hide 2D axis in the 2D view.");
}

//---------------------------------------------------------------------------
void qMRMLSliceAstroControllerWidgetPrivate::setMRMLSliceNodeInternal(vtkMRMLSliceNode* newSliceNode)
{
  Q_Q(qMRMLSliceAstroControllerWidget);

  if (newSliceNode == q->mrmlSliceNode())
    {
    return;
    }

  this->qvtkReconnect(q->mrmlSliceNode(), newSliceNode, vtkCommand::ModifiedEvent,
                      this, SLOT(updateWidgetFromMRMLSliceNode()));
  this->qvtkReconnect(q->mrmlSliceNode(), newSliceNode, vtkCommand::ModifiedEvent,
                      this, SLOT(updateAstroWidgetFromMRMLSliceNode()));

  q->setMRMLSliceNode(newSliceNode);

  // Update widget state given the new slice node
  this->updateWidgetFromMRMLSliceNode();
  this->updateAstroWidgetFromMRMLSliceNode();
  // Enable/disable widget
  q->setDisabled(newSliceNode == 0);
}

//---------------------------------------------------------------------------
void qMRMLSliceAstroControllerWidgetPrivate::updateAstroWidgetFromMRMLSliceNode()
{
  Q_Q(qMRMLSliceAstroControllerWidget);

  if (!q->mrmlSliceNode())
    {
    return;
    }

  q->setWCSDisplay();

  if (!q->mrmlSliceNode()->GetAttribute("SlicerAstro.Beam"))
    {
    return;
    }

  if (!strcmp(q->mrmlSliceNode()->GetAttribute("SlicerAstro.Beam"), "on"))
    {
    this->BeamButton->setChecked(true);
    }
  else
    {
    this->BeamButton->setChecked(false);
    }
}

//---------------------------------------------------------------------------
void qMRMLSliceAstroControllerWidgetPrivate::onBeamToggled(bool toggled)
{
  Q_Q(qMRMLSliceAstroControllerWidget);

  vtkSmartPointer<vtkCollection> sliceNodes = vtkSmartPointer<vtkCollection>::Take
    (q->mrmlScene()->GetNodesByClass("vtkMRMLSliceNode"));

  for (int ii = 0; ii < sliceNodes->GetNumberOfItems(); ii++)
    {
    vtkMRMLSliceNode* sliceNode =
      vtkMRMLSliceNode::SafeDownCast(sliceNodes->GetItemAsObject(ii));
    if (!sliceNode)
      {
      return;
      }

    if ((sliceNode == q->mrmlSliceNode() || q->isLinked()) && toggled)
      {
      sliceNode->SetAttribute("SlicerAstro.Beam", "on");
      }
    else if ((sliceNode == q->mrmlSliceNode() || q->isLinked()) && !toggled)
      {
      sliceNode->SetAttribute("SlicerAstro.Beam", "off");
      }
    }
}

//---------------------------------------------------------------------------
// qMRMLSliceView methods

//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
qMRMLSliceAstroControllerWidget::~qMRMLSliceAstroControllerWidget()
{
}

//---------------------------------------------------------------------------
void qMRMLSliceAstroControllerWidget::setWCSDisplay()
{
  Q_D(qMRMLSliceAstroControllerWidget);

  if (!this->mrmlSliceNode() || !d->col || !d->WCSDisplay)
    {
    return;
    }

  std::string orientation = this->mrmlSliceNode()->GetOrientation();

  if (!orientation.compare("Reformat"))
    {
    d->WCSDisplay->setText("");
    d->WCSDisplay->setFixedWidth(10);
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
    d->WCSDisplay->setFixedWidth(10);
    return;
    }

  d->col->RemoveAllItems();

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
    d->WCSDisplay->setFixedWidth(10);
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

    vtkMRMLAstroLabelMapVolumeNode* astroLabelMapVolume =
      vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
        (sliceLayerLogic->GetVolumeNode());

    vtkMRMLAstroLabelMapVolumeDisplayNode* astroLabelMapDisplay =
      vtkMRMLAstroLabelMapVolumeDisplayNode::SafeDownCast
        (sliceLayerLogic->GetVolumeDisplayNode());

    if (astroVolume && displayNode)
      {

      if (StringToInt(astroVolume->GetAttribute("SlicerAstro.NAXIS")) < 3)
        {
        hasDisplay = false;
        break;
        }

      hasDisplay = true;

      if (!strcmp(displayNode->GetSpace(), "WCS"))
        {
        double offset = this->mrmlSliceNode()->GetSliceOffset();
        double world [] = {0., 0., 0.};
        double ijk [] = {0., 0., 0.};
        vtkNew<vtkMatrix4x4> RAStoIJK;
        astroVolume->GetRASToIJKMatrix(RAStoIJK.GetPointer());
        int Dims[3];
        astroVolume->GetImageData()->GetDimensions(Dims);
        double OriginRAS[3];
        astroVolume->GetOrigin(OriginRAS);
        double Spacing[3];
        astroVolume->GetSpacing(Spacing);

        ijk[0] = Dims[0] * 0.5;
        ijk[1] = Dims[1] * 0.5;
        ijk[2] = Dims[2] * 0.5;

        if(!orientation.compare("XZ"))
          {
          offset -= OriginRAS[2];
          ijk[1] = offset * RAStoIJK->GetElement(0, 1) * Spacing[0] +
                   offset * RAStoIJK->GetElement(1, 1) * Spacing[1] +
                   offset * RAStoIJK->GetElement(2, 1) * Spacing[2];
          ijk[1] /= Spacing[1];
          }
        else if(!orientation.compare("XY"))
          {
          offset -= OriginRAS[1];
          ijk[2] = offset * RAStoIJK->GetElement(0, 2) * Spacing[0] +
                   offset * RAStoIJK->GetElement(1, 2) * Spacing[1] +
                   offset * RAStoIJK->GetElement(2, 2) * Spacing[2];
          ijk[2] /= Spacing[2];
          }
        else if(!orientation.compare("ZY"))
          {
          offset -= OriginRAS[0];
          ijk[0] = offset * RAStoIJK->GetElement(0, 0) * Spacing[0] +
                   offset * RAStoIJK->GetElement(1, 0) * Spacing[1] +
                   offset * RAStoIJK->GetElement(2, 0) * Spacing[2];
          ijk[0] /= Spacing[0];
          }
        else
          {
          d->WCSDisplay->setText("");
          d->WCSDisplay->setFixedWidth(10);
          break;
          }

        displayNode->GetReferenceSpace(ijk, world);
        d->WCSDisplay->setFixedWidth(115);

        double outputValues[3] = {0.}, oldOutputValues[3] = {0.};
        if(!orientation.compare("XZ"))
          {
          d->WCSDisplay->setText(QString::fromStdString(astroVolume->GetAstroVolumeDisplayNode()
                                 ->GetDisplayStringFromValueY(world[1], oldOutputValues, outputValues, 3)));
          }
        else if(!orientation.compare("XY"))
          {
          d->WCSDisplay->setText(QString::fromStdString(astroVolume->GetAstroVolumeDisplayNode()
                                 ->GetDisplayStringFromValueZ(world[2], oldOutputValues, outputValues, 3)));
          }
        else if(!orientation.compare("ZY"))
          {
          d->WCSDisplay->setText(QString::fromStdString(astroVolume->GetAstroVolumeDisplayNode()
                                 ->GetDisplayStringFromValueX(world[0], oldOutputValues, outputValues, 3)));
          }
        }
      break;
      }
    else if (astroLabelMapVolume && astroLabelMapDisplay)
      {

      if (StringToInt(astroLabelMapVolume->GetAttribute("SlicerAstro.NAXIS")) < 3)
        {
        hasDisplay = false;
        break;
        }

      hasDisplay = true;
      if (!strcmp(astroLabelMapDisplay->GetSpace(), "WCS"))
        {
        double offset = this->mrmlSliceNode()->GetSliceOffset();
        double world [] = {0., 0., 0.};
        double ijk [] = {0., 0., 0.};
        vtkNew<vtkMatrix4x4> RAStoIJK;
        astroLabelMapVolume->GetRASToIJKMatrix(RAStoIJK.GetPointer());
        int Dims[3];
        astroLabelMapVolume->GetImageData()->GetDimensions(Dims);
        double OriginRAS[3];
        astroLabelMapVolume->GetOrigin(OriginRAS);
        double Spacing[3];
        astroLabelMapVolume->GetSpacing(Spacing);

        ijk[0] = Dims[0] * 0.5;
        ijk[1] = Dims[1] * 0.5;
        ijk[2] = Dims[2] * 0.5;

        if(!orientation.compare("XZ"))
          {
          offset -= OriginRAS[2];
          ijk[1] = offset * RAStoIJK->GetElement(0, 1) * Spacing[0] +
                   offset * RAStoIJK->GetElement(1, 1) * Spacing[1] +
                   offset * RAStoIJK->GetElement(2, 1) * Spacing[2];
          ijk[1] /= Spacing[1];
          }
        else if(!orientation.compare("XY"))
          {
          offset -= OriginRAS[1];
          ijk[2] = offset * RAStoIJK->GetElement(0, 2) * Spacing[0] +
                   offset * RAStoIJK->GetElement(1, 2) * Spacing[1] +
                   offset * RAStoIJK->GetElement(2, 2) * Spacing[2];
          ijk[2] /= Spacing[2];
          }
        else if(!orientation.compare("ZY"))
          {
          offset -= OriginRAS[0];
          ijk[0] = offset * RAStoIJK->GetElement(0, 0) * Spacing[0] +
                   offset * RAStoIJK->GetElement(1, 0) * Spacing[1] +
                   offset * RAStoIJK->GetElement(2, 0) * Spacing[2];
          ijk[0] /= Spacing[0];
          }
        else
          {
          d->WCSDisplay->setText("");
          d->WCSDisplay->setFixedWidth(10);
          break;
          }

        astroLabelMapDisplay->GetReferenceSpace(ijk, world);
        d->WCSDisplay->setFixedWidth(115);

        double outputValues[3] = {0.}, oldOutputValues[3] = {0.};
        if(!orientation.compare("XZ"))
          {
          d->WCSDisplay->setText(QString::fromStdString(astroLabelMapVolume->GetAstroLabelMapVolumeDisplayNode()
                                 ->GetDisplayStringFromValueY(world[1], outputValues, oldOutputValues, 3)));
          }
        else if(!orientation.compare("XY"))
          {
          d->WCSDisplay->setText(QString::fromStdString(astroLabelMapVolume->GetAstroLabelMapVolumeDisplayNode()
                                 ->GetDisplayStringFromValueZ(world[2], outputValues, oldOutputValues, 3)));
          }
        else if(!orientation.compare("ZY"))
          {
          d->WCSDisplay->setText(QString::fromStdString(astroLabelMapVolume->GetAstroLabelMapVolumeDisplayNode()
                                 ->GetDisplayStringFromValueX(world[0], outputValues, oldOutputValues, 3)));
          }
        }
      break;
      }
    else
      {
      continue;
      }
    }

    if (!hasDisplay)
      {
      d->WCSDisplay->setText("");
      d->WCSDisplay->setFixedWidth(10);
    }
}
