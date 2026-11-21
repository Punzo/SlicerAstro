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

#ifndef __qSlicerAstroColorPicker_h
#define __qSlicerAstroColorPicker_h

// Qt includes
#include <QColor>
#include <QDialog>
#include <QList>
#include <QScopedPointer>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qSlicerAstroColorPickerPrivate;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Colour picker of SlicerAstro: the colours of the astronomical palette
/// (the colours of the types of the SlicerAstro terminology), the hex code
/// and, with "More colours", Qt's full colour dialog (themed).
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroColorPicker : public QDialog
{
  Q_OBJECT
  Q_PROPERTY(QColor color READ color WRITE setColor)

public:
  explicit qSlicerAstroColorPicker(QWidget* parent = nullptr);
  ~qSlicerAstroColorPicker() override;

  QColor color() const;
  /// Colours of the swatches (default: the SlicerAstro palette)
  Q_INVOKABLE void setPalette(const QList<QColor>& colors);
  Q_INVOKABLE QList<QColor> paletteColors() const;

  /// Asks for a colour; ok (if given) tells whether the user accepted
  static QColor getColor(const QColor& initial, QWidget* parent, const QString& title, bool* ok = nullptr);
  /// The SlicerAstro palette (colour-blind safe colours of the terminology)
  static QList<QColor> defaultPalette();
  /// A rounded swatch icon of a colour
  static QIcon swatchIcon(const QColor& color, int size = 16);

public slots:
  void setColor(const QColor& color);
  /// Qt's full colour dialog, themed
  void showMoreColors();

protected slots:
  void onHexEdited();

protected:
  QScopedPointer<qSlicerAstroColorPickerPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroColorPicker);
  Q_DISABLE_COPY(qSlicerAstroColorPicker);
};

#endif
