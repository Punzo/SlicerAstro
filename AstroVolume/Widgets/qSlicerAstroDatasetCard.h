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

#ifndef __qSlicerAstroDatasetCard_h
#define __qSlicerAstroDatasetCard_h

// Qt includes
#include <QFrame>
#include <QStringList>

// CTK includes
#include <ctkVTKObject.h>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class QLabel;
class QToolButton;
class QVBoxLayout;
class qSlicerAstroDatasetCardHeader;
class vtkMRMLVolumeNode;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Card of a dataset in the Datasets drawer of the Workbench: preview
/// computed from the data (moment-0 map of a cube), name, dimensions and
/// unit, integrated spectrum, chips (beam, channel width, line) and, for the
/// active dataset, its derived products (lineage). The kind of the dataset
/// (cube, image, mask, catalogue...) selects how it is drawn.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroDatasetCard
  : public QFrame
{
  Q_OBJECT
  QVTK_OBJECT
  Q_PROPERTY(bool active READ isActive WRITE setActive)
  Q_PROPERTY(QString kind READ kind WRITE setKind)
  Q_PROPERTY(QStringList chips READ chips WRITE setChips)

public:
  explicit qSlicerAstroDatasetCard(QWidget* parent = nullptr);
  ~qSlicerAstroDatasetCard() override;

  Q_INVOKABLE vtkMRMLVolumeNode* volume() const;
  Q_INVOKABLE QString volumeID() const;

  bool isActive() const;
  /// "cube", "image", "mask", "catalogue" (later: "particles", "grid")
  QString kind() const;
  void setKind(const QString& kind);
  QStringList chips() const;
  void setChips(const QStringList& chips);

  /// Derived products listed under the active card (lineage)
  Q_INVOKABLE void clearProducts();
  Q_INVOKABLE void addProduct(const QString& id, const QString& name, const QString& detail,
                              const QString& iconName);
  Q_INVOKABLE int productCount() const;

public slots:
  void setVolume(vtkMRMLVolumeNode* volume);
  void setActive(bool active);

signals:
  void clicked();
  void productClicked(const QString& id);

protected slots:
  void onVolumeModified();

protected:
  void mouseReleaseEvent(QMouseEvent* event) override;

  qSlicerAstroDatasetCardHeader* Header;
  QWidget* ChipsRow;
  QWidget* Products;
  QVBoxLayout* ProductsLayout;
  vtkMRMLVolumeNode* Volume{nullptr};
  QString Kind;
  QStringList Chips;
};

#endif
