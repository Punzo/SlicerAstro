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

#ifndef __qSlicerAstroPanelHeader_h
#define __qSlicerAstroPanelHeader_h

// Qt includes
#include <QFrame>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class QHBoxLayout;
class QLabel;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Header of a panel: module icon (with its amber stroke) and title; extra
/// widgets (e.g. a menu button) can be added on the right.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroPanelHeader
  : public QFrame
{
  Q_OBJECT
  Q_PROPERTY(QString title READ title WRITE setTitle)
  Q_PROPERTY(QString iconName READ iconName WRITE setIconName)

public:
  explicit qSlicerAstroPanelHeader(QWidget* parent = nullptr);
  ~qSlicerAstroPanelHeader() override;

  QString title() const;
  void setTitle(const QString& title);
  /// Icon of the SlicerAstro set, e.g. "module-moment-maps"
  QString iconName() const;
  void setIconName(const QString& name);

  Q_INVOKABLE void addWidget(QWidget* widget);

protected slots:
  void updateIcon();

protected:
  QLabel* IconLabel;
  QLabel* TitleLabel;
  QHBoxLayout* Layout;
  QString IconName;
};

#endif
