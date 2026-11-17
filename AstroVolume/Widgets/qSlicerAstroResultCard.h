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

#ifndef __qSlicerAstroResultCard_h
#define __qSlicerAstroResultCard_h

// Qt includes
#include <QFrame>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class QLabel;
class QPushButton;
class vtkMRMLVolumeNode;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// A result of a panel (e.g. a moment map): preview, name, unit and a Show
/// button that puts it in the views.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroResultCard
  : public QFrame
{
  Q_OBJECT
  Q_PROPERTY(QString title READ title WRITE setTitle)
  Q_PROPERTY(QString subtitle READ subtitle WRITE setSubtitle)

public:
  explicit qSlicerAstroResultCard(QWidget* parent = nullptr);
  ~qSlicerAstroResultCard() override;

  QString title() const;
  void setTitle(const QString& title);
  QString subtitle() const;
  void setSubtitle(const QString& subtitle);

  /// Preview, name and unit of a volume
  Q_INVOKABLE void setVolume(vtkMRMLVolumeNode* volume);
  Q_INVOKABLE QPushButton* showButton() const;

signals:
  void showRequested();

protected:
  QLabel* ThumbnailLabel;
  QLabel* TitleLabel;
  QLabel* SubtitleLabel;
  QPushButton* ShowButton;
};

#endif
