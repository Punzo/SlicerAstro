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

#ifndef __qSlicerAstroBeam3DButton_h
#define __qSlicerAstroBeam3DButton_h

// Qt includes
#include <QToolButton>

// CTK includes
#include <ctkVTKObject.h>

// VTK includes
#include <vtkWeakPointer.h>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class vtkMRMLAbstractViewNode;
class vtkMRMLNode;

/// \brief Toggle of the 3D resolution element of a 3D view: the ellipsoid of
/// the beam (FWHM) times the spectral resolution (FWHM) of the cube rendered
/// in the view (vtkMRMLAstroBeam3DDisplayableManager).
///
/// The one implementation of this control: SlicerAstro adds it to the bar of
/// the 3D view controllers of Slicer, and the AstroWorkbench to its 3D view
/// bars. It sets the attribute SlicerAstro.Beam3D ("on"/"off") of the view
/// node; the tooltip says why nothing is shown when no cube with a beam is
/// rendered in the view.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroBeam3DButton
  : public QToolButton
{
  Q_OBJECT
  QVTK_OBJECT
  /// Theme colour token of the icon (default "muted"; the AstroWorkbench
  /// bars use "view-subtle"), kept when the theme changes
  Q_PROPERTY(QString iconColorToken READ iconColorToken WRITE setIconColorToken)

public:
  explicit qSlicerAstroBeam3DButton(QWidget* parent = nullptr);
  ~qSlicerAstroBeam3DButton() override;

  Q_INVOKABLE vtkMRMLAbstractViewNode* mrmlViewNode() const;

  QString iconColorToken() const;
  void setIconColorToken(const QString& token);

public slots:
  /// The 3D view node whose resolution element the button shows or hides
  void setMRMLViewNode(vtkMRMLNode* viewNode);

  /// Checked state and tooltip from the view node
  void updateFromMRML();

protected slots:
  void onToggled(bool checked);
  void updateIcon();

protected:
  /// The tooltip is updated when it is shown (the cubes rendered in the view
  /// change without the view node)
  bool event(QEvent* event) override;

private:
  vtkWeakPointer<vtkMRMLAbstractViewNode> ViewNode;
  QString IconColorToken{"muted"};
};

#endif
