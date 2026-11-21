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
#include <QAbstractItemModel>
#include <QFrame>
#include <QHeaderView>
#include <QTableView>
#include <QTimer>
#include <QToolButton>

// MRML includes
#include <vtkMRMLSegmentationNode.h>

// Segmentations includes
#include <qMRMLSegmentsModel.h>
#include <qMRMLSegmentsTableView.h>

#include "qSlicerAstroSegmentDialog.h"
#include "qSlicerAstroSegmentsTableStyler.h"
#include "qSlicerAstroTheme.h"

//-----------------------------------------------------------------------------
qSlicerAstroSegmentsTableStyler::qSlicerAstroSegmentsTableStyler(qMRMLSegmentsTableView* table)
  : QObject(table)
  , Table(table)
{
  this->setObjectName("AstroSegmentsTableStyler");
  if (!table)
    {
    return;
    }
  // a plain list: visibility, colour, name
  table->setHeaderVisible(false);
  table->setOpacityColumnVisible(false);
  table->setStatusColumnVisible(false);
  table->setLayerColumnVisible(false);
  QTableView* tableView = table->tableWidget();
  tableView->setProperty("astroRole", "segments");
  tableView->setShowGrid(false);
  tableView->setFrameShape(QFrame::NoFrame);
  tableView->verticalHeader()->setDefaultSectionSize(30);
  QAbstractItemModel* model = tableView->model();
  for (const char* modelSignal : { SIGNAL(rowsInserted(QModelIndex,int,int)), SIGNAL(modelReset()),
                                   SIGNAL(layoutChanged()),
                                   SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)) })
    {
    QObject::connect(model, modelSignal, this, SLOT(scheduleUpdate()));
    }
  QObject::connect(qSlicerAstroTheme::instance(), SIGNAL(themeChanged()), this, SLOT(scheduleUpdate()));
  this->scheduleUpdate();
}

//-----------------------------------------------------------------------------
qSlicerAstroSegmentsTableStyler::~qSlicerAstroSegmentsTableStyler() = default;

//-----------------------------------------------------------------------------
qSlicerAstroSegmentsTableStyler* qSlicerAstroSegmentsTableStyler::styleTable(qMRMLSegmentsTableView* table)
{
  if (!table)
    {
    return nullptr;
    }
  qSlicerAstroSegmentsTableStyler* styler =
    table->findChild<qSlicerAstroSegmentsTableStyler*>("AstroSegmentsTableStyler", Qt::FindDirectChildrenOnly);
  return styler ? styler : new qSlicerAstroSegmentsTableStyler(table);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSegmentsTableStyler::scheduleUpdate()
{
  // after the table has set Slicer's icons on its buttons
  QTimer::singleShot(0, this, SLOT(updateRows()));
}

//-----------------------------------------------------------------------------
void qSlicerAstroSegmentsTableStyler::updateRows()
{
  QTableView* tableView = this->Table ? this->Table->tableWidget() : nullptr;
  QAbstractItemModel* model = tableView ? tableView->model() : nullptr;
  if (!model || !this->Table->model())
    {
    return;
    }
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  const int visibilityColumn = this->Table->model()->visibilityColumn();
  const int colorColumn = this->Table->model()->colorColumn();
  for (int row = 0; row < model->rowCount(); row++)
    {
    // the eye of the theme
    const QModelIndex visibilityIndex = model->index(row, visibilityColumn);
    QToolButton* eye = qobject_cast<QToolButton*>(tableView->indexWidget(visibilityIndex));
    if (eye)
      {
      const bool visible = visibilityIndex.data(qMRMLSegmentsModel::VisibilityRole).toInt() != 0;
      eye->setIcon(theme->icon(visible ? "eye" : "eyeOff", visible ? "muted" : "subtle"));
      eye->setIconSize(QSize(16, 16));
      eye->setToolTip(visible ? tr("Hide the segment") : tr("Show the segment"));
      }
    // the colour swatch opens the segment dialog of SlicerAstro (name,
    // type, colour) instead of Slicer's terminology dialog
    const QModelIndex colorIndex = model->index(row, colorColumn);
    QToolButton* swatch = qobject_cast<QToolButton*>(tableView->indexWidget(colorIndex));
    if (swatch && !swatch->property("astroSegmentDialog").toBool())
      {
      swatch->setProperty("astroSegmentDialog", true);
      QObject::disconnect(swatch, &QToolButton::clicked, nullptr, nullptr);
      const QString segmentID = colorIndex.data(qMRMLSegmentsModel::SegmentIDRole).toString();
      swatch->setToolTip(tr("Name, type and colour of the segment"));
      QObject::connect(swatch, &QToolButton::clicked, this, [this, segmentID]() { this->editSegment(segmentID); });
      }
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroSegmentsTableStyler::editSegment(const QString& segmentID)
{
  if (!this->Table)
    {
    return false;
    }
  const bool changed = qSlicerAstroSegmentDialog::editSegment(this->Table->segmentationNode(), segmentID,
                                                              this->Table);
  if (changed)
    {
    emit this->segmentEdited(segmentID);
    }
  return changed;
}
