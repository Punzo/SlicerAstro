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

#ifndef __qSlicerAstroDisplaySection_h
#define __qSlicerAstroDisplaySection_h

// Qt includes
#include <QScopedPointer>
#include <QWidget>

// CTK includes
#include <ctkVTKObject.h>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qSlicerAstroDisplaySectionPrivate;
class vtkMRMLNode;
class vtkObject;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// 2D display of an astronomical volume, a reusable section (AstroVolume
/// panel, Workbench): colour table and its options (reverse, invert, log),
/// slice interpolation and fitting, histogram of the data with the window of
/// the 2D colours and the noise of the 3D colour functions (both draggable),
/// minimum / maximum / noise fields, windows with a percentage of the data.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroDisplaySection : public QWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  explicit qSlicerAstroDisplaySection(QWidget* parent = nullptr);
  ~qSlicerAstroDisplaySection() override;

  /// Percentages of the window buttons (100: minimum to maximum)
  Q_INVOKABLE QList<double> percentages() const;

public slots:
  /// The astronomical volume shown (nullptr: none)
  void setVolumeNode(vtkMRMLNode* node);
  /// Window of the 2D colours (data units)
  void setWindow(double minimum, double maximum);
  /// Noise sigma of the volume (data units): the 3D colour functions follow
  void setNoise(double noise);
  /// Window with the percentage of index in percentages()
  void applyPercentage(int index);
  /// Updates the widgets from the volume and its display node
  void updateFromVolume();
  /// The lines of the histogram dragged: the window (2D colour function)
  /// and the noise (3D colour function) applied while dragging
  void onHistogramWindowDragged(double minimum, double maximum);
  void onHistogramNoiseDragged(double noise);

protected slots:
  void onColorTableActivated(int index);
  void onOptionToggled(bool on);
  void onInterpolateToggled(bool on);
  void onFitSlicesToggled(bool on);
  void onFieldsEdited();
  void onWindowChanging(double minimum, double maximum);
  void onNoiseChanging(double noise);
  void applyPendingNoise();
  void onViewRangeChanged(double minimum, double maximum);
  void onVolumeModified(vtkObject* caller);

protected:
  QScopedPointer<qSlicerAstroDisplaySectionPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroDisplaySection);
  Q_DISABLE_COPY(qSlicerAstroDisplaySection);
};

#endif
