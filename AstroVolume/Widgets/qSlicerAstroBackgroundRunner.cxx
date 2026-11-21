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


// Qt includes
#include <QApplication>
#include <QEvent>
#include <QEventLoop>
#include <QProgressDialog>
#include <QPushButton>
#include <QThread>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QList>
#include <QTimer>

#include "qSlicerAstroBackgroundRunner.h"
#include "qSlicerAstroRunButton.h"

//---------------------------------------------------------------------------
namespace
{
// While a background computation runs: user input (mouse, keyboard) goes only
// to the progress dialog (no other operation can start, e.g. before the
// dialog is shown)
class BackgroundInputBlocker : public QObject
{
public:
  explicit BackgroundInputBlocker(QWidget* allowed)
    : Allowed(allowed)
    {
    }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override
    {
    switch (event->type())
      {
      case QEvent::MouseButtonPress:
      case QEvent::MouseButtonRelease:
      case QEvent::MouseButtonDblClick:
      case QEvent::KeyPress:
      case QEvent::KeyRelease:
      case QEvent::Wheel:
      case QEvent::Shortcut:
      case QEvent::ShortcutOverride:
      case QEvent::ContextMenu:
      // data dropped on the application (files, datasets dragged onto views)
      case QEvent::DragEnter:
      case QEvent::DragMove:
      case QEvent::Drop:
        {
        QWidget* widget = qobject_cast<QWidget*>(watched);
        return !(widget && this->Allowed && (widget == this->Allowed || this->Allowed->isAncestorOf(widget)));
        }
      // windows not closed while the work uses the data (e.g. the main window:
      // the modules would be unloaded under the worker thread)
      case QEvent::Close:
        {
        QWidget* widget = qobject_cast<QWidget*>(watched);
        if (widget && widget->isWindow() && !(this->Allowed && widget == this->Allowed->window()))
          {
          event->ignore();
          return true;
          }
        return false;
        }
      default:
        return false;
      }
    }

private:
  QWidget* Allowed;
};

//---------------------------------------------------------------------------
// Number of computations running (nested runs counted; also when a run
// returns early)
int RunDepth = 0;

// Jobs started with start() and not finished
QList<QPointer<qSlicerAstroBackgroundRunner> >& RunningJobs()
{
  static QList<QPointer<qSlicerAstroBackgroundRunner> > jobs;
  return jobs;
}

class RunDepthGuard
{
public:
  RunDepthGuard()
    {
    RunDepth++;
    }
  ~RunDepthGuard()
    {
    RunDepth--;
    }
};
} // end of anonymous namespace

//---------------------------------------------------------------------------
qSlicerAstroBackgroundRunner::qSlicerAstroBackgroundRunner(QObject* parent)
  : Superclass(parent)
{
}

//---------------------------------------------------------------------------
qSlicerAstroBackgroundRunner::~qSlicerAstroBackgroundRunner() = default;

//---------------------------------------------------------------------------
bool qSlicerAstroBackgroundRunner::isRunning()
{
  return RunDepth > 0 || qSlicerAstroBackgroundRunner::runningJobs() > 0;
}

//---------------------------------------------------------------------------
int qSlicerAstroBackgroundRunner::runningJobs()
{
  int count = 0;
  for (const QPointer<qSlicerAstroBackgroundRunner>& job : RunningJobs())
    {
    count += job ? 1 : 0;
    }
  return count;
}

//---------------------------------------------------------------------------
void qSlicerAstroBackgroundRunner::start(QWidget* parent, const QString& label, const std::function<void()>& work,
                                         const std::function<int()>& progress, const std::function<void()>& cancel,
                                         const std::function<void()>& finished)
{
  qSlicerAstroBackgroundRunner* job = new qSlicerAstroBackgroundRunner(qApp);
  job->Progress = progress;
  job->Cancel = cancel;
  job->Finished = finished;
  job->Thread = QThread::create(work);
  const QString text = label.endsWith("...") ? label.left(label.size() - 3) : label;
  // the progress in the run button of the panel that started the work (also
  // when the button is hidden, e.g. in a collapsed section: shown when it is
  // opened), or in a dialog that blocks nothing
  qSlicerAstroRunButton* runButton = qSlicerAstroRunButton::activeButton();
  if (runButton)
    {
    job->Button = runButton;
    runButton->setProgressText(text);
    runButton->setRunning(true);
    runButton->setProgress(0);
    job->ButtonCancel = QObject::connect(runButton, &qSlicerAstroRunButton::cancelRequested, job,
                                         [job]() { if (job->Cancel) { job->Cancel(); } });
    }
  else
    {
    QProgressDialog* dialog = new QProgressDialog(label, QObject::tr("Cancel"), 0, 100, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);
    dialog->setWindowTitle(QObject::tr("SlicerAstro"));
    dialog->setWindowModality(Qt::NonModal);
    dialog->setMinimumDuration(500);
    dialog->setAutoClose(false);
    dialog->setAutoReset(false);
    dialog->setValue(0);
    QObject::connect(dialog, &QProgressDialog::canceled, job, [job]()
      {
      if (job->Dialog && !job->Finishing)
        {
        job->Dialog->setLabelText(QObject::tr("Cancelling..."));
        }
      if (job->Cancel && !job->Finishing)
        {
        job->Cancel();
        }
      });
    job->Dialog = dialog;
    }
  job->Timer = new QTimer(job);
  job->Timer->setInterval(100);
  QObject::connect(job->Timer, SIGNAL(timeout()), job, SLOT(poll()));
  RunningJobs().append(job);
  job->Thread->start();
  job->Timer->start();
}

