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

  This file was originally developed by Davide Punzo, Kapteyn Astronomical Institute,
  and was supported through the European Research Consil grant nr. 291531.

==============================================================================*/

/// Qt includes
#include <QFileInfo>

// CTK includes
#include <ctkFlowLayout.h>
#include <ctkUtils.h>

// VTK includes
#include <vtkNew.h>

// AstroVolume includes
#include <qSlicerIOOptions_p.h>
#include <qSlicerAstroVolumeIOOptionsWidget.h>
#include <ui_qSlicerAstroVolumeIOOptionsWidget.h>

/// Slicer includes
#include "qSlicerCoreApplication.h"
#include "vtkMRMLColorLogic.h"
#include "vtkMRMLVolumeArchetypeStorageNode.h"
#include "vtkSlicerApplicationLogic.h"

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroVolume
class qSlicerAstroVolumeIOOptionsWidgetPrivate
  : public qSlicerIOOptionsPrivate
  , public Ui_qSlicerAstroVolumeIOOptionsWidget
{
public:
};

//-----------------------------------------------------------------------------
qSlicerAstroVolumeIOOptionsWidget::qSlicerAstroVolumeIOOptionsWidget(QWidget* parentWidget)
  : qSlicerIOOptionsWidget(new qSlicerAstroVolumeIOOptionsWidgetPrivate, parentWidget)
{
  Q_D(qSlicerAstroVolumeIOOptionsWidget);
  d->setupUi(this);

  ctkFlowLayout::replaceLayout(this);
  /*
  // Replace the horizontal layout with a flow layout
  ctkFlowLayout* flowLayout = new ctkFlowLayout;
  flowLayout->setPreferredExpandingDirections(Qt::Horizontal);
  flowLayout->setAlignItems(false);
  QLayout* oldLayout = this->layout();
  int margins[4];
  oldLayout->getContentsMargins(&margins[0],&margins[1],&margins[2],&margins[3]);
  QLayoutItem* item = 0;
  while((item = oldLayout->takeAt(0)))
    {
    if (item->widget())
      {
      flowLayout->addWidget(item->widget());
      }
    }
  // setLayout() will take care or reparenting layouts and widgets
  delete oldLayout;
  flowLayout->setContentsMargins(0,0,0,0);
  this->setLayout(flowLayout);
  */

  connect(d->NameLineEdit, SIGNAL(textChanged(QString)),
          this, SLOT(updateProperties()));
  connect(d->LabelMapCheckBox, SIGNAL(toggled(bool)),
          this, SLOT(updateProperties()));
  connect(d->CenteredCheckBox, SIGNAL(toggled(bool)),
          this, SLOT(updateProperties()));
  connect(d->SingleFileCheckBox, SIGNAL(toggled(bool)),
          this, SLOT(updateProperties()));
  connect(d->ColorTableComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
          this, SLOT(updateProperties()));


  // need to update the color selector when the label map check box is toggled
    connect(d->LabelMapCheckBox, SIGNAL(toggled(bool)),
            this, SLOT(updateColorSelector()));

  // Single file by default
  d->SingleFileCheckBox->setChecked(true);
  d->CenteredCheckBox->setChecked(true);
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeIOOptionsWidget::~qSlicerAstroVolumeIOOptionsWidget()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeIOOptionsWidget::updateProperties()
{
  Q_D(qSlicerAstroVolumeIOOptionsWidget);
  if (!d->NameLineEdit->text().isEmpty())
    {
    QStringList names = d->NameLineEdit->text().split(';');
    for (int i = 0; i < names.count(); ++i)
      {
      names[i] = names[i].trimmed();
      }
    d->Properties["name"] = names;
    }
  else
    {
    d->Properties.remove("name");
    }
  d->Properties["labelmap"] = d->LabelMapCheckBox->isChecked();
  d->Properties["center"] = d->CenteredCheckBox->isChecked();
  d->Properties["singleFile"] = d->SingleFileCheckBox->isChecked();
  d->Properties["colorNodeID"] = d->ColorTableComboBox->currentNodeID();
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeIOOptionsWidget::setFileName(const QString& fileName)
{
  this->setFileNames(QStringList(fileName));
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeIOOptionsWidget::setFileNames(const QStringList& fileNames)
{
  Q_D(qSlicerAstroVolumeIOOptionsWidget);
  QStringList names;
  bool onlyNumberInName = false;
  bool onlyNumberInExtension = false;
  bool hasLabelMapName = false;

  vtkNew<vtkMRMLVolumeArchetypeStorageNode> snode;
  foreach(const QString& fileName, fileNames)
    {
    QFileInfo fileInfo(fileName);
    QString fileBaseName = fileInfo.baseName();
    if (fileInfo.isFile())
      {
      std::string fileNameStd = fileInfo.fileName().toStdString();
      std::string filenameWithoutExtension = snode->GetFileNameWithoutExtension(fileNameStd.c_str());
      fileBaseName = QString(filenameWithoutExtension.c_str());
      names << fileBaseName;
      // Single file
      // If the name (or the extension) is just a number, then it must be a 2D
      // slice from a 3D volume, so uncheck Single File.
      onlyNumberInName = QRegExp("[0-9\\.\\-\\_\\@\\(\\)\\~]+").exactMatch(fileBaseName);
      fileInfo.suffix().toInt(&onlyNumberInExtension);
      }
      // Because '_' is considered as a word character (\w), \b
      // doesn't consider '_' as a word boundary.
    QRegExp labelMapName("(\\b|_)([Ll]abel(s)?)(\\b|_)");
    QRegExp segName("(\\b|_)([Ss]eg)(\\b|_)");
    QRegExp maskName("(\\b|_)([Mm]ask)(\\b|_)");
    if (fileInfo.baseName().contains(labelMapName) ||
        fileInfo.baseName().contains(segName) ||
        fileInfo.baseName().contains(maskName))
      {
      hasLabelMapName = true;
      }
    }
  d->NameLineEdit->setText( names.join("; ") );
  d->SingleFileCheckBox->setChecked(!onlyNumberInName && !onlyNumberInExtension);
  d->LabelMapCheckBox->setChecked(hasLabelMapName);
  this->qSlicerIOOptionsWidget::setFileNames(fileNames);

  // update the color selector since the label map check box may not
  // have changed on setting this new name
  this->updateColorSelector();
}


//-----------------------------------------------------------------------------
void qSlicerAstroVolumeIOOptionsWidget::updateColorSelector()
{
  Q_D(qSlicerAstroVolumeIOOptionsWidget);

  if (qSlicerCoreApplication::application() != NULL)
    {
    // access the color logic which has information about default color nodes
    vtkSlicerApplicationLogic* appLogic = qSlicerCoreApplication::application()->applicationLogic();
    if (appLogic && appLogic->GetColorLogic())
      {
      if (d->LabelMapCheckBox->isChecked())
        {
        d->ColorTableComboBox->setCurrentNodeID(appLogic->GetColorLogic()->GetDefaultLabelMapColorNodeID());
        }
      else
        {
        d->ColorTableComboBox->setCurrentNodeID(appLogic->GetColorLogic()->GetDefaultVolumeColorNodeID());
        }
      }
    }
}
