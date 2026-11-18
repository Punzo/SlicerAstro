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

#ifndef __qSlicerAstroRenderingSection_h
#define __qSlicerAstroRenderingSection_h

// Qt includes
#include <QScopedPointer>
#include <QStringList>
#include <QWidget>

// CTK includes
#include <ctkVTKObject.h>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qSlicerAstroRenderingSectionPrivate;
class vtkMRMLNode;
class vtkObject;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// 3D rendering of an astronomical cube, a reusable section (AstroVolume
/// panel, Workbench): show in the 3D views, 3D color function (presets
/// scaled to the noise), shift, stretch and opacity of the color function,
/// crop to a box, noise of the voxels in the box, rendering quality. The
/// behaviour is in vtkSlicerAstroVolumeLogic (ApplyPreset,
/// SetColorFunctionShift, ...).
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroRenderingSection : public QWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  explicit qSlicerAstroRenderingSection(QWidget* parent = nullptr);
  ~qSlicerAstroRenderingSection() override;

  /// Names of the 3D color functions (presets), in the order of the list
  Q_INVOKABLE QStringList presetNames() const;
  /// Index of the color function of the volume (-1: none)
  Q_INVOKABLE int currentPreset() const;
  /// Text of the noise (and of the last result)
  Q_INVOKABLE QString status() const;

public slots:
  /// The cube shown (nullptr or not a cube: the section is disabled)
  void setVolumeNode(vtkMRMLNode* node);
  /// Show or hide the cube in the 3D views (the default color function is
  /// applied the first time)
  void setRenderingVisible(bool visible);
  /// Color function presetNames()[index], scaled to the noise of the data
  void applyPreset(int index);
  /// Shift, stretch and opacity shift of the color function
  void setShift(double shift);
  void setStretch(double stretch);
  void setOpacityShift(double shift);
  /// Crop the rendering to the box (shown while cropping)
  void setCropping(bool crop);
  /// Fit the box to the whole cube
  void fitBox();
  /// Noise of the voxels in the box: it becomes the noise of the data
  /// (returns it, 0 on failure)
  double computeNoiseInBox();
  /// Rendering quality: 0 adaptive, 1 normal, 2 maximum
  void setQuality(int quality);
  /// Updates the widgets from the volume and its display nodes
  void updateFromVolume();

protected slots:
  void onNoiseModified();
  void onPresetActivated(int index);

protected:
  QScopedPointer<qSlicerAstroRenderingSectionPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroRenderingSection);
  Q_DISABLE_COPY(qSlicerAstroRenderingSection);
};

#endif
