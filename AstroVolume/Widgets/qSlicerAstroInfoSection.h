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

#ifndef __qSlicerAstroInfoSection_h
#define __qSlicerAstroInfoSection_h

// Qt includes
#include <QScopedPointer>
#include <QWidget>

// CTK includes
#include <ctkVTKObject.h>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qSlicerAstroInfoSectionPrivate;
class vtkMRMLNode;
class vtkObject;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Information about an astronomical volume, a reusable section
/// (AstroVolume panel, Workbench): facts (object, telescope, dimensions,
/// unit, beam, channel width, noise, spectral axis, file), coordinate
/// conventions of the readouts (right ascension in hours or degrees, radio
/// or optical velocities), the beam (estimate from the noise, edit, apply)
/// and the FITS header (searchable).
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroInfoSection : public QWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  explicit qSlicerAstroInfoSection(QWidget* parent = nullptr);
  ~qSlicerAstroInfoSection() override;

  /// Text of a fact ("Object", "Telescope", "Dimensions", "Unit", "Beam",
  /// "Channel width", "Noise (σ)", "Spectral axis", "File")
  Q_INVOKABLE QString fact(const QString& name) const;
  /// Text of the beam state (estimated, applied, missing)
  Q_INVOKABLE QString beamStatus() const;
  /// Number of FITS keywords shown in the header table
  Q_INVOKABLE int keywordRowCount() const;

public slots:
  /// The volume described (nullptr: none)
  void setVolumeNode(vtkMRMLNode* node);
  /// Estimate the beam from the noise into the fields (not applied)
  bool estimateBeam();
  /// Apply the beam of the fields (arc seconds, degrees) to the data
  bool applyBeam();
  /// Show only the keywords (or values) containing the text
  void setKeywordFilter(const QString& text);
  /// Show or hide the FITS header table
  void setHeaderVisible(bool visible);
  /// Updates the widgets from the volume
  void updateFromVolume();

protected slots:
  void onRightAscensionFormatChanged(int index);
  void onVelocityDefinitionChanged(int index);
  void onBeamEdited();

protected:
  void fillKeywords();

  QScopedPointer<qSlicerAstroInfoSectionPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroInfoSection);
  Q_DISABLE_COPY(qSlicerAstroInfoSection);
};

#endif
