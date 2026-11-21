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
#include <QAbstractButton>
#include <QApplication>
#include <QFileInfo>
#include <QMainWindow>
#include <QMessageBox>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>

// SlicerQt includes
#include "qSlicerAstroSegmentationReader.h"

// SlicerQt includes
#include <qSlicerApplication.h>

// AstroVolume widgets
#include <qSlicerAstroTheme.h>

// MRML includes
#include <vtkMRMLAstroSegmentationStorageNode.h>
#include <vtkMRMLMessageCollection.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentationNode.h>

// Logic includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkSlicerAstroVolumeLogic.h>

// vtkFits includes
#include <vtkFITSReader.h>

// VTK includes
#include <vtkSmartPointer.h>

//-----------------------------------------------------------------------------
qSlicerAstroSegmentationReader::qSlicerAstroSegmentationReader(QObject* parent)
  : Superclass(parent)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroSegmentationReader::~qSlicerAstroSegmentationReader() = default;

//-----------------------------------------------------------------------------
QString qSlicerAstroSegmentationReader::description() const
{
  return "FITS mask (segmentation)";
}

//-----------------------------------------------------------------------------
qSlicerIO::IOFileType qSlicerAstroSegmentationReader::fileType() const
{
  return QString("SegmentationFile");
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroSegmentationReader::extensions() const
{
  return QStringList() << "FITS mask (*.fits *.fit *.fts *.fz)"
                       << "FITS mask (*.fits.gz *.fit.gz *.fts.gz)";
}

//-----------------------------------------------------------------------------
double qSlicerAstroSegmentationReader::canLoadFileConfidence(const QString& fileName) const
{
  const double confidence = this->Superclass::canLoadFileConfidence(fileName);
  if (confidence <= 0.)
    {
    return 0.;
    }
  // Masks: segmentation rather than volume (AstroVolume reader: 0.5 + extension);
  // other FITS files can still be loaded as segmentations on request.
  return vtkFITSReader::IsMaskFile(fileName.toUtf8().constData()) ? confidence + 0.1 : 0.2;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroSegmentationReader::load(const IOProperties& properties)
{
  Q_ASSERT(properties.contains("fileName"));
  const QString fileName = properties["fileName"].toString();
  vtkMRMLScene* scene = this->mrmlScene();
  if (!scene)
    {
    return false;
    }
  this->userMessages()->ClearMessages();

  QString name = QFileInfo(fileName).baseName();
  if (properties.contains("name"))
    {
    name = properties["name"].toString();
    }

  vtkSmartPointer<vtkMRMLAstroSegmentationStorageNode> storageNode = vtkMRMLAstroSegmentationStorageNode::SafeDownCast(
    scene->AddNewNodeByClass("vtkMRMLAstroSegmentationStorageNode"));
  vtkSmartPointer<vtkMRMLSegmentationNode> segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(
    scene->AddNewNodeByClass("vtkMRMLSegmentationNode", name.toStdString()));
  if (!storageNode || !segmentationNode)
    {
    return false;
    }
  storageNode->SetFileName(fileName.toUtf8().constData());
  // centered on its voxel grid, as all the astronomical data
  storageNode->SetCenterImage(true);
  segmentationNode->SetAndObserveStorageNodeID(storageNode->GetID());

  const bool success = storageNode->ReadData(segmentationNode) != 0;
  this->userMessages()->AddMessages(storageNode->GetUserMessages());
  if (!success)
    {
    scene->RemoveNode(segmentationNode);
    scene->RemoveNode(storageNode);
    this->setLoadedNodes(QStringList());
    return false;
    }

  segmentationNode->CreateDefaultDisplayNodes();
  vtkSlicerAstroVolumeLogic::SetBinaryLabelmapDisplay(segmentationNode);
  // opacity saved with the mask (SEGOPAC keyword)
  vtkMRMLAstroSegmentationStorageNode::ApplySavedOpacity(segmentationNode);
  // mask of a loaded data cube (same voxel lattice, WCS; also the mask of a
  // part of the cube): it is placed on the grid of the cube and becomes its
  // mask (replacing the empty mask created with the cube)
  int offset[3] = {0, 0, 0};
  vtkMRMLAstroVolumeNode* volumeNode = vtkSlicerAstroVolumeLogic::FindVolumeOfMask(scene, segmentationNode, offset);
  if (volumeNode)
    {
    vtkSlicerAstroVolumeLogic::AttachMaskToVolume(segmentationNode, volumeNode, offset);
    }
  // one mask per dataset: the dataset has a mask with segments already, the
  // loaded mask replaces it, is merged into it, or is not loaded (property
  // "maskConflict": "replace", "merge", "cancel"; otherwise the user is
  // asked, or "merge" without a main window, e.g. in tests)
  if (vtkSlicerAstroVolumeLogic::GetConflictingMask(segmentationNode))
    {
    vtkMRMLSegmentationNode* currentMask = vtkSlicerAstroVolumeLogic::GetConflictingMask(segmentationNode);
    int resolution = vtkSlicerAstroVolumeLogic::MaskConflictMerge;
    const QString option = properties.value("maskConflict").toString();
    if (option == "replace")
      {
      resolution = vtkSlicerAstroVolumeLogic::MaskConflictReplace;
      }
    else if (option == "cancel")
      {
      resolution = vtkSlicerAstroVolumeLogic::MaskConflictCancel;
      }
    else if (option.isEmpty() && qSlicerApplication::application() &&
             qSlicerApplication::application()->mainWindow() &&
             !qSlicerApplication::application()->testAttribute(qSlicerCoreApplication::AA_EnableTesting))
      {
      QMessageBox question(QMessageBox::Question, tr("Mask of %1").arg(QString::fromUtf8(volumeNode->GetName())),
        tr("%1 has a mask with %2 segments already. Replace it with the mask of %3, merge the two masks, or do "
           "not load the file?").arg(QString::fromUtf8(volumeNode->GetName()))
           .arg(currentMask->GetSegmentation()->GetNumberOfSegments()).arg(QFileInfo(fileName).fileName()),
        QMessageBox::NoButton, qSlicerApplication::application()->mainWindow());
      QPushButton* replaceButton = question.addButton(tr("Replace"), QMessageBox::DestructiveRole);
      QPushButton* mergeButton = question.addButton(tr("Merge"), QMessageBox::AcceptRole);
      question.addButton(QMessageBox::Cancel);
      question.setDefaultButton(mergeButton);
      for (QAbstractButton* button : question.buttons())
        {
        // no icons of the Slicer style on the buttons
        button->setIcon(QIcon());
        }
      qSlicerAstroTheme::instance()->applyToDialog(&question);
      // the "Loading data" progress of Slicer would cover the question
      const QList<QWidget*> hiddenProgress = qSlicerAstroTheme::suspendProgressDialogs(&question);
      question.exec();
      qSlicerAstroTheme::resumeProgressDialogs(hiddenProgress);
      resolution = question.clickedButton() == replaceButton ? vtkSlicerAstroVolumeLogic::MaskConflictReplace :
        (question.clickedButton() == mergeButton ? vtkSlicerAstroVolumeLogic::MaskConflictMerge :
                                                   vtkSlicerAstroVolumeLogic::MaskConflictCancel);
      }
    vtkMRMLSegmentationNode* mask = vtkSlicerAstroVolumeLogic::ResolveMaskConflict(segmentationNode, resolution);
    if (resolution == vtkSlicerAstroVolumeLogic::MaskConflictCancel || !mask)
      {
      this->setLoadedNodes(QStringList());
      return false;
      }
    this->setLoadedNodes(QStringList() << QString(mask->GetID()));
    return true;
    }
  this->setLoadedNodes(QStringList() << QString(segmentationNode->GetID()));
  return true;
}
