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

#ifndef __qSlicerAstroBackgroundRunner_h
#define __qSlicerAstroBackgroundRunner_h

// Qt includes
#include <QObject>
#include <QPointer>
#include <QString>

// STD includes
#include <functional>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

#include <QProgressDialog>
#include "qSlicerAstroRunButton.h"

class QThread;
class QTimer;
class QWidget;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// Runs long computations of SlicerAstro in a worker thread while the GUI
/// stays responsive. run() returns when the work is done:
/// - the progress is shown in the active run button of a panel
///   (qSlicerAstroRunButton::activeButton(), with its Cancel button) or
///   otherwise in a progress dialog with a Cancel button;
/// - user input goes only to that Cancel button: mouse, keyboard, wheel,
///   shortcuts, context menus and drag and drop elsewhere are blocked, and
///   windows are not closed (the main window: the modules would be unloaded
///   under the worker thread), so the data used by the work stay in the
///   scene until it ends.
/// The work must not modify the scene (no MRML events from a worker thread).
/// Timers and posted events still run during the work: they must not start
/// other computations or remove data (vtkSlicerAstroVolumeLogic::
/// IsRunningInBackground, isRunning()).
/// The logics of the modules use it through
/// vtkSlicerAstroVolumeLogic::RunInBackground (the AstroVolume module sets
/// run() as the background runner of its logic).
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroBackgroundRunner : public QObject
{
  Q_OBJECT
public:
  typedef QObject Superclass;
  explicit qSlicerAstroBackgroundRunner(QObject* parent = nullptr);
  ~qSlicerAstroBackgroundRunner() override;

  /// Run work in a worker thread and return when it is done. label
  /// describes the work (e.g. "Smoothing the data..."), progress returns its
  /// progress (0-100) and cancel requests to stop it. parent: the window of
  /// the progress dialog.
  static void run(QWidget* parent, const QString& label, const std::function<void()>& work,
                  const std::function<int()>& progress, const std::function<void()>& cancel);

  /// True while run() runs a computation (also nested) or jobs started with
  /// start() run.
  static bool isRunning();

  /// Start work in a worker thread and return at once: the user interface is
  /// NOT blocked (the user keeps working while the work runs). The progress
  /// is shown in the active run button of a panel (with its Cancel button)
  /// or, without one, in a progress dialog that blocks nothing. finished
  /// runs in the GUI thread after the work (also when cancelled): it gives
  /// the results to the scene. The work must follow the rules of
  /// vtkSlicerAstroVolumeLogic::RunInBackground (no scene access, data held
  /// by the job: the data may be removed meanwhile).
  static void start(QWidget* parent, const QString& label, const std::function<void()>& work,
                    const std::function<int()>& progress, const std::function<void()>& cancel,
                    const std::function<void()>& finished);

  /// Jobs started with start() still running.
  static int runningJobs();

  /// Process the events until the jobs started with start() are finished
  /// (their results given to the scene), at most timeoutMs. Returns false on
  /// timeout. For scripts and tests (Python: an instance,
  /// slicer.qSlicerAstroBackgroundRunner().waitForJobs()).
  Q_INVOKABLE bool waitForJobs(int timeoutMs = 600000) const;

  /// Cancel all the running jobs, wait for their threads and run their
  /// finished functions (e.g. before the scene is closed or the application
  /// quits).
  static void cancelAndWaitAll();

protected slots:
  /// Progress of a job started with start(); its end
  void poll();

private:
  Q_DISABLE_COPY(qSlicerAstroBackgroundRunner);
  /// End of a job: user interface restored, finished called, job deleted
  void finish();

  QThread* Thread{nullptr};
  QTimer* Timer{nullptr};
  QPointer<qSlicerAstroRunButton> Button;
  QPointer<QProgressDialog> Dialog;
  std::function<int()> Progress;
  std::function<void()> Cancel;
  std::function<void()> Finished;
  QMetaObject::Connection ButtonCancel;
  bool Finishing{false};
};

#endif
