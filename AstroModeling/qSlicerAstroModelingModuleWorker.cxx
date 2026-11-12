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
#include <QEventLoop>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

// AstroModeling includes
#include <qSlicerAstroModelingModuleWorker.h>
#include <vtkMRMLAstroModelingParametersNode.h>
#include <vtkMRMLTableNode.h>
#include <vtkSlicerAstroModelingLogic.h>

namespace
{
//-----------------------------------------------------------------------------
// Name of the operation of 3DBarolo for the application log
QString OperationName(vtkMRMLAstroModelingParametersNode* parametersNode)
{
  if (!parametersNode)
    {
    return QString("operation");
    }
  switch (parametersNode->GetOperation())
    {
    case vtkMRMLAstroModelingParametersNode::ESTIMATE:
      return QString("estimate of the initial parameters");
    case vtkMRMLAstroModelingParametersNode::CREATE:
      return QString("create model");
    case vtkMRMLAstroModelingParametersNode::FIT:
      return QString("fit model");
    case vtkMRMLAstroModelingParametersNode::UPDATE:
      return QString("calculate model");
    default:
      return QString("operation");
    }
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWorker::qSlicerAstroModelingModuleWorker(QObject *parent) :
    QObject(parent)
{
  _working =false;
  _abort = false;
  astroModelingLogic = nullptr;
}

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWorker::~qSlicerAstroModelingModuleWorker()
{
  astroModelingLogic = nullptr;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWorker::requestWork()
{
  mutex.lock();
  _working = true;
  _abort = false;
  mutex.unlock();

  emit workRequested();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWorker::abort()
{
  mutex.lock();
  if (_working)
    {
    _abort = true;
    qInfo().noquote() << "3DBarolo:" << OperationName(parametersNode) << "- cancel requested.";
    }
  mutex.unlock();
}

//-----------------------------------------------------------------------------
bool qSlicerAstroModelingModuleWorker::abortRequested()
{
  QMutexLocker locker(&mutex);
  return _abort;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWorker::SetAstroModelingLogic(vtkSlicerAstroModelingLogic *logic)
{
  astroModelingLogic = logic;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWorker::SetAstroModelingParametersNode(vtkMRMLAstroModelingParametersNode *pnode)
{
  parametersNode = pnode;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWorker::SetTableNode(vtkMRMLTableNode *tnode)
{
  internalTableNode = tnode;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWorker::doWork()
{
  // Checks if the process should be aborted
  mutex.lock();
  bool abort = _abort;
  mutex.unlock();

  lastResult = 0;
  if (abort)
    {
    qInfo().noquote() << "3DBarolo:" << OperationName(parametersNode) << "- cancelled before starting.";
    parametersNode->SetStatus(0);
    mutex.lock();
    _working = false;
    mutex.unlock();
    emit finished();
    return;
    }

  // model from the rings of the table, or estimate / create / fit
  qInfo().noquote() << "3DBarolo:" << OperationName(parametersNode) << "- started.";
  if (parametersNode->GetOperation() == vtkMRMLAstroModelingParametersNode::UPDATE)
    {
    lastResult = astroModelingLogic->UpdateModelFromTable(parametersNode, internalTableNode);
    }
  else
    {
    lastResult = astroModelingLogic->OperateModel(parametersNode, internalTableNode);
    }
  const bool cancelled = parametersNode->GetStatus() == -1 || abortRequested();
  if (cancelled)
    {
    qInfo().noquote() << "3DBarolo:" << OperationName(parametersNode) << "- cancelled.";
    }
  else if (!lastResult)
    {
    qWarning().noquote() << "3DBarolo:" << OperationName(parametersNode)
                         << "- failed (see the 3DBarolo Output section of the Modeling module).";
    }
  else
    {
    qInfo().noquote() << "3DBarolo:" << OperationName(parametersNode) << "- finished.";
    }

  // Set _working to false, meaning the process can't be aborted anymore.
  mutex.lock();
  _working = false;
  mutex.unlock();

  emit finished();
}
