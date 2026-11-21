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

#ifndef __qSlicerAstroSegmentDialog_h
#define __qSlicerAstroSegmentDialog_h

// Qt includes
#include <QColor>
#include <QDialog>
#include <QScopedPointer>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qSlicerAstroSegmentDialogPrivate;
class vtkMRMLNode;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// A segment of a mask: its name, its astronomical type (the types of the
/// SlicerAstro terminology, by category, with their colours) and its colour
/// (qSlicerAstroColorPicker). Used instead of Slicer's terminology dialog by
/// the segment lists of SlicerAstro.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroSegmentDialog : public QDialog
{
  Q_OBJECT

public:
  explicit qSlicerAstroSegmentDialog(QWidget* parent = nullptr);
  ~qSlicerAstroSegmentDialog() override;

  /// The segment shown (segmentation node and segment ID)
  Q_INVOKABLE bool setSegment(vtkMRMLNode* segmentationNode, const QString& segmentID);
  Q_INVOKABLE QString name() const;
  Q_INVOKABLE QColor color() const;
  /// Terminology entry of the chosen type (empty: none)
  Q_INVOKABLE QString terminology() const;
  /// Index of the chosen type in the colour table of the terminology (-1: none)
  Q_INVOKABLE int typeIndex() const;
  Q_INVOKABLE void setTypeIndex(int index);
  Q_INVOKABLE void setName(const QString& name);
  /// Writes the name, the type and the colour to the segment
  Q_INVOKABLE bool apply();

  /// Shows the dialog for a segment; true if the segment was changed
  static bool editSegment(vtkMRMLNode* segmentationNode, const QString& segmentID, QWidget* parent);

public slots:
  void setColor(const QColor& color);
  void chooseColor();

protected slots:
  void onTypeChanged();

protected:
  QScopedPointer<qSlicerAstroSegmentDialogPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroSegmentDialog);
  Q_DISABLE_COPY(qSlicerAstroSegmentDialog);
};

#endif
