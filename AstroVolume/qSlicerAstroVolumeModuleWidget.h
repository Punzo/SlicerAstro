/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Jean-Christophe Fillion-Robin, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

#ifndef __qSlicerAstroVolumeModuleWidget_h
#define __qSlicerAstroVolumeModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroVolumeModuleExport.h"

class qSlicerAstroVolumeModuleWidgetPrivate;
class vtkMRMLNode;

/// \ingroup Slicer_QtModules_AstroVolume
class Q_SLICER_QTMODULES_ASTROVOLUME_EXPORT qSlicerAstroVolumeModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroVolumeModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroVolumeModuleWidget();

protected:
  virtual void setup();

protected:
  QScopedPointer<qSlicerAstroVolumeModuleWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroVolumeModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroVolumeModuleWidget);
};

#endif
