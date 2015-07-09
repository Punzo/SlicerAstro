/// Qt includes
#include <QFileInfo>

// CTK includes
#include <ctkFlowLayout.h>
#include <ctkUtils.h>

// AstroVolume includes
#include "qSlicerIOOptions_p.h"
#include "qSlicerAstroVolumeIOOptionsWidget.h"
#include "ui_qSlicerAstroVolumeIOOptionsWidget.h"

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
  connect(d->OrientationCheckBox, SIGNAL(toggled(bool)),
          this, SLOT(updateProperties()));

  // Single file by default
  d->SingleFileCheckBox->setChecked(true);
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
  d->Properties["discardOrientation"] = d->OrientationCheckBox->isChecked();
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
  foreach(const QString& fileName, fileNames)
    {
    QFileInfo fileInfo(fileName);
    if (fileInfo.isFile())
      {
      names << fileInfo.completeBaseName();
      // Single file
      // If the name (or the extension) is just a number, then it must be a 2D
      // slice from a 3D volume, so uncheck Single File.
      onlyNumberInName = QRegExp("[0-9\\.\\-\\_\\@\\(\\)\\~]+").exactMatch(fileInfo.baseName());
      fileInfo.suffix().toInt(&onlyNumberInExtension);
      }
    // Because '_' is considered as a word character (\w), \b
    // doesn't consider '_' as a word boundary.
    QRegExp labelMapName("(\\b|_)([Ll]abel(s)?)(\\b|_)");
    QRegExp segName("(\\b|_)([Ss]eg)(\\b|_)");
    if (fileInfo.baseName().contains(labelMapName) ||
        fileInfo.baseName().contains(segName))
      {
      hasLabelMapName = true;
      }
    }
  d->NameLineEdit->setText( names.join("; ") );
  d->SingleFileCheckBox->setChecked(!onlyNumberInName && !onlyNumberInExtension);
  d->LabelMapCheckBox->setChecked(hasLabelMapName);
  this->qSlicerIOOptionsWidget::setFileNames(fileNames);
}
