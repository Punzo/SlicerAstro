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

#ifndef __qSlicerAstroToggleCard_h
#define __qSlicerAstroToggleCard_h

// Qt includes
#include <QAbstractButton>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Checkable card with a short title and a caption (e.g. the moment maps
/// "0 flux", "1 velocity", "2 dispersion"). Checked: amber border.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroToggleCard
  : public QAbstractButton
{
  Q_OBJECT
  Q_PROPERTY(QString caption READ caption WRITE setCaption)

public:
  explicit qSlicerAstroToggleCard(QWidget* parent = nullptr);
  qSlicerAstroToggleCard(const QString& title, const QString& caption, QWidget* parent = nullptr);
  ~qSlicerAstroToggleCard() override;

  QString caption() const;
  void setCaption(const QString& caption);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;
  bool event(QEvent* event) override;

  QString Caption;
};

#endif
