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

  This file was developed by Davide Punzo, Kapteyn Astronomical Institute,
  and was supported through the European Research Council grant nr. 291531.

==============================================================================*/

// Segmentations includes
#include "qSlicerSegmentEditorAstroContoursEffect.h"
#include "vtkMRMLSegmentationDisplayNode.h"
#include "vtkMRMLSegmentationNode.h"
#include "vtkMRMLSegmentEditorNode.h"
#include "vtkOrientedImageData.h"
#include <vtkSlicerSegmentationsModuleLogic.h>

// Qt includes
#include <QObject>
#include <QDebug>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QString>

// VTK includes
#include <vtkImageThreshold.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkPointData.h>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationNode.h>

// AstroMRML includes
#include <vtkMRMLAstroVolumeNode.h>

//-----------------------------------------------------------------------------
class qSlicerSegmentEditorAstroContoursEffectPrivate: public QObject
{
  Q_DECLARE_PUBLIC(qSlicerSegmentEditorAstroContoursEffect);
protected:
  qSlicerSegmentEditorAstroContoursEffect* const q_ptr;
public:
  qSlicerSegmentEditorAstroContoursEffectPrivate(qSlicerSegmentEditorAstroContoursEffect& object);
  ~qSlicerSegmentEditorAstroContoursEffectPrivate();
  void init();

  QIcon AstroContoursIcon;

  QFrame* AstroContournsFrame;
  QLabel* DataInfoLabel;
  QLabel* ContourLevelsLabel;
  QLabel* ContoursNamePrefix;
  QLineEdit* ContourLevelsLineEdit;
  QLineEdit* ContoursNamePrefixLineEdit;
  QPushButton* ApplyButton;
  QSpacerItem *verticalSpacer_1;
  QSpacerItem *verticalSpacer_2;
  QSpacerItem *verticalSpacer_3;
  QHBoxLayout *horizontalLayout_1;
  QHBoxLayout *horizontalLayout_2;
  QVBoxLayout *verticalLayout_1;
  QVBoxLayout *verticalLayout_2;
  QVBoxLayout *verticalLayout_3;

protected slots:

  void onDisplayThresholdLevelsChanged(QString DisplayThresholdLevels);
};

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
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
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

