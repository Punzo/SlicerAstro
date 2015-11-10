// QT includes
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QSpinBox>

// CTK includes
#include <ctkUtils.h>

// qMRML includes
#include "qMRMLAstroVolumeInfoWidget.h"
#include "qMRMLCoordinatesWidget.h"

// MRML includes
#include <vtkMRMLLabelMapVolumeDisplayNode.h>
#include <vtkMRMLLabelMapVolumeNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLScalarVolumeNode.h>
#include <vtkMRMLScalarVolumeDisplayNode.h>
#include <vtkMRMLStorageNode.h>

// VTK includes
#include <vtkDataArray.h>
#include <vtkImageAlgorithm.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkTrivialProducer.h>
#include <vtkWeakPointer.h>

//------------------------------------------------------------------------------
class qMRMLAstroVolumeInfoWidgetPrivate
{
  Q_DECLARE_PUBLIC(qMRMLAstroVolumeInfoWidget);

protected:
  qMRMLAstroVolumeInfoWidget* const q_ptr;

public:
  qMRMLAstroVolumeInfoWidgetPrivate(qMRMLAstroVolumeInfoWidget& object);
  void init();
  bool centeredOrigin(double* origin)const;

  vtkWeakPointer<vtkMRMLVolumeNode> VolumeNode;
  QFormLayout *formLayout;
  QLabel *ImageDimensionsLabel;
  qMRMLCoordinatesWidget *ImageDimensionsWidget;
  QLabel *ImageSpacingLabel;
  qMRMLCoordinatesWidget *ImageSpacingWidget;
  QLabel *ImageOriginLabel;
  qMRMLCoordinatesWidget *ImageOriginWidget;
  QPushButton *CenterVolumePushButton;
  QLabel *NumberOfScalarsLabel;
  QSpinBox *NumberOfScalarsSpinBox;
  QLabel *FileNameLabel;
  QLineEdit *FileNameLineEdit;
  QLabel *LabelMapLabel;
  QCheckBox *LabelMapCheckBox;
};

//------------------------------------------------------------------------------
qMRMLAstroVolumeInfoWidgetPrivate::qMRMLAstroVolumeInfoWidgetPrivate(qMRMLAstroVolumeInfoWidget& object)
  : q_ptr(&object)
{
}

