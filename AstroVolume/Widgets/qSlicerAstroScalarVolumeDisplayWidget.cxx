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
#include <QtDebug>
#include <QMessageBox>
#include <QSettings>
#include <QString>
#include <QStringList>

// SlicerQt includes
#include <qMRMLSliceAstroWidget.h>
#include <qSlicerApplication.h>
#include <qSlicerLayoutManager.h>
#include <vtkSlicerApplicationLogic.h>

// MRML includes
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLColorNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLUnitNode.h>

// VTK includes
#include <vtkAlgorithm.h>
#include <vtkAlgorithmOutput.h>
#include <vtkColorTransferFunction.h>
#include <vtkDoubleArray.h>
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
#include <ctkPopupWidget.h>

// logic includes
#include <vtkSlicerSegmentationsModuleLogic.h>

// STD includes
#include <limits>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_Volumes
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
  int nameIndex;
};

//-----------------------------------------------------------------------------
qSlicerAstroScalarVolumeDisplayWidgetPrivate::qSlicerAstroScalarVolumeDisplayWidgetPrivate(
  qSlicerAstroScalarVolumeDisplayWidget& object)
  : q_ptr(&object)
{
  this->ColorTransferFunction = vtkSmartPointer<vtkColorTransferFunction>::New();
  nameIndex = 0;
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

  QObject::connect(this->ColorTableComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setColorNode(vtkMRMLNode*)));
  QObject::connect(this->ReversePushButton, SIGNAL(toggled(bool)),
                   q, SLOT(setReverse(bool)));
  QObject::connect(this->InversePushButton, SIGNAL(toggled(bool)),
                   q, SLOT(setInverse(bool)));
  QObject::connect(this->LogPushButton, SIGNAL(toggled(bool)),
                   q, SLOT(setLog(bool)));
  QObject::connect(this->WindowLevelPopupButton, SIGNAL(toggled(bool)),
                   q, SLOT(onWindowLevelPopupShow(bool)));
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

  QComboBox* AutoManualComboBoxWidget = this->MRMLWindowLevelWidget->findChild<QComboBox*>
      (QString("AutoManualComboBox"));
  if (AutoManualComboBoxWidget)
    {
    QObject::connect(AutoManualComboBoxWidget,
                     SIGNAL(currentIndexChanged(int)),
                     q, SLOT(onWindowLevelPopupShow(int)));
    }

  QObject::connect(this->ColorPickerButton, SIGNAL(colorChanged(QColor)),
                   q, SLOT(onColorChanged(QColor)));
  QObject::connect(this->ContourPushButton, SIGNAL(clicked()),
                   q, SLOT(onCreateContours()));

  this->WindowLevelPopupButton->setChecked(false);
  q->onWindowLevelPopupShow(false);
  this->ThresholdPushButton->setChecked(false);
  q->setThreshold(false);
}

// --------------------------------------------------------------------------
qSlicerAstroScalarVolumeDisplayWidget::qSlicerAstroScalarVolumeDisplayWidget(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroScalarVolumeDisplayWidgetPrivate(*this))
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);
  d->init();

  // disable as there is not MRML Node associated with the widget
  this->setEnabled(false);
}

