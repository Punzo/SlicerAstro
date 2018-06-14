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

#ifndef __qSlicerAstroModelingModuleWorker_h
#define __qSlicerAstroModelingModuleWorker_h

#include <QObject>
#include <QMutex>

#include "qSlicerAstroModelingModuleExport.h"

#include "vtkSmartPointer.h"

class vtkMRMLAstroModelingParametersNode;
class vtkMRMLTableNode;
class vtkSlicerAstroModelingLogic;

/// \ingroup SlicerAstro_QtModules_AstroModeling
class Q_SLICERASTRO_QTMODULES_ASTROMODELING_EXPORT qSlicerAstroModelingModuleWorker :
  public QObject
{
  Q_OBJECT

public:
  qSlicerAstroModelingModuleWorker(QObject *parent = 0);
  virtual ~qSlicerAstroModelingModuleWorker();

  /// External thread has requested to start computations
  void requestWork();

  /// External thread has requested to stop computations
  void abort();

  /// Set AstroModeling module logic
  void SetAstroModelingLogic(vtkSlicerAstroModelingLogic* logic);

  /// Set AstroModeling module parameters node
  void SetAstroModelingParametersNode(vtkMRMLAstroModelingParametersNode* pnode);

  /// Set AstroModeling module table node
  void SetTableNode(vtkMRMLTableNode* tnode);

private:
  bool _abort;
  bool _working;
  QMutex mutex;
  vtkSmartPointer<vtkMRMLAstroModelingParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLTableNode> internalTableNode;
  vtkSlicerAstroModelingLogic* astroModelingLogic;

signals:
  void workRequested();
  void finished();

public slots:
  /// Run the computation
  void doWork();
};

#endif
