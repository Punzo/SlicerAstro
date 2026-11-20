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

==============================================================================*/

#ifndef __qSlicerAstroDatasetSelector_h
#define __qSlicerAstroDatasetSelector_h

// Qt includes
#include <QAbstractButton>
#include <QStringList>

// CTK includes
#include <ctkVTKObject.h>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class vtkMRMLNode;
class vtkMRMLScene;
class vtkObject;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Selector of the input data of a panel: preview (moment-0 map of a cube),
/// name and a summary line (dimensions and unit, or what the panel needs,
/// e.g. "σ 0.62 mJy/beam · 2.58 km/s"). Clicking opens the list of the data
/// in the scene with their previews.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroDatasetSelector
  : public QAbstractButton
{
  Q_OBJECT
  QVTK_OBJECT
  Q_PROPERTY(QStringList nodeTypes READ nodeTypes WRITE setNodeTypes)
  Q_PROPERTY(int requiredDimensions READ requiredDimensions WRITE setRequiredDimensions)
  Q_PROPERTY(QString subtitle READ subtitle WRITE setSubtitle)
  Q_PROPERTY(QString noneText READ noneText WRITE setNoneText)

public:
  explicit qSlicerAstroDatasetSelector(QWidget* parent = nullptr);
  ~qSlicerAstroDatasetSelector() override;

  vtkMRMLScene* mrmlScene() const;

  QStringList nodeTypes() const;
  void setNodeTypes(const QStringList& types);
  /// Only data with this number of dimensions (SlicerAstro.NAXIS), 0: any
  int requiredDimensions() const;
  void setRequiredDimensions(int dimensions);
  /// Summary line; empty: dimensions and unit of the data
  QString subtitle() const;
  void setSubtitle(const QString& subtitle);
  QString noneText() const;
  void setNoneText(const QString& text);
  /// Only these nodes (IDs) can be selected (e.g. a dataset and its
  /// products); empty: any node of the types
  Q_INVOKABLE QStringList nodeIDFilter() const;
  Q_INVOKABLE void setNodeIDFilter(const QStringList& nodeIDs);

  Q_INVOKABLE vtkMRMLNode* currentNode() const;
  Q_INVOKABLE QString currentNodeID() const;
  /// Nodes that can be selected
  Q_INVOKABLE QList<vtkMRMLNode*> nodes() const;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

public slots:
  void setMRMLScene(vtkMRMLScene* scene);
  void setCurrentNode(vtkMRMLNode* node);
  void setCurrentNodeID(const QString& nodeID);
  /// Opens the list of the data
  void showMenu();

signals:
  void currentNodeChanged(vtkMRMLNode* node);

protected slots:
  void onNodeRemoved(vtkObject* scene, vtkObject* node);
  void onSceneClosed();
  void onCurrentNodeModified();

protected:
  void paintEvent(QPaintEvent* event) override;
  bool event(QEvent* event) override;
  bool accepts(vtkMRMLNode* node) const;
  QString defaultSubtitle(vtkMRMLNode* node) const;

  vtkMRMLScene* Scene{nullptr};
  vtkMRMLNode* CurrentNode{nullptr};
  QStringList NodeTypes;
  int RequiredDimensions{0};
  QString Subtitle;
  QString NoneText;
  QStringList NodeIDFilter;
};

#endif
