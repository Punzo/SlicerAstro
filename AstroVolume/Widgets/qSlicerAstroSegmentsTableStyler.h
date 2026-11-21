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

#ifndef __qSlicerAstroSegmentsTableStyler_h
#define __qSlicerAstroSegmentsTableStyler_h

// Qt includes
#include <QObject>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qMRMLSegmentsTableView;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// The look of a segment list of SlicerAstro (Masks section, module panels
/// with a mask input) on Slicer's qMRMLSegmentsTableView: a plain list
/// (visibility, colour, name: no header, opacity or status columns), the
/// eye icons of the theme, and the colour swatch opening the segment dialog
/// of SlicerAstro (name, type, colour). Owned by the table.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroSegmentsTableStyler : public QObject
{
  Q_OBJECT

public:
  explicit qSlicerAstroSegmentsTableStyler(qMRMLSegmentsTableView* table);
  ~qSlicerAstroSegmentsTableStyler() override;

  /// Styles a table (once): returns its styler
  Q_INVOKABLE static qSlicerAstroSegmentsTableStyler* styleTable(qMRMLSegmentsTableView* table);

signals:
  /// A segment was changed with the segment dialog
  void segmentEdited(const QString& segmentID);

public slots:
  /// The icons of the rows after the table set Slicer's ones
  void scheduleUpdate();
  void updateRows();
  /// The segment dialog of SlicerAstro for a segment of the table
  bool editSegment(const QString& segmentID);

protected:
  qMRMLSegmentsTableView* Table{nullptr};
};

#endif
