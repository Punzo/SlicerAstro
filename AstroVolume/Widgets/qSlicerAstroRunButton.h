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

#ifndef __qSlicerAstroRunButton_h
#define __qSlicerAstroRunButton_h

// Qt includes
#include <QElapsedTimer>
#include <QWidget>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class QPushButton;
class QStackedWidget;
class qSlicerAstroRunProgress;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// The one action of a panel that runs something (amber). While it runs,
/// the button shows the progress inline, with the time left and a Cancel
/// button: no progress dialog.
///
/// The background runner of SlicerAstro (vtkSlicerAstroVolumeLogic::
/// RunInBackground) reports to the run button that was clicked, if any
/// (activeButton()); otherwise it shows a progress dialog.
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroRunButton
  : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(QString text READ text WRITE setText)
  Q_PROPERTY(bool running READ isRunning)

public:
  explicit qSlicerAstroRunButton(QWidget* parent = nullptr);
  ~qSlicerAstroRunButton() override;

  QString text() const;
  void setText(const QString& text);

  /// The amber button (e.g. to name it or to click it in tests)
  Q_INVOKABLE QPushButton* pushButton() const;
  Q_INVOKABLE QPushButton* cancelButton() const;

  bool isRunning() const;

  /// The run button whose clicked() signal is being handled (the work
  /// started by that click reports its progress to it), or nullptr
  static qSlicerAstroRunButton* activeButton();

public slots:
  void setRunning(bool running);
  /// Progress in percent (0-100); a negative value: unknown
  void setProgress(int percent);
  /// Text shown during the work (e.g. "Computing the moment maps")
  void setProgressText(const QString& text);

signals:
  void clicked();
  void cancelRequested();

protected slots:
  void onClicked();
  void onCancel();

protected:
  QPushButton* PushButton;
  QPushButton* CancelButton;
  QStackedWidget* Stack;
  qSlicerAstroRunProgress* Progress;
  QElapsedTimer Elapsed;
};

#endif