//------------------------------------------------------------------------------
void qMRMLAstroVolumeInfoWidgetPrivate::init()
{
  Q_Q(qMRMLAstroVolumeInfoWidget);

  q->setObjectName(QString::fromUtf8("qMRMLAstroVolumeInfoWidget"));

  q->resize(660, 424);
  QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  sizePolicy.setHorizontalStretch(0);
  sizePolicy.setVerticalStretch(0);
  sizePolicy.setHeightForWidth(q->sizePolicy().hasHeightForWidth());
  q->setSizePolicy(sizePolicy);
  this->formLayout = new QFormLayout(q);
  this->formLayout->setObjectName(QString::fromUtf8("formLayout"));
  this->ImageDimensionsLabel = new QLabel(q);
  this->ImageDimensionsLabel->setObjectName(QString::fromUtf8("ImageDimensionsLabel"));

  this->formLayout->setWidget(0, QFormLayout::LabelRole, ImageDimensionsLabel);

  this->ImageDimensionsWidget = new qMRMLCoordinatesWidget(q);
  this->ImageDimensionsWidget->setObjectName(QString::fromUtf8("ImageDimensionsWidget"));
  this->ImageDimensionsWidget->setEnabled(false);
  this->ImageDimensionsWidget->setDecimals(0);
  this->ImageDimensionsWidget->setMinimum(0);
  this->ImageDimensionsWidget->setMaximum(1e+06);
  this->ImageDimensionsWidget->setUnitAwareProperties(qMRMLCoordinatesWidget::MaximumValue|qMRMLCoordinatesWidget::MinimumValue|qMRMLCoordinatesWidget::Precision|qMRMLCoordinatesWidget::Prefix|qMRMLCoordinatesWidget::Scaling|qMRMLCoordinatesWidget::Suffix);

  this->formLayout->setWidget(0, QFormLayout::FieldRole, ImageDimensionsWidget);

  this->ImageSpacingLabel = new QLabel(q);
  this->ImageSpacingLabel->setObjectName(QString::fromUtf8("ImageSpacingLabel"));

  this->formLayout->setWidget(1, QFormLayout::LabelRole, ImageSpacingLabel);

  this->ImageSpacingWidget = new qMRMLCoordinatesWidget(q);
  this->ImageSpacingWidget->setObjectName(QString::fromUtf8("ImageSpacingWidget"));
  this->ImageSpacingWidget->setDecimals(4);
  this->ImageSpacingWidget->setDecimalsOption(ctkDoubleSpinBox::DecimalsByKey|ctkDoubleSpinBox::DecimalsByShortcuts|ctkDoubleSpinBox::DecimalsByValue);
  this->ImageSpacingWidget->setMinimum(0.005);
  this->ImageSpacingWidget->setSingleStep(0.5);
  this->ImageSpacingWidget->setMaximum(1e+09);
  this->ImageSpacingWidget->setUnitAwareProperties(qMRMLCoordinatesWidget::Precision|qMRMLCoordinatesWidget::Prefix|qMRMLCoordinatesWidget::Scaling|qMRMLCoordinatesWidget::Suffix);

  this->formLayout->setWidget(1, QFormLayout::FieldRole, ImageSpacingWidget);

  this->ImageOriginLabel = new QLabel(q);
  this->ImageOriginLabel->setObjectName(QString::fromUtf8("ImageOriginLabel"));

  this->formLayout->setWidget(2, QFormLayout::LabelRole, ImageOriginLabel);

  this->ImageOriginWidget = new qMRMLCoordinatesWidget(q);
  this->ImageOriginWidget->setObjectName(QString::fromUtf8("ImageOriginWidget"));
  this->ImageOriginWidget->setDecimals(4);
  this->ImageOriginWidget->setDecimalsOption(ctkDoubleSpinBox::DecimalsByKey|ctkDoubleSpinBox::DecimalsByShortcuts|ctkDoubleSpinBox::DecimalsByValue);
  this->ImageOriginWidget->setMinimum(-1e+09);
  this->ImageOriginWidget->setMaximum(1e+09);
  this->ImageOriginWidget->setUnitAwareProperties(qMRMLCoordinatesWidget::Precision|qMRMLCoordinatesWidget::Prefix|qMRMLCoordinatesWidget::Scaling|qMRMLCoordinatesWidget::Suffix);

  this->formLayout->setWidget(2, QFormLayout::FieldRole, ImageOriginWidget);

  this->CenterVolumePushButton = new QPushButton(q);
  this->CenterVolumePushButton->setObjectName(QString::fromUtf8("CenterVolumePushButton"));

  this->formLayout->setWidget(4, QFormLayout::FieldRole, CenterVolumePushButton);

  this->NumberOfScalarsLabel = new QLabel(q);
  this->NumberOfScalarsLabel->setObjectName(QString::fromUtf8("NumberOfScalarsLabel"));

  this->formLayout->setWidget(6, QFormLayout::LabelRole, NumberOfScalarsLabel);

  this->NumberOfScalarsSpinBox = new QSpinBox(q);
  this->NumberOfScalarsSpinBox->setObjectName(QString::fromUtf8("NumberOfScalarsSpinBox"));
  this->NumberOfScalarsSpinBox->setMinimum(1);
  this->NumberOfScalarsSpinBox->setMaximum(1000000000);
  this->NumberOfScalarsSpinBox->setValue(1);

  this->formLayout->setWidget(6, QFormLayout::FieldRole, NumberOfScalarsSpinBox);

  this->FileNameLabel = new QLabel(q);
  this->FileNameLabel->setObjectName(QString::fromUtf8("FileNameLabel"));

  this->formLayout->setWidget(9, QFormLayout::LabelRole, FileNameLabel);

  this->FileNameLineEdit = new QLineEdit(q);
  this->FileNameLineEdit->setObjectName(QString::fromUtf8("FileNameLineEdit"));
  this->FileNameLineEdit->setReadOnly(true);

  this->formLayout->setWidget(9, QFormLayout::FieldRole, FileNameLineEdit);

  this->LabelMapLabel = new QLabel(q);
  this->LabelMapLabel->setObjectName(QString::fromUtf8("LabelMapLabel"));

  this->formLayout->setWidget(10, QFormLayout::LabelRole, LabelMapLabel);

  this->LabelMapCheckBox = new QCheckBox(q);
  this->LabelMapCheckBox->setObjectName(QString::fromUtf8("LabelMapCheckBox"));
  this->LabelMapCheckBox->setEnabled(false);

  this->formLayout->setWidget(10, QFormLayout::FieldRole, LabelMapCheckBox);

  QMetaObject::connectSlotsByName(q);

  q->setWindowTitle("Volume Information");
  this->ImageDimensionsLabel->setText("Image Dimensions:");
  this->ImageDimensionsWidget->setProperty("coordinates", QVariant("1,1,1"));
  this->ImageSpacingLabel->setText("Image Spacing:");
  this->ImageSpacingWidget->setProperty("coordinates", QVariant("0,0,0"));
  this->ImageOriginLabel->setText("Image Origin:");
  this->CenterVolumePushButton->setText("Center Volume");
  this->NumberOfScalarsLabel->setText("Number of Scalars:");
  this->FileNameLabel->setText("File Name:");
  this->LabelMapLabel->setText("LabelMap:");
  this->LabelMapCheckBox->setText(QString());

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->ImageDimensionsWidget, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->ImageOriginWidget, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->ImageSpacingWidget, SLOT(setMRMLScene(vtkMRMLScene*)));


  // Image dimension is read-only
  QObject::connect(this->ImageSpacingWidget, SIGNAL(coordinatesChanged(double*)),
                   q, SLOT(setImageSpacing(double*)));
  QObject::connect(this->ImageOriginWidget, SIGNAL(coordinatesChanged(double*)),
                   q, SLOT(setImageOrigin(double*)));
  QObject::connect(this->CenterVolumePushButton, SIGNAL(clicked()),
                   q, SLOT(center()));

  q->setEnabled(this->VolumeNode != 0);
}

