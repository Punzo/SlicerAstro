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

#include "qSlicerAstroScalarVolumeDisplayWidget.h"
#include "ui_qSlicerAstroScalarVolumeDisplayWidget.h"

// Qt includes
#include <QApplication>
#include <QLabel>
#include <QSignalBlocker>
#include <QStyle>
#include <QIcon>
#include <QHBoxLayout>
#include <QToolButton>
#include <QMessageBox>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QDebug>

// SlicerQt includes
#include <qMRMLSliceAstroWidget.h>
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerAbstractModuleFactoryManager.h>
#include <qSlicerApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleFactoryManager.h>
#include <qSlicerModuleManager.h>
#include <vtkSlicerApplicationLogic.h>
#include <atomic>
#include <cmath>
#include <vtkSegmentationConverter.h>
#include <vtkSegmentation.h>
#include <vtkSegment.h>
#include "qSlicerAstroVolumeModuleWidget.h"

// MRML includes
#include <vtkMRMLAstroReprojectParametersNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLColorNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSliceLayerLogic.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

// VTK includes
#include <vtkAlgorithm.h>
#include <vtkAlgorithmOutput.h>
#include <vtkColorTransferFunction.h>
#include <vtkDoubleArray.h>
#include <vtkGeneralTransform.h>
#include <vtkImageData.h>
#include <vtkImageThreshold.h>
#include <vtkLookupTable.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkOrientedImageData.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>

// CTK includes

// logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerSegmentationsModuleLogic.h>

// STD includes
#include <limits>

//-----------------------------------------------------------------------------
namespace
{
//-----------------------------------------------------------------------------
// Color function options of SlicerAstro (attributes "on"/"off" of the color
// nodes); color nodes of Slicer have none of these attributes
bool IsColorAttributeOn(vtkMRMLNode* colorNode, const char* attribute)
{
  const char* value = colorNode ? colorNode->GetAttribute(attribute) : nullptr;
  return value && !strcmp(value, "on");
}

vtkSlicerAstroVolumeLogic* AstroVolumeLogic()
{
  qSlicerAbstractCoreModule* astroVolumeModule = (qSlicerApplication::application() &&
    qSlicerApplication::application()->moduleManager()) ?
    qSlicerApplication::application()->moduleManager()->module("AstroVolume") : nullptr;
  return astroVolumeModule ? vtkSlicerAstroVolumeLogic::SafeDownCast(astroVolumeModule->logic()) : nullptr;
}

// contours segmentation of the active volume (contours drawn over it)
vtkMRMLSegmentationNode* ContoursSegmentationNode(vtkMRMLAstroVolumeNode* activeVolume, bool create = false)
{
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  vtkMRMLSegmentationNode* node = (logic && activeVolume) ?
    logic->GetContoursSegmentationForVolume(activeVolume, create) : nullptr;
  return (node && node->GetSegmentation()) ? node : nullptr;
}

// only the contours of the data (segments tagged with its ID) are shown
void ShowContoursOfData(vtkMRMLSegmentationNode* contours, vtkMRMLAstroVolumeNode* data)
{
  vtkMRMLSegmentationDisplayNode* displayNode = contours ?
    vtkMRMLSegmentationDisplayNode::SafeDownCast(contours->GetDisplayNode()) : nullptr;
  if (!displayNode || !data || !data->GetID())
    {
    return;
    }
  std::vector<std::string> segmentIDs;
  contours->GetSegmentation()->GetSegmentIDs(segmentIDs);
  for (const std::string& segmentID : segmentIDs)
    {
    vtkSegment* segment = contours->GetSegmentation()->GetSegment(segmentID);
    std::string sourceVolumeID;
    const bool ofData = segment &&
      segment->GetTag(vtkSlicerAstroVolumeLogic::GetContoursVolumeTagName(), sourceVolumeID) &&
      sourceVolumeID == data->GetID();
    displayNode->SetSegmentVisibility(segmentID, ofData);
    }
}
} // end of anonymous namespace

/// \ingroup SlicerAstro_QtModules_Volumes
class qSlicerAstroScalarVolumeDisplayWidgetPrivate
  : public Ui_qSlicerAstroScalarVolumeDisplayWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroScalarVolumeDisplayWidget);
protected:
  qSlicerAstroScalarVolumeDisplayWidget* const q_ptr;
public:
  qSlicerAstroScalarVolumeDisplayWidgetPrivate(qSlicerAstroScalarVolumeDisplayWidget& object);
  ~qSlicerAstroScalarVolumeDisplayWidgetPrivate();
  void init();

  vtkSmartPointer<vtkColorTransferFunction> ColorTransferFunction;
  vtkWeakPointer<vtkMRMLAstroVolumeNode> contoursVolumeNode;
  QLabel* ChannelVelocityLabel = nullptr;
  QToolButton* ShowContoursButton = nullptr;
  QToolButton* SmoothContoursButton = nullptr;
  QToolButton* DeleteContoursButton = nullptr;
  int nameIndex;
};

//-----------------------------------------------------------------------------
qSlicerAstroScalarVolumeDisplayWidgetPrivate::qSlicerAstroScalarVolumeDisplayWidgetPrivate(
  qSlicerAstroScalarVolumeDisplayWidget& object)
  : q_ptr(&object)
{
  this->ColorTransferFunction = vtkSmartPointer<vtkColorTransferFunction>::New();
  this->contoursVolumeNode = 0;
  this->nameIndex = 0;
}

//-----------------------------------------------------------------------------
qSlicerAstroScalarVolumeDisplayWidgetPrivate::~qSlicerAstroScalarVolumeDisplayWidgetPrivate()
{
}

namespace
{
//----------------------------------------------------------------------------
template <typename T> T StringToNumber(const char* num)
{
  std::stringstream ss;
  ss << num;
  T result;
  return ss >> result ? result : 0;
}

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

//----------------------------------------------------------------------------
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
}

//----------------------------------------------------------------------------
template <typename T> std::string NumberToString(T V)
{
  std::string stringValue;
  std::stringstream strstream;
  strstream << V;
  strstream >> stringValue;
  return stringValue;
}

//----------------------------------------------------------------------------
std::string IntToString(int Value)
{
  return NumberToString<int>(Value);
}

} // end namespace