// --------------------------------------------------------------------------
qSlicerAstroScalarVolumeDisplayWidget::~qSlicerAstroScalarVolumeDisplayWidget()
{
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
  if (!this->mrmlScene() || !this->volumeNode())
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();

  if (!displayNode || !displayNode->GetFitSlices())
    {
    return;
    }

  int dims[3];
  this->volumeNode()->GetImageData()->GetDimensions(dims);

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
  vtkMRMLAstroVolumeDisplayNode* astroDisplayNode = this->volumeDisplayNode();
  if (!astroDisplayNode)
    {
    QString message = QString("Display node not found.");
    qWarning() << Q_FUNC_INFO << ": " << message;
    return;
    }

  astroDisplayNode->SetContoursColor(0, color.red() / 255.);
  astroDisplayNode->SetContoursColor(1, color.green() / 255.);
  astroDisplayNode->SetContoursColor(2, color.blue() / 255.);
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::onCreateContours()
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  QString ContourLevels = d->ContourLevelsLineEdit->text();
  std::string LevelsStdString = ContourLevels.toStdString();

  vtkMRMLAstroVolumeNode* masterVolume = this->volumeNode();
  if (!masterVolume)
    {
    QString message = QString("Master volume not found.");
    qWarning() << Q_FUNC_INFO << ": " << message;
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* astroDisplayNode = this->volumeDisplayNode();
  if (!astroDisplayNode)
    {
    QString message = QString("Display node not found.");
    qWarning() << Q_FUNC_INFO << ": " << message;
    return;
    }

  std::string NamePrefixStdString = masterVolume->GetName();

  if (!this->mrmlScene())
    {
    return;
    }

  std::string segmentEditorSingletonTag = "SegmentEditor";
  vtkMRMLSegmentEditorNode *segmentEditorNode = vtkMRMLSegmentEditorNode::SafeDownCast(
    this->mrmlScene()->GetSingletonNode(segmentEditorSingletonTag.c_str(), "vtkMRMLSegmentEditorNode"));
  if (!segmentEditorNode->GetSegmentationNode())
    {
    vtkSmartPointer<vtkMRMLNode> segmentationNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLSegmentationNode");
    segmentationNode.TakeReference(foo);
    this->mrmlScene()->AddNode(segmentationNode);
    segmentEditorNode->SetAndObserveSegmentationNode
      (vtkMRMLSegmentationNode::SafeDownCast(segmentationNode));
    }

  vtkMRMLSegmentationNode* segmentationNode = segmentEditorNode->GetSegmentationNode();

  if (!segmentationNode)
    {
    QString message = QString("segmentation node not found.");
    qWarning() << Q_FUNC_INFO << ": " << message;
    return;
    }

  if (!segmentationNode->GetDisplayNode())
    {
    segmentationNode->CreateDefaultDisplayNodes();
    }

  bool duplicateName = false;
  for (int ii = 1; ii <segmentationNode->GetSegmentation()->GetNumberOfSegments(); ii++)
    {
    std::string segmentID = segmentationNode->GetSegmentation()->GetNthSegmentID(ii);
    if (segmentID.find(NamePrefixStdString) != std::string::npos)
      {
      duplicateName = true;
      }
    }

  if (duplicateName)
    {
    NamePrefixStdString += IntToString(d->nameIndex);
    d->nameIndex++;
    }

  vtkNew<vtkDoubleArray> Levels;
  vtkNew<vtkDoubleArray> Delimiters;
  vtkNew<vtkStringArray> SegmentIDs;
  std::stringstream ss(LevelsStdString);

  double value;
  double MIN = StringToDouble(masterVolume->GetAttribute("SlicerAstro.DATAMIN"));
  double MAX = StringToDouble(masterVolume->GetAttribute("SlicerAstro.DATAMAX"));
  double DisplayThreshold = StringToDouble(masterVolume->GetAttribute("SlicerAstro.3DDisplayThreshold"));

  bool convert = false;
  std::size_t found = LevelsStdString.find("3DDisplayThreshold");
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

  if ((!list && !increment && !renzogram && !histo) || (list && increment) || (renzogram && increment)
      || (increment && !linearIncrement && !nonLinearIncrement) ||
      (list && (linearIncrement || nonLinearIncrement)) ||
      (histo && (list || renzogram || increment || linearIncrement || nonLinearIncrement)))
    {
    QString message = QString("The input string defining the Contour Levels"
                              " is wrongly formatted. Check the ToolTip.");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to create Contours"), message);
    return;
    }

  if (convert)
    {
    for (int ii = 0; ii < 18; ii++)
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
    QString message = QString("The value of some Contour Levels exceeds the MIN and MAX values. "
                              "These will be ignored.");
    qWarning() << Q_FUNC_INFO << ": " << message;
    }

  // Create empty segment in current segmentation
  this->mrmlScene()->SaveStateForUndo();

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
    vtkSegment *Segment = segmentationNode->GetSegmentation()->GetSegment(SegmentID);
    if(!Segment)
      {
      SegmentID = segmentationNode->GetSegmentation()->AddEmptySegment(SegmentID, SegmentID);
      Segment = segmentationNode->GetSegmentation()->GetSegment(SegmentID);
      }

    vtkDoubleArray* ContoursColor =  astroDisplayNode->GetContoursColor();
    Segment->SetColor(ContoursColor->GetValue(0),
                      ContoursColor->GetValue(1),
                      ContoursColor->GetValue(2));

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
    modifierLabelmap->DeepCopy(imageThreshold->GetOutput());
    vtkNew<vtkMatrix4x4> IJKToRASMatrix;
    masterVolume->GetIJKToRASMatrix(IJKToRASMatrix.GetPointer());
    modifierLabelmap->SetGeometryFromImageToWorldMatrix(IJKToRASMatrix.GetPointer());

    if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
        modifierLabelmap.GetPointer(), segmentationNode, SegmentID, vtkSlicerSegmentationsModuleLogic::MODE_REPLACE))
      {
      qCritical() << Q_FUNC_INFO << ": Failed to add modifier labelmap to selected segment";
      }

    int dims[3];
    modifierLabelmap->GetDimensions(dims);
    if (LevelDim < dims[0] * dims[1] * dims[2])
      {
      LevelDim = dims[0] * dims[1] * dims[2];
      }
    }

  const int DataType = masterVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  double datacubeDim = 0;
  switch (DataType)
    {
    case VTK_FLOAT:
      datacubeDim = LevelDim * 4;
      break;
    case VTK_DOUBLE:
      datacubeDim = LevelDim * 8;
      break;
    default:
      qCritical() << Q_FUNC_INFO << ": attempt to allocate scalars of type not allowed.";
      return;
    }

  if (datacubeDim < 1.E+7)
    {
    segmentationNode->GetSegmentation()->CreateRepresentation(
      vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName());
    }
  else
    {
    segmentationNode->GetSegmentation()->CreateRepresentation(
      vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName());
    }

  for (int ii = 0; ii < segmentationNode->GetNumberOfDisplayNodes(); ii++)
    {
    vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(segmentationNode->GetNthDisplayNode(ii));
    SegmentationDisplayNode->SetAllSegmentsVisibility(false);
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
    SegmentationDisplayNode->SetPreferredDisplayRepresentationName3D(
      vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName());

    if (datacubeDim < 1.E+7)
      {
      SegmentationDisplayNode->SetPreferredDisplayRepresentationName2D(
        vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName());
      }
    else
      {
      SegmentationDisplayNode->SetPreferredDisplayRepresentationName2D(
        vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName());
      }
  }
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
void qSlicerAstroScalarVolumeDisplayWidget::onWindowLevelPopupShow(bool show)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  ctkPopupWidget* PopupWidget = d->MRMLWindowLevelWidget->findChild<ctkPopupWidget*>
      (QString("RangeWidgetPopup"));

  if (!PopupWidget)
    {
    return;
    }

  if (show)
    {
    d->WindowLevelPopupButton->setIcon(QIcon(":Icons/PushPinIn.png"));
    PopupWidget->setAutoShow(true);
    PopupWidget->showPopup();
    }
  else
    {
    d->WindowLevelPopupButton->setIcon(QIcon(":Icons/PushPinOut.png"));
    PopupWidget->setAutoShow(false);
    PopupWidget->hidePopup();
  }
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::onWindowLevelPopupShow(int)
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);

  ctkPopupWidget* PopupWidget = d->MRMLWindowLevelWidget->findChild<ctkPopupWidget*>
      (QString("RangeWidgetPopup"));

  if (!PopupWidget)
    {
    return;
    }

  if (d->WindowLevelPopupButton->isChecked())
    {
    PopupWidget->setAutoShow(true);
    PopupWidget->showPopup();
    }
  else
    {
    PopupWidget->setAutoShow(false);
    PopupWidget->hidePopup();
    }
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

  if(!volumeNode)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* oldVolumeDisplayNode = this->volumeDisplayNode();
  d->MRMLWindowLevelWidget->setMRMLVolumeNode(volumeNode);
  d->MRMLVolumeThresholdWidget->setMRMLVolumeNode(volumeNode);

  d->WindowLevelPopupButton->setChecked(false);
  this->onWindowLevelPopupShow(false);
  d->ThresholdPushButton->setChecked(false);
  this->setThreshold(false);

  qvtkReconnect(oldVolumeDisplayNode, volumeNode ? volumeNode->GetDisplayNode() :0,
                vtkCommand::ModifiedEvent,
                this, SLOT(updateWidgetFromMRML()));
  this->setEnabled(volumeNode != 0);
  this->updateWidgetFromMRML();
}