//---------------------------------------------------------------------------
void qSlicerAstroBackgroundRunner::poll()
{
  if (this->Finishing)
    {
    return;
    }
  const int value = this->Progress ? this->Progress() : -1;
  if (value >= 0 && value <= 100)
    {
    if (this->Button)
      {
      this->Button->setProgress(value);
      }
    if (this->Dialog && !this->Dialog->wasCanceled())
      {
      this->Dialog->setValue(value);
      }
    }
  if (this->Thread && this->Thread->isFinished())
    {
    this->finish();
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroBackgroundRunner::finish()
{
  if (this->Finishing)
    {
    return;
    }
  this->Finishing = true;
  this->Timer->stop();
  this->Thread->wait();
  delete this->Thread;
  this->Thread = nullptr;
  if (this->Button)
    {
    QObject::disconnect(this->ButtonCancel);
    this->Button->setRunning(false);
    }
  if (this->Dialog)
    {
    this->Dialog->close();
    this->Dialog->deleteLater();
    }
  RunningJobs().removeAll(QPointer<qSlicerAstroBackgroundRunner>(this));
  // results to the scene, in the GUI thread; work started by finished (a
  // next step, e.g. the contours after a reprojection) reports its progress
  // to the same run button (no dialog)
  if (this->Finished)
    {
    if (this->Button)
      {
      this->Button->runAsActive(this->Finished);
      }
    else
      {
      this->Finished();
      }
    }
  this->deleteLater();
}

//---------------------------------------------------------------------------
bool qSlicerAstroBackgroundRunner::waitForJobs(int timeoutMs) const
{
  QElapsedTimer timer;
  timer.start();
  while (qSlicerAstroBackgroundRunner::runningJobs() > 0)
    {
    if (timer.elapsed() > timeoutMs)
      {
      return false;
      }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QThread::msleep(5);
    }
  QCoreApplication::processEvents();
  return true;
}

//---------------------------------------------------------------------------
void qSlicerAstroBackgroundRunner::cancelAndWaitAll()
{
  const QList<QPointer<qSlicerAstroBackgroundRunner> > jobs = RunningJobs();
  for (const QPointer<qSlicerAstroBackgroundRunner>& job : jobs)
    {
    if (job && !job->Finishing && job->Cancel)
      {
      job->Cancel();
      }
    }
  for (const QPointer<qSlicerAstroBackgroundRunner>& job : jobs)
    {
    if (job && !job->Finishing)
      {
      job->finish();
      }
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroBackgroundRunner::run(QWidget* parent, const QString& label, const std::function<void()>& work,
                                       const std::function<int()>& progress, const std::function<void()>& cancel)
{
  RunDepthGuard depthGuard;
  // started by a run button of a panel: the progress is shown in the button
  // (no dialog), only its Cancel button accepts input
  qSlicerAstroRunButton* runButton = qSlicerAstroRunButton::activeButton();
  if (runButton && runButton->isVisible())
    {
    QThread* buttonThread = QThread::create(work);
    runButton->setProgressText(label.endsWith("...") ? label.left(label.size() - 3) : label);
    runButton->setRunning(true);
    runButton->setProgress(0);
    const QMetaObject::Connection buttonCancel =
      QObject::connect(runButton, &qSlicerAstroRunButton::cancelRequested, [cancel]() { cancel(); });
    QEventLoop buttonLoop;
    QTimer buttonTimer;
    buttonTimer.setInterval(100);
    QObject::connect(&buttonTimer, &QTimer::timeout, [runButton, &buttonLoop, buttonThread, progress]()
      {
      const int value = progress();
      if (value >= 0 && value <= 100)
        {
        runButton->setProgress(value);
        }
      if (buttonThread->isFinished())
        {
        buttonLoop.quit();
        }
      });
    BackgroundInputBlocker buttonInputBlocker(runButton->cancelButton());
    qApp->installEventFilter(&buttonInputBlocker);
    buttonThread->start();
    buttonTimer.start();
    buttonLoop.exec();
    qApp->removeEventFilter(&buttonInputBlocker);
    buttonTimer.stop();
    buttonThread->wait();
    delete buttonThread;
    QObject::disconnect(buttonCancel);
    runButton->setRunning(false);
    return;
    }

  QThread* thread = QThread::create(work);
  QProgressDialog dialog(label, QObject::tr("Cancel"), 0, 100, parent);
  dialog.setWindowTitle(QObject::tr("SlicerAstro"));
  dialog.setWindowModality(Qt::WindowModal);
  dialog.setMinimumDuration(500);
  dialog.setAutoClose(false);
  dialog.setAutoReset(false);
  dialog.setValue(0);
  const QMetaObject::Connection cancelConnection =
    QObject::connect(&dialog, &QProgressDialog::canceled, [&dialog, cancel]()
    {
    dialog.setLabelText(QObject::tr("Cancelling..."));
    cancel();
    });
  QEventLoop loop;
  QTimer timer;
  timer.setInterval(100);
  // the end of the work is polled (no lost signal before the loop starts)
  QObject::connect(&timer, &QTimer::timeout, [&dialog, &loop, thread, progress]()
    {
    const int value = progress();
    if (value >= 0 && value <= 100 && !dialog.wasCanceled())
      {
      dialog.setValue(value);
      }
    if (thread->isFinished())
      {
      loop.quit();
      }
    });
  BackgroundInputBlocker inputBlocker(&dialog);
  qApp->installEventFilter(&inputBlocker);
  thread->start();
  timer.start();
  loop.exec();
  qApp->removeEventFilter(&inputBlocker);
  timer.stop();
  thread->wait();
  delete thread;
  // closing a QProgressDialog emits canceled(): not a cancel request
  QObject::disconnect(cancelConnection);
  dialog.close();
}