//------------------------------------------------------------------------------
bool qMRMLAstroVolumeInfoWidgetPrivate::centeredOrigin(double* origin)const
{
  vtkImageData *imageData = this->VolumeNode ? this->VolumeNode->GetImageData() : 0;
  if (!imageData)
    {
    qWarning() << __FUNCTION__ << "No image data, can't retrieve origin.";
    return false;
    }

  int *dims = imageData->GetDimensions();
  double dimsH[4];
  dimsH[0] = dims[0] - 1;
  dimsH[1] = dims[1] - 1;
  dimsH[2] = dims[2] - 1;
  dimsH[3] = 0.;

  vtkNew<vtkMatrix4x4> ijkToRAS;
  this->VolumeNode->GetIJKToRASMatrix(ijkToRAS.GetPointer());
  double rasCorner[4];
  ijkToRAS->MultiplyPoint(dimsH, rasCorner);

  origin[0] = -0.5 * rasCorner[0];
  origin[1] = -0.5 * rasCorner[1];
  origin[2] = -0.5 * rasCorner[2];

  return true;
}

//------------------------------------------------------------------------------
qMRMLAstroVolumeInfoWidget::qMRMLAstroVolumeInfoWidget(QWidget *_parent)
  : Superclass(_parent)
  , d_ptr(new qMRMLAstroVolumeInfoWidgetPrivate(*this))
{
  Q_D(qMRMLAstroVolumeInfoWidget);
  d->init();
}

//------------------------------------------------------------------------------
qMRMLAstroVolumeInfoWidget::~qMRMLAstroVolumeInfoWidget()
{
}


//------------------------------------------------------------------------------
vtkMRMLVolumeNode* qMRMLAstroVolumeInfoWidget::volumeNode()const
{
  Q_D(const qMRMLAstroVolumeInfoWidget);
  return d->VolumeNode;
}

//------------------------------------------------------------------------------
void qMRMLAstroVolumeInfoWidget::setVolumeNode(vtkMRMLNode* node)
{
  this->setVolumeNode(vtkMRMLVolumeNode::SafeDownCast(node));
}

//------------------------------------------------------------------------------
void qMRMLAstroVolumeInfoWidget::setVolumeNode(vtkMRMLVolumeNode* volumeNode)
{
  Q_D(qMRMLAstroVolumeInfoWidget);
  qvtkReconnect(d->VolumeNode, volumeNode, vtkCommand::ModifiedEvent,
                this, SLOT(updateWidgetFromMRML()));
  qvtkReconnect(d->VolumeNode, volumeNode, vtkMRMLVolumeNode::ImageDataModifiedEvent,
                this, SLOT(updateWidgetFromMRML()));
  d->VolumeNode = volumeNode;
  this->updateWidgetFromMRML();
}

