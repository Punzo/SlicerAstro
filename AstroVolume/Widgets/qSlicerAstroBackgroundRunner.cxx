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
  return RunDepth > 0;
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