// --------------------------------------------------------------------------
void qSlicerAstroScalarVolumeDisplayWidget::updateWidgetFromMRML()
{
  Q_D(qSlicerAstroScalarVolumeDisplayWidget);
  vtkMRMLAstroVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();

  if (displayNode)
    {
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
    d->FitSlicesToViewsPushButton->setChecked(displayNode->GetFitSlices());
    if (displayNode->GetFitSlices())
      {
      this->ExtendAllSlices();
      }
    else
      {
      qSlicerApplication* app = qSlicerApplication::application();
      if (app && app->applicationLogic())
        {
        app->applicationLogic()->FitSliceToAll(true);
        }
      }

    vtkDoubleArray *contoursColor =  displayNode->GetContoursColor();
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

    vtkMRMLColorNode *colorNode = displayNode->GetColorNode();
    if (colorNode)
      {
      if (!strcmp(colorNode->GetAttribute("SlicerAstro.AddFunctions"), "on"))
        {
        d->ReversePushButton->show();
        d->InversePushButton->show();
        d->LogPushButton->show();
        d->ReversePushButton->blockSignals(true);
        d->InversePushButton->blockSignals(true);
        d->LogPushButton->blockSignals(true);
        if (!strcmp(colorNode->GetAttribute("SlicerAstro.Reverse"), "on"))
          {
          d->ReversePushButton->setChecked(true);
          }
        else
          {
          d->ReversePushButton->setChecked(false);
          }
        if (!strcmp(colorNode->GetAttribute("SlicerAstro.Inverse"), "on"))
          {
          d->InversePushButton->setChecked(true);
          }
        else
          {
          d->InversePushButton->setChecked(false);
          }
        if (!strcmp(colorNode->GetAttribute("SlicerAstro.Log"), "on"))
          {
          d->LogPushButton->setChecked(true);
          }
        else
          {
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
  if (this->isVisible())
    {
    this->updateTransferFunction();
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
    if (colorNode->GetLookupTable())
      {
      colorNode->GetLookupTable()->SetScaleToLog10();
      }
    }
  else
    {
    colorNode->SetAttribute("SlicerAstro.Log", "off");
    if (colorNode->GetLookupTable())
      {
      colorNode->GetLookupTable()->SetScaleToLinear();
      }
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
void qSlicerAstroScalarVolumeDisplayWidget::setColorNode(vtkMRMLNode* colorNode)
{
  vtkMRMLAstroVolumeDisplayNode* displayNode =
    this->volumeDisplayNode();
  if (!displayNode || !colorNode)
    {
    return;
    }
  Q_ASSERT(vtkMRMLColorNode::SafeDownCast(colorNode));
  displayNode->SetAndObserveColorNodeID(colorNode->GetID());
}
