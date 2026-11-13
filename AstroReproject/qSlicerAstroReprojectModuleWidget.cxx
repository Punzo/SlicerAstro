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
  and was supported through the European Research Council grant nr. 291531.

==============================================================================*/

// Qt includes
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

// CTK includes
#include <ctkCollapsibleButton.h>

// AstroReproject includes
#include "qSlicerAstroReprojectModuleWidget.h"
#include "ui_qSlicerAstroReprojectModuleWidget.h"

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroReprojectLogic.h>

// SlicerQt includes
#include <qMRMLCheckableNodeComboBox.h>
#include <qMRMLNodeComboBox.h>
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerAstroVolumeModuleWidget.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>

// MRML includes
#include <vtkMRMLAstroReprojectParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkSlicerApplicationLogic.h>

// STD includes
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <sstream>

typedef vtkMRMLAstroReprojectParametersNode Parameters;

//-----------------------------------------------------------------------------
/// \ingroup SlicerAstro_QtModules_AstroReproject
class qSlicerAstroReprojectModuleWidgetPrivate: public Ui_qSlicerAstroReprojectModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroReprojectModuleWidget);
protected:
  qSlicerAstroReprojectModuleWidget* const q_ptr;
public:

  qSlicerAstroReprojectModuleWidgetPrivate(qSlicerAstroReprojectModuleWidget& object);
  void init();
  vtkSlicerAstroReprojectLogic* logic() const;

  /// widgets -> parameters node
  QComboBox* addComboBox(QFormLayout* layout, const QString& label, const char* name,
                         const QStringList& items, const QStringList& data = QStringList());
  QDoubleSpinBox* addDoubleSpinBox(QFormLayout* layout, const QString& label, const char* name,
                                   double minimum, double maximum, int decimals, const QString& suffix = QString());
  QSpinBox* addSpinBox(QFormLayout* layout, const QString& label, const char* name, int minimum, int maximum);
  void setRowVisible(QFormLayout* layout, QWidget* field, bool visible);
  /// Explanation of the selected option (label below the combo box) and
  /// tooltips of the options
  void explain(QFormLayout* layout, QComboBox* combo, const QString& tooltip, const QStringList& texts);
  void updateExplanations();
  QHash<QComboBox*, QLabel*> Explanations;
  QHash<QComboBox*, QStringList> ExplanationTexts;
  void updateVisibility();
  void updateInputsTable();
  void updateSummary();

  qSlicerAstroVolumeModuleWidget* astroVolumeWidget = nullptr;
  vtkSmartPointer<vtkMRMLAstroReprojectParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  bool updatingWidgets = false;

  // inputs
  qMRMLNodeComboBox* ParametersNodeComboBox = nullptr;
  qMRMLCheckableNodeComboBox* InputVolumesComboBox = nullptr;
  QTableWidget* InputsTable = nullptr;
  qMRMLNodeComboBox* OutputVolumeNodeSelector = nullptr;
  // grid
  QFormLayout* GridLayout = nullptr;
  QComboBox* SpatialGridModeComboBox = nullptr;
  qMRMLNodeComboBox* ReferenceVolumeNodeSelector = nullptr;
  QDoubleSpinBox* SpatialResampleFactorSpinBox = nullptr;
  QComboBox* CelestialFrameComboBox = nullptr;
  QDoubleSpinBox* EquinoxSpinBox = nullptr;
  QComboBox* ProjectionComboBox = nullptr;
  QDoubleSpinBox* CenterLongitudeSpinBox = nullptr;
  QDoubleSpinBox* CenterLatitudeSpinBox = nullptr;
  QDoubleSpinBox* PixelSizeXSpinBox = nullptr;
  QDoubleSpinBox* PixelSizeYSpinBox = nullptr;
  QDoubleSpinBox* RotationSpinBox = nullptr;
  QSpinBox* SizeXSpinBox = nullptr;
  QSpinBox* SizeYSpinBox = nullptr;
  QWidget* ManualGridButtons = nullptr;
  QComboBox* SpectralGridModeComboBox = nullptr;
  QDoubleSpinBox* SpectralResampleFactorSpinBox = nullptr;
  QComboBox* SpectralTypeComboBox = nullptr;
  QComboBox* SpectralFrameComboBox = nullptr;
  QDoubleSpinBox* RestFrequencySpinBox = nullptr;
  QDoubleSpinBox* SpectralStartSpinBox = nullptr;
  QDoubleSpinBox* SpectralStepSpinBox = nullptr;
  QSpinBox* SpectralCountSpinBox = nullptr;
  QPushButton* SpectralFromInputButton = nullptr;
  QComboBox* SpectralMatchingComboBox = nullptr;
  QLabel* GridSummaryLabel = nullptr;
  // methods
  QFormLayout* MethodLayout = nullptr;
  QComboBox* SpatialMethodComboBox = nullptr;
  QSpinBox* SupersamplingSpinBox = nullptr;
  QComboBox* SpectralMethodComboBox = nullptr;
  QDoubleSpinBox* MinimumCoverageSpinBox = nullptr;
  // resolution
  QFormLayout* ResolutionLayout = nullptr;
  QComboBox* ResolutionModeComboBox = nullptr;
  QDoubleSpinBox* TargetBeamMajorSpinBox = nullptr;
  QDoubleSpinBox* TargetBeamMinorSpinBox = nullptr;
  QDoubleSpinBox* TargetBeamPASpinBox = nullptr;
  QLabel* CommonBeamLabel = nullptr;
  // mosaic
  QFormLayout* MosaicLayout = nullptr;
  QComboBox* CombinationComboBox = nullptr;
  QComboBox* InputCorrectionComboBox = nullptr;
  QComboBox* PrimaryBeamModelComboBox = nullptr;
  QDoubleSpinBox* DishDiameterSpinBox = nullptr;
  QDoubleSpinBox* PrimaryBeamFactorSpinBox = nullptr;
  QDoubleSpinBox* PrimaryBeamCutoffSpinBox = nullptr;
  QCheckBox* NoiseWeightingCheckBox = nullptr;
  QCheckBox* ComputeWeightCheckBox = nullptr;
  QCheckBox* ComputeSensitivityCheckBox = nullptr;
  QCheckBox* SaultImageCheckBox = nullptr;
  QDoubleSpinBox* SaultFractionSpinBox = nullptr;
  // run
  QPushButton* ApplyButton = nullptr;
};