//-----------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidgetPrivate::init()
{
  Q_Q(qSlicerAstroScalarVolumeDisplayWidget);

  this->setupUi(q);

  // only a color table chosen by the user is applied: the selector also
  // changes its current node when its list is rebuilt (e.g. it selects the
  // first color table while a scene is loaded, with Qt6), which must not
  // replace the color table of the volume
  QObject::connect(this->ColorTableComboBox, SIGNAL(nodeActivated(vtkMRMLNode*)),
                   q, SLOT(setColorNode(vtkMRMLNode*)));
  QObject::connect(this->ReversePushButton, SIGNAL(toggled(bool)),
                   q, SLOT(setReverse(bool)));
  QObject::connect(this->InversePushButton, SIGNAL(toggled(bool)),
                   q, SLOT(setInverse(bool)));
  QObject::connect(this->LogPushButton, SIGNAL(toggled(bool)),
                   q, SLOT(setLog(bool)));
  QObject::connect(this->InterpolatePushButton, SIGNAL(toggled(bool)),
                   q, SLOT(setInterpolate(bool)));
  QObject::connect(this->FitSlicesToViewsPushButton, SIGNAL(toggled(bool)),
                   q, SLOT(onFitSlicesToViewsChanged(bool)));
  QObject::connect(this->ThresholdPushButton, SIGNAL(toggled(bool)),
                   q, SLOT(setThreshold(bool)));

  qSlicerApplication* app = qSlicerApplication::application();
  if (app && app->layoutManager())
    {
    qMRMLSliceAstroWidget *RedSliceWidget = qobject_cast<qMRMLSliceAstroWidget*>
      (app->layoutManager()->sliceWidget("Red"));

    QObject::connect(RedSliceWidget, SIGNAL(windowsResized()),
                     q, SLOT(ExtendAllSlices()));

    qMRMLSliceAstroWidget *YellowSliceWidget = qobject_cast<qMRMLSliceAstroWidget*>
      (app->layoutManager()->sliceWidget("Yellow"));

    QObject::connect(YellowSliceWidget, SIGNAL(windowsResized()),
                     q, SLOT(ExtendAllSlices()));

    qMRMLSliceAstroWidget *GreenSliceWidget = qobject_cast<qMRMLSliceAstroWidget*>
      (app->layoutManager()->sliceWidget("Green"));

    QObject::connect(GreenSliceWidget, SIGNAL(windowsResized()),
                     q, SLOT(ExtendAllSlices()));
    }

  this->ThresholdPushButton->setChecked(false);
  q->setThreshold(false);

  QObject::connect(this->ContoursVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onContoursVolumeChanged(vtkMRMLNode*)));
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->ContoursVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(this->ColorPickerButton, SIGNAL(colorChanged(QColor)),
                   q, SLOT(onColorChanged(QColor)));
  QObject::connect(this->ContourPushButton, SIGNAL(clicked()),
                   q, SLOT(onCreateContours()));
  QObject::connect(this->Contours2DSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onContours2DOriginChanged(double)));

  this->SliceSegmentationLabel->hide();
  this->Contours2DSliderWidget->hide();

  // channel map of 3D contours over a 2D image: channel and its velocity
  this->SliceSegmentationLabel->setText(qSlicerAstroScalarVolumeDisplayWidget::tr("Channel:"));
  this->SliceSegmentationLabel->setToolTip(qSlicerAstroScalarVolumeDisplayWidget::tr(
    "Channel map: channel of the contours data (3D) shown over the 2D image."));
  this->ChannelVelocityLabel = new QLabel(q);
  this->ChannelVelocityLabel->setObjectName("ChannelVelocityLabel");
  this->ChannelVelocityLabel->hide();
  QHBoxLayout* channelLayout = q->findChild<QHBoxLayout*>("horizontalLayout_10");
  if (channelLayout)
    {
    channelLayout->addWidget(this->ChannelVelocityLabel);
    }
  this->Contours2DSliderWidget->setToolTip(qSlicerAstroScalarVolumeDisplayWidget::tr(
    "Channel map: moves the 2D image along the spectral axis of the contours data (3D), "
    "so that the contours of the chosen channel are shown over the image. The velocity "
    "of the channel is shown on the right."));

  // contours: visibility, smooth 2D lines, deletion
  this->ShowContoursButton = new QToolButton(q);
  this->ShowContoursButton->setObjectName("ShowContoursButton");
  this->ShowContoursButton->setCheckable(true);
  this->ShowContoursButton->setChecked(true);
  this->ShowContoursButton->setIcon(QIcon(":/Icons/Small/SlicerVisible.png"));
  this->ShowContoursButton->setToolTip(qSlicerAstroScalarVolumeDisplayWidget::tr("Show or hide the contours."));
  this->SmoothContoursButton = new QToolButton(q);
  this->SmoothContoursButton->setObjectName("SmoothContoursButton");
  this->SmoothContoursButton->setCheckable(true);
  this->SmoothContoursButton->setText(qSlicerAstroScalarVolumeDisplayWidget::tr("Smooth"));
  this->SmoothContoursButton->setToolTip(qSlicerAstroScalarVolumeDisplayWidget::tr(
    "Show the contours in the 2D views as smooth lines (closed surfaces, computed in the background: "
    "it can take time for large data) instead of the outlines of the voxels."));
  this->DeleteContoursButton = new QToolButton(q);
  this->DeleteContoursButton->setObjectName("DeleteContoursButton");
  this->DeleteContoursButton->setIcon(q->style()->standardIcon(QStyle::SP_TrashIcon));
  this->DeleteContoursButton->setToolTip(qSlicerAstroScalarVolumeDisplayWidget::tr("Delete all the contours."));
  QHBoxLayout* contoursLayout = q->findChild<QHBoxLayout*>("horizontalLayout_5");
  if (contoursLayout)
    {
    contoursLayout->addWidget(this->ShowContoursButton);
    contoursLayout->addWidget(this->SmoothContoursButton);
    contoursLayout->addWidget(this->DeleteContoursButton);
    }
  QObject::connect(this->ShowContoursButton, SIGNAL(toggled(bool)), q, SLOT(onShowContoursToggled(bool)));
  QObject::connect(this->SmoothContoursButton, SIGNAL(toggled(bool)), q, SLOT(onSmoothContoursToggled(bool)));
  QObject::connect(this->DeleteContoursButton, SIGNAL(clicked()), q, SLOT(onDeleteContours()));
  q->updateContoursButtons();
}

// --------------------------------------------------------------------------
qSlicerAstroScalarVolumeDisplayWidget::qSlicerAstroScalarVolumeDisplayWidget(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroScalarVolumeDisplayWidgetPrivate(*this))
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);
  d->init();

  this->contoursSegNode = nullptr;
  // disable as there is not MRML Node associated with the widget
  this->setEnabled(false);
}

// --------------------------------------------------------------------------
qSlicerAstroScalarVolumeDisplayWidget::~qSlicerAstroScalarVolumeDisplayWidget()
{
  // the contours segmentation belongs to its volume (not removed here)
  this->contoursSegNode = nullptr;
}

// --------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroScalarVolumeDisplayWidget::volumeNode()const
{
  Q_D(const qSlicerAstroScalarVolumeDisplayWidget);
  return vtkMRMLAstroVolumeNode::SafeDownCast(
    d->MRMLWindowLevelWidget->mrmlVolumeNode());
}

// --------------------------------------------------------------------------
bool qSlicerAstroScalarVolumeDisplayWidget::isColorTableComboBoxEnabled()const
{
  Q_D(const qSlicerAstroScalarVolumeDisplayWidget);
  return d->ColorTableComboBox->isEnabled();
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setColorTableComboBoxEnabled(bool enable)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);
  d->ColorTableComboBox->setEnabled(enable);
}

