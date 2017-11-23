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
#include <QThread>
#include <QTimer>

// AstroModeling includes
#include <qSlicerAstroModelingModuleWorker.h>
#include <vtkMRMLAstroModelingParametersNode.h>
#include <vtkMRMLTableNode.h>
#include <vtkSlicerAstroModelingLogic.h>

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWorker::qSlicerAstroModelingModuleWorker(QObject *parent) :
    QObject(parent)
{
  _working =false;
  _abort = false;
  astroModelingLogic = NULL;
}

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWorker::~qSlicerAstroModelingModuleWorker()
{
  astroModelingLogic = NULL;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWorker::requestWork()
{
  mutex.lock();
  _working = true;
  _abort = false;
  qDebug()<<"Request qSlicerAstroModelingModuleWorker start in Thread "<<thread()->currentThreadId();
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
    qDebug()<<"Request qSlicerAstroModelingModuleWorker aborting in Thread "<<thread()->currentThreadId();
    }
  mutex.unlock();
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
  qDebug()<<"Starting qSlicerAstroModelingModuleWorker process in Thread "<<thread()->currentThreadId();

  // Checks if the process should be aborted
  mutex.lock();
  bool abort = _abort;
  mutex.unlock();

  if (abort)
    {
    qDebug()<<"Aborting qSlicerAstroModelingModuleWorker process in Thread "<<thread()->currentThreadId();
    parametersNode->SetStatus(0);
    return;
    }

  if (!astroModelingLogic->OperateModel(parametersNode, internalTableNode))
    {
    qDebug()<<"Aborting qSlicerAstroModelingModuleWorker process in Thread "<<thread()->currentThreadId();
    }

  // Set _working to false, meaning the process can't be aborted anymore.
  mutex.lock();
  _working = false;
  mutex.unlock();

  qDebug()<<"qSlicerAstroModelingModuleWorker process finished in Thread "<<thread()->currentThreadId();

  emit finished();
}