namespace
{
//-----------------------------------------------------------------------------
/// UI unit of a spectral type: (SI value per UI unit, unit)
void SpectralUnit(const QString& type, double& scale, QString& unit)
{
  if (type == "FREQ")
    {
    scale = 1.e6;
    unit = " MHz";
    }
  else if (type == "WAVE")
    {
    scale = 1.e-9;
    unit = " nm";
    }
  else
    {
    scale = 1.e3;
    unit = " km/s";
    }
}

//-----------------------------------------------------------------------------
void SetComboData(QComboBox* combo, const QString& data)
{
  const int index = combo->findData(data);
  combo->setCurrentIndex(index >= 0 ? index : 0);
}

//-----------------------------------------------------------------------------
std::vector<std::string> SplitList(const std::string& text)
{
  std::vector<std::string> items;
  std::stringstream stream(text);
  std::string item;
  while (std::getline(stream, item, ';'))
    {
    items.push_back(item);
    }
  return items;
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
qSlicerAstroReprojectModuleWidgetPrivate::qSlicerAstroReprojectModuleWidgetPrivate(qSlicerAstroReprojectModuleWidget& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
vtkSlicerAstroReprojectLogic* qSlicerAstroReprojectModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroReprojectModuleWidget);
  return vtkSlicerAstroReprojectLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
QComboBox* qSlicerAstroReprojectModuleWidgetPrivate::addComboBox(QFormLayout* layout, const QString& label,
  const char* name, const QStringList& items, const QStringList& data)
{
  QComboBox* combo = new QComboBox;
  combo->setObjectName(name);
  for (int index = 0; index < items.size(); index++)
    {
    combo->addItem(items[index], index < data.size() ? QVariant(data[index]) : QVariant(index));
    }
  layout->addRow(label, combo);
  return combo;
}

//-----------------------------------------------------------------------------
QDoubleSpinBox* qSlicerAstroReprojectModuleWidgetPrivate::addDoubleSpinBox(QFormLayout* layout, const QString& label,
  const char* name, double minimum, double maximum, int decimals, const QString& suffix)
{
  QDoubleSpinBox* box = new QDoubleSpinBox;
  box->setObjectName(name);
  box->setDecimals(decimals);
  box->setRange(minimum, maximum);
  box->setSuffix(suffix);
  box->setKeyboardTracking(false);
  layout->addRow(label, box);
  return box;
}

//-----------------------------------------------------------------------------
QSpinBox* qSlicerAstroReprojectModuleWidgetPrivate::addSpinBox(QFormLayout* layout, const QString& label,
  const char* name, int minimum, int maximum)
{
  QSpinBox* box = new QSpinBox;
  box->setObjectName(name);
  box->setRange(minimum, maximum);
  box->setKeyboardTracking(false);
  layout->addRow(label, box);
  return box;
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::setRowVisible(QFormLayout* layout, QWidget* field, bool visible)
{
  field->setVisible(visible);
  QWidget* label = layout->labelForField(field);
  if (label)
    {
    label->setVisible(visible);
    }
  QComboBox* combo = qobject_cast<QComboBox*>(field);
  if (combo && this->Explanations.contains(combo))
    {
    this->Explanations[combo]->setVisible(visible);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::explain(QFormLayout* layout, QComboBox* combo,
  const QString& tooltip, const QStringList& texts)
{
  combo->setToolTip(tooltip);
  for (int index = 0; index < combo->count() && index < texts.size(); index++)
    {
    combo->setItemData(index, texts[index], Qt::ToolTipRole);
    }
  QLabel* explanation = new QLabel;
  explanation->setObjectName(combo->objectName() + "Explanation");
  explanation->setWordWrap(true);
  QFont font = explanation->font();
  font.setItalic(true);
  explanation->setFont(font);
  explanation->setForegroundRole(QPalette::PlaceholderText);
  layout->addRow(QString(), explanation);
  this->Explanations[combo] = explanation;
  this->ExplanationTexts[combo] = texts;
  QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), explanation,
                   [explanation, texts](int index) { explanation->setText(texts.value(index)); });
  explanation->setText(texts.value(combo->currentIndex()));
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::updateExplanations()
{
  for (QHash<QComboBox*, QLabel*>::const_iterator it = this->Explanations.constBegin();
       it != this->Explanations.constEnd(); ++it)
    {
    it.value()->setText(this->ExplanationTexts.value(it.key()).value(it.key()->currentIndex()));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroReprojectModuleWidget);
  this->setupUi(q);

  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerAbstractCoreModule* astroVolume = app ? app->moduleManager()->module("AstroVolume") : nullptr;
  if (astroVolume)
    {
    this->astroVolumeWidget = dynamic_cast<qSlicerAstroVolumeModuleWidget*>(astroVolume->widgetRepresentation());
    }

  // --- quick guide
  ctkCollapsibleButton* guideButton = new ctkCollapsibleButton(q->tr("Quick guide"));
  guideButton->setObjectName("GuideCollapsibleButton");
  guideButton->setCollapsed(true);
  QVBoxLayout* guideLayout = new QVBoxLayout(guideButton);
  QLabel* guide = new QLabel(q->tr(
    "<p>The module puts the <b>input volumes</b> (checked in <i>Input volumes</i>) on a new grid of "
    "<b>sky pixels</b> and <b>channels</b> and creates a new volume. The values of the output pixels are "
    "computed from the input pixels at the same sky position and frequency/velocity, after the conversion "
    "of the celestial frames (e.g. FK4 to ICRS, Galactic) and of the spectral frames (e.g. topocentric to LSRK). "
    "The summary at the bottom of <i>Output grid</i> shows the grid that will be created.</p>"
    "<p><b>Common tasks</b></p><ul>"
    "<li><b>Overlay or compare two data sets</b> (e.g. HI contours on an optical image): input = the data to move, "
    "Sky grid = <i>Grid of a reference volume</i>, Reference = the other data.</li>"
    "<li><b>Larger or smaller pixels</b>: Sky grid = <i>Input grid, other pixel size</i>, factor 2 = pixels twice "
    "as large. For larger pixels use <i>Area average</i> or <i>Exact overlap</i> in Resampling (flux conserved).</li>"
    "<li><b>North up, other projection or celestial frame</b>: Sky grid = <i>Manual</i>, press "
    "<i>From input, north up</i>, then change projection, frame, centre or size.</li>"
    "<li><b>Wider channels, frequency to velocity, other spectral frame</b> (e.g. topocentric to LSRK): "
    "Channels = <i>Input channels, other width</i> or <i>Manual</i>.</li>"
    "<li><b>Same resolution for several data</b>: Resolution = <i>Common beam of the inputs</i>.</li>"
    "<li><b>Mosaic</b> of several pointings or fields: check all of them in Input volumes, Sky grid = "
    "<i>Covering all the inputs</i>, Channels = <i>Covering all the inputs</i>, then the primary beam options in "
    "<i>Combination (mosaic)</i>.</li>"
    "<li><b>Different lines</b> (e.g. CO on the channels of HI): Channels = <i>Channels of the reference volume</i>, "
    "Match channels by = <i>Velocity</i>.</li></ul>"
    "<p>Every option has a tooltip, and the text under each choice explains the selected one.</p>"));
  guide->setObjectName("GuideLabel");
  guide->setWordWrap(true);
  guide->setTextFormat(Qt::RichText);
  guideLayout->addWidget(guide);
  this->verticalLayout->addWidget(guideButton);

  // --- inputs
  ctkCollapsibleButton* inputsButton = new ctkCollapsibleButton(q->tr("Inputs and output"));
  inputsButton->setObjectName("InputsCollapsibleButton");
  QFormLayout* inputsLayout = new QFormLayout(inputsButton);
  this->InputVolumesComboBox = new qMRMLCheckableNodeComboBox;
  this->InputVolumesComboBox->setObjectName("InputVolumesComboBox");
  this->InputVolumesComboBox->setNodeTypes(QStringList() << "vtkMRMLAstroVolumeNode");
  this->InputVolumesComboBox->setToolTip(q->tr(
    "The data to put on the new grid (check one or more volumes). These are 'the inputs' of all the options "
    "below. One volume: reprojection/resampling. Several volumes: they are combined into one volume (mosaic, "
    "see Combination). The first checked volume is used where a single input is needed (e.g. 'Grid of the input')."));
  inputsLayout->addRow(q->tr("Input volumes:"), this->InputVolumesComboBox);
  this->InputsTable = new QTableWidget(0, 3);
  this->InputsTable->setObjectName("InputsTable");
  this->InputsTable->setHorizontalHeaderLabels(QStringList() << q->tr("Data") << q->tr("Noise") << q->tr("Primary beam"));
  this->InputsTable->horizontalHeader()->setStretchLastSection(true);
  this->InputsTable->verticalHeader()->hide();
  this->InputsTable->setToolTip(q->tr(
    "One row per input volume.\n"
    "Noise: rms noise of the data (data units), used by the noise weighting and the sensitivity output of mosaics. "
    "auto: the noise estimated by SlicerAstro when the data were loaded (first and last channels).\n"
    "Primary beam: a volume with the primary beam response of that input (values 0-1), used with the "
    "primary beam model 'Volumes'."));
  inputsLayout->addRow(this->InputsTable);
  this->OutputVolumeNodeSelector = new qMRMLNodeComboBox;
  this->OutputVolumeNodeSelector->setObjectName("OutputVolumeNodeSelector");
  this->OutputVolumeNodeSelector->setNodeTypes(QStringList() << "vtkMRMLAstroVolumeNode");
  this->OutputVolumeNodeSelector->setNoneEnabled(true);
  this->OutputVolumeNodeSelector->setNoneDisplay(q->tr("New volume"));
  this->OutputVolumeNodeSelector->setAddEnabled(false);
  this->OutputVolumeNodeSelector->setRemoveEnabled(false);
  this->OutputVolumeNodeSelector->setToolTip(q->tr(
    "New volume: a new volume is created (<input>_Reprojected, or <input>_Mosaic for several inputs). "
    "An existing volume is replaced by the result (the inputs are never modified)."));
  inputsLayout->addRow(q->tr("Output:"), this->OutputVolumeNodeSelector);
  this->ParametersNodeComboBox = new qMRMLNodeComboBox;
  this->ParametersNodeComboBox->setObjectName("ParametersNodeComboBox");
  this->ParametersNodeComboBox->setNodeTypes(QStringList() << "vtkMRMLAstroReprojectParametersNode");
  this->ParametersNodeComboBox->setShowHidden(true);
  this->ParametersNodeComboBox->setNoneEnabled(false);
  this->ParametersNodeComboBox->setAddEnabled(true);
  this->ParametersNodeComboBox->setRemoveEnabled(true);
  this->ParametersNodeComboBox->setRenameEnabled(true);
  this->ParametersNodeComboBox->setToolTip(q->tr(
    "Set of the settings of this module (saved with the scene). Create several to switch between "
    "configurations."));
  inputsLayout->addRow(q->tr("Parameters:"), this->ParametersNodeComboBox);
  this->verticalLayout->addWidget(inputsButton);

  // --- output grid
  ctkCollapsibleButton* gridButton = new ctkCollapsibleButton(q->tr("Output grid"));
  gridButton->setObjectName("GridCollapsibleButton");
  this->GridLayout = new QFormLayout(gridButton);
  QFormLayout* grid = this->GridLayout;
  this->SpatialGridModeComboBox = this->addComboBox(grid, q->tr("Sky grid:"), "SpatialGridModeComboBox",
    QStringList() << q->tr("Grid of a reference volume") << q->tr("Grid of the input") << q->tr("Input grid, other pixel size")
                  << q->tr("Covering all the inputs (mosaic)") << q->tr("Area common to all the inputs") << q->tr("Manual"));
  this->explain(grid, this->SpatialGridModeComboBox, q->tr(
    "Pixels on the sky of the output: projection, celestial frame, centre, pixel size, rotation and size."),
    QStringList()
    << q->tr("The output has exactly the sky pixels of the Reference volume (projection, frame, centre, pixel "
             "size, rotation, size): use it to overlay or compare the inputs with the reference pixel by pixel.")
    << q->tr("The sky pixels of the (first) input are kept: use it to change only the channels or the resolution.")
    << q->tr("The sky grid of the input with pixels 'Pixel size factor' times larger (factor > 1: fewer, larger "
             "pixels) or smaller (factor < 1: finer sampling); the field of view stays the same.")
    << q->tr("A new grid that contains all the input volumes (their union on the sky), with the frame, "
             "projection, pixel size and rotation below: use it to mosaic several pointings or fields.")
    << q->tr("A new grid that covers only the sky area where all the input volumes overlap (their "
             "intersection), with the frame, projection, pixel size and rotation below.")
    << q->tr("A grid defined by you: frame, projection, centre, pixel size, rotation and number of pixels. "
             "'From input' fills the fields with the grid of the first input, 'north up' also removes its rotation."));
  this->ReferenceVolumeNodeSelector = new qMRMLNodeComboBox;
  this->ReferenceVolumeNodeSelector->setObjectName("ReferenceVolumeNodeSelector");
  this->ReferenceVolumeNodeSelector->setNodeTypes(QStringList() << "vtkMRMLAstroVolumeNode");
  this->ReferenceVolumeNodeSelector->setNoneEnabled(true);
  this->ReferenceVolumeNodeSelector->setAddEnabled(false);
  this->ReferenceVolumeNodeSelector->setRemoveEnabled(false);
  this->ReferenceVolumeNodeSelector->setToolTip(q->tr(
    "Volume whose grid (sky pixels and/or channels) is used for the output. Its data are not used or modified."));
  grid->addRow(q->tr("Reference:"), this->ReferenceVolumeNodeSelector);
  this->SpatialResampleFactorSpinBox = this->addDoubleSpinBox(grid, q->tr("Pixel size factor:"),
    "SpatialResampleFactorSpinBox", 0.01, 100., 3);
  this->SpatialResampleFactorSpinBox->setToolTip(q->tr(
    "Output pixel size / input pixel size. 2: pixels twice as large (half the pixels per axis, lower "
    "sampling; use Area average or Exact overlap). 0.5: pixels twice as small (finer sampling, no new "
    "information: the resolution is set by the beam)."));
  this->CelestialFrameComboBox = this->addComboBox(grid, q->tr("Frame:"), "CelestialFrameComboBox",
    QStringList() << "ICRS" << q->tr("FK5 (equatorial)") << q->tr("FK4 (equatorial)") << q->tr("Galactic") << q->tr("Supergalactic"),
    QStringList() << "ICRS" << "FK5" << "FK4" << "GALACTIC" << "SUPERGALACTIC");
  this->explain(grid, this->CelestialFrameComboBox, q->tr(
    "Celestial coordinate system of the output. The inputs are converted (ERFA, as astropy)."),
    QStringList()
    << q->tr("ICRS: the modern equatorial system (RA, Dec), within 0.02\" of FK5 J2000.")
    << q->tr("FK5: equatorial RA, Dec at the equinox below (J2000 for most data).")
    << q->tr("FK4: equatorial RA, Dec of older data (B1950), with the elliptic aberration terms.")
    << q->tr("Galactic longitude and latitude (l, b).")
    << q->tr("Supergalactic longitude and latitude (SGL, SGB)."));
  this->EquinoxSpinBox = this->addDoubleSpinBox(grid, q->tr("Equinox:"), "EquinoxSpinBox", 1800., 2200., 1);
  this->EquinoxSpinBox->setToolTip(q->tr("Equinox of the FK5 (Julian, e.g. 2000) or FK4 (Besselian, e.g. 1950) frame."));
  this->ProjectionComboBox = this->addComboBox(grid, q->tr("Projection:"), "ProjectionComboBox",
    QStringList() << q->tr("SIN (orthographic)") << q->tr("TAN (gnomonic)") << q->tr("ARC (zenithal equidistant)")
                  << q->tr("ZEA (zenithal equal area)") << q->tr("STG (stereographic)") << q->tr("CAR (plate carree)")
                  << q->tr("SFL (Sanson-Flamsteed)") << q->tr("AIT (Hammer-Aitoff)") << q->tr("MER (Mercator)"),
    QStringList() << "SIN" << "TAN" << "ARC" << "ZEA" << "STG" << "CAR" << "SFL" << "AIT" << "MER");
  this->explain(grid, this->ProjectionComboBox, q->tr(
    "Map projection of the sky on the pixels (FITS WCS, Calabretta & Greisen 2002). For fields of a few "
    "degrees or less all projections are almost identical."),
    QStringList()
    << q->tr("SIN: usual for radio interferometers (and SlicerAstro default).")
    << q->tr("TAN: usual for optical and infrared images.")
    << q->tr("ARC: distances from the centre are preserved (e.g. single-dish surveys).")
    << q->tr("ZEA: areas preserved; good for large fields.")
    << q->tr("STG: angles preserved (conformal).")
    << q->tr("CAR: longitude and latitude on a regular grid; all-sky and Galactic plane surveys.")
    << q->tr("SFL: areas preserved, all-sky.")
    << q->tr("AIT: areas preserved, all-sky maps.")
    << q->tr("MER: conformal cylindrical projection."));
  this->CenterLongitudeSpinBox = this->addDoubleSpinBox(grid, q->tr("Centre longitude:"), "CenterLongitudeSpinBox", -360., 360., 7, " deg");
  this->CenterLongitudeSpinBox->setToolTip(q->tr("Sky position of the centre of the grid (RA or longitude, degrees, in the frame above)."));
  this->CenterLatitudeSpinBox = this->addDoubleSpinBox(grid, q->tr("Centre latitude:"), "CenterLatitudeSpinBox", -90., 90., 7, " deg");
  this->CenterLatitudeSpinBox->setToolTip(q->tr("Sky position of the centre of the grid (Dec or latitude, degrees, in the frame above)."));
  this->PixelSizeXSpinBox = this->addDoubleSpinBox(grid, q->tr("Pixel size:"), "PixelSizeXSpinBox", 0., 1.e6, 4, "\"");
  this->PixelSizeXSpinBox->setSpecialValueText(q->tr("smallest of the inputs"));
  this->PixelSizeXSpinBox->setToolTip(q->tr(
    "Size of the output pixels on the sky (arcsec). 0: the smallest pixel of the inputs. Sample the beam with at "
    "least 3-5 pixels across its FWHM."));
  this->PixelSizeYSpinBox = this->addDoubleSpinBox(grid, q->tr("Pixel size (y):"), "PixelSizeYSpinBox", 0., 1.e6, 4, "\"");
  this->PixelSizeYSpinBox->setToolTip(q->tr("Size of the output pixels along y (arcsec); usually equal to the x size."));
  this->RotationSpinBox = this->addDoubleSpinBox(grid, q->tr("Rotation (CROTA2):"), "RotationSpinBox", -360., 360., 4, " deg");
  this->RotationSpinBox->setToolTip(q->tr(
    "Rotation of the pixel grid on the sky (FITS CROTA2). 0: north up and east to the left."));
  this->SizeXSpinBox = this->addSpinBox(grid, q->tr("Pixels (x):"), "SizeXSpinBox", 1, 100000);
  this->SizeXSpinBox->setToolTip(q->tr("Number of pixels along x (the field of view is pixels x pixel size)."));
  this->SizeYSpinBox = this->addSpinBox(grid, q->tr("Pixels (y):"), "SizeYSpinBox", 1, 100000);
  this->SizeYSpinBox->setToolTip(q->tr("Number of pixels along y."));
  this->ManualGridButtons = new QWidget;
  {
  QHBoxLayout* buttons = new QHBoxLayout(this->ManualGridButtons);
  buttons->setContentsMargins(0, 0, 0, 0);
  QPushButton* fromInput = new QPushButton(q->tr("From input"));
  fromInput->setObjectName("ManualGridFromInputButton");
  fromInput->setToolTip(q->tr("Fill the fields with the sky grid of the first input (starting point to edit)."));
  QPushButton* northUp = new QPushButton(q->tr("From input, north up"));
  northUp->setObjectName("ManualGridNorthUpButton");
  northUp->setToolTip(q->tr("As 'From input', without rotation: north up, the grid enlarged to contain the rotated data."));
  buttons->addWidget(fromInput);
  buttons->addWidget(northUp);
  QObject::connect(fromInput, SIGNAL(clicked()), q, SLOT(setManualGridFromInput()));
  QObject::connect(northUp, SIGNAL(clicked()), q, SLOT(setManualGridFromInputNorthUp()));
  }
  grid->addRow(QString(), this->ManualGridButtons);
  this->SpectralGridModeComboBox = this->addComboBox(grid, q->tr("Channels:"), "SpectralGridModeComboBox",
    QStringList() << q->tr("Channels of the input") << q->tr("Channels of the reference volume")
                  << q->tr("Input channels, other width") << q->tr("Covering all the inputs")
                  << q->tr("Range common to all the inputs") << q->tr("Manual"));
  this->explain(grid, this->SpectralGridModeComboBox, q->tr(
    "Spectral axis (channels) of the output. Ignored for 2D images."),
    QStringList()
    << q->tr("The channels of the (first) input are kept (no spectral resampling).")
    << q->tr("The output has the channels of the Reference volume: the inputs are resampled on them, after the "
             "conversion of the spectral frames (and of the lines, see 'Match channels by').")
    << q->tr("The channels of the input 'Channel width factor' times wider (factor > 1: fewer, wider "
             "channels; noise averaged down with Channel overlap) or narrower.")
    << q->tr("New channels covering the spectral ranges of all the inputs, with the spectral axis, frame and "
             "width below (width 0: the narrowest channel of the inputs).")
    << q->tr("New channels covering only the spectral range common to all the inputs.")
    << q->tr("Channels defined by you: spectral axis, frame, rest frequency, first channel, width and number. "
             "'Channels from input' fills the fields from the first input."));
  this->SpectralResampleFactorSpinBox = this->addDoubleSpinBox(grid, q->tr("Channel width factor:"),
    "SpectralResampleFactorSpinBox", 0.01, 1000., 3);
  this->SpectralResampleFactorSpinBox->setToolTip(q->tr(
    "Output channel width / input channel width. 2: channels twice as wide (half the channels, noise lower by "
    "sqrt(2) with Channel overlap). 0.5: twice as narrow."));
  this->SpectralTypeComboBox = this->addComboBox(grid, q->tr("Spectral axis:"), "SpectralTypeComboBox",
    QStringList() << q->tr("Radio velocity") << q->tr("Optical velocity") << q->tr("Relativistic velocity")
                  << q->tr("Frequency") << q->tr("Wavelength"),
    QStringList() << "VRAD" << "VOPT" << "VELO" << "FREQ" << "WAVE");
  this->explain(grid, this->SpectralTypeComboBox, q->tr(
    "Quantity that is linear along the output channels. SlicerAstro shows frequency axes as radio velocities "
    "and wavelength axes as optical velocities; the file keeps the chosen axis."),
    QStringList()
    << q->tr("Radio velocity v = c (1 - f / f0): radio astronomy (e.g. HI, CO); channels equally spaced in frequency.")
    << q->tr("Optical velocity v = c (f0 / f - 1) = cz: optical spectra; channels equally spaced in wavelength.")
    << q->tr("Relativistic (apparent radial) velocity.")
    << q->tr("Frequency (no rest frequency needed for the axis itself).")
    << q->tr("Wavelength (vacuum)."));
  this->SpectralFrameComboBox = this->addComboBox(grid, q->tr("Spectral frame:"), "SpectralFrameComboBox",
    QStringList() << q->tr("Barycentric") << q->tr("LSR kinematic") << q->tr("LSR dynamic") << q->tr("Topocentric")
                  << q->tr("Geocentric") << q->tr("Galactocentric") << q->tr("Local Group") << q->tr("CMB dipole"),
    QStringList() << "BARYCENT" << "LSRK" << "LSRD" << "TOPOCENT" << "GEOCENTR" << "GALACTOC" << "LOCALGRP" << "CMBDIPOL");
  this->explain(grid, this->SpectralFrameComboBox, q->tr(
    "Rest frame of the frequencies/velocities of the output (FITS SPECSYS). The inputs are converted from "
    "their SPECSYS (casacore conventions)."),
    QStringList()
    << q->tr("Barycentric: the solar system barycentre (optical and extragalactic data; ~heliocentric).")
    << q->tr("LSRK: kinematic local standard of rest (20 km/s towards RA 18h, Dec +30 deg, B1900); radio "
             "observatories and Galactic data.")
    << q->tr("LSRD: dynamical local standard of rest (9, 12, 7 km/s in Galactic coordinates).")
    << q->tr("Topocentric: the observatory (needs DATE-OBS/MJD-OBS and OBSGEO in the headers).")
    << q->tr("Geocentric: the centre of the Earth (needs the date of the observation).")
    << q->tr("Galactocentric: the centre of the Galaxy (LSRD + 220 km/s of Galactic rotation).")
    << q->tr("Local Group barycentre (308 km/s).")
    << q->tr("Frame of the cosmic microwave background (dipole, 369.5 km/s)."));
  this->RestFrequencySpinBox = this->addDoubleSpinBox(grid, q->tr("Rest frequency:"), "RestFrequencySpinBox", 0., 1.e9, 6, " MHz");
  this->RestFrequencySpinBox->setToolTip(q->tr(
    "Rest frequency of the line, for velocity axes: HI 1420.405752 MHz, CO(1-0) 115271.2018 MHz, "
    "CO(2-1) 230538.0 MHz, CO(3-2) 345795.99 MHz."));
  this->SpectralStartSpinBox = this->addDoubleSpinBox(grid, q->tr("First channel:"), "SpectralStartSpinBox", -1.e12, 1.e12, 6);
  this->SpectralStartSpinBox->setToolTip(q->tr("Value of the first output channel (centre), in the spectral axis and frame above."));
  this->SpectralStepSpinBox = this->addDoubleSpinBox(grid, q->tr("Channel width:"), "SpectralStepSpinBox", -1.e12, 1.e12, 6);
  this->SpectralStepSpinBox->setToolTip(q->tr(
    "Width of the output channels (negative: decreasing values). For 'Covering all the inputs': 0 = the "
    "narrowest channel of the inputs."));
  this->SpectralCountSpinBox = this->addSpinBox(grid, q->tr("Channels:"), "SpectralCountSpinBox", 1, 1000000);
  this->SpectralCountSpinBox->setToolTip(q->tr("Number of output channels."));
  this->SpectralFromInputButton = new QPushButton(q->tr("Channels from input"));
  this->SpectralFromInputButton->setObjectName("SpectralFromInputButton");
  this->SpectralFromInputButton->setToolTip(q->tr("Fill the channel fields with the spectral axis of the first input."));
  QObject::connect(this->SpectralFromInputButton, SIGNAL(clicked()), q, SLOT(setManualSpectralGridFromInput()));
  grid->addRow(QString(), this->SpectralFromInputButton);
  this->SpectralMatchingComboBox = this->addComboBox(grid, q->tr("Match channels by:"), "SpectralMatchingComboBox",
    QStringList() << q->tr("Frequency") << q->tr("Velocity (rest frequency of each input)"));
  this->explain(grid, this->SpectralMatchingComboBox, q->tr(
    "How the input channels are matched to the output channels."),
    QStringList()
    << q->tr("By observed frequency: the usual choice for data of the same line.")
    << q->tr("By velocity: each input is converted to velocity with its own rest frequency (as CASA imregrid). "
             "Use it to align different lines, e.g. CO on the channels of HI."));
  this->GridSummaryLabel = new QLabel;
  this->GridSummaryLabel->setObjectName("GridSummaryLabel");
  this->GridSummaryLabel->setWordWrap(true);
  this->GridSummaryLabel->setToolTip(q->tr("The grid that will be created with the current settings."));
  grid->addRow(this->GridSummaryLabel);
  this->verticalLayout->addWidget(gridButton);

  // --- methods
  ctkCollapsibleButton* methodButton = new ctkCollapsibleButton(q->tr("Resampling"));
  methodButton->setObjectName("MethodCollapsibleButton");
  this->MethodLayout = new QFormLayout(methodButton);
  this->SpatialMethodComboBox = this->addComboBox(this->MethodLayout, q->tr("Sky:"), "SpatialMethodComboBox",
    QStringList() << q->tr("Nearest neighbour") << q->tr("Bilinear") << q->tr("Bicubic") << q->tr("Lanczos-3")
                  << q->tr("Area average (flux conserving)") << q->tr("Exact overlap (flux conserving, slow)"));
  this->explain(this->MethodLayout, this->SpatialMethodComboBox, q->tr(
    "How the value of an output pixel is computed from the input pixels around the same sky position."),
    QStringList()
    << q->tr("Value of the closest input pixel: keeps the original values (masks, labels), blocky.")
    << q->tr("Linear interpolation of the 4 closest pixels: good default when the output pixels are similar to "
             "or smaller than the input pixels.")
    << q->tr("Cubic convolution of the 16 closest pixels (as CASA): smoother and more accurate than bilinear "
             "for well-sampled data.")
    << q->tr("Lanczos kernel on 6 x 6 pixels: the sharpest interpolation; may ring around bright compact sources.")
    << q->tr("Average of the input sampled on a sub-grid of each output pixel: conserves the flux; use it when "
             "the output pixels are larger than the input pixels.")
    << q->tr("Average of the input pixels weighted by their exact overlap with the output pixel on the sphere "
             "(as Montage and astropy reproject 'exact'): exact flux conservation, slowest."));
  this->SupersamplingSpinBox = this->addSpinBox(this->MethodLayout, q->tr("Subpixels:"), "SupersamplingSpinBox", 0, 32);
  this->SupersamplingSpinBox->setSpecialValueText(q->tr("automatic"));
  this->SupersamplingSpinBox->setToolTip(q->tr(
    "Area average: sub-grid points per axis in each output pixel (automatic: from the ratio of the pixel sizes)."));
  this->SpectralMethodComboBox = this->addComboBox(this->MethodLayout, q->tr("Channels:"), "SpectralMethodComboBox",
    QStringList() << q->tr("Nearest channel") << q->tr("Linear") << q->tr("Cubic") << q->tr("Channel overlap (flux conserving)")
                  << q->tr("Automatic (overlap for wider channels)"));
  this->explain(this->MethodLayout, this->SpectralMethodComboBox, q->tr(
    "How the value of an output channel is computed from the input channels."),
    QStringList()
    << q->tr("Value of the closest input channel.")
    << q->tr("Linear interpolation of the 2 closest channels.")
    << q->tr("Cubic interpolation of the 4 closest channels.")
    << q->tr("Average of the input channels weighted by their overlap with the output channel: conserves the "
             "integrated flux and averages the noise down (CASA does not average channels).")
    << q->tr("Channel overlap when the output channels are wider than the input channels, cubic interpolation "
             "otherwise (recommended)."));
  this->MinimumCoverageSpinBox = this->addDoubleSpinBox(this->MethodLayout, q->tr("Minimum coverage:"),
    "MinimumCoverageSpinBox", 0., 1., 2);
  this->MinimumCoverageSpinBox->setToolTip(q->tr(
    "Area average, exact overlap and channel overlap: output pixels/channels covered by valid (not blank) data "
    "for less than this fraction are blank."));
  this->verticalLayout->addWidget(methodButton);

  // --- resolution
  ctkCollapsibleButton* resolutionButton = new ctkCollapsibleButton(q->tr("Resolution"));
  resolutionButton->setObjectName("ResolutionCollapsibleButton");
  resolutionButton->setCollapsed(true);
  this->ResolutionLayout = new QFormLayout(resolutionButton);
  this->ResolutionModeComboBox = this->addComboBox(this->ResolutionLayout, q->tr("Beam:"), "ResolutionModeComboBox",
    QStringList() << q->tr("Keep the beams of the data") << q->tr("Common beam of the inputs") << q->tr("Target beam"));
  this->explain(this->ResolutionLayout, this->ResolutionModeComboBox, q->tr(
    "Angular resolution (beam) of the output. The inputs are convolved with Gaussian kernels before the "
    "resampling; Jy/beam data keep the peak of point sources, Jy/pixel data become Jy/beam."),
    QStringList()
    << q->tr("No smoothing: each input keeps its beam (with several inputs of different beams the resolution "
             "of the result varies across it).")
    << q->tr("Every input is smoothed to the smallest beam all the beams fit in (CASA algorithm), so the result "
             "has one resolution: needed to combine or compare data with different beams.")
    << q->tr("Every input is smoothed to the beam given below, which must be larger than the beams of the data "
             "in every direction."));
  this->CommonBeamLabel = new QLabel;
  this->CommonBeamLabel->setObjectName("CommonBeamLabel");
  this->CommonBeamLabel->setToolTip(q->tr("Common beam of the inputs: FWHM major x minor axis, position angle."));
  this->ResolutionLayout->addRow(q->tr("Common beam:"), this->CommonBeamLabel);
  this->TargetBeamMajorSpinBox = this->addDoubleSpinBox(this->ResolutionLayout, q->tr("Major axis:"), "TargetBeamMajorSpinBox", 0., 1.e6, 3, "\"");
  this->TargetBeamMajorSpinBox->setToolTip(q->tr("FWHM of the major axis of the target beam (arcsec)."));
  this->TargetBeamMinorSpinBox = this->addDoubleSpinBox(this->ResolutionLayout, q->tr("Minor axis:"), "TargetBeamMinorSpinBox", 0., 1.e6, 3, "\"");
  this->TargetBeamMinorSpinBox->setToolTip(q->tr("FWHM of the minor axis of the target beam (arcsec)."));
  this->TargetBeamPASpinBox = this->addDoubleSpinBox(this->ResolutionLayout, q->tr("Position angle:"), "TargetBeamPASpinBox", -180., 180., 2, " deg");
  this->TargetBeamPASpinBox->setToolTip(q->tr("Position angle of the major axis, from north through east (degrees)."));
  QPushButton* commonBeam = new QPushButton(q->tr("Use the common beam"));
  commonBeam->setObjectName("TargetFromCommonBeamButton");
  commonBeam->setToolTip(q->tr("Copy the common beam of the inputs into the target beam (e.g. to round it up)."));
  QObject::connect(commonBeam, SIGNAL(clicked()), q, SLOT(setTargetBeamFromCommonBeam()));
  this->ResolutionLayout->addRow(QString(), commonBeam);
  this->verticalLayout->addWidget(resolutionButton);

  // --- mosaic
  ctkCollapsibleButton* mosaicButton = new ctkCollapsibleButton(q->tr("Combination (mosaic)"));
  mosaicButton->setObjectName("MosaicCollapsibleButton");
  mosaicButton->setCollapsed(true);
  this->MosaicLayout = new QFormLayout(mosaicButton);
  QLabel* mosaicNote = new QLabel(q->tr(
    "Used when several input volumes are checked: where they overlap, the output is a weighted combination "
    "(linear mosaic, Sault, Staveley-Smith & Brouw 1996; CASA linearmosaic, Miriad linmos)."));
  mosaicNote->setWordWrap(true);
  this->MosaicLayout->addRow(mosaicNote);
  this->CombinationComboBox = this->addComboBox(this->MosaicLayout, q->tr("Combination:"), "CombinationComboBox",
    QStringList() << q->tr("Mean") << q->tr("Optimal (primary beam squared)") << q->tr("Primary beam weighted"));
  this->explain(this->MosaicLayout, this->CombinationComboBox, q->tr(
    "Weights of the inputs where they overlap. A = primary beam, w = 1 or 1 / noise^2."),
    QStringList()
    << q->tr("Plain average of the inputs covering each pixel (no primary beam): for data already on a common "
             "flux scale with uniform noise, e.g. adjacent fields.")
    << q->tr("sum(A I w) / sum(A^2 w): the lowest noise for interferometer pointings (Sault et al. 1996).")
    << q->tr("sum(I w) / sum(A w): weights proportional to the primary beam."));
  this->InputCorrectionComboBox = this->addComboBox(this->MosaicLayout, q->tr("The inputs are:"), "InputCorrectionComboBox",
    QStringList() << q->tr("Primary beam corrected") << q->tr("Not corrected (flat noise)") << q->tr("Multiplied by the primary beam"));
  this->explain(this->MosaicLayout, this->InputCorrectionComboBox, q->tr(
    "Primary beam state of the input images."),
    QStringList()
    << q->tr("Divided by the primary beam (flux correct, noise rising towards the edges): e.g. CASA .pbcor images.")
    << q->tr("Not divided by the primary beam (uniform noise, sources attenuated away from the centre): e.g. CASA "
             ".image products.")
    << q->tr("Already multiplied by the primary beam (A I).")) ;
  this->PrimaryBeamModelComboBox = this->addComboBox(this->MosaicLayout, q->tr("Primary beam:"), "PrimaryBeamModelComboBox",
    QStringList() << q->tr("None") << q->tr("Gaussian") << q->tr("Airy disk") << q->tr("Volumes (inputs table)"));
  this->explain(this->MosaicLayout, this->PrimaryBeamModelComboBox, q->tr(
    "Response of the antennas around each pointing centre (OBSRA/OBSDEC of the header, otherwise the "
    "reference position CRVAL), at the frequency of each channel."),
    QStringList()
    << q->tr("No primary beam (A = 1): only the mean combination is meaningful.")
    << q->tr("Gaussian with FWHM = factor x wavelength / dish diameter.")
    << q->tr("Airy pattern of a uniformly illuminated dish of the given diameter.")
    << q->tr("The primary beam volumes selected for each input in the inputs table."));
  this->DishDiameterSpinBox = this->addDoubleSpinBox(this->MosaicLayout, q->tr("Dish diameter:"), "DishDiameterSpinBox", 0., 1000., 2, " m");
  this->DishDiameterSpinBox->setToolTip(q->tr("Diameter of the antennas: VLA and WSRT 25 m, ALMA 12 m, ATCA 22 m, MeerKAT 13.5 m."));
  this->PrimaryBeamFactorSpinBox = this->addDoubleSpinBox(this->MosaicLayout, q->tr("FWHM:"), "PrimaryBeamFactorSpinBox", 0.1, 5., 3, " lambda/D");
  this->PrimaryBeamFactorSpinBox->setToolTip(q->tr(
    "FWHM of the Gaussian primary beam in units of wavelength / dish diameter (1.13 for ALMA, about 1.02-1.2 "
    "for other arrays, depending on the illumination)."));
  this->PrimaryBeamCutoffSpinBox = this->addDoubleSpinBox(this->MosaicLayout, q->tr("Cutoff:"), "PrimaryBeamCutoffSpinBox", 0., 1., 3);
  this->PrimaryBeamCutoffSpinBox->setToolTip(q->tr(
    "An input is not used where its primary beam is below this level (e.g. 0.2: 20% of the peak response)."));
  this->NoiseWeightingCheckBox = new QCheckBox(q->tr("Weights 1 / noise^2 (Miriad linmos)"));
  this->NoiseWeightingCheckBox->setObjectName("NoiseWeightingCheckBox");
  this->NoiseWeightingCheckBox->setToolTip(q->tr(
    "Weight each input by 1 / noise^2 (noise from the inputs table): noisier inputs count less."));
  this->MosaicLayout->addRow(QString(), this->NoiseWeightingCheckBox);
  this->ComputeWeightCheckBox = new QCheckBox(q->tr("Weight volume"));
  this->ComputeWeightCheckBox->setObjectName("ComputeWeightCheckBox");
  this->ComputeWeightCheckBox->setToolTip(q->tr(
    "Also create a volume with the sum of the weights at each pixel (where and how much the inputs contribute)."));
  this->MosaicLayout->addRow(q->tr("Outputs:"), this->ComputeWeightCheckBox);
  this->ComputeSensitivityCheckBox = new QCheckBox(q->tr("Sensitivity (noise) volume"));
  this->ComputeSensitivityCheckBox->setObjectName("ComputeSensitivityCheckBox");
  this->ComputeSensitivityCheckBox->setToolTip(q->tr(
    "Also create a volume with the expected noise of the result at each pixel (needs the noise of every input)."));
  this->MosaicLayout->addRow(QString(), this->ComputeSensitivityCheckBox);
  this->SaultImageCheckBox = new QCheckBox(q->tr("Sault-weighted volume"));
  this->SaultImageCheckBox->setObjectName("SaultImageCheckBox");
  this->SaultImageCheckBox->setToolTip(q->tr(
    "Also create a display version of the mosaic: flux correct where the weight is above the threshold, "
    "tapered towards the edges (uniform noise) below it (Sault et al. 1996)."));
  this->MosaicLayout->addRow(QString(), this->SaultImageCheckBox);
  this->SaultFractionSpinBox = this->addDoubleSpinBox(this->MosaicLayout, q->tr("Sault threshold:"), "SaultFractionSpinBox", 0., 1., 3);
  this->SaultFractionSpinBox->setToolTip(q->tr("Fraction of the peak weight above which the Sault volume is flux correct."));
  this->verticalLayout->addWidget(mosaicButton);

  // --- run (progress and cancel: progress dialog of the background runner)
  this->ApplyButton = new QPushButton(q->tr("Apply"));
  this->ApplyButton->setObjectName("ApplyButton");
  this->verticalLayout->addWidget(this->ApplyButton);
  this->verticalLayout->addStretch(1);

  // --- connections
  for (qMRMLNodeComboBox* combo : {this->ParametersNodeComboBox, this->OutputVolumeNodeSelector,
                                   this->ReferenceVolumeNodeSelector})
    {
    QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)), combo, SLOT(setMRMLScene(vtkMRMLScene*)));
    }
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)), this->InputVolumesComboBox, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroReprojectParametersNode(vtkMRMLNode*)));
  QObject::connect(this->InputVolumesComboBox, SIGNAL(checkedNodesChanged()), q, SLOT(onInputVolumesChanged()));
  QObject::connect(this->ApplyButton, SIGNAL(clicked()), q, SLOT(onApply()));

  // widgets -> parameters node
  std::function<bool()> ready = [this]() { return this->parametersNode && !this->updatingWidgets; };
  QObject::connect(this->OutputVolumeNodeSelector, &qMRMLNodeComboBox::currentNodeIDChanged, q, [this, ready](const QString& id)
    { if (ready()) { this->parametersNode->SetOutputVolumeNodeID(id.toUtf8().constData()); } });
  QObject::connect(this->ReferenceVolumeNodeSelector, &qMRMLNodeComboBox::currentNodeIDChanged, q, [this, ready](const QString& id)
    { if (ready()) { this->parametersNode->SetReferenceVolumeNodeID(id.toUtf8().constData()); } });
  typedef void (Parameters::*IntSetter)(int);
  typedef void (Parameters::*DoubleSetter)(double);
  typedef void (Parameters::*StringSetter)(const char*);
  std::function<void(QComboBox*, IntSetter)> connectIndex = [this, q, ready](QComboBox* combo, IntSetter setter)
    {
    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this, ready, combo, setter](int)
      { if (ready()) { (this->parametersNode.GetPointer()->*setter)(combo->currentData().toInt()); } });
    };
  std::function<void(QComboBox*, StringSetter)> connectString = [this, q, ready](QComboBox* combo, StringSetter setter)
    {
    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this, ready, combo, setter](int)
      { if (ready()) { (this->parametersNode.GetPointer()->*setter)(combo->currentData().toString().toUtf8().constData()); } });
    };
  std::function<void(QDoubleSpinBox*, DoubleSetter, std::function<double()>)> connectDouble =
    [this, q, ready](QDoubleSpinBox* box, DoubleSetter setter, std::function<double()> scale)
    {
    QObject::connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), q, [this, ready, setter, scale](double value)
      { if (ready()) { (this->parametersNode.GetPointer()->*setter)(value * (scale ? scale() : 1.)); } });
    };
  std::function<void(QSpinBox*, IntSetter)> connectInt = [this, q, ready](QSpinBox* box, IntSetter setter)
    {
    QObject::connect(box, QOverload<int>::of(&QSpinBox::valueChanged), q, [this, ready, setter](int value)
      { if (ready()) { (this->parametersNode.GetPointer()->*setter)(value); } });
    };
  std::function<void(QCheckBox*, IntSetter)> connectCheck = [this, q, ready](QCheckBox* box, IntSetter setter)
    {
    QObject::connect(box, &QCheckBox::toggled, q, [this, ready, setter](bool checked)
      { if (ready()) { (this->parametersNode.GetPointer()->*setter)(checked ? 1 : 0); } });
    };
  // SI value per UI unit of the spectral fields
  std::function<double()> spectralScale = [this]()
    {
    double scale = 1.;
    QString unit;
    SpectralUnit(this->SpectralTypeComboBox->currentData().toString(), scale, unit);
    return scale;
    };
  connectIndex(this->SpatialGridModeComboBox, &Parameters::SetSpatialGridMode);
  connectDouble(this->SpatialResampleFactorSpinBox, &Parameters::SetSpatialResampleFactor, nullptr);
  connectString(this->CelestialFrameComboBox, &Parameters::SetCelestialFrame);
  connectDouble(this->EquinoxSpinBox, &Parameters::SetEquinox, nullptr);
  connectString(this->ProjectionComboBox, &Parameters::SetProjection);
  connectDouble(this->CenterLongitudeSpinBox, &Parameters::SetCenterLongitude, nullptr);
  connectDouble(this->CenterLatitudeSpinBox, &Parameters::SetCenterLatitude, nullptr);
  connectDouble(this->PixelSizeXSpinBox, &Parameters::SetPixelSizeX, nullptr);
  connectDouble(this->PixelSizeYSpinBox, &Parameters::SetPixelSizeY, nullptr);
  connectDouble(this->RotationSpinBox, &Parameters::SetRotation, nullptr);
  connectInt(this->SizeXSpinBox, &Parameters::SetSizeX);
  connectInt(this->SizeYSpinBox, &Parameters::SetSizeY);
  connectIndex(this->SpectralGridModeComboBox, &Parameters::SetSpectralGridMode);
  // switching to a manual grid: the fields start from the grid of the first input
  QObject::connect(this->SpatialGridModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), q,
    [this, ready](int)
    {
    if (ready() && this->parametersNode->GetSpatialGridMode() == Parameters::SpatialManual)
      {
      this->logic()->SetManualGridFromInput(this->parametersNode, true, false, false);
      }
    });
  QObject::connect(this->SpectralGridModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), q,
    [this, ready](int)
    {
    if (ready() && this->parametersNode->GetSpectralGridMode() == Parameters::SpectralManual)
      {
      this->logic()->SetManualGridFromInput(this->parametersNode, false, true, false);
      }
    });
  connectDouble(this->SpectralResampleFactorSpinBox, &Parameters::SetSpectralResampleFactor, nullptr);
  connectString(this->SpectralTypeComboBox, &Parameters::SetSpectralType);
  connectString(this->SpectralFrameComboBox, &Parameters::SetSpectralFrame);
  connectDouble(this->RestFrequencySpinBox, &Parameters::SetRestFrequency, []() { return 1.e6; });
  connectDouble(this->SpectralStartSpinBox, &Parameters::SetSpectralStart, spectralScale);
  connectDouble(this->SpectralStepSpinBox, &Parameters::SetSpectralStep, spectralScale);
  connectInt(this->SpectralCountSpinBox, &Parameters::SetSpectralCount);
  connectIndex(this->SpectralMatchingComboBox, &Parameters::SetSpectralMatching);
  connectIndex(this->SpatialMethodComboBox, &Parameters::SetInterpolationOrder);
  connectInt(this->SupersamplingSpinBox, &Parameters::SetSupersampling);
  connectIndex(this->SpectralMethodComboBox, &Parameters::SetSpectralMethod);
  connectDouble(this->MinimumCoverageSpinBox, &Parameters::SetMinimumCoverage, nullptr);
  connectIndex(this->ResolutionModeComboBox, &Parameters::SetResolutionMode);
  connectDouble(this->TargetBeamMajorSpinBox, &Parameters::SetTargetBeamMajor, nullptr);
  connectDouble(this->TargetBeamMinorSpinBox, &Parameters::SetTargetBeamMinor, nullptr);
  connectDouble(this->TargetBeamPASpinBox, &Parameters::SetTargetBeamPA, nullptr);
  connectIndex(this->CombinationComboBox, &Parameters::SetCombination);
  connectIndex(this->InputCorrectionComboBox, &Parameters::SetInputCorrection);
  connectIndex(this->PrimaryBeamModelComboBox, &Parameters::SetPrimaryBeamModel);
  connectDouble(this->DishDiameterSpinBox, &Parameters::SetDishDiameter, nullptr);
  connectDouble(this->PrimaryBeamFactorSpinBox, &Parameters::SetPrimaryBeamFactor, nullptr);
  connectDouble(this->PrimaryBeamCutoffSpinBox, &Parameters::SetPrimaryBeamCutoff, nullptr);
  connectCheck(this->NoiseWeightingCheckBox, &Parameters::SetNoiseWeighting);
  connectCheck(this->ComputeWeightCheckBox, &Parameters::SetComputeWeight);
  connectCheck(this->ComputeSensitivityCheckBox, &Parameters::SetComputeSensitivity);
  connectCheck(this->SaultImageCheckBox, &Parameters::SetSaultImage);
  connectDouble(this->SaultFractionSpinBox, &Parameters::SetSaultFraction, nullptr);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::updateVisibility()
{
  if (!this->parametersNode)
    {
    return;
    }
  const int spatial = this->parametersNode->GetSpatialGridMode();
  const int spectral = this->parametersNode->GetSpectralGridMode();
  const bool manual = spatial == Parameters::SpatialManual;
  const bool fitted = spatial == Parameters::SpatialUnion || spatial == Parameters::SpatialIntersection;
  QFormLayout* grid = this->GridLayout;
  this->setRowVisible(grid, this->ReferenceVolumeNodeSelector,
                      spatial == Parameters::SpatialReference || spectral == Parameters::SpectralReference);
  this->setRowVisible(grid, this->SpatialResampleFactorSpinBox, spatial == Parameters::SpatialResample);
  const std::string frame = this->parametersNode->GetCelestialFrame();
  for (QWidget* field : std::vector<QWidget*>{this->CelestialFrameComboBox, this->ProjectionComboBox,
                                              this->PixelSizeXSpinBox, this->RotationSpinBox})
    {
    this->setRowVisible(grid, field, manual || fitted);
    }
  this->setRowVisible(grid, this->EquinoxSpinBox, (manual || fitted) && (frame == "FK5" || frame == "FK4"));
  for (QWidget* field : std::vector<QWidget*>{this->CenterLongitudeSpinBox, this->CenterLatitudeSpinBox,
                                              this->PixelSizeYSpinBox, this->SizeXSpinBox, this->SizeYSpinBox,
                                              this->ManualGridButtons})
    {
    this->setRowVisible(grid, field, manual);
    }
  this->PixelSizeXSpinBox->setMinimum(manual ? 1.e-4 : 0.);
  const bool spectralFitted = spectral == Parameters::SpectralUnion || spectral == Parameters::SpectralIntersection;
  const bool spectralManual = spectral == Parameters::SpectralManual;
  this->setRowVisible(grid, this->SpectralResampleFactorSpinBox, spectral == Parameters::SpectralResample);
  for (QWidget* field : std::vector<QWidget*>{this->SpectralTypeComboBox, this->SpectralFrameComboBox,
                                              this->RestFrequencySpinBox, this->SpectralStepSpinBox})
    {
    this->setRowVisible(grid, field, spectralFitted || spectralManual);
    }
  for (QWidget* field : std::vector<QWidget*>{this->SpectralStartSpinBox, this->SpectralCountSpinBox,
                                              this->SpectralFromInputButton})
    {
    this->setRowVisible(grid, field, spectralManual);
    }
  this->SpectralStepSpinBox->setSpecialValueText(spectralFitted ? QString("narrowest of the inputs") : QString());
  this->setRowVisible(this->MethodLayout, this->SupersamplingSpinBox,
                      this->parametersNode->GetInterpolationOrder() == Parameters::AreaAverage);
  const int resolution = this->parametersNode->GetResolutionMode();
  this->setRowVisible(this->ResolutionLayout, this->CommonBeamLabel, resolution == Parameters::ResolutionCommonBeam);
  for (QWidget* field : std::vector<QWidget*>{this->TargetBeamMajorSpinBox, this->TargetBeamMinorSpinBox,
                                              this->TargetBeamPASpinBox})
    {
    this->setRowVisible(this->ResolutionLayout, field, resolution == Parameters::ResolutionTarget);
    }
  const int model = this->parametersNode->GetPrimaryBeamModel();
  const bool combined = this->parametersNode->GetCombination() != 0;
  this->setRowVisible(this->MosaicLayout, this->InputCorrectionComboBox, combined);
  this->setRowVisible(this->MosaicLayout, this->PrimaryBeamModelComboBox, combined);
  this->setRowVisible(this->MosaicLayout, this->DishDiameterSpinBox, combined && (model == 1 || model == 2));
  this->setRowVisible(this->MosaicLayout, this->PrimaryBeamFactorSpinBox, combined && model == 1);
  this->setRowVisible(this->MosaicLayout, this->PrimaryBeamCutoffSpinBox, combined && model != 0);
  this->setRowVisible(this->MosaicLayout, this->SaultFractionSpinBox, this->parametersNode->GetSaultImage() != 0);
  this->InputsTable->setColumnHidden(2, !(combined && model == 3));
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::updateInputsTable()
{
  Q_Q(qSlicerAstroReprojectModuleWidget);
  if (!this->parametersNode || !this->logic())
    {
    return;
    }
  const std::vector<vtkMRMLAstroVolumeNode*> volumes = this->logic()->GetInputVolumes(this->parametersNode);
  const std::vector<std::string> noises = SplitList(this->parametersNode->GetInputNoises());
  const std::vector<std::string> beams = SplitList(this->parametersNode->GetPrimaryBeamVolumeNodeIDs());
  this->InputsTable->setRowCount(static_cast<int>(volumes.size()));
  for (int row = 0; row < static_cast<int>(volumes.size()); row++)
    {
    QTableWidgetItem* name = new QTableWidgetItem(volumes[row]->GetName());
    name->setFlags(Qt::ItemIsEnabled);
    this->InputsTable->setItem(row, 0, name);
    QDoubleSpinBox* noise = qobject_cast<QDoubleSpinBox*>(this->InputsTable->cellWidget(row, 1));
    if (!noise)
      {
      noise = new QDoubleSpinBox;
      noise->setDecimals(8);
      noise->setRange(0., 1.e12);
      noise->setSpecialValueText(q->tr("auto"));
      noise->setKeyboardTracking(false);
      this->InputsTable->setCellWidget(row, 1, noise);
      QObject::connect(noise, SIGNAL(valueChanged(double)), q, SLOT(onInputsTableChanged()));
      }
    {
    const QSignalBlocker blocker(noise);
    noise->setValue(row < static_cast<int>(noises.size()) && !noises[row].empty() ? std::atof(noises[row].c_str()) : 0.);
    }
    qMRMLNodeComboBox* beam = qobject_cast<qMRMLNodeComboBox*>(this->InputsTable->cellWidget(row, 2));
    if (!beam)
      {
      beam = new qMRMLNodeComboBox;
      beam->setNodeTypes(QStringList() << "vtkMRMLAstroVolumeNode");
      beam->setNoneEnabled(true);
      beam->setAddEnabled(false);
      beam->setRemoveEnabled(false);
      beam->setMRMLScene(q->mrmlScene());
      this->InputsTable->setCellWidget(row, 2, beam);
      QObject::connect(beam, SIGNAL(currentNodeChanged(vtkMRMLNode*)), q, SLOT(onInputsTableChanged()));
      }
    {
    const QSignalBlocker blocker(beam);
    beam->setCurrentNodeID(row < static_cast<int>(beams.size()) ? QString::fromStdString(beams[row]) : QString());
    }
    }
  this->InputsTable->setVisible(volumes.size() > 1 || this->parametersNode->GetPrimaryBeamModel() == 3);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::updateSummary()
{
  Q_Q(qSlicerAstroReprojectModuleWidget);
  if (!this->parametersNode || !this->logic())
    {
    return;
    }
  this->GridSummaryLabel->setText(QString::fromStdString(this->logic()->DescribeOutputGrid(this->parametersNode)));
  if (this->parametersNode->GetResolutionMode() == Parameters::ResolutionCommonBeam)
    {
    double beam[3];
    if (this->logic()->GetCommonBeam(this->parametersNode, beam))
      {
      this->CommonBeamLabel->setText(q->tr("%1\" x %2\", %3 deg").arg(beam[0], 0, 'f', 2).arg(beam[1], 0, 'f', 2)
                                     .arg(beam[2], 0, 'f', 1));
      }
    else
      {
      this->CommonBeamLabel->setText(QString::fromStdString(this->logic()->GetLastError()));
      }
    }
}

//-----------------------------------------------------------------------------
qSlicerAstroReprojectModuleWidget::qSlicerAstroReprojectModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroReprojectModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroReprojectModuleWidget::~qSlicerAstroReprojectModuleWidget()
{
}

//----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::enter()
{
  this->Superclass::enter();
  this->onMRMLAstroReprojectParametersNodeModified();
}

//----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::exit()
{
  this->Superclass::exit();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (!scene)
    {
    return;
    }
  this->Superclass::setMRMLScene(scene);
  vtkSlicerApplicationLogic* appLogic = this->module()->appLogic();
  d->selectionNode = appLogic ? appLogic->GetSelectionNode() : nullptr;
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroReprojectModuleWidget::setMRMLScene : selectionNode not found!";
    return;
    }
  this->initializeNodes();
  this->qvtkReconnect(scene, vtkMRMLScene::EndCloseEvent, this, SLOT(onEndCloseEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::StartImportEvent, this, SLOT(onStartImportEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent, this, SLOT(onEndImportEvent()));
  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent, this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
  this->onMRMLSelectionNodeModified(d->selectionNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::initializeNodes(bool forceNew /*= false*/)
{
  this->initializeParameterNode(forceNew);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onEndCloseEvent()
{
  this->initializeNodes(true);
  this->onMRMLAstroReprojectParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onEndImportEvent()
{
  this->initializeNodes();
  this->onMRMLAstroReprojectParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onStartImportEvent()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (!this->mrmlScene() || !d->selectionNode)
    {
    return;
    }
  vtkMRMLAstroReprojectParametersNode* astroParametersNode = nullptr;
  const int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroReprojectParametersNode");
  if (numNodes > 0 && !forceNew)
    {
    astroParametersNode = vtkMRMLAstroReprojectParametersNode::SafeDownCast(
      this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroReprojectParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> parametersNode;
    parametersNode.TakeReference(this->mrmlScene()->CreateNodeByClass("vtkMRMLAstroReprojectParametersNode"));
    this->mrmlScene()->AddNode(parametersNode);
    astroParametersNode = vtkMRMLAstroReprojectParametersNode::SafeDownCast(parametersNode);
    astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
    astroParametersNode->SetInputVolumeNodeIDs(d->selectionNode->GetActiveVolumeID() ? d->selectionNode->GetActiveVolumeID() : "");
    }
  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onMRMLSelectionNodeModified(vtkObject* sender)
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  vtkMRMLSelectionNode* selectionNode = vtkMRMLSelectionNode::SafeDownCast(sender);
  if (!selectionNode || !d->parametersNode || !selectionNode->GetActiveVolumeID())
    {
    return;
    }
  // the active volume is the input when no inputs are chosen
  if (std::string(d->parametersNode->GetInputVolumeNodeIDs()).empty())
    {
    d->parametersNode->SetInputVolumeNodeID(selectionNode->GetActiveVolumeID());
    d->parametersNode->SetInputVolumeNodeIDs(selectionNode->GetActiveVolumeID());
    }
}

//--------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setMRMLAstroReprojectParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  vtkMRMLAstroReprojectParametersNode* node = vtkMRMLAstroReprojectParametersNode::SafeDownCast(mrmlNode);
  this->qvtkReconnect(d->parametersNode, node, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroReprojectParametersNodeModified()));
  d->parametersNode = node;
  this->onMRMLAstroReprojectParametersNodeModified();
}

//--------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onMRMLAstroReprojectParametersNodeModified()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }
  Parameters* p = d->parametersNode;
  d->updatingWidgets = true;
  {
  // inputs
  const QSignalBlocker inputsBlocker(d->InputVolumesComboBox);
  const std::vector<std::string> ids = SplitList(p->GetInputVolumeNodeIDs());
  for (vtkMRMLNode* node : d->InputVolumesComboBox->nodes())
    {
    const bool checked = std::find(ids.begin(), ids.end(), std::string(node->GetID())) != ids.end();
    d->InputVolumesComboBox->setCheckState(node, checked ? Qt::Checked : Qt::Unchecked);
    }
  }
  {
  const QSignalBlocker blocker(d->OutputVolumeNodeSelector);
  d->OutputVolumeNodeSelector->setCurrentNodeID(p->GetOutputVolumeNodeID() ? p->GetOutputVolumeNodeID() : "");
  }
  {
  const QSignalBlocker blocker(d->ReferenceVolumeNodeSelector);
  d->ReferenceVolumeNodeSelector->setCurrentNodeID(p->GetReferenceVolumeNodeID() ? p->GetReferenceVolumeNodeID() : "");
  }
  std::function<void(QComboBox*, int)> setIndex = [](QComboBox* combo, int value)
    {
    const QSignalBlocker blocker(combo);
    const int index = combo->findData(value);
    combo->setCurrentIndex(index >= 0 ? index : 0);
    };
  std::function<void(QComboBox*, const char*)> setText = [](QComboBox* combo, const char* value)
    {
    const QSignalBlocker blocker(combo);
    SetComboData(combo, QString(value ? value : ""));
    };
  std::function<void(QDoubleSpinBox*, double)> setDouble = [](QDoubleSpinBox* box, double value)
    {
    const QSignalBlocker blocker(box);
    box->setValue(value);
    };
  std::function<void(QSpinBox*, int)> setInt = [](QSpinBox* box, int value)
    {
    const QSignalBlocker blocker(box);
    box->setValue(value);
    };
  std::function<void(QCheckBox*, bool)> setChecked = [](QCheckBox* box, bool value)
    {
    const QSignalBlocker blocker(box);
    box->setChecked(value);
    };
  setIndex(d->SpatialGridModeComboBox, p->GetSpatialGridMode());
  setDouble(d->SpatialResampleFactorSpinBox, p->GetSpatialResampleFactor());
  setText(d->CelestialFrameComboBox, p->GetCelestialFrame());
  setDouble(d->EquinoxSpinBox, p->GetEquinox());
  setText(d->ProjectionComboBox, p->GetProjection());
  setDouble(d->CenterLongitudeSpinBox, p->GetCenterLongitude());
  setDouble(d->CenterLatitudeSpinBox, p->GetCenterLatitude());
  setDouble(d->PixelSizeXSpinBox, p->GetPixelSizeX());
  setDouble(d->PixelSizeYSpinBox, p->GetPixelSizeY());
  setDouble(d->RotationSpinBox, p->GetRotation());
  setInt(d->SizeXSpinBox, p->GetSizeX());
  setInt(d->SizeYSpinBox, p->GetSizeY());
  setIndex(d->SpectralGridModeComboBox, p->GetSpectralGridMode());
  setDouble(d->SpectralResampleFactorSpinBox, p->GetSpectralResampleFactor());
  setText(d->SpectralTypeComboBox, p->GetSpectralType());
  setText(d->SpectralFrameComboBox, p->GetSpectralFrame());
  setDouble(d->RestFrequencySpinBox, p->GetRestFrequency() / 1.e6);
  double scale = 1.;
  QString unit;
  SpectralUnit(QString(p->GetSpectralType()), scale, unit);
  d->SpectralStartSpinBox->setSuffix(unit);
  d->SpectralStepSpinBox->setSuffix(unit);
  setDouble(d->SpectralStartSpinBox, p->GetSpectralStart() / scale);
  setDouble(d->SpectralStepSpinBox, p->GetSpectralStep() / scale);
  setInt(d->SpectralCountSpinBox, p->GetSpectralCount());
  setIndex(d->SpectralMatchingComboBox, p->GetSpectralMatching());
  setIndex(d->SpatialMethodComboBox, p->GetInterpolationOrder());
  setInt(d->SupersamplingSpinBox, p->GetSupersampling());
  setIndex(d->SpectralMethodComboBox, p->GetSpectralMethod());
  setDouble(d->MinimumCoverageSpinBox, p->GetMinimumCoverage());
  setIndex(d->ResolutionModeComboBox, p->GetResolutionMode());
  setDouble(d->TargetBeamMajorSpinBox, p->GetTargetBeamMajor());
  setDouble(d->TargetBeamMinorSpinBox, p->GetTargetBeamMinor());
  setDouble(d->TargetBeamPASpinBox, p->GetTargetBeamPA());
  setIndex(d->CombinationComboBox, p->GetCombination());
  setIndex(d->InputCorrectionComboBox, p->GetInputCorrection());
  setIndex(d->PrimaryBeamModelComboBox, p->GetPrimaryBeamModel());
  setDouble(d->DishDiameterSpinBox, p->GetDishDiameter());
  setDouble(d->PrimaryBeamFactorSpinBox, p->GetPrimaryBeamFactor());
  setDouble(d->PrimaryBeamCutoffSpinBox, p->GetPrimaryBeamCutoff());
  setChecked(d->NoiseWeightingCheckBox, p->GetNoiseWeighting() != 0);
  setChecked(d->ComputeWeightCheckBox, p->GetComputeWeight() != 0);
  setChecked(d->ComputeSensitivityCheckBox, p->GetComputeSensitivity() != 0);
  setChecked(d->SaultImageCheckBox, p->GetSaultImage() != 0);
  setDouble(d->SaultFractionSpinBox, p->GetSaultFraction());
  d->updateInputsTable();
  d->updateExplanations();
  d->updateVisibility();
  d->updatingWidgets = false;
  d->updateSummary();
  const bool running = p->GetStatus() != 0;
  d->ApplyButton->setEnabled(!running && !d->logic()->GetInputVolumes(p).empty());
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onInputVolumesChanged()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (!d->parametersNode || d->updatingWidgets)
    {
    return;
    }
  // the order of the previous inputs is kept, new inputs at the end
  std::vector<std::string> ids = SplitList(d->parametersNode->GetInputVolumeNodeIDs());
  std::vector<std::string> checked;
  for (vtkMRMLNode* node : d->InputVolumesComboBox->checkedNodes())
    {
    checked.push_back(node->GetID());
    }
  std::vector<std::string> result;
  for (const std::string& id : ids)
    {
    if (std::find(checked.begin(), checked.end(), id) != checked.end())
      {
      result.push_back(id);
      }
    }
  for (const std::string& id : checked)
    {
    if (std::find(result.begin(), result.end(), id) == result.end())
      {
      result.push_back(id);
      }
    }
  std::string joined;
  for (const std::string& id : result)
    {
    joined += (joined.empty() ? "" : ";") + id;
    }
  const int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetInputVolumeNodeIDs(joined.c_str());
  d->parametersNode->SetInputVolumeNodeID(result.empty() ? nullptr : result[0].c_str());
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onInputsTableChanged()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (!d->parametersNode || d->updatingWidgets)
    {
    return;
    }
  std::string noises, beams;
  for (int row = 0; row < d->InputsTable->rowCount(); row++)
    {
    QDoubleSpinBox* noise = qobject_cast<QDoubleSpinBox*>(d->InputsTable->cellWidget(row, 1));
    qMRMLNodeComboBox* beam = qobject_cast<qMRMLNodeComboBox*>(d->InputsTable->cellWidget(row, 2));
    std::ostringstream value;
    value.precision(10);
    if (noise && noise->value() > 0.)
      {
      value << noise->value();
      }
    noises += (row ? ";" : "") + value.str();
    beams += (row ? ";" : "") + std::string(beam && beam->currentNode() ? beam->currentNode()->GetID() : "");
    }
  const int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetInputNoises(noises.c_str());
  d->parametersNode->SetPrimaryBeamVolumeNodeIDs(beams.c_str());
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setManualGridFromInput()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (d->parametersNode && !d->logic()->SetManualGridFromInput(d->parametersNode, true, false, false))
    {
    qSlicerAstroVolumeModuleWidget::informUser(this, QMessageBox::Warning, tr("Reprojection"),
                                               QString::fromStdString(d->logic()->GetLastError()));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setManualGridFromInputNorthUp()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (d->parametersNode && !d->logic()->SetManualGridFromInput(d->parametersNode, true, false, true))
    {
    qSlicerAstroVolumeModuleWidget::informUser(this, QMessageBox::Warning, tr("Reprojection"),
                                               QString::fromStdString(d->logic()->GetLastError()));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setManualSpectralGridFromInput()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (d->parametersNode && !d->logic()->SetManualGridFromInput(d->parametersNode, false, true, false))
    {
    qSlicerAstroVolumeModuleWidget::informUser(this, QMessageBox::Warning, tr("Reprojection"),
                                               QString::fromStdString(d->logic()->GetLastError()));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setTargetBeamFromCommonBeam()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  double beam[3];
  if (!d->parametersNode)
    {
    return;
    }
  if (!d->logic()->GetCommonBeam(d->parametersNode, beam))
    {
    qSlicerAstroVolumeModuleWidget::informUser(this, QMessageBox::Warning, tr("Reprojection"),
                                               QString::fromStdString(d->logic()->GetLastError()));
    return;
    }
  const int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetTargetBeamMajor(beam[0]);
  d->parametersNode->SetTargetBeamMinor(beam[1]);
  d->parametersNode->SetTargetBeamPA(beam[2]);
  d->parametersNode->SetResolutionMode(Parameters::ResolutionTarget);
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onApply()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (!d->parametersNode || !d->logic())
    {
    return;
    }
  const QString title = tr("Reprojection");
  d->ApplyButton->setEnabled(false);
  const bool success = d->logic()->Reproject(d->parametersNode);
  d->ApplyButton->setEnabled(true);
  const QString warnings = QString::fromStdString(d->logic()->GetLastWarnings()).trimmed();
  if (!success)
    {
    const QString error = QString::fromStdString(d->logic()->GetLastError());
    if (error != "cancelled")
      {
      qSlicerAstroVolumeModuleWidget::informUser(this, QMessageBox::Warning, title,
        error + (warnings.isEmpty() ? QString() : "\n\n" + warnings));
      }
    return;
    }
  if (!warnings.isEmpty())
    {
    qSlicerAstroVolumeModuleWidget::informUser(this, QMessageBox::Information, title, warnings);
    }
  const std::vector<vtkMRMLAstroVolumeNode*> inputs = d->logic()->GetInputVolumes(d->parametersNode);
  const char* outputID = d->parametersNode->GetOutputVolumeNodeID();
  if (d->astroVolumeWidget && !inputs.empty() && outputID)
    {
    d->astroVolumeWidget->setThreeComparativeView(inputs[0]->GetID(), outputID, outputID);
    }
}

//---------------------------------------------------------------------------
vtkMRMLAstroReprojectParametersNode* qSlicerAstroReprojectModuleWidget::mrmlAstroReprojectParametersNode()const
{
  Q_D(const qSlicerAstroReprojectModuleWidget);
  return d->parametersNode;
}