//----------------------------------------------------------------------------
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
}

}// end namespace

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAstroContoursEffectPrivate::qSlicerSegmentEditorAstroContoursEffectPrivate(qSlicerSegmentEditorAstroContoursEffect& object)
  : q_ptr(&object)
{
  this->AstroContoursIcon = QIcon(":Icons/AstroContours.png");
}

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAstroContoursEffectPrivate::~qSlicerSegmentEditorAstroContoursEffectPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroContoursEffectPrivate::init()
{
  Q_Q(qSlicerSegmentEditorAstroContoursEffect);
  // Create options frame for this effect
  this->AstroContournsFrame = new QFrame();
  this->AstroContournsFrame->setLayout(new QHBoxLayout());
  q->addOptionsWidget(this->AstroContournsFrame);

  this->DataInfoLabel = new QLabel("Data Info: ");
  this->verticalLayout_1 = new QVBoxLayout();
  this->verticalLayout_1->setObjectName(QLatin1String("verticalLayout_1"));
  this->verticalLayout_1->addWidget(this->DataInfoLabel);
  this->verticalSpacer_1 = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
  this->verticalLayout_1->addItem(this->verticalSpacer_1);
  q->addOptionsWidget(this->verticalLayout_1);

  this->horizontalLayout_1 = new QHBoxLayout();
  this->horizontalLayout_1->setObjectName(QLatin1String("horizontalLayout_1"));
  this->ContoursNamePrefix = new QLabel("Contours name prefix: ");
  this->ContoursNamePrefix->setMinimumSize(QSize(140, 30));
  this->horizontalLayout_1->addWidget(this->ContoursNamePrefix);
  this->ContoursNamePrefixLineEdit = new QLineEdit("Galaxy");
  this->ContoursNamePrefixLineEdit->setMinimumSize(QSize(0, 30));
  this->horizontalLayout_1->addWidget(this->ContoursNamePrefixLineEdit);
  this->verticalLayout_2 = new QVBoxLayout();
  this->verticalLayout_2->setObjectName(QLatin1String("verticalLayout_2"));
  this->verticalLayout_2->addLayout(this->horizontalLayout_1);
  this->verticalSpacer_2 = new QSpacerItem(20, 5, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
  this->verticalLayout_2->addItem(this->verticalSpacer_2);
  q->addOptionsWidget(this->verticalLayout_2);

  this->horizontalLayout_2 = new QHBoxLayout();
  this->horizontalLayout_2->setObjectName(QLatin1String("horizontalLayout_2"));
  this->ContourLevelsLabel = new QLabel("Contour levels: ");
  this->ContourLevelsLabel->setMinimumSize(QSize(140, 30));
  this->horizontalLayout_2->addWidget(this->ContourLevelsLabel);

  this->ContourLevelsLineEdit = new QLineEdit("3DDisplayThreshold -3;3;7;15");
  this->ContourLevelsLineEdit->setMinimumSize(QSize(0, 30));
  this->ContourLevelsLineEdit->setToolTip("Contour levels: "
                                          "to specify the levels use the following format: 'FISRT:LAST+SPACING' for linear spacing "
                                          "or 'FISRT:LAST*SPACING' for non linear spacing. \n\n"
                                          "The levels can also be specified as a list (e.g., 'VALUE1;VALUE2;VALUE3'). "
                                          "In the case of a list, it is possible also to specify for each contour"
                                          " both the MIN and MAX intensity values of the level "
                                          "(e.g., 'CONTOUR1MIN,CONTOUR1MAX;CONTOUR2MIN,CONTOUR2MAX'). \n\n "
                                          "If the string is preceded by '3DDisplayThreshold', the levels are evaluated"
                                          " in units of the value of 3DDisplayThreshold of the dataset. \n\n"
                                          "If the Histogram is active in the plot view and a selection is made, by typing the string"
                                          " 'Histogram' will create a segmentation around the selection.  \n\n"
                                          "The contours in SlicerAstro are a full 3D segmentation. "
                                          "Therefore it can be computationally heavy to segment "
                                          "(marching cubes algorithm) and visualize around the noise "
                                          "range [-2, 2] RMS for large datacubes.");
  this->horizontalLayout_2->addWidget(this->ContourLevelsLineEdit);
  this->verticalLayout_3 = new QVBoxLayout();
  this->verticalLayout_3->setObjectName(QLatin1String("verticalLayout_3"));
  this->verticalLayout_3->addLayout(this->horizontalLayout_2);
  this->verticalSpacer_3 = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
  this->verticalLayout_3->addItem(this->verticalSpacer_3);
  q->addOptionsWidget(this->verticalLayout_3);

  this->ApplyButton = new QPushButton("Create Contours");
  this->ApplyButton->setMinimumSize(QSize(0, 35));
  this->ApplyButton->setToolTip("Push to create and visualize new contours. The color of the "
                                "contours will be set as current color of the first segment "
                                "(the one manually created to enable and edit the effect).");
  q->addOptionsWidget(this->ApplyButton);

  QObject::connect(this->ApplyButton, SIGNAL(clicked()), q, SLOT(CreateContours()));
}

//----------------------------------------------------------------------------
qSlicerSegmentEditorAstroContoursEffect::qSlicerSegmentEditorAstroContoursEffect(QObject* parent)
 : Superclass(parent)
 , d_ptr( new qSlicerSegmentEditorAstroContoursEffectPrivate(*this) )
{
  this->m_Name = QString("AstroContours");
  this->m_ShowEffectCursorInThreeDView = false;
  this->m_ShowEffectCursorInSliceView = false;
}

//----------------------------------------------------------------------------
qSlicerSegmentEditorAstroContoursEffect::~qSlicerSegmentEditorAstroContoursEffect()
{
}

//---------------------------------------------------------------------------
QIcon qSlicerSegmentEditorAstroContoursEffect::icon()
{
  Q_D(qSlicerSegmentEditorAstroContoursEffect);

  return d->AstroContoursIcon;
}

//---------------------------------------------------------------------------
QString const qSlicerSegmentEditorAstroContoursEffect::helpText()const
{
    return QString("Tool for the creation of Contours.  ");
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroContoursEffect::setupOptionsFrame()
{
  Q_D(qSlicerSegmentEditorAstroContoursEffect);

  // Setup widgets corresponding to the parent class of this effect
  Superclass::setupOptionsFrame();

  d->init();
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroContoursEffect::setMRMLDefaults()
{
  Q_D(qSlicerSegmentEditorAstroContoursEffect);
  Superclass::setMRMLDefaults();
  this->setCommonParameterDefault("3DDisplayThreshold", 0.);
  this->setCommonParameterDefault("MIN", 0.);
  this->setCommonParameterDefault("MAX", 0.);
  this->setCommonParameterDefault("FluxUnit", "JY/BEAM");
  this->setCommonParameterDefault("ContourLeveles", "3DDisplayThreshold -3;3;7;15");
  this->setCommonParameterDefault("NamePrefix", "Galaxy");
  this->setCommonParameterDefault("NameIndex", "1");
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroContoursEffect::updateGUIFromMRML()
{
  Q_D(qSlicerSegmentEditorAstroContoursEffect);

  if (!this->active())
    {
    // updateGUIFromMRML is called when the effect is activated
    return;
    }

  if (!this->scene())
    {
    return;
    }

  Superclass::updateGUIFromMRML();

  QString Levels = this->parameter("ContourLeveles");
  d->ContourLevelsLineEdit->blockSignals(true);
  d->ContourLevelsLineEdit->setText(Levels);
  d->ContourLevelsLineEdit->blockSignals(false);

  QString NamePrefix = this->parameter("NamePrefix");
  d->ContoursNamePrefixLineEdit->blockSignals(true);
  d->ContoursNamePrefixLineEdit->setText(NamePrefix);
  d->ContoursNamePrefixLineEdit->blockSignals(false);

  std::string DataInfo;
  double MIN = this->doubleParameter("MIN");
  double MAX = this->doubleParameter("MAX");
  double DisplayThreshold = this->doubleParameter("3DDisplayThreshold");

  DataInfo = "Dataset info: \n"
             "MIN = " + DoubleToString(MIN) +
             " " + this->parameter("FluxUnit").toStdString()+ "\n"
             "MAX = " + DoubleToString(MAX) +
             " " + this->parameter("FluxUnit").toStdString()+ "\n"
             "3DDisplayThreshold = " + DoubleToString(DisplayThreshold) +
             " " + this->parameter("FluxUnit").toStdString();
  d->DataInfoLabel->setText(QString::fromStdString(DataInfo));
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroContoursEffect::updateMRMLFromGUI()
{
  Q_D(qSlicerSegmentEditorAstroContoursEffect);

  Superclass::updateMRMLFromGUI();

}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroContoursEffect::CreateContours()
{
  Q_D(qSlicerSegmentEditorAstroContoursEffect);

  QString ContourLevels = d->ContourLevelsLineEdit->text();
  this->setParameter("ContourLeveles", ContourLevels);
  std::string LevelsStdString = ContourLevels.toStdString();

  QString NamePrefix = d->ContoursNamePrefixLineEdit->text();
  this->setParameter("NamePrefix", NamePrefix);
  std::string NamePrefixStdString = NamePrefix.toStdString();

  vtkMRMLSegmentationNode* segmentationNode = this->parameterSetNode()->GetSegmentationNode();

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
    NamePrefixStdString += this->parameter("NameIndex").toStdString();
    int nameIndex = StringToInt(this->parameter("NameIndex").toStdString().c_str());
    nameIndex++;
    this->setParameter("NameIndex", IntToString(nameIndex).c_str());
    }

  vtkNew<vtkDoubleArray> Levels;
  vtkNew<vtkDoubleArray> Delimiters;
  vtkNew<vtkStringArray> SegmentIDs;
  std::stringstream ss(LevelsStdString);

  double value;
  double MIN = this->doubleParameter("MIN");
  double MAX = this->doubleParameter("MAX");
  double DisplayThreshold = this->doubleParameter("3DDisplayThreshold");

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

  vtkMRMLAstroVolumeNode* masterVolume = vtkMRMLAstroVolumeNode::SafeDownCast(
    this->parameterSetNode()->GetMasterVolumeNode());

  if (!masterVolume)
    {
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
  this->scene()->SaveStateForUndo();

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

    double color[3];
    if (segmentationNode->GetSegmentation()->GetNthSegment(0))
      {
      segmentationNode->GetSegmentation()->GetNthSegment(0)->GetColor(color);
      }
    Segment->SetColor(color);

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

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAbstractEffect* qSlicerSegmentEditorAstroContoursEffect::clone()
{
    return new qSlicerSegmentEditorAstroContoursEffect();
}

void qSlicerSegmentEditorAstroContoursEffect::activate()
{
  Q_D(qSlicerSegmentEditorAstroContoursEffect);
  Superclass::activate();
  this->masterVolumeNodeChanged();
}

//---------------------------------------------------------------------------
void qSlicerSegmentEditorAstroContoursEffect::deactivate()
{
  Q_D(qSlicerSegmentEditorAstroContoursEffect);
  Superclass::deactivate();
}

//---------------------------------------------------------------------------
void qSlicerSegmentEditorAstroContoursEffect::masterVolumeNodeChanged()
{
  vtkMRMLScalarVolumeNode *masterVolume = this->parameterSetNode()->GetMasterVolumeNode();

  if (!masterVolume)
    {
    return;
    }

  vtkMRMLAstroVolumeNode *astroMasterVolume = vtkMRMLAstroVolumeNode::SafeDownCast(masterVolume);

  if (!astroMasterVolume)
    {
    return;
    }

  double DisplayThreshold = StringToDouble(astroMasterVolume->GetAttribute("SlicerAstro.3DDisplayThreshold"));
  double MAX = StringToDouble(astroMasterVolume->GetAttribute("SlicerAstro.DATAMAX"));
  double MIN = StringToDouble(astroMasterVolume->GetAttribute("SlicerAstro.DATAMIN"));
  QString unit(astroMasterVolume->GetAttribute("SlicerAstro.BUNIT"));

  this->setCommonParameter("3DDisplayThreshold", DisplayThreshold);
  this->setCommonParameter("MAX", MAX);
  this->setCommonParameter("MIN", MIN);
  this->setCommonParameter("FluxUnit", unit);
  this->setCommonParameter("NamePrefix", masterVolume->GetName());
}
