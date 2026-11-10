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
#include <QFileInfo>

// SlicerQt includes
#include "qSlicerAstroSegmentationReader.h"

// MRML includes
#include <vtkMRMLAstroSegmentationStorageNode.h>
#include <vtkMRMLMessageCollection.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentationNode.h>

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
  return QStringList() << "FITS mask (*.fits)"
                       << "FITS mask (*.fits.gz)";
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
  storageNode->SetCenterImage(properties.value("center", true).toBool());
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
  this->setLoadedNodes(QStringList() << QString(segmentationNode->GetID()));
  return true;
}