// --------------------------------------------------------------------------
bool qSlicerAstroScalarVolumeDisplayWidget::isMRMLWindowLevelWidgetEnabled()const
{
  Q_D(const qSlicerAstroScalarVolumeDisplayWidget);
  return d->MRMLWindowLevelWidget->isEnabled();
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setMRMLWindowLevelWidgetEnabled(bool enable)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);
  d->MRMLWindowLevelWidget->setEnabled(enable);
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::ExtendAllSlices()
{
  if (!this->mrmlScene() || !this->volumeNode() ||
      !this->volumeNode()->GetImageData())
    {
    return;
    }

  qSlicerApplication* app = qSlicerApplication::application();
  if (!app || !app->applicationLogic())
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();

  if (!displayNode)
    {
    return;
    }

  int dims[3];
  vtkSmartPointer<vtkCollection> sliceNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLSliceNode"));

  for(int sliceIndex = 0; sliceIndex < sliceNodes->GetNumberOfItems(); sliceIndex++)
    {
    vtkMRMLSliceNode* sliceNode =
        vtkMRMLSliceNode::SafeDownCast(sliceNodes->GetItemAsObject(sliceIndex));
    if (!sliceNode)
      {
      continue;
      }
    if (strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeRed") &&
        strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeYellow") &&
        strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeGreen"))
      {
      continue;
      }

    vtkMRMLSliceLogic* sliceLogic = app->applicationLogic()->GetSliceLogic(sliceNode);
    if (!sliceLogic)
      {
      continue;
      }

    vtkSmartPointer<vtkCollection> col = vtkSmartPointer<vtkCollection>::New();

    col->AddItem(sliceLogic->GetBackgroundLayer());
    col->AddItem(sliceLogic->GetForegroundLayer());

    bool apply = false;
    for (int layer = 0; layer < col->GetNumberOfItems(); layer++)
      {
      vtkMRMLSliceLayerLogic* sliceLayerLogic =
        vtkMRMLSliceLayerLogic::SafeDownCast
          (col->GetItemAsObject(layer));

      if(!sliceLayerLogic)
        {
        return;
        }

      vtkMRMLAstroVolumeNode* astroVolumeNode =
        vtkMRMLAstroVolumeNode::SafeDownCast
          (sliceLayerLogic->GetVolumeNode());

      if (!astroVolumeNode || !astroVolumeNode->GetImageData() ||
          !astroVolumeNode->GetAstroVolumeDisplayNode())
        {
        continue;
        }

      if (!strcmp(astroVolumeNode->GetName(), this->volumeNode()->GetName()) &&
          displayNode->GetFitSlices())
        {
        this->volumeNode()->GetImageData()->GetDimensions(dims);
        apply = true;
        break;
        }
      else if (astroVolumeNode->GetAstroVolumeDisplayNode()->GetFitSlices())
        {
        astroVolumeNode->GetImageData()->GetDimensions(dims);
        apply = true;
        break;
        }
      }

    col->RemoveAllItems();

    if (!apply)
      {
      sliceLogic->FitSliceToAll(true);
      continue;
      }

    double rasDimensions[3], rasCenter[3];
    sliceLogic->GetVolumeRASBox(this->volumeNode(), rasDimensions, rasCenter);

    double FieldOfView[3];
    sliceNode->GetFieldOfView(FieldOfView);
    if (!sliceNode->GetOrientation().compare("PVMajor") ||
        !sliceNode->GetOrientation().compare("PVMinor") ||
        !sliceNode->GetOrientation().compare("Reformat"))
      {
      FieldOfView[0] = sqrt(dims[0] * dims[0] + dims[1] * dims[1]);
      FieldOfView[1] = dims[2] + (dims[2] * 0.2);
      }
    else if (!sliceNode->GetOrientation().compare("XY") ||
             !sliceNode->GetOrientation().compare("PVDiagram"))
      {
      FieldOfView[0] = dims[0];
      FieldOfView[1] = dims[1];
      }
    else if (!sliceNode->GetOrientation().compare("ZY"))
      {
      FieldOfView[0] = dims[2];
      FieldOfView[1] = dims[1];
      }
    else if (!sliceNode->GetOrientation().compare("XZ"))
      {
      FieldOfView[0] = dims[0];
      FieldOfView[1] = dims[2];
      }
    else
      {
      continue;
      }

    sliceNode->SetFieldOfView(FieldOfView[0], FieldOfView[1], FieldOfView[2]);

    //
    // set the origin to be the center of the volume in RAS
    //
    vtkNew<vtkMatrix4x4> sliceToRAS;
    sliceToRAS->DeepCopy(sliceNode->GetSliceToRAS());
    sliceToRAS->SetElement(0, 3, rasCenter[0]);
    sliceToRAS->SetElement(1, 3, rasCenter[1]);
    sliceToRAS->SetElement(2, 3, rasCenter[2]);
    sliceNode->GetSliceToRAS()->DeepCopy(sliceToRAS.GetPointer());
    sliceNode->SetSliceOrigin(0,0,0);

    sliceLogic->SnapSliceOffsetToIJK();
    sliceNode->UpdateMatrices();
    sliceNode->Modified();
    }
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::onFitSlicesToViewsChanged(bool toggled)
{
  vtkMRMLAstroVolumeDisplayNode* astroDisplayNode = this->volumeDisplayNode();
  if (!astroDisplayNode)
    {
    return;
    }

  astroDisplayNode->SetFitSlices(toggled);
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::onColorChanged(QColor color)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  if (!d->contoursVolumeNode)
    {
    QString message = QString("Contours volume not found.");
    qWarning() << Q_FUNC_INFO << ": " << message;
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* astroDisplayNode =
    d->contoursVolumeNode->GetAstroVolumeDisplayNode();
  if (!astroDisplayNode)
    {
    QString message = QString("Contours display node not found.");
    qWarning() << Q_FUNC_INFO << ": " << message;
    return;
    }

  astroDisplayNode->SetContoursColor(0, color.red() / 255.);
  astroDisplayNode->SetContoursColor(1, color.green() / 255.);
  astroDisplayNode->SetContoursColor(2, color.blue() / 255.);

  // the contours of these data already shown take the new color
  vtkMRMLSegmentationNode* contoursSegmentationNode = ContoursSegmentationNode(this->volumeNode());
  vtkSegmentation* contoursSegmentation = contoursSegmentationNode ?
    contoursSegmentationNode->GetSegmentation() : nullptr;
  if (!contoursSegmentation)
    {
    return;
    }
  std::vector<std::string> segmentIDs;
  contoursSegmentation->GetSegmentIDs(segmentIDs);
  for (const std::string& segmentID : segmentIDs)
    {
    vtkSegment* segment = contoursSegmentation->GetSegment(segmentID);
    std::string sourceVolumeID;
    if (segment && segment->GetTag(vtkSlicerAstroVolumeLogic::GetContoursVolumeTagName(), sourceVolumeID) &&
        sourceVolumeID == d->contoursVolumeNode->GetID())
      {
      segment->SetColor(color.redF(), color.greenF(), color.blueF());
      }
    }
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::onContours2DOriginChanged(double value)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  vtkMRMLAstroVolumeNode *astroVolume = this->volumeNode();

  if (!astroVolume)
    {
    return;
    }

  if (!d->contoursVolumeNode)
    {
    return;
    }

  vtkNew<vtkGeneralTransform> IJKtoRASTransform;
  IJKtoRASTransform->Identity();
  IJKtoRASTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> IJKtoRASMatrix;
  astroVolume->GetIJKToRASMatrix(IJKtoRASMatrix.GetPointer());
  IJKtoRASMatrix->SetElement(0,3,0.);
  IJKtoRASMatrix->SetElement(1,3,0.);
  IJKtoRASMatrix->SetElement(2,3,0.);
  IJKtoRASTransform->Concatenate(IJKtoRASMatrix.GetPointer());
  vtkImageData* image = astroVolume->GetImageData();
  if (!image)
    {
    return;
    }
  int* dims = image->GetDimensions();
  vtkImageData* contoursImage = d->contoursVolumeNode->GetImageData();
  if (!contoursImage)
    {
    return;
    }
  int* contoursDims = contoursImage->GetDimensions();
  double IJKOrigin[3] = {0.}, RASOrigin[3] = {0.};
  IJKOrigin[0] = - dims[0] * 0.5;
  IJKOrigin[1] = - dims[1] * 0.5;
  IJKOrigin[2] = value - contoursDims[2] * 0.5;
  IJKtoRASTransform->TransformPoint(IJKOrigin, RASOrigin);
  astroVolume->SetOrigin(RASOrigin);
  this->updateContoursChannelLabel();
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::updateContoursChannelLabel()
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  // channel of the contours data at the plane of the 2D image, and its velocity
  const bool shown = d->Contours2DSliderWidget->isVisibleTo(this);
  d->SliceSegmentationLabel->setVisible(shown);
  d->ChannelVelocityLabel->setVisible(shown);
  vtkMRMLAstroVolumeNode* image = this->volumeNode();
  vtkMRMLAstroVolumeNode* contoursData = d->contoursVolumeNode;
  vtkMRMLAstroVolumeDisplayNode* contoursDisplay = contoursData ? contoursData->GetAstroVolumeDisplayNode() : nullptr;
  if (!shown || !image || !image->GetImageData() || !contoursData || !contoursData->GetImageData() ||
      !contoursDisplay || !contoursDisplay->GetWCSStruct())
    {
    d->ChannelVelocityLabel->clear();
    return;
    }
  int imageDims[3] = {0, 0, 0}, contoursDims[3] = {0, 0, 0};
  image->GetImageData()->GetDimensions(imageDims);
  contoursData->GetImageData()->GetDimensions(contoursDims);
  vtkNew<vtkMatrix4x4> imageIJKToRAS, contoursRASToIJK;
  image->GetIJKToRASMatrix(imageIJKToRAS.GetPointer());
  contoursData->GetRASToIJKMatrix(contoursRASToIJK.GetPointer());
  double imageCenter[4] = {imageDims[0] * 0.5, imageDims[1] * 0.5, 0., 1.}, ras[4] = {0., 0., 0., 1.};
  double contoursIJK[4] = {0., 0., 0., 1.};
  imageIJKToRAS->MultiplyPoint(imageCenter, ras);
  contoursRASToIJK->MultiplyPoint(ras, contoursIJK);
  const int channel = static_cast<int>(std::lround(contoursIJK[2]));
  if (channel < 0 || channel >= contoursDims[2])
    {
    d->ChannelVelocityLabel->setText(tr("outside of the data"));
    return;
    }
  double ijk[3] = {contoursDims[0] * 0.5, contoursDims[1] * 0.5, static_cast<double>(channel)};
  double world[3] = {0., 0., 0.};
  if (!contoursDisplay->GetReferenceSpace(ijk, world))
    {
    d->ChannelVelocityLabel->setText(tr("channel %1").arg(channel));
    return;
    }
  const wcsprm* wcs = contoursDisplay->GetWCSStruct();
  const bool metersPerSecond = wcs->naxis > 2 && !strcmp(wcs->cunit[2], "m/s");
  const double velocity = metersPerSecond ? world[2] * 0.001 : world[2];
  d->ChannelVelocityLabel->setText(tr("channel %1: %2 %3 (%4)")
    .arg(channel)
    .arg(velocity, 0, 'f', 1)
    .arg(metersPerSecond ? QString("km/s") : QString(wcs->naxis > 2 ? wcs->cunit[2] : ""))
    .arg(QString::fromStdString(contoursDisplay->GetVelocityDefinition())));
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::onContoursVolumeChanged(vtkMRMLNode *node)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  vtkMRMLAstroVolumeNode* volumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(node);

  if (d->contoursVolumeNode == volumeNode)
    {
    return;
    }

  d->contoursVolumeNode = volumeNode;

  qvtkReconnect(d->contoursVolumeNode, vtkCommand::ModifiedEvent,
                this, SLOT(updateWidgetFromContoursMRML()));

  this->updateWidgetFromContoursMRML();

  // new contours data (3D over a 2D image): channel map from the middle channel
  if (d->Contours2DSliderWidget->isVisibleTo(this) && d->contoursVolumeNode &&
      d->contoursVolumeNode->GetImageData())
    {
    int contoursDims[3] = {0, 0, 0};
    d->contoursVolumeNode->GetImageData()->GetDimensions(contoursDims);
    d->Contours2DSliderWidget->setValue(std::floor(contoursDims[2] * 0.5));
    // also when the value does not change (no valueChanged signal)
    this->onContours2DOriginChanged(d->Contours2DSliderWidget->value());
    }

  // the contours of these data are shown, the others hidden
  ShowContoursOfData(ContoursSegmentationNode(this->volumeNode()), d->contoursVolumeNode);
  this->updateContoursButtons();
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::onCreateContours()
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  if (!this->mrmlScene())
    {
    return;
    }

  QString ContourLevels = d->ContourLevelsLineEdit->text();
  std::string LevelsStdString = ContourLevels.toStdString();

  vtkMRMLAstroVolumeNode* masterVolume = this->volumeNode();
  if (!masterVolume)
    {
    QString message = QString("Master volume not found.");
    qWarning() << Q_FUNC_INFO << ": " << message;
    return;
    }

  vtkMRMLAstroVolumeNode* contoursVolume = d->contoursVolumeNode;
  if (!contoursVolume)
    {
    QString message = QString("Contours volume not found.");
    qWarning() << Q_FUNC_INFO << ": " << message;
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* astroDisplayNode = contoursVolume->GetAstroVolumeDisplayNode();
  if (!astroDisplayNode)
    {
    QString message = QString("Display node not found.");
    qWarning() << Q_FUNC_INFO << ": " << message;
    return;
    }

  // Contours of other data: reprojected on the celestial grid of the active
  // data (temporary volume with the channels of the other data). Rotated
  // grids and different celestial frames are handled by the reprojection.
  vtkMRMLAstroVolumeNode* temporaryVolume = nullptr;
  if (contoursVolume && masterVolume != contoursVolume)
    {
    const QString title = tr("Contours");
    qSlicerAbstractCoreModule* astroVolumeModule = qSlicerApplication::application() &&
      qSlicerApplication::application()->moduleManager() ?
      qSlicerApplication::application()->moduleManager()->module("AstroVolume") : nullptr;
    vtkSlicerAstroVolumeLogic* astroVolumeLogic = astroVolumeModule ?
      vtkSlicerAstroVolumeLogic::SafeDownCast(astroVolumeModule->logic()) : nullptr;
    if (!astroVolumeLogic)
      {
      qCritical() << Q_FUNC_INFO << ": AstroVolume logic not found.";
      return;
      }

    // same celestial grid: the contours are drawn from the data directly
    bool sameGrid = masterVolume->GetImageData() && contoursVolume->GetImageData() &&
      masterVolume->GetImageData()->GetDimensions()[0] == contoursVolume->GetImageData()->GetDimensions()[0] &&
      masterVolume->GetImageData()->GetDimensions()[1] == contoursVolume->GetImageData()->GetDimensions()[1];
    const char* gridKeys[] = {"CTYPE1", "CTYPE2", "CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2", "CDELT1", "CDELT2",
                              "CROTA2", "PC1_1", "PC1_2", "PC2_1", "PC2_2", "EQUINOX", "RADESYS"};
    for (const char* key : gridKeys)
      {
      const std::string attribute = std::string("SlicerAstro.") + key;
      const char* masterValue = masterVolume->GetAttribute(attribute.c_str());
      const char* contoursValue = contoursVolume->GetAttribute(attribute.c_str());
      if ((masterValue == nullptr) != (contoursValue == nullptr) ||
          (masterValue && strcmp(masterValue, contoursValue)))
        {
        sameGrid = false;
        }
      }

    if (sameGrid)
      {
      masterVolume = contoursVolume;
      }
    else
      {
      qInfo().noquote() << title << ":" << tr("the data of the contours are reprojected on the grid of %1 "
                                               "(bilinear interpolation).").arg(masterVolume->GetName());
      vtkNew<vtkMRMLAstroReprojectParametersNode> reprojectParaNode;
      reprojectParaNode->SetInputVolumeNodeID(contoursVolume->GetID());
      reprojectParaNode->SetInputVolumeNodeIDs(contoursVolume->GetID());
      reprojectParaNode->SetReferenceVolumeNodeID(masterVolume->GetID());
      reprojectParaNode->SetSpatialGridMode(vtkMRMLAstroReprojectParametersNode::SpatialReference);
      reprojectParaNode->SetSpectralGridMode(vtkMRMLAstroReprojectParametersNode::SpectralInput);
      reprojectParaNode->SetInterpolationOrder(vtkMRMLAstroReprojectParametersNode::Bilinear);
      if (!astroVolumeLogic->Reproject(reprojectParaNode))
        {
        const std::string error = astroVolumeLogic->GetLastReprojectError();
        if (error != "cancelled")
          {
          qSlicerAstroVolumeModuleWidget::informUser(this, QMessageBox::Warning, title,
            tr("The reprojection of %1 on the grid of %2 failed: %3. No contours will be generated.")
              .arg(contoursVolume->GetName(), masterVolume->GetName(), QString::fromStdString(error)));
          }
        return;
        }
      vtkMRMLAstroVolumeNode* outputVolume = vtkMRMLAstroVolumeNode::SafeDownCast(
        this->mrmlScene()->GetNodeByID(reprojectParaNode->GetOutputVolumeNodeID()));
      if (!outputVolume || !outputVolume->GetAstroVolumeDisplayNode())
        {
        qCritical() << Q_FUNC_INFO << ": the reprojected data of the contours were not found.";
        return;
        }

      // temporary volume: not rendered in 3D
      for (int ii = outputVolume->GetNumberOfDisplayNodes() - 1; ii >= 0; ii--)
        {
        if (vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(outputVolume->GetNthDisplayNode(ii)))
          {
          outputVolume->RemoveNthDisplayNodeID(ii);
          }
        }
      astroVolumeLogic->CenterVolume(masterVolume);
      astroVolumeLogic->CenterVolume(outputVolume);

      vtkMRMLAstroVolumeDisplayNode* outputDisplayNode = outputVolume->GetAstroVolumeDisplayNode();
      vtkMRMLAstroVolumeDisplayNode* referenceDisplayNode = masterVolume->GetAstroVolumeDisplayNode();
      outputDisplayNode->SetFitSlices(referenceDisplayNode->GetFitSlices());
      outputDisplayNode->SetContoursColor(referenceDisplayNode->GetContoursColor());
      outputDisplayNode->SetSpaceQuantities(referenceDisplayNode->GetSpaceQuantities());
      outputDisplayNode->SetSpace(referenceDisplayNode->GetSpace());

      temporaryVolume = outputVolume;
      masterVolume = outputVolume;
      }
    }

  // segments named after the data of the contours (not the reprojected copy)
  std::string NamePrefixStdString = contoursVolume->GetName();

  // contours segmentation of the active volume
  this->contoursSegNode = ContoursSegmentationNode(this->volumeNode(), true);
  if (!this->contoursSegNode)
    {
    qCritical() << Q_FUNC_INFO << ": the contours segmentation of the volume could not be created.";
    if (temporaryVolume)
      {
      this->mrmlScene()->RemoveNode(temporaryVolume);
      }
    QApplication::restoreOverrideCursor();
    return;
    }

  // the contours are not edited: the Segment Editor keeps its segmentation (mask)

  if (!this->contoursSegNode->GetDisplayNode())
    {
    this->contoursSegNode->CreateDefaultDisplayNodes();
    }

  vtkNew<vtkDoubleArray> Levels;
  vtkNew<vtkDoubleArray> Delimiters;
  vtkNew<vtkStringArray> SegmentIDs;
  std::stringstream ss(LevelsStdString);

  double value;
  double MIN = StringToDouble(masterVolume->GetAttribute("SlicerAstro.DATAMIN"));
  double MAX = StringToDouble(masterVolume->GetAttribute("SlicerAstro.DATAMAX"));
  double DisplayThreshold = StringToDouble(masterVolume->GetAttribute("SlicerAstro.DisplayThreshold"));
  if (DisplayThreshold < 1.E-6)
    {
    DisplayThreshold = (MAX - MIN) * 0.01;
    }

  bool convert = false;
  std::size_t found = LevelsStdString.find("DT");
  if (found != std::string::npos)
    {
    convert = true;
    }

  bool histo = false;
  found = LevelsStdString.find("Histogram");
  if (found != std::string::npos)
    {
    histo = true;
    }

  bool overlay = false;
  found = LevelsStdString.find("overlay");
  if (found != std::string::npos)
    {
    overlay = true;
    }

  bool list = false;
  bool increment = false;
  bool linearIncrement = false;
  bool nonLinearIncrement = false;
  bool renzogram = false;
  found = LevelsStdString.find(";");
  if (LevelsStdString.find(";") != std::string::npos)
    {
    list = true;
    }
  if (LevelsStdString.find(",") != std::string::npos)
    {
    renzogram = true;
    }
  if (LevelsStdString.find(":") != std::string::npos)
    {
    increment = true;
    }
  if (LevelsStdString.find("+") != std::string::npos)
    {
    linearIncrement = true;
    }
  if (LevelsStdString.find("*") != std::string::npos)
    {
    nonLinearIncrement = true;
    }

  if (!list && !increment && !renzogram && !histo)
    {
    list = true;
    }

  if ((!list && !increment && !renzogram && !histo) || (list && increment) || (renzogram && increment)
      || (increment && !linearIncrement && !nonLinearIncrement) ||
      (list && (linearIncrement || nonLinearIncrement)) ||
      (histo && (list || renzogram || increment || linearIncrement || nonLinearIncrement)))
    {
    QString message = QString("The input string defining the Contour Levels"
                              " is wrongly formatted. Check the ToolTip.");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to create Contours"), message);
    QApplication::restoreOverrideCursor();
    return;
    }

  if (convert)
    {
    for (int ii = 0; ii < 2; ii++)
      {
      ss.ignore();
      }
    }

  if (list)
    {
    while (ss >> value)
      {
      if (convert)
        {
        Levels->InsertNextValue(value * DisplayThreshold);
        }
      else
        {
        Levels->InsertNextValue(value);
        }
      if (ss.peek() == ';')
        {
        ss.ignore();
        }
      }
    }
  else if (renzogram)
    {
    while (ss >> value)
      {
      if (convert)
        {
        Levels->InsertNextValue(value * DisplayThreshold);
        }
      else
        {
        Levels->InsertNextValue(value);
        }
      if (ss.peek() == ';' || ss.peek() == ',')
        {
        ss.ignore();
        }
      }
    }
  else if (increment)
    {
    while (ss >> value)
      {
      Delimiters->InsertNextValue(value);
      if (ss.peek() == ':' || ss.peek() == '+' || ss.peek() == '*')
        {
        ss.ignore();
        }
      }

    if(Delimiters->GetNumberOfValues() != 3)
      {
      QString message = QString("The input string defining the Contour Levels"
                                " is formatted wrongly. Check the ToolTip.");
      qCritical() << Q_FUNC_INFO << ": " << message;
      QMessageBox::warning(NULL, tr("Failed to create Contours"), message);
      QApplication::restoreOverrideCursor();
      return;
      }

    double iter = Delimiters->GetValue(0);
    int cont = 1;
    while (fabs(iter) <= fabs(Delimiters->GetValue(1)))
      {
      if (convert)
        {
        Levels->InsertNextValue(iter * DisplayThreshold);
        }
      else
        {
        Levels->InsertNextValue(iter);
        }
      if (linearIncrement)
        {
        iter += Delimiters->GetValue(2);
        }
      else if (nonLinearIncrement)
        {
        iter += pow(Delimiters->GetValue(2), cont);
        cont++;
        }
      }
    }
  else if (histo)
    {
    renzogram = true;
    Levels->InsertNextValue(StringToDouble(masterVolume->GetAttribute("SlicerAstro.HistoMinSel")));
    Levels->InsertNextValue(StringToDouble(masterVolume->GetAttribute("SlicerAstro.HistoMaxSel")));
    }
  else
    {  
    qCritical() << Q_FUNC_INFO << ": the input string defining the Contour Levels is formatted wrongly ";
    QApplication::restoreOverrideCursor();
    return;
    }

  double range[2];
  Levels->GetRange(range);
  if ((range[0] < MIN) || (range[1] > MAX))
    {
    for (int ii = 0; ii < Levels->GetNumberOfValues(); ii++)
      {
      if (Levels->GetValue(ii) < MIN || Levels->GetValue(ii) > MAX)
        {
        Levels->RemoveTuple(ii);
        if (renzogram)
          {
          if (ii % 2 == 0)
            {
            Levels->RemoveTuple(ii+1);
            }
          else
            {
            Levels->RemoveTuple(ii-1);
            }
          }
        }
      }

    Levels->Squeeze();
    if (Levels->GetNumberOfValues() == 0)
      {
      QApplication::restoreOverrideCursor();
      if (temporaryVolume)
        {
        this->mrmlScene()->RemoveNode(temporaryVolume);
        }
      qSlicerAstroVolumeModuleWidget::informUser(this, QMessageBox::Warning, tr("Contours"),
        tr("All the contour levels are outside the range of the data (%1, %2): "
           "no contours will be generated.").arg(MIN).arg(MAX));
      return;
      }
    else
      {
      qSlicerAstroVolumeModuleWidget::informUser(this, QMessageBox::Warning, tr("Contours"),
        tr("Some contour levels are outside the range of the data (%1, %2): "
           "they are ignored.").arg(MIN).arg(MAX));
      }
    }

  // The new contours replace the previous ones: all of them, or with
  // "overlay" only those of the same data (contours of other data stay)
  vtkSegmentation* contoursSegmentation = this->contoursSegNode->GetSegmentation();
  std::vector<std::string> previousSegmentIDs;
  contoursSegmentation->GetSegmentIDs(previousSegmentIDs);
  for (const std::string& previousSegmentID : previousSegmentIDs)
    {
    vtkSegment* previousSegment = contoursSegmentation->GetSegment(previousSegmentID);
    std::string sourceVolumeID;
    if (!overlay || (previousSegment &&
        previousSegment->GetTag(vtkSlicerAstroVolumeLogic::GetContoursVolumeTagName(), sourceVolumeID) &&
        sourceVolumeID == contoursVolume->GetID()))
      {
      contoursSegmentation->RemoveSegment(previousSegmentID);
      }
    }

  // Create empty segment in current segmentation
  int LevelDim = 0;

  for (int ii = 0; ii < Levels->GetNumberOfValues(); ii++)
    {
    double ContourLevel = Levels->GetValue(ii);
    double ContourLevelNext = 0.;
    if (renzogram)
      {
      ii++;
      ContourLevelNext = Levels->GetValue(ii);
      }
    std::string SegmentID = NamePrefixStdString;
    SegmentID += "Contour" + IntToString(ii + 1);
    SegmentIDs->InsertNextValue(SegmentID.c_str());
    vtkNew<vtkImageThreshold> imageThreshold;
    imageThreshold->SetInputData(masterVolume->GetImageData());

    double lower, higher;
    if (!renzogram)
      {
      if (ContourLevel < 0.)
        {
        lower = MIN;
        higher = ContourLevel;
        }
      else
        {
        lower = ContourLevel;
        higher = MAX;
        }
      }
    else
      {
      if (ContourLevel < 0.)
        {
        lower = ContourLevelNext;
        higher = ContourLevel;
        }
      else
        {
        lower = ContourLevel;
        higher = ContourLevelNext;
        }
      }

    imageThreshold->ThresholdBetween(lower, higher);
    imageThreshold->SetInValue(1);
    imageThreshold->SetOutValue(0);
    imageThreshold->SetOutputScalarType(VTK_SHORT);
    imageThreshold->Update();
    vtkNew<vtkOrientedImageData> modifierLabelmap;
    modifierLabelmap->ShallowCopy(imageThreshold->GetOutput());
    vtkNew<vtkMatrix4x4> IJKToRASMatrix;
    masterVolume->GetIJKToRASMatrix(IJKToRASMatrix.GetPointer());
    modifierLabelmap->SetGeometryFromImageToWorldMatrix(IJKToRASMatrix.GetPointer());

    vtkSlicerAstroVolumeLogic::PrepareContoursUpdate(this->contoursSegNode);

    // Each contour level is a segment with its own labelmap (own layer, own
    // geometry): contours of data with different grids (e.g. 3D HI contours
    // reprojected on an optical image and 2D contours of the image) and
    // overlapping levels are kept as they are (a shared layer would resample
    // or overwrite them)
    vtkSegmentation* contoursSegmentation = this->contoursSegNode->GetSegmentation();
    if (contoursSegmentation->GetSegment(SegmentID))
      {
      contoursSegmentation->RemoveSegment(SegmentID);
      }
    vtkNew<vtkSegment> Segment;
    Segment->SetName(SegmentID.c_str());
    vtkDoubleArray* ContoursColor = astroDisplayNode->GetContoursColor();
    Segment->SetColor(ContoursColor->GetValue(0),
                      ContoursColor->GetValue(1),
                      ContoursColor->GetValue(2));
    // data of the contours: color changes and replacement of the contours
    Segment->SetTag(vtkSlicerAstroVolumeLogic::GetContoursVolumeTagName(), contoursVolume->GetID());
    Segment->SetLabelValue(1);
    Segment->AddRepresentation(vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName(),
                               modifierLabelmap.GetPointer());
    if (!contoursSegmentation->AddSegment(Segment, SegmentID))
      {
      qCritical() << Q_FUNC_INFO << ": failed to add the contour segment" << SegmentID.c_str();
      }

    int dims[3];
    modifierLabelmap->GetDimensions(dims);
    if (LevelDim < dims[0] * dims[1] * dims[2])
      {
      LevelDim = dims[0] * dims[1] * dims[2];
      }
    }

  for (int ii = 0; ii < this->contoursSegNode->GetNumberOfDisplayNodes(); ii++)
    {
    vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(this->contoursSegNode->GetNthDisplayNode(ii));
    if (!SegmentationDisplayNode)
      {
      continue;
      }
    if (!overlay)
      {
      SegmentationDisplayNode->SetAllSegmentsVisibility(false);
      }
    for (int jj = 0; jj < SegmentIDs->GetNumberOfValues(); jj++)
      {
      std::string SegmentID = SegmentIDs->GetValue(jj);
      SegmentationDisplayNode->SetSegmentVisibility(SegmentID, true);
      if (histo)
        {
        SegmentationDisplayNode->SetSegmentVisibility3D(SegmentID, true);
        }
      else
        {
        SegmentationDisplayNode->SetSegmentVisibility3D(SegmentID, false);
        }
      SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentID, false);
      SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentID, true);
      }
    }

  // Contours: closed surfaces in 2D, binary labelmaps in 3D (not a mask)
  vtkSlicerAstroVolumeLogic::SetContoursDisplay(this->contoursSegNode);

  if (temporaryVolume)
    {
    this->mrmlScene()->RemoveNode(temporaryVolume->GetAstroVolumeDisplayNode());
    this->mrmlScene()->RemoveNode(temporaryVolume);
    }

  QApplication::restoreOverrideCursor();

  // new contours: shown, outlines of the voxels in 2D (fast)
  if (this->contoursSegNode && this->contoursSegNode->GetDisplayNode())
    {
    this->contoursSegNode->GetDisplayNode()->SetVisibility(true);
    }
    {
    const QSignalBlocker showBlocker(d->ShowContoursButton);
    d->ShowContoursButton->setChecked(true);
    const QSignalBlocker smoothBlocker(d->SmoothContoursButton);
    d->SmoothContoursButton->setChecked(false);
    }
  this->updateContoursButtons();

}

// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::updateContoursButtons()
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);
  vtkMRMLSegmentationNode* node = ContoursSegmentationNode(this->volumeNode());
  const bool hasContours = node && node->GetSegmentation()->GetNumberOfSegments() > 0;
  d->ShowContoursButton->setEnabled(hasContours);
  d->SmoothContoursButton->setEnabled(hasContours);
  d->DeleteContoursButton->setEnabled(hasContours);
  if (hasContours && node->GetDisplayNode())
    {
    const QSignalBlocker blocker(d->ShowContoursButton);
    d->ShowContoursButton->setChecked(node->GetDisplayNode()->GetVisibility());
    }
  d->ShowContoursButton->setIcon(QIcon(d->ShowContoursButton->isChecked() ?
    ":/Icons/Small/SlicerVisible.png" : ":/Icons/Small/SlicerInvisible.png"));
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::onShowContoursToggled(bool show)
{
  vtkMRMLSegmentationNode* node = ContoursSegmentationNode(this->volumeNode());
  if (node && node->GetDisplayNode())
    {
    node->GetDisplayNode()->SetVisibility(show);
    }
  this->updateContoursButtons();
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::onDeleteContours()
{
  vtkMRMLSegmentationNode* node = ContoursSegmentationNode(this->volumeNode());
  if (node)
    {
    node->GetSegmentation()->RemoveAllSegments();
    }
  this->updateContoursButtons();
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::onSmoothContoursToggled(bool smooth)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);
  vtkMRMLSegmentationNode* node = ContoursSegmentationNode(this->volumeNode());
  if (!node)
    {
    return;
    }
  vtkSegmentation* segmentation = node->GetSegmentation();
  const char* labelmapName = vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName();
  const char* surfaceName = vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName();
  if (!smooth || segmentation->ContainsRepresentation(surfaceName))
    {
    vtkSlicerAstroVolumeLogic::SetContoursSmooth2D(node, smooth);
    return;
    }

  // Closed surfaces of the contours, in the background (one segment at a time:
  // progress and cancel). The worker converts copies of the segments (no
  // scene objects); the surfaces are added to the segments afterwards.
  std::vector<std::string> segmentIDs;
  segmentation->GetSegmentIDs(segmentIDs);
  std::vector<vtkSmartPointer<vtkSegmentation> > copies;
  for (const std::string& segmentID : segmentIDs)
    {
    vtkSmartPointer<vtkSegmentation> copy = vtkSmartPointer<vtkSegmentation>::New();
    copy->SetSourceRepresentationName(labelmapName);
    vtkNew<vtkSegment> segmentCopy;
    segmentCopy->DeepCopy(segmentation->GetSegment(segmentID));
    copy->AddSegment(segmentCopy, segmentID);
    copies.push_back(copy);
    }
  std::atomic<int> converted{0};
  std::atomic<bool> cancelled{false};
  const int total = static_cast<int>(copies.size());
  qSlicerAstroVolumeModuleWidget::runInBackground(this, tr("Computing the smooth contour lines..."),
    [&copies, &converted, &cancelled, surfaceName]()
      {
      for (const vtkSmartPointer<vtkSegmentation>& copy : copies)
        {
        if (cancelled)
          {
          return;
          }
        copy->CreateRepresentation(surfaceName);
        converted++;
        }
      },
    [&converted, total]() { return total > 0 ? (100 * converted) / total : 100; },
    [&cancelled]() { cancelled = true; });

  if (cancelled || !this->mrmlScene() || ContoursSegmentationNode(this->volumeNode()) != node)
    {
    const QSignalBlocker blocker(d->SmoothContoursButton);
    d->SmoothContoursButton->setChecked(false);
    return;
    }
  for (size_t ii = 0; ii < segmentIDs.size(); ii++)
    {
    vtkSegment* segment = segmentation->GetSegment(segmentIDs[ii]);
    vtkSegment* segmentCopy = copies[ii]->GetSegment(segmentIDs[ii]);
    vtkDataObject* surface = segmentCopy ? segmentCopy->GetRepresentation(surfaceName) : nullptr;
    if (segment && surface)
      {
      segment->AddRepresentation(surfaceName, surface);
      segmentation->InvokeEvent(vtkSegmentation::RepresentationModified, (void*)segmentIDs[ii].c_str());
      }
    }
  segmentation->InvokeEvent(vtkSegmentation::ContainedRepresentationNamesModified);
  vtkSlicerAstroVolumeLogic::SetContoursSmooth2D(node, true);
}

// --------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode* qSlicerAstroScalarVolumeDisplayWidget::volumeDisplayNode()const
{
  vtkMRMLVolumeNode* volumeNode = this->volumeNode();
  return volumeNode ? vtkMRMLAstroVolumeDisplayNode::SafeDownCast(
    volumeNode->GetDisplayNode()) : 0;
}

// --------------------------------------------------------------------------
vtkImageData* qSlicerAstroScalarVolumeDisplayWidget::volumeImageData()const
{
  vtkMRMLVolumeNode* volumeNode = this->volumeNode();
  return volumeNode ? vtkImageData::SafeDownCast(
    volumeNode->GetImageData()) : 0;
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setMRMLVolumeNode(vtkMRMLNode* node)
{
  this->setMRMLVolumeNode(vtkMRMLAstroVolumeNode::SafeDownCast(node));
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setReverse(bool toggled)
{
  vtkMRMLAstroVolumeDisplayNode *displayNode = this->volumeDisplayNode();
  if (!displayNode)
    {
    return;
    }

  vtkMRMLColorNode *colorNode = displayNode->GetColorNode();
  if (!colorNode)
    {
    return;
    }

  if (toggled)
    {
    colorNode->SetAttribute("SlicerAstro.Reverse", "on");
    }
  else
    {
    colorNode->SetAttribute("SlicerAstro.Reverse", "off");
    }

  this->ReverseColorFunction(colorNode);
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::ReverseColorFunction(vtkMRMLColorNode *colorNode)
{
  if (!colorNode)
    {
    return;
    }

  vtkLookupTable* lookupTable = colorNode->GetLookupTable();
  if (!lookupTable)
    {
    return;
    }

  int wasModifying = colorNode->StartModify();
  for (int ii = 0; ii < 128; ii++)
    {
    double RGBA1[4], RGBA2[4];
    lookupTable->GetTableValue(ii, RGBA1);
    lookupTable->GetTableValue(255 - ii, RGBA2);
    lookupTable->SetTableValue(ii, RGBA2);
    lookupTable->SetTableValue(255 - ii, RGBA1);
    }

  colorNode->SetNamesFromColors();
  colorNode->EndModify(wasModifying);
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setMRMLVolumeNode(vtkMRMLAstroVolumeNode* volumeNode)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  if(!volumeNode || volumeNode == this->volumeNode())
    {
    return;
    }

  vtkMRMLAstroVolumeNode *oldVolumeNode = this->volumeNode();
  vtkMRMLAstroVolumeDisplayNode* oldVolumeDisplayNode = this->volumeDisplayNode();
  d->MRMLWindowLevelWidget->setMRMLVolumeNode(volumeNode);
  d->MRMLVolumeThresholdWidget->setMRMLVolumeNode(volumeNode);

  d->ThresholdPushButton->setChecked(false);
  this->setThreshold(false);

  qvtkReconnect(oldVolumeDisplayNode, volumeNode->GetDisplayNode(),
                vtkCommand::ModifiedEvent,
                this, SLOT(updateWidgetFromDisplayMRML()));

  qvtkReconnect(oldVolumeDisplayNode, volumeNode->GetDisplayNode(),
                vtkMRMLAstroVolumeDisplayNode::FitSlicesModifiedEvent,
                this, SLOT(onFitSlicesModified()));

  qvtkReconnect(oldVolumeNode, volumeNode,
                vtkCommand::ModifiedEvent,
                this, SLOT(updateWidgetFromActiveVolumeMRML()));

  this->setEnabled(volumeNode != 0);
  this->onFitSlicesModified();
  this->updateWidgetFromDisplayMRML();
  // the data of the contours are kept (e.g. HI contours over a new optical
  // image); by default the active volume
  if (!d->contoursVolumeNode || !d->contoursVolumeNode->GetScene())
    {
    this->onContoursVolumeChanged(volumeNode);
    }
  else
    {
    this->updateWidgetFromContoursMRML();
    }
  // the contours drawn over the active volume are shown, those over the
  // other volumes are hidden
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (logic && this->mrmlScene())
    {
    std::vector<vtkMRMLNode*> astroVolumes;
    this->mrmlScene()->GetNodesByClass("vtkMRMLAstroVolumeNode", astroVolumes);
    for (vtkMRMLNode* node : astroVolumes)
      {
      vtkMRMLSegmentationNode* contours = logic->GetContoursSegmentationForVolume(
        vtkMRMLAstroVolumeNode::SafeDownCast(node), false);
      if (contours && contours->GetDisplayNode())
        {
        contours->GetDisplayNode()->SetVisibility(node == volumeNode);
        }
      }
    }
  this->updateContoursButtons();
  this->updateWidgetFromActiveVolumeMRML();
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::updateWidgetFromDisplayMRML()
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  if (this->isVisible())
    {
    this->updateTransferFunction();
    }

  vtkMRMLAstroVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (!displayNode)
    {
    return;
    }

  d->ColorTableComboBox->setCurrentNode(displayNode->GetColorNode());
  d->InterpolatePushButton->setChecked(displayNode->GetInterpolate());
  if (displayNode->GetInterpolate())
    {
    d->InterpolatePushButton->setIcon(QIcon(":Icons/SliceInterpolationOn.png"));
    }
  else
    {
    d->InterpolatePushButton->setIcon(QIcon(":Icons/SliceInterpolationOff.png"));
    }  

  vtkMRMLColorNode *colorNode = displayNode->GetColorNode();
  if (colorNode)
    {
    if (IsColorAttributeOn(colorNode, "SlicerAstro.AddFunctions"))
      {
      d->ReversePushButton->show();
      d->InversePushButton->show();
      d->LogPushButton->show();
      d->ReversePushButton->blockSignals(true);
      d->InversePushButton->blockSignals(true);
      d->LogPushButton->blockSignals(true);
      if (IsColorAttributeOn(colorNode, "SlicerAstro.Reverse"))
        {
        if (!d->ReversePushButton->isChecked())
          {
          this->ReverseColorFunction(colorNode);
          }
        d->ReversePushButton->setChecked(true);
        }
      else
        {
        if (d->ReversePushButton->isChecked())
          {
          this->ReverseColorFunction(colorNode);
          }
        d->ReversePushButton->setChecked(false);
        }
      if (IsColorAttributeOn(colorNode, "SlicerAstro.Inverse"))
        {
        if (!d->InversePushButton->isChecked())
          {
          this->InvertColorFunction(colorNode);
          }
        d->InversePushButton->setChecked(true);
        }
      else
        {
        if (d->InversePushButton->isChecked())
          {
          this->InvertColorFunction(colorNode);
          }
        d->InversePushButton->setChecked(false);
        }
      if (IsColorAttributeOn(colorNode, "SlicerAstro.Log"))
        {
        if (colorNode->GetLookupTable())
          {
          colorNode->GetLookupTable()->SetScaleToLog10();
          }
        d->LogPushButton->setChecked(true);
        }
      else
        {
        if (colorNode->GetLookupTable())
          {
          colorNode->GetLookupTable()->SetScaleToLinear();
          }
        d->LogPushButton->setChecked(false);
        }
      d->ReversePushButton->blockSignals(false);
      d->InversePushButton->blockSignals(false);
      d->LogPushButton->blockSignals(false);
      }
    else
      {
      d->ReversePushButton->hide();
      d->InversePushButton->hide();
      d->LogPushButton->hide();
      }
    }
}

//----------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::updateTransferFunction()
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);
  // from vtkKWWindowLevelThresholdEditor::UpdateTransferFunction
  vtkMRMLVolumeNode* volumeNode = d->MRMLWindowLevelWidget->mrmlVolumeNode();
  Q_ASSERT(volumeNode == d->MRMLVolumeThresholdWidget->mrmlVolumeNode());
  vtkImageData* imageData = volumeNode ? volumeNode->GetImageData() : 0;
  if (imageData == 0)
    {
    d->ColorTransferFunction->RemoveAllPoints();
    return;
    }
  double range[2] = {0,255};
  vtkMRMLAstroVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (displayNode)
    {
    displayNode->GetDisplayScalarRange(range);
    }
  else
    {
    imageData->GetScalarRange(range);
    }
  // AdjustRange call will take out points that are outside of the new
  // range, but it needs the points to be there in order to work, so call
  // RemoveAllPoints after it's done

  d->ColorTransferFunction->AdjustRange(range);
  d->ColorTransferFunction->RemoveAllPoints();

  double min = d->MRMLWindowLevelWidget->level() - 0.5 * d->MRMLWindowLevelWidget->window();
  double max = d->MRMLWindowLevelWidget->level() + 0.5 * d->MRMLWindowLevelWidget->window();
  double minVal = 0;
  double maxVal = 1;
  double low   = d->MRMLVolumeThresholdWidget->isOff() ? range[0] : d->MRMLVolumeThresholdWidget->lowerThreshold();
  double upper = d->MRMLVolumeThresholdWidget->isOff() ? range[1] : d->MRMLVolumeThresholdWidget->upperThreshold();

  d->ColorTransferFunction->SetColorSpaceToRGB();

  if (low >= max || upper <= min)
    {
    d->ColorTransferFunction->AddRGBPoint(range[0], 0, 0, 0);
    d->ColorTransferFunction->AddRGBPoint(range[1], 0, 0, 0);
    }
  else
    {
    max = qMax(min+0.000001, max);
    low = qMax(range[0] + 0.000001, low);
    min = qMax(range[0] + 0.000001, min);
    upper = qMin(range[1] - 0.000001, upper);

    if (min <= low)
      {
      minVal = (low - min)/(max - min);
      min = low + 0.000001;
      }

    if (max >= upper)
      {
      maxVal = (upper - min)/(max-min);
      max = upper - 0.000001;
      }

    d->ColorTransferFunction->AddRGBPoint(range[0], 0, 0, 0);
    d->ColorTransferFunction->AddRGBPoint(low, 0, 0, 0);
    d->ColorTransferFunction->AddRGBPoint(min, minVal, minVal, minVal);
    d->ColorTransferFunction->AddRGBPoint(max, maxVal, maxVal, maxVal);
    d->ColorTransferFunction->AddRGBPoint(upper, maxVal, maxVal, maxVal);
    if (upper+0.000001 < range[1])
      {
      d->ColorTransferFunction->AddRGBPoint(upper+0.000001, 0, 0, 0);
      d->ColorTransferFunction->AddRGBPoint(range[1], 0, 0, 0);
      }
    }

  d->ColorTransferFunction->SetAlpha(1.0);
  d->ColorTransferFunction->Build();
}

// -----------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::InvertColorFunction(vtkMRMLColorNode *colorNode)
{
  if (!colorNode)
    {
    return;
    }

  vtkLookupTable* lookupTable = colorNode->GetLookupTable();
  if (!lookupTable)
    {
    return;
    }

  int wasModifying = colorNode->StartModify();
  for (int ii = 0; ii < 256; ii++)
    {
    double RGBA[4];
    lookupTable->GetTableValue(ii, RGBA);
    RGBA[0] = 1. - RGBA[0];
    RGBA[1] = 1. - RGBA[1];
    RGBA[2] = 1. - RGBA[2];
    lookupTable->SetTableValue(ii, RGBA);
    }

  colorNode->SetNamesFromColors();
  colorNode->EndModify(wasModifying);
}

// -----------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::showEvent( QShowEvent * event )
{
  this->updateTransferFunction();
  this->Superclass::showEvent(event);
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setInterpolate(bool interpolate)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  vtkMRMLAstroVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (!displayNode)
    {
    return;
    }
  displayNode->SetInterpolate(interpolate);
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setInverse(bool toggled)
{
  vtkMRMLAstroVolumeDisplayNode *displayNode = this->volumeDisplayNode();
  if (!displayNode)
    {
    return;
    }

  vtkMRMLColorNode *colorNode = displayNode->GetColorNode();
  if (!colorNode)
    {
    return;
    }

  if (toggled)
    {
    colorNode->SetAttribute("SlicerAstro.Inverse", "on");
    }
  else
    {
    colorNode->SetAttribute("SlicerAstro.Inverse", "off");
    }

  this->InvertColorFunction(colorNode);
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setLog(bool toggled)
{
  vtkMRMLAstroVolumeDisplayNode *displayNode = this->volumeDisplayNode();
  if (!displayNode)
    {
    return;
    }

  vtkMRMLColorNode *colorNode = displayNode->GetColorNode();
  if (!colorNode)
    {
    return;
    }

  if (toggled)
    {
    colorNode->SetAttribute("SlicerAstro.Log", "on");
    }
  else
    {
    colorNode->SetAttribute("SlicerAstro.Log", "off");
    }
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setThreshold(bool threshold)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  if (threshold)
    {
    d->MRMLVolumeThresholdWidget->show();
    d->MRMLVolumeThresholdWidget->setAutoThreshold(qMRMLVolumeThresholdWidget::Manual);
    }
  else
    {
    d->MRMLVolumeThresholdWidget->hide();
    d->MRMLVolumeThresholdWidget->setAutoThreshold(qMRMLVolumeThresholdWidget::Off);
  }
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::onFitSlicesModified()
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  vtkMRMLAstroVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();

  if (!displayNode)
    {
    return;
    }

  d->FitSlicesToViewsPushButton->setChecked(displayNode->GetFitSlices());
  this->ExtendAllSlices();
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::updateWidgetFromActiveVolumeMRML()
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  vtkMRMLAstroVolumeNode* astroVolume = this->volumeNode();
  if (!astroVolume)
    {
    return;
    }

  if (!d->contoursVolumeNode)
    {
    return;
    }

  if (d->contoursVolumeNode != astroVolume)
    {
    vtkImageData* contoursImage = d->contoursVolumeNode->GetImageData();
    if (!contoursImage)
      {
      return;
      }
    int* contoursDims = contoursImage->GetDimensions();

    double* RASOrigin = astroVolume->GetOrigin();
    vtkNew<vtkGeneralTransform> RAStoIJKTransform;
    RAStoIJKTransform->Identity();
    RAStoIJKTransform->PostMultiply();
    vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
    astroVolume->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
    RAStoIJKMatrix->SetElement(0,3,0.);
    RAStoIJKMatrix->SetElement(1,3,0.);
    RAStoIJKMatrix->SetElement(2,3,0.);
    RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());
    double IJKOrigin[3];
    RAStoIJKTransform->TransformPoint(RASOrigin, IJKOrigin);
    IJKOrigin[2] += contoursDims[2] * 0.5;

    d->Contours2DSliderWidget->blockSignals(true);
    d->Contours2DSliderWidget->setValue(IJKOrigin[2]);
    d->Contours2DSliderWidget->setSingleStep(1);
    d->Contours2DSliderWidget->setPageStep((int) (contoursDims[2] * 0.1));
    d->Contours2DSliderWidget->blockSignals(false);
    }
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::updateWidgetFromContoursMRML()
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  if (!d->contoursVolumeNode)
    {
    d->SliceSegmentationLabel->hide();
    d->Contours2DSliderWidget->hide();
    return;
    }

  d->ContoursVolumeNodeSelector->setCurrentNodeID(d->contoursVolumeNode->GetID());

if (!d->contoursVolumeNode->GetAstroVolumeDisplayNode())
  {
  d->SliceSegmentationLabel->hide();
  d->Contours2DSliderWidget->hide();
  return;
  }

  vtkDoubleArray *contoursColor =  d->contoursVolumeNode->
    GetAstroVolumeDisplayNode()->GetContoursColor();
  QColor color;
  double red, green, blue;
  red = contoursColor->GetValue(0) * 256;
  green = contoursColor->GetValue(1) * 256;
  blue = contoursColor->GetValue(2) * 256;
  if (red > 255)
    {
    red = 255;
    }
  if (green > 255)
    {
    green = 255;
    }
  if (blue > 255)
    {
    blue = 255;
    }
  color.setRed(red);
  color.setGreen(green);
  color.setBlue(blue);
  d->ColorPickerButton->setColor(color);

  vtkMRMLAstroVolumeNode* astroVolume = this->volumeNode();
  if (!astroVolume)
    {
    d->SliceSegmentationLabel->hide();
    d->Contours2DSliderWidget->hide();
    return;
    }

  if (d->contoursVolumeNode != astroVolume)
    {
    int ActiveDataN = StringToInt(astroVolume->GetAttribute("SlicerAstro.NAXIS"));
    int ContoursDataN = StringToInt(d->contoursVolumeNode->GetAttribute("SlicerAstro.NAXIS"));
    if (ActiveDataN == 2 && ContoursDataN == 3)
      {
      d->Contours2DSliderWidget->show();
      }
    else
      {
      d->Contours2DSliderWidget->hide();
      }

    vtkImageData* contoursImage = d->contoursVolumeNode->GetImageData();
    if (!contoursImage)
      {
      d->SliceSegmentationLabel->hide();
      d->Contours2DSliderWidget->hide();
      return;
      }
    int* contoursDims = contoursImage->GetDimensions();

    double* RASOrigin = astroVolume->GetOrigin();
    vtkNew<vtkGeneralTransform> RAStoIJKTransform;
    RAStoIJKTransform->Identity();
    RAStoIJKTransform->PostMultiply();
    vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
    astroVolume->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
    RAStoIJKMatrix->SetElement(0,3,0.);
    RAStoIJKMatrix->SetElement(1,3,0.);
    RAStoIJKMatrix->SetElement(2,3,0.);
    RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());
    double IJKOrigin[3];
    RAStoIJKTransform->TransformPoint(RASOrigin, IJKOrigin);
    IJKOrigin[2] += contoursDims[2] * 0.5;
    d->Contours2DSliderWidget->setValue(IJKOrigin[2]);
    d->Contours2DSliderWidget->setRange(0., contoursDims[2]);
    d->Contours2DSliderWidget->setSingleStep(1);
    d->Contours2DSliderWidget->setPageStep((int) (contoursDims[2] * 0.1));
    }
  else
    {
    d->SliceSegmentationLabel->hide();
    d->Contours2DSliderWidget->hide();
    }
  this->updateContoursChannelLabel();
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::setColorNode(vtkMRMLNode* colorNode)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  vtkMRMLAstroVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (!displayNode || !colorNode)
    {
    return;
    }
  Q_ASSERT(vtkMRMLColorNode::SafeDownCast(colorNode));

  d->ReversePushButton->blockSignals(true);
  if (IsColorAttributeOn(colorNode, "SlicerAstro.Reverse"))
    {
    d->ReversePushButton->setChecked(true);
    }
  else
    {
    d->ReversePushButton->setChecked(false);
    }
  d->ReversePushButton->blockSignals(false);

  d->InversePushButton->blockSignals(true);
  if (IsColorAttributeOn(colorNode, "SlicerAstro.Inverse"))
    {
    d->InversePushButton->setChecked(true);
    }
  else
    {
    d->InversePushButton->setChecked(false);
    }
  d->InversePushButton->blockSignals(false);

  displayNode->SetAndObserveColorNodeID(colorNode->GetID());
}
