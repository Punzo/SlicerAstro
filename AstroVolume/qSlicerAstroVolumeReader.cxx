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
#include <QFileInfo>
#include <QSettings>

// SlicerQt includes
#include "qSlicerAstroVolumeIOOptionsWidget.h"
#include "qSlicerAstroVolumeReader.h"
#include "qSlicerAstroSubCubeDialog.h"
#include "qSlicerAstroTheme.h"
#include <qSlicerApplication.h>
#include <QMainWindow>
#include <vtkFITSReader.h>
#include <vtkNew.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkIntArray.h>

// Logic includes
#include <vtkMRMLColorLogic.h>
#include <vtkSlicerApplicationLogic.h>
#include <vtkSlicerVolumesLogic.h>
#include <vtkSlicerAstroVolumeLogic.h>

// Slicer includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerModuleManager.h>

// MRML includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLMessageCollection.h>
#include <vtkMRMLSelectionNode.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>

//-----------------------------------------------------------------------------
class qSlicerAstroVolumeReaderPrivate
{
  public:
  vtkSmartPointer<vtkSlicerVolumesLogic> Logic;
};

//-----------------------------------------------------------------------------
qSlicerAstroVolumeReader::qSlicerAstroVolumeReader(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroVolumeReaderPrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeReader::qSlicerAstroVolumeReader(vtkSlicerVolumesLogic* logic, QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroVolumeReaderPrivate)
{
  this->setLogic(logic);
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeReader::~qSlicerAstroVolumeReader()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeReader::setLogic(vtkSlicerVolumesLogic* logic)
{
  Q_D(qSlicerAstroVolumeReader);
  d->Logic = logic;
}

//-----------------------------------------------------------------------------
vtkSlicerVolumesLogic* qSlicerAstroVolumeReader::logic()const
{
  Q_D(const qSlicerAstroVolumeReader);
  return d->Logic.GetPointer();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroVolumeReader::description()const
{
  return "AstroVolume";
}

//-----------------------------------------------------------------------------
qSlicerIO::IOFileType qSlicerAstroVolumeReader::fileType()const
{
  return QString("AstroVolumeFile");
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroVolumeReader::extensions()const
{
  return QStringList()
    << "Volume (*.fits *.fit *.fts *.fz)"
    << "Volume (*.fits.gz *.fit.gz *.fts.gz)"
    << "Image (*.fits *.fit *.fts *.fz)"
    << "Image (*.fits.gz *.fit.gz *.fts.gz)"
    << "All Files (*)";
}

//-----------------------------------------------------------------------------
qSlicerIOOptions* qSlicerAstroVolumeReader::options()const
{
  // set the mrml scene on the options widget to allow selecting a color node
  qSlicerIOOptionsWidget* options = new qSlicerAstroVolumeIOOptionsWidget;
  options->setMRMLScene(this->mrmlScene());
  return options;
}

namespace
{
/// Data larger than this fraction of the memory: a part may be loaded
/// (SlicerAstro settings, default 25%)
double LargeDataFraction()
{
  const double fraction = QSettings().value("SlicerAstro/LargeDataFraction", 0.25).toDouble();
  return fraction > 0. ? fraction : 0.25;
}
}

//-----------------------------------------------------------------------------
bool qSlicerAstroVolumeReader::load(const IOProperties& properties)
{
  Q_D(qSlicerAstroVolumeReader);
  Q_ASSERT(properties.contains("fileName"));
  QString fileName = properties["fileName"].toString();

  QString name = QFileInfo(fileName).baseName();

  if (properties.contains("name"))
    {
    name = properties["name"].toString();
    }
  // FITS masks are segmentations (SegmentationFile): no label map volumes
  if (properties.contains("labelmap") && properties["labelmap"].toBool())
    {
    QString message = tr("%1: FITS masks are loaded as segmentations in SlicerAstro, not as label map volumes "
                         "(load the file as a Segmentation).").arg(fileName);
    qWarning() << Q_FUNC_INFO << message;
    this->userMessages()->AddMessage(vtkCommand::ErrorEvent, message.toStdString());
    return false;
    }
  int options = 0;
  // always centered on the voxel grid (as the FITS masks): cubes and masks
  // overlap whatever the order of loading
  options |= 0x2;
  if (properties.contains("singleFile"))
    {
    options |= properties["singleFile"].toBool() ? 0x4 : 0x0;
    }
  if (properties.contains("autoWindowLevel"))
    {
    options |= properties["autoWindowLevel"].toBool() ? 0x8: 0x0;
    }
  vtkSmartPointer<vtkStringArray> fileList;
  if (properties.contains("fileNames"))
    {
    fileList = vtkSmartPointer<vtkStringArray>::New();
    for (const QString& file : properties["fileNames"].toStringList())
      {
      fileList->InsertNextValue(file.toLatin1());
      }
    }

  Q_ASSERT(d->Logic);

  // the extension (HDU) and the part of the file to read: properties "hdu"
  // (cfitsio HDU number; default the first image HDU with data) and
  // "subExtent" (6 voxel indices i0, i1, j0, j1, k0, k1 of the data of the
  // file): given to the storage node created for this load
  int hdu = properties.value("hdu", 0).toInt();
  int subExtent[6] = {0, -1, 0, -1, 0, -1};
  bool useSubExtent = false;
  const QVariantList extent = properties.value("subExtent").toList();
  if (extent.size() == 6)
    {
    for (int index = 0; index < 6; index++)
      {
      subExtent[index] = extent[index].toInt();
      }
    useSubExtent = true;
    }
  // data that would take a large part of the memory: the user chooses the
  // whole data or a part (only that part is read), and the extension;
  // property "askLargeData" false: not asked (e.g. already chosen)
  qSlicerApplication* app = qSlicerApplication::application();
  if (!useSubExtent && properties.value("askLargeData", true).toBool() && app && app->mainWindow() &&
      !app->testAttribute(qSlicerCoreApplication::AA_EnableTesting))
    {
    const double bytes = qSlicerAstroSubCubeDialog::dataBytes(fileName, hdu);
    const double memory = qSlicerAstroSubCubeDialog::physicalMemory();
    if (bytes > 0. && memory > 0. && bytes > LargeDataFraction() * memory)
      {
      const QList<QWidget*> hiddenProgress = qSlicerAstroTheme::suspendProgressDialogs();
      const int choice = qSlicerAstroSubCubeDialog::ask(fileName, &hdu, app->mainWindow(), subExtent);
      qSlicerAstroTheme::resumeProgressDialogs(hiddenProgress);
      if (choice == qSlicerAstroSubCubeDialog::Cancelled)
        {
        return false;
        }
      useSubExtent = choice == qSlicerAstroSubCubeDialog::SubCube;
      }
    }
  if (hdu > 0 && !properties.contains("name"))
    {
    // the name of the extension in the name of the data (e.g. cube_STAT)
    vtkNew<vtkIntArray> numbers;
    vtkNew<vtkStringArray> names;
    vtkFITSReader::GetImageHDUs(fileName.toUtf8().constData(), numbers, names, nullptr);
    for (vtkIdType index = 0; index < numbers->GetNumberOfValues(); index++)
      {
      if (numbers->GetValue(index) == hdu && index > 0)
        {
        name += QString("_") + QString::fromStdString(names->GetValue(index)).replace(' ', '_');
        }
      }
    }
  if (useSubExtent && !properties.contains("name"))
    {
    name += "_part";
    }
  vtkMRMLAstroVolumeStorageNode::SetNextReadOptions(hdu, subExtent, useSubExtent);
  vtkMRMLVolumeNode* node = d->Logic->AddArchetypeVolume(
    fileName.toLatin1(),
    name.toLatin1(),
    options,
    fileList.GetPointer());
  vtkMRMLAstroVolumeStorageNode::ClearNextReadOptions();
  if (node)
    {
    // the first free name (the scene's own numbering never restarts: data
    // loaded again after deleting the previous copies would be name_2, _3)
    if (node->GetScene())
      {
      node->SetName(vtkSlicerAstroVolumeLogic::GetFreeNodeName(node->GetScene(), name.toStdString(), node).c_str());
      }
    // the colour table chosen in the load options; otherwise (or Slicer's
    // default grey) the SlicerAstro default: Observatory, Doppler for
    // velocity fields, Ember for dispersions
    vtkSlicerApplicationLogic* coreAppLogic = qSlicerCoreApplication::application() ?
      qSlicerCoreApplication::application()->applicationLogic() : nullptr;
    const QString defaultGrey = coreAppLogic && coreAppLogic->GetColorLogic() ?
      QString::fromUtf8(coreAppLogic->GetColorLogic()->GetDefaultVolumeColorNodeID()) : QString();
    const QString colorNodeID = properties.value("colorNodeID").toString();
    if (!colorNodeID.isEmpty() && colorNodeID != defaultGrey)
      {
      if (node->GetDisplayNode())
        {
        node->GetDisplayNode()->SetAndObserveColorNodeID(colorNodeID.toLatin1());
        }
      }
    else if (node->IsA("vtkMRMLAstroVolumeNode"))
      {
      vtkSlicerAstroVolumeLogic::SetDefaultColorTable(node);
      }
    // contours and mask segmentations of the loaded astronomical volume
    vtkMRMLAstroVolumeNode* astroVolume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
    qSlicerAbstractCoreModule* astroVolumeModule = qSlicerCoreApplication::application() ?
      qSlicerCoreApplication::application()->moduleManager()->module("AstroVolume") : nullptr;
    vtkSlicerAstroVolumeLogic* astroVolumeLogic = astroVolumeModule ?
      vtkSlicerAstroVolumeLogic::SafeDownCast(astroVolumeModule->logic()) : nullptr;
    if (astroVolume && astroVolumeLogic)
      {
      astroVolumeLogic->EnsureVolumeSegmentations(astroVolume);
      }

    vtkSlicerApplicationLogic* appLogic =
      d->Logic->GetApplicationLogic();
    vtkMRMLSelectionNode* selectionNode =
      appLogic ? appLogic->GetSelectionNode() : 0;
    if (selectionNode)
      {
      selectionNode->SetActiveVolumeID(node->GetID());
      if (appLogic)
        {
        appLogic->PropagateVolumeSelection(); // includes FitSliceToAll by default
        }
      }
    this->setLoadedNodes(QStringList(QString(node->GetID())));
    }
  else
    {
    this->setLoadedNodes(QStringList());
    }

  return node != 0;
}
