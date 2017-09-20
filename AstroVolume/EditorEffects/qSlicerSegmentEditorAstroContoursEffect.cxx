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
  QLineEdit* ContourLevelsLineEdit;
  QPushButton* ApplyButton;

protected slots:

  void onRMSLevelsChanged(QString RMSLevels);
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

//------------S-----------------------------------------------------------------
void qSlicerSegmentEditorAstroContoursEffectPrivate::init()
{
  Q_Q(qSlicerSegmentEditorAstroContoursEffect);
  // Create options frame for this effect
  this->AstroContournsFrame = new QFrame();
  this->AstroContournsFrame->setLayout(new QHBoxLayout());
  q->addOptionsWidget(this->AstroContournsFrame);

  this->DataInfoLabel = new QLabel("Data Info: ");
  q->addOptionsWidget(this->DataInfoLabel);

  this->ContourLevelsLabel = new QLabel("Contour Levels: ");
  q->addOptionsWidget(this->ContourLevelsLabel);

  this->ContourLevelsLineEdit = new QLineEdit("RMS -3;3;7;15");
  this->ContourLevelsLineEdit->setToolTip("Contour Levels. The Levels can be specified as a list (e.g., 'VALUE1;VALUE2;VALUE3') "
                                          "or in the following format 'FISRT:LAST:SPACING'. "
                                          "In addition, in teh case of a list, it is possible also to specify for each Contour both the MIN and MAX intensity values of the level "
                                          "(e.g., 'CONTOUR1MIN,CONTOUR1MAX;CONTOUR2MIN,CONTOUR2MAX'). \n\n "
                                          "If the string is preceded by 'RMS', the levels are evaluated in units of RMS of the datacube. \n\n"
                                          "The contours in SlicerAstro are a full 3D segmentation. Therefore it can be computationally heavy "
                                          "to segment (marching cubes algorithm) and visualize around the noise range [-2, 2] RMS for large datacubes.");
  q->addOptionsWidget(this->ContourLevelsLineEdit);

  this->ApplyButton = new QPushButton("Create Contours");
  q->addOptionsWidget(this->ApplyButton);

  QObject::connect(this->ContourLevelsLineEdit, SIGNAL(textChanged(QString)), q, SLOT(onContourLevelsChanged(QString)));
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
    return QString("Tool for the creation of Coutours.  ");
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
  this->setCommonParameterDefault("RMS", 0.);
  this->setCommonParameterDefault("MIN", 0.);
  this->setCommonParameterDefault("MAX", 0.);
  this->setCommonParameterDefault("FluxUnit", "JY/BEAM");
  this->setCommonParameterDefault("ContourLeveles", "RMS -3;3;7;15");
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
  std::string DataInfo;
  double MIN = this->doubleParameter("MIN");
  double MAX = this->doubleParameter("MAX");
  double RMS = this->doubleParameter("RMS");

  DataInfo = "Data Info: MIN = " + DoubleToString(MIN) +
             " ; MAX = " + DoubleToString(MAX) +
             " ; RMS = " + DoubleToString(RMS) +
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
void qSlicerSegmentEditorAstroContoursEffect::onContourLevelsChanged(QString ContourLevels)
{
  Q_D(qSlicerSegmentEditorAstroContoursEffect);

  this->setParameter("ContourLeveles", ContourLevels);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroContoursEffect::CreateContours()
{
  Q_D(qSlicerSegmentEditorAstroContoursEffect);

  QString LevelsString = this->parameter("ContourLeveles");
  std::string LevelsStdString = LevelsString.toStdString();

  vtkNew<vtkDoubleArray> Levels;
  vtkNew<vtkDoubleArray> Delimiters;
  vtkNew<vtkStringArray> SegmentIDs;
  std::stringstream ss(LevelsStdString);

  double value;
  double MIN = this->doubleParameter("MIN");
  double MAX = this->doubleParameter("MAX");
  double RMS = this->doubleParameter("RMS");

  bool convert = false;
  std::size_t found = LevelsStdString.find("RMS");
  if (found != std::string::npos)
    {
    convert = true;
    }

  bool list = false;
  bool increment = false;
  bool renzogram = false;
  found = LevelsStdString.find(";");
  if (found != std::string::npos)
    {
    found = LevelsStdString.find(",");
    if (found != std::string::npos)
      {
      renzogram = true;
      }
    else
      {
      list = true;
      }
    }
  else
    {
    found = LevelsStdString.find(":");
    if (found != std::string::npos)
      {
      increment = true;
      }
    else
      {
      found = LevelsStdString.find(",");
      if (found != std::string::npos)
        {
        renzogram = true;
        }
      }
    }

  if ((list && increment) || (renzogram && increment) || (!list && !increment && !renzogram))
    {
    QString message = QString("The input string defining the Contour Levels is formatted wrongly. Check the ToolTip.");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to create Contours"), message);
    return;
    }

  if (list)
    { 
    if (convert)
      {
      for (int ii = 0; ii < 3; ii++)
        {
        ss.ignore();
        }
      }
    while (ss >> value)
      {
      if (convert)
        {
        Levels->InsertNextValue(value * RMS);
        }
      else
        {
        Levels->InsertNextValue(value);
        }
      if (ss.peek() == ';' || ss.peek() == 'R' || ss.peek() == 'M' || ss.peek() == 'S')
        {
        ss.ignore();
        }
      }
    }
  else if (renzogram)
    {
    if (convert)
      {
      for (int ii = 0; ii < 3; ii++)
        {
        ss.ignore();
        }
      }
    while (ss >> value)
      {
      if (convert)
        {
        Levels->InsertNextValue(value * RMS);
        }
      else
        {
        Levels->InsertNextValue(value);
        }
      if (ss.peek() == ';' || ss.peek() == ',' || ss.peek() == 'R' || ss.peek() == 'M' || ss.peek() == 'S')
        {
        ss.ignore();
        }
      }
    }
  else if (increment)
    {
    if (convert)
      {
      for (int ii = 0; ii < 3; ii++)
        {
        ss.ignore();
        }
      }
    while (ss >> value)
      {
      Delimiters->InsertNextValue(value);
      if (ss.peek() == ':' || ss.peek() == 'R' || ss.peek() == 'M' || ss.peek() == 'S')
        {
        ss.ignore();
        }
      }
    double iter = Delimiters->GetValue(0);
    while (iter <= Delimiters->GetValue(1))
      {
      if (convert)
        {
        Levels->InsertNextValue(iter * RMS);
        }
      else
        {
        Levels->InsertNextValue(iter);
        }
      iter += Delimiters->GetValue(2);
      }
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

  vtkMRMLSegmentationNode* segmentationNode = this->parameterSetNode()->GetSegmentationNode();

  if (!segmentationNode->GetDisplayNode())
    {
    segmentationNode->CreateDefaultDisplayNodes();
    }

  // Create empty segment in current segmentation
  this->scene()->SaveStateForUndo();

  vtkMRMLAstroVolumeNode* masterVolume = vtkMRMLAstroVolumeNode::SafeDownCast(
    this->parameterSetNode()->GetMasterVolumeNode());

  for (int ii = 0; ii < Levels->GetNumberOfValues(); ii++)
    {
    double ContourLevel = Levels->GetValue(ii);
    double ContourLevelNext = 0.;
    if (renzogram)
      {
      ii++;
      ContourLevelNext = Levels->GetValue(ii);
      }
    std::string SegmentID = masterVolume->GetName();
    SegmentID += "Contour" + IntToString(ii + 1);
    SegmentIDs->InsertNextValue(SegmentID.c_str());
    vtkSegment *Segment = segmentationNode->GetSegmentation()->GetSegment(SegmentID);
    if(!Segment)
      {
      SegmentID = segmentationNode->GetSegmentation()->AddEmptySegment(SegmentID, SegmentID);
      }

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
    }

  for (int ii = 0; ii < segmentationNode->GetNumberOfDisplayNodes(); ii++)
    {
    vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(segmentationNode->GetNthDisplayNode(ii));
    SegmentationDisplayNode->SetAllSegmentsVisibility(false);

    for (int jj = 0; jj < SegmentIDs->GetNumberOfValues(); jj ++)
      {
      std::string SegmentID = SegmentIDs->GetValue(jj);
      SegmentationDisplayNode->SetSegmentVisibility(SegmentID, true);
      SegmentationDisplayNode->SetSegmentVisibility3D(SegmentID, true);
      double opacity = 0.5 + double((jj + 1.) / ( 2. * SegmentIDs->GetNumberOfValues()));
      SegmentationDisplayNode->SetSegmentOpacity3D(SegmentID, opacity);
      SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentID, true);
      SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentID, false);
      }
    }

  this->CreateSurface(true);
}

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAbstractEffect* qSlicerSegmentEditorAstroContoursEffect::clone()
{
  return new qSlicerSegmentEditorAstroContoursEffect();
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

  double RMS = StringToDouble(astroMasterVolume->GetAttribute("SlicerAstro.RMS"));
  double MAX = StringToDouble(astroMasterVolume->GetAttribute("SlicerAstro.DATAMAX"));
  double MIN = StringToDouble(astroMasterVolume->GetAttribute("SlicerAstro.DATAMIN"));
  QString unit(astroMasterVolume->GetAttribute("SlicerAstro.BUNIT"));

  this->setCommonParameter("RMS", RMS);
  this->setCommonParameter("MAX", MAX);
  this->setCommonParameter("MIN", MIN);
  this->setCommonParameter("FluxUnit", unit);
}

//---------------------------------------------------------------------------
void qSlicerSegmentEditorAstroContoursEffect::CreateSurface(bool on)
{
  if (!this->parameterSetNode())
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segment editor parameter set node";
    return;
    }

  vtkMRMLSegmentationNode* segmentationNode = this->parameterSetNode()->GetSegmentationNode();
  if (!segmentationNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentationNode";
    return;
    }
  vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(
    segmentationNode->GetDisplayNode());
  if (!displayNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentationDisplayNode";
    return;
    }

  // If just have been checked, then create closed surface representation and show it
  if (on)
    {
    // Make sure closed surface representation exists
    if (segmentationNode->GetSegmentation()->CreateRepresentation(
      vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName() ))
      {
      // Set closed surface as displayed poly data representation
      displayNode->SetPreferredDisplayRepresentationName3D(
        vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName() );
      // But keep binary labelmap for 2D
      if (segmentationNode->GetSegmentation()->ContainsRepresentation(
            vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName()))
        {
        displayNode->SetPreferredDisplayRepresentationName2D(
          vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName());
        }
      else
        {
        displayNode->SetPreferredDisplayRepresentationName2D(
          vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName());
        }
      }
    }
}