//------------------------------------------------------------------------------
void qMRMLAstroVolumeInfoWidget::updateWidgetFromMRML()
{
  Q_D(qMRMLAstroVolumeInfoWidget);
  this->setEnabled(d->VolumeNode != 0);
  d->LabelMapCheckBox->setEnabled(false);
  d->NumberOfScalarsSpinBox->setEnabled(false);
  d->FileNameLineEdit->setEnabled(false);

  if (!d->VolumeNode)
    {
    double dimensions[3] = {0.,0.,0.};
    d->ImageDimensionsWidget->setCoordinates(dimensions);

    double spacing[3] = {1.,1.,1.};
    d->ImageSpacingWidget->setCoordinates(spacing);

    double origin[3] = {0.,0.,0.};
    d->ImageOriginWidget->setCoordinates(origin);

    d->NumberOfScalarsSpinBox->setValue(1);

    d->FileNameLineEdit->setText("");

    d->LabelMapCheckBox->setChecked(false);

    return;
    }
  vtkImageData* image = d->VolumeNode->GetImageData();
  double dimensions[3] = {0.,0.,0.};
  int* dims = image ? image->GetDimensions() : 0;
  if (dims)
    {
    dimensions[0] = dims[0];
    dimensions[1] = dims[1];
    dimensions[2] = dims[2];
    }
  d->ImageDimensionsWidget->setCoordinates(dimensions);

  double* spacing = d->VolumeNode->GetSpacing();
  d->ImageSpacingWidget->setCoordinates(spacing);

  double* origin = d->VolumeNode->GetOrigin();
  d->ImageOriginWidget->setCoordinates(origin);

  d->CenterVolumePushButton->setEnabled(!this->isCentered());

  d->NumberOfScalarsSpinBox->setValue(
    image ? image->GetNumberOfScalarComponents() : 0);

  vtkMRMLStorageNode* storageNode = d->VolumeNode->GetStorageNode();
  d->FileNameLineEdit->setText(storageNode ? storageNode->GetFileName() : "");

  //vtkMRMLScalarVolumeNode *scalarNode = vtkMRMLScalarVolumeNode::SafeDownCast( d->VolumeNode );

  vtkMRMLLabelMapVolumeNode *labelMapNode = vtkMRMLLabelMapVolumeNode::SafeDownCast( d->VolumeNode );
  d->LabelMapCheckBox->setChecked(labelMapNode!=0);
}

//------------------------------------------------------------------------------
void qMRMLAstroVolumeInfoWidget::setImageSpacing(double* spacing)
{
  Q_D(qMRMLAstroVolumeInfoWidget);
  if (d->VolumeNode == 0)
    {
    return;
    }
  d->VolumeNode->SetSpacing(spacing);
}

//------------------------------------------------------------------------------
void qMRMLAstroVolumeInfoWidget::setImageOrigin(double* origin)
{
  Q_D(qMRMLAstroVolumeInfoWidget);
  if (d->VolumeNode == 0)
    {
    return;
    }
  d->VolumeNode->SetOrigin(origin);
}

//------------------------------------------------------------------------------
bool qMRMLAstroVolumeInfoWidget::isCentered()const
{
  Q_D(const qMRMLAstroVolumeInfoWidget);
  double centerOrigin[3];
  if (!d->centeredOrigin(centerOrigin))
    {
    return false;
    }
  double* volumeOrigin = d->VolumeNode->GetOrigin();
  return qFuzzyCompare(centerOrigin[0], volumeOrigin[0]) &&
         qFuzzyCompare(centerOrigin[1], volumeOrigin[1]) &&
         qFuzzyCompare(centerOrigin[2], volumeOrigin[2]);
}

//------------------------------------------------------------------------------
void qMRMLAstroVolumeInfoWidget::center()
{
  Q_D(qMRMLAstroVolumeInfoWidget);
  double origin[3];
  if (!d->centeredOrigin(origin))
    {
    return;
    }
  d->VolumeNode->SetOrigin(origin);
}

//------------------------------------------------------------------------------
void qMRMLAstroVolumeInfoWidget::setNumberOfScalars(int number)
{
  Q_D(qMRMLAstroVolumeInfoWidget);
  vtkImageData* imageData = d->VolumeNode ? d->VolumeNode->GetImageData() : 0;
  if (imageData == 0)
    {
    return;
    }
#if (VTK_MAJOR_VERSION <= 5)
  imageData->SetNumberOfScalarComponents(number);
#else
  vtkNew<vtkTrivialProducer> tp;
  tp->SetOutput(imageData);
  vtkInformation* outInfo = tp->GetOutputInformation(0);
  vtkDataObject::SetPointDataActiveScalarInfo(outInfo,
      vtkImageData::GetScalarType(outInfo), number);
#endif
}
