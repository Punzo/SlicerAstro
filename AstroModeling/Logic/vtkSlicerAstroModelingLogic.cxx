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

// Logic includes
#include "vtkSlicerAstroVolumeLogic.h"
#include "vtkSlicerAstroModelingLogic.h"
#include "vtkSlicerAstroConfigure.h"
#include "vtkSlicerMarkupsLogic.h"

//3DBarolo includes
#include <Arrays/param.hh>
#include <Arrays/cube.hh>
#include <Arrays/header.hh>
#include <Tasks/galfit.hh>
#include <Utilities/fatal.hh>

// STD includes
#include <memory>
#include <algorithm>
#include <vector>
#include <sstream>
#include <atomic>
#include <mutex>
#include <streambuf>

// Errors and warnings of the operations: in the application log and in the
// output of the operations (shown by the Modeling module)
#define vtkModelingErrorMacro(x)                                    \
  {                                                                 \
  std::ostringstream modelingMessage;                               \
  modelingMessage << x;                                             \
  this->AppendLog("Error: " + modelingMessage.str() + "\n");        \
  vtkErrorMacro(<< modelingMessage.str());                          \
  }
#define vtkModelingWarningMacro(x)                                  \
  {                                                                 \
  std::ostringstream modelingMessage;                               \
  modelingMessage << x;                                             \
  this->AppendLog("Warning: " + modelingMessage.str() + "\n");      \
  vtkWarningMacro(<< modelingMessage.str());                        \
  }

// MRML includes
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroModelingParametersNode.h>
#include <vtkMRMLTableNode.h>

// VTK includes
#include <vtkCacheManager.h>
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkTable.h>
#include <vtkVersion.h>

// STD includes
#include <cassert>
#include <cstdlib>
#include <iostream>

//----------------------------------------------------------------------------
class vtkSlicerAstroModelingLogic::vtkInternal
{
public:
  std::mutex LogMutex;
  std::string Log;
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkSlicerMarkupsLogic> MarkupsLogic;
  Param *par;
  Header *head;
  Cube<float> *cubeF;
  Cube<double> *cubeD;
  Model::Galmod<float> *modF;
  Model::Galmod<double> *modD;
  Model::Galfit<float> *fitF;
  Model::Galfit<double> *fitD;
  /// Abort requested (RequestAbort, or status -1 of the parameter node)
  std::atomic<bool> AbortRequested{false};
};

//----------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = nullptr;
  this->MarkupsLogic = nullptr;
  this->par = nullptr;
  this->head = nullptr;
  this->cubeF = nullptr;
  this->cubeD = nullptr;
  this->modF = nullptr;
  this->modD = nullptr;
  this->fitF = nullptr;
  this->fitD = nullptr;
}

//---------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::vtkInternal::~vtkInternal()
{
  if (head != nullptr)
    {
    delete head;
    }
  head = nullptr;

  if (par != nullptr)
    {
    delete par;
    }
  par = nullptr;

  if (cubeF != nullptr)
    {
    delete cubeF;
    }
  cubeF = nullptr;

  if (cubeD != nullptr)
    {
    delete cubeD;
    }
  cubeD = nullptr;

  if (modF != nullptr)
    {
    delete modF;
    }
  modF = nullptr;

  if (modD != nullptr)
    {
    delete modD;
    }
  modD = nullptr;

  if (fitF != nullptr)
    {
    delete fitF;
    }
  fitF = nullptr;

  if (fitD != nullptr)
    {
    delete fitD;
    }
  fitD = nullptr;
}


namespace
{
//----------------------------------------------------------------------------
// Run the BBarolo ring fitting (first or second stage). The SlicerAstro
// cancel request (parameters node status set to -1) is forwarded to BBarolo
// through its abort flag, checked after each fitted ring.
//----------------------------------------------------------------------------
// Numbers of a comma (or space) separated list
std::vector<double> ParseNumberList(const std::string& text)
{
  std::vector<double> values;
  std::string normalized(text);
  std::replace(normalized.begin(), normalized.end(), ',', ' ');
  std::stringstream stream(normalized);
  double value = 0.;
  while (stream >> value)
    {
    values.push_back(value);
    }
  return values;
}

//----------------------------------------------------------------------------
// Threads of 3DBarolo: all the cores (std::thread::hardware_concurrency) by
// default, as many as OMP_NUM_THREADS when it is set (e.g. to leave cores to
// other work, or for reproducible fits: 3DBarolo's fits depend slightly on
// the number of threads)
void ApplyThreadsFromEnvironment(Param* par)
{
  const char* value = std::getenv("OMP_NUM_THREADS");
  const int threads = value ? std::atoi(value) : 0;
  if (threads > 0)
    {
    par->setThreads(threads);
    }
}

//----------------------------------------------------------------------------
// Parameters of the fit, of the source search and of the emission lines
void ApplyBBaroloParameters(Param* par, vtkMRMLAstroModelingParametersNode* pnode)
{
  GALFIT_PAR& galfit = par->getParGF();
  galfit.TWOSTAGE = pnode->GetTwoStage();
  galfit.REGTYPE = pnode->GetRegularizationType().empty() ? std::string("auto") : pnode->GetRegularizationType();
  galfit.SIDE = pnode->GetSide().empty() ? std::string("B") : pnode->GetSide();
  galfit.DELTAVROT = pnode->GetRotationVelocityError();
  galfit.MINVDISP = pnode->GetMinimumVelocityDispersion();
  galfit.MAXVDISP = pnode->GetMaximumVelocityDispersion();
  galfit.flagERRORS = pnode->GetEstimateErrors();
  galfit.STARTRAD = pnode->GetStartRing();
  galfit.BWEIGHT = pnode->GetBlankWeight();
  galfit.NORMALCUBE = pnode->GetNormalizeCube();
  galfit.REDSHIFT = pnode->GetRedshift();
  const std::vector<double> restFrequencies = ParseNumberList(pnode->GetRestFrequencies());
  if (!restFrequencies.empty())
    {
    std::vector<double> intensities = ParseNumberList(pnode->GetRelativeIntensities());
    if (intensities.size() != restFrequencies.size())
      {
      intensities.assign(restFrequencies.size(), 1.);
      }
    galfit.RESTFREQ = restFrequencies;
    galfit.RELINT = intensities;
    }
  par->setLinear(pnode->GetInstrumentalBroadening());

  SEARCH_PAR& search = par->getParSE();
  search.snrCut = pnode->GetSearchSNRCut();
  search.UserThreshold = pnode->GetSearchThreshold() > 0.;
  if (search.UserThreshold)
    {
    search.threshold = pnode->GetSearchThreshold();
    }
  search.minChannels = pnode->GetSearchMinChannels();
  search.growthCut = pnode->GetSearchGrowthCut();
}

//----------------------------------------------------------------------------
// Median of the values of the rings (single value for the UI: robust to the
// rings poorly constrained by the data, e.g. the innermost ones)
template <typename T>
double MedianOfRings(const std::vector<T>& values)
{
  if (values.empty())
    {
    return 0.;
    }
  std::vector<T> sorted(values);
  std::sort(sorted.begin(), sorted.end());
  const size_t middle = sorted.size() / 2;
  return sorted.size() % 2 ? sorted[middle] : 0.5 * (sorted[middle - 1] + sorted[middle]);
}

//----------------------------------------------------------------------------
// 3DBarolo refines the initial rotation velocities ring by ring with a 2D
// tilted-ring fit of the velocity field, except the innermost ring (within
// about a beam of the centre), which keeps the global estimate of the flat
// part of the rotation curve. The innermost ring is extrapolated as a solid
// body rotation from the next ring (the model of Create is not fitted).
template <class T>
void ExtrapolateInnerRotationVelocity(Rings<T>* rings)
{
  if (!rings || rings->nr < 2 || rings->vrot.size() < 2 || rings->radii.size() < 2 ||
      rings->radii[1] <= 0)
    {
    return;
    }
  bool refined = false;
  for (int ii = 1; ii < rings->nr; ii++)
    {
    if (rings->vrot[ii] != rings->vrot[0])
      {
      refined = true;
      break;
      }
    }
  if (refined)
    {
    rings->vrot[0] = rings->vrot[1] * std::max(rings->radii[0], static_cast<T>(0)) / rings->radii[1];
    }
}

//----------------------------------------------------------------------------
// The 2D tilted-ring refinement of 3DBarolo can "succeed" on rings with very
// few pixels of the velocity field (e.g. an outermost ring beyond the gas)
// and give meaningless values (e.g. position angle 0, a few km/s of rotation),
// which the model of Create (no fit) would use. A refined ring whose position
// angle is far from the median, or whose rotation is not positive, keeps the
// values of the previous ring (as 3DBarolo does for the rings whose 2D fit
// fails).
template <class T>
void RejectOutlierInitialRings(Rings<T>* rings)
{
  if (!rings || rings->nr < 3 || rings->phi.size() < static_cast<size_t>(rings->nr) ||
      rings->vrot.size() < static_cast<size_t>(rings->nr))
    {
    return;
    }
  std::vector<T> phi(rings->phi.begin() + 1, rings->phi.begin() + rings->nr);
  std::sort(phi.begin(), phi.end());
  const double medianPhi = phi[phi.size() / 2];
  const double maximumPhiDifference = 30.;
  for (int ii = 1; ii < rings->nr; ii++)
    {
    const double phiDifference = fabs(remainder(rings->phi[ii] - medianPhi, 360.));
    if (phiDifference > maximumPhiDifference || !(rings->vrot[ii] > 0) || isNaN(rings->phi[ii]))
      {
      rings->phi[ii] = rings->phi[ii - 1];
      rings->vrot[ii] = rings->vrot[ii - 1];
      }
    }
}

//----------------------------------------------------------------------------
// Number of rings of a set of rings of 3DBarolo that have all their
// parameters (the vectors can be shorter than nr, e.g. when 3DBarolo drops
// the outermost ring): reading beyond them gives undefined values.
template <class T>
int NumberOfCompleteRings(Rings<T>* rings)
{
  if (!rings || rings->nr <= 0)
    {
    return 0;
    }
  size_t count = static_cast<size_t>(rings->nr);
  const std::vector<T>* parameters[] = {&rings->radii, &rings->xpos, &rings->ypos, &rings->vsys,
                                        &rings->vrot, &rings->vrad, &rings->vdisp, &rings->dens,
                                        &rings->z0, &rings->inc, &rings->phi};
  for (const std::vector<T>* parameter : parameters)
    {
    count = std::min(count, parameter->size());
    }
  return static_cast<int>(count);
}

//----------------------------------------------------------------------------
// Progress (status) of an operation: only increases, and a cancel request
// (status -1) is kept
void SetProgress(vtkMRMLAstroModelingParametersNode* pnode, int value)
{
  const int status = pnode ? pnode->GetStatus() : -1;
  if (status != -1 && value > status && value < 100)
    {
    pnode->SetStatus(value);
    }
}

//----------------------------------------------------------------------------
template <class T>
void RunGalfitStage(Model::Galfit<T>* fit, vtkMRMLAstroModelingParametersNode* pnode, bool secondStage,
                    int progressFrom, int progressTo, std::atomic<bool>& abortRequested)
{
  // progress of the fit: rings done / total rings in [progressFrom, progressTo];
  // the abort flag is checked by BBarolo at each iteration of the minimization
  fit->setAbortFlag(&abortRequested);
  fit->setProgressCallback([pnode, &abortRequested, progressFrom, progressTo](int done, int total)
    {
    if (pnode->GetStatus() == -1)
      {
      abortRequested = true;
      return;
      }
    if (total > 0)
      {
      SetProgress(pnode, progressFrom + ((progressTo - progressFrom) * done) / total);
      }
    });
  if (secondStage)
    {
    fit->SecondStage();
    }
  else
    {
    fit->galfit();
    }
  fit->setAbortFlag(nullptr);
  fit->setProgressCallback(nullptr);
}

//----------------------------------------------------------------------------
// Numbers (from 1) of the rings with a null column density, i.e. the rings
// whose fit did not converge (Galfit::galfit)
template <class T>
std::string UnconvergedRings(Rings<T>* rings, int numberOfRings)
{
  std::string numbers;
  if (!rings)
    {
    return numbers;
    }
  for (int ii = 0; ii < numberOfRings && ii < static_cast<int>(rings->dens.size()); ii++)
    {
    if (rings->dens[ii] == 0)
      {
      numbers += (numbers.empty() ? "" : ", ") + std::to_string(ii + 1);
      }
    }
  return numbers;
}

//----------------------------------------------------------------------------
// Initial value of a parameter for the UI: the value of the user, unless it
// is to be estimated (-1), or the one of the initial rings of 3DBarolo
double UserOrInitialValue(bool keepUserValues, double userValue, double initialValue)
{
  return keepUserValues && userValue > -1 ? userValue : initialValue;
}

//----------------------------------------------------------------------------
// Same, for the parameters that are always given (e.g. the radial velocity,
// negative for inflows)
double GivenOrInitialValue(bool keepUserValues, double userValue, double initialValue)
{
  return keepUserValues ? userValue : initialValue;
}

//----------------------------------------------------------------------------
// Value of a cell of the rings table, with full precision (the cell text has
// 6 significant digits)
double TableValue(vtkMRMLTableNode* tableNode, int row, int column)
{
  return tableNode->GetTable()->GetValue(row, column).ToDouble();
}

//----------------------------------------------------------------------------
// Final model of the output rings, with the progress (rings done, then the
// convolution with the beam) in [progressFrom, progressTo]
template <class T>
Model::Galmod<T>* BuildFinalModel(Model::Galfit<T>* fit, vtkMRMLAstroModelingParametersNode* pnode,
                                  int progressFrom, int progressTo)
{
  fit->setProgressCallback([pnode, progressFrom, progressTo](int done, int total)
    {
    if (total > 0)
      {
      SetProgress(pnode, progressFrom + ((progressTo - progressFrom) * done) / total);
      }
    });
  // profile, normalization (AZIM, LOCAL, NONE) and model as 3DBarolo's writeModel
  Model::Galmod<T>* model = fit->buildFinalModel(pnode->GetNormalize() ? pnode->GetNormalize() : "LOCAL");
  fit->setProgressCallback(nullptr);
  return model;
}
} // end of anonymous namespace

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroModelingLogic);

namespace
{
//----------------------------------------------------------------------------
template <typename T> T StringToNumber(const char* num)
{
  std::stringstream ss;
  ss << num;
  T result;
  return ss >> result ? result : 0;
}

//----------------------------------------------------------------------------
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
}

//----------------------------------------------------------------------------
long StringToLong(const char* str)
{
  return StringToNumber<long>(str);
}

//----------------------------------------------------------------------------
float StringToFloat(const char* str)
{
  return StringToNumber<float>(str);
}

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

//----------------------------------------------------------------------------
template <typename T> std::string NumberToString(T V)
{
  std::string stringValue;
  std::stringstream strstream;
  strstream << V;
  strstream >> stringValue;
  return stringValue;
}

//----------------------------------------------------------------------------
std::string IntToString(int Value)
{
  return NumberToString<int>(Value);
}

//----------------------------------------------------------------------------
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
}

//----------------------------------------------------------------------------
// Copy of what is written to a stream (std::cout, std::cerr) in the output of
// the operations of the logic; the text still goes to the original stream.
class LogStreamBuffer : public std::streambuf
{
public:
  LogStreamBuffer(std::streambuf* original, vtkSlicerAstroModelingLogic* logic)
    : Original(original), Logic(logic)
    {
    }

protected:
  int overflow(int c) override
    {
    if (c == traits_type::eof())
      {
      return traits_type::not_eof(c);
      }
    const char character = traits_type::to_char_type(c);
    this->Logic->AppendLog(std::string(1, character));
    return this->Original ? this->Original->sputc(character) : c;
    }
  std::streamsize xsputn(const char* text, std::streamsize count) override
    {
    this->Logic->AppendLog(std::string(text, static_cast<size_t>(count)));
    return this->Original ? this->Original->sputn(text, count) : count;
    }
  int sync() override
    {
    return this->Original ? this->Original->pubsync() : 0;
    }

private:
  std::streambuf* Original;
  vtkSlicerAstroModelingLogic* Logic;
};

//----------------------------------------------------------------------------
// What 3DBarolo prints during an operation goes also to the output of the
// operations (restored at the end of the operation, whatever its outcome)
class BBaroloOutputCapture
{
public:
  explicit BBaroloOutputCapture(vtkSlicerAstroModelingLogic* logic)
    : OutputBuffer(std::cout.rdbuf(), logic),
      ErrorBuffer(std::cerr.rdbuf(), logic)
    {
    this->OriginalOutput = std::cout.rdbuf(&this->OutputBuffer);
    this->OriginalError = std::cerr.rdbuf(&this->ErrorBuffer);
    }
  ~BBaroloOutputCapture()
    {
    std::cout.flush();
    std::cerr.flush();
    std::cout.rdbuf(this->OriginalOutput);
    std::cerr.rdbuf(this->OriginalError);
    }
  BBaroloOutputCapture(const BBaroloOutputCapture&) = delete;
  BBaroloOutputCapture& operator=(const BBaroloOutputCapture&) = delete;

private:
  LogStreamBuffer OutputBuffer;
  LogStreamBuffer ErrorBuffer;
  std::streambuf* OriginalOutput = nullptr;
  std::streambuf* OriginalError = nullptr;
};

//----------------------------------------------------------------------------
// 3DBarolo works with the optical velocity definition: the definition of the
// input volume (e.g. radio for HI data) is restored at the end of the
// operation, whatever its outcome.
class VelocityDefinitionGuard
{
public:
  explicit VelocityDefinitionGuard(vtkMRMLAstroVolumeDisplayNode* displayNode)
    : DisplayNode(displayNode),
      Radio(displayNode && !strncmp(displayNode->GetVelocityDefinition().c_str(), "VRAD", 4))
    {
    }
  ~VelocityDefinitionGuard()
    {
    if (this->DisplayNode && this->Radio)
      {
      this->DisplayNode->SetRadioVelocityDefinition(false);
      }
    }
private:
  vtkMRMLAstroVolumeDisplayNode* DisplayNode;
  const bool Radio;
};

} // end namespace

//----------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::vtkSlicerAstroModelingLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::~vtkSlicerAstroModelingLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
double vtkSlicerAstroModelingLogic::CalculateCentralVelocity(vtkMRMLAstroVolumeDisplayNode *volumeDisplayNode)
{
  if (!volumeDisplayNode)
    {
    return 0.;
    }

  vtkMRMLAstroVolumeNode* inputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
    (volumeDisplayNode->GetVolumeNode());
  if (!inputVolume)
    {
    return 0.;
    }

  double world [] = {0., 0., 0.}, ijk [] = {0., 0., 0.};
  ijk[0] = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1")) * 0.5;
  ijk[1] = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2")) * 0.5;
  ijk[2] = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS3")) * 0.5;
  volumeDisplayNode->GetReferenceSpace(ijk, world);
  return world[2];
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::cleanPointers()
{
  if (this->Internal->fitF != nullptr)
    {
    delete this->Internal->fitF;
    this->Internal->fitF = nullptr;
    }

  if (this->Internal->cubeF != nullptr)
    {
    delete this->Internal->cubeF;
    this->Internal->cubeF = nullptr;
    }

  if (this->Internal->modF != nullptr)
    {
    delete this->Internal->modF;
    this->Internal->modF = nullptr;
    }

  if (this->Internal->fitD != nullptr)
    {
    delete this->Internal->fitD;
    this->Internal->fitD = nullptr;
    }

  if (this->Internal->cubeD != nullptr)
    {
    delete this->Internal->cubeD;
    this->Internal->cubeD = nullptr;
    }

  if (this->Internal->modD != nullptr)
    {
    delete this->Internal->modD;
    this->Internal->modD = nullptr;
    }

  if (this->Internal->par != nullptr)
    {
    delete this->Internal->par;
    this->Internal->par = nullptr;
    }

  if (this->Internal->head != nullptr)
    {
    delete this->Internal->head;
    this->Internal->head = nullptr;
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerAstroModelingLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::SetMarkupsLogic(vtkSlicerMarkupsLogic *logic)
{
  this->Internal->MarkupsLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerMarkupsLogic *vtkSlicerAstroModelingLogic::GetMarkupsLogic()
{
  return this->Internal->MarkupsLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerAstroModelingLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  vtkMRMLAstroModelingParametersNode* pNode = vtkMRMLAstroModelingParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

//----------------------------------------------------------------------------
int vtkSlicerAstroModelingLogic::OperateModel(vtkMRMLAstroModelingParametersNode* pnode,
                                              vtkMRMLTableNode* tnode)
{
  this->Internal->AbortRequested = false;
  BBaroloOutputCapture outputCapture(this);
  try
    {
    return this->OperateModelInternal(pnode, tnode);
    }
  catch (const std::exception& e)
    {
    this->cleanPointers();
    if (pnode)
      {
      pnode->SetStatus(100);
      pnode->SetFitSuccess(false);
      }
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : 3DBarolo failed ("
                  << e.what() << "). See the application log for details.");
    return 0;
    }
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroModelingLogic::TakeLog()
{
  std::lock_guard<std::mutex> lock(this->Internal->LogMutex);
  std::string log;
  log.swap(this->Internal->Log);
  return log;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::AppendLog(const std::string& text)
{
  std::lock_guard<std::mutex> lock(this->Internal->LogMutex);
  this->Internal->Log += text;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::RequestAbort()
{
  this->Internal->AbortRequested = true;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroModelingLogic::UpdateModelFromTable(vtkMRMLAstroModelingParametersNode* pnode,
                                                      vtkMRMLTableNode* tableNode)
{
  this->Internal->AbortRequested = false;
  BBaroloOutputCapture outputCapture(this);
  try
    {
    return this->UpdateModelFromTableInternal(pnode, tableNode);
    }
  catch (const std::exception& e)
    {
    if (pnode)
      {
      pnode->SetStatus(100);
      }
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : 3DBarolo failed ("
                  << e.what() << "). See the application log for details.");
    return 0;
    }
}

//----------------------------------------------------------------------------
int vtkSlicerAstroModelingLogic::OperateModelInternal(vtkMRMLAstroModelingParametersNode* pnode,
                                              vtkMRMLTableNode *tnode)
{
  int wasModifying = 0;

  if(!pnode)
    {
    vtkModelingWarningMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " parameter node not found!");
    return 0;
    }

  if (!this->GetMRMLScene())
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " scene not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " inputVolume not found!");
    return 0;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if(!outputVolume || !outputVolume->GetImageData())
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " outputVolume not found!");
    return 0;
    }

  vtkMRMLAstroVolumeNode *residualVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetResidualVolumeNodeID()));
  if(!residualVolume || !residualVolume->GetImageData())
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " residualVolume not found!");
    return 0;
    }

  vtkMRMLAstroLabelMapVolumeNode *maskVolume =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetMaskVolumeNodeID()));

  bool maskActive = pnode->GetMaskActive();

  if((!maskVolume || !maskVolume->GetImageData()) && maskActive)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " maskVolume not found!");
    return 0;
    }

  vtkImageData* imageData = inputVolume->GetImageData();
  if (!imageData)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " input imageData nullptr!");
    return 0;
    }

  vtkPointData* pointData = imageData->GetPointData();
  if (!pointData)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " input pointData nullptr!");
    return 0;
    }

  vtkDataArray *dataArray = pointData->GetScalars();
  if (!dataArray)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " input dataArray nullptr!");
    return 0;
    }

  vtkMRMLAstroVolumeDisplayNode* inputVolumeDisplay =
    inputVolume->GetAstroVolumeDisplayNode();
  if (!inputVolumeDisplay)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " inputVolumeDisplay nullptr!");
    return 0;
    }

  // 3DBarolo works on input with the optical velocity definition (the
  // definition of the input is restored at the end)
  VelocityDefinitionGuard velocityDefinitionGuard(inputVolumeDisplay);
  if (!strncmp(inputVolumeDisplay->GetVelocityDefinition().c_str(), "VRAD", 4))
    {
    if (!inputVolumeDisplay->SetOpticalVelocityDefinition(false))
      {
      vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                    " Conversion to radio velocity definition failed!");
      return 0;
      }
    }

  const int DataType = dataArray->GetDataType();

  string file = inputVolume->GetName();

  if (this->Internal->par != nullptr)
    {
    delete this->Internal->par;
    this->Internal->par = nullptr;
    }

  this->Internal->par = new Param;
  ApplyThreadsFromEnvironment(this->Internal->par);

  if (this->Internal->head != nullptr)
    {
    delete this->Internal->head;
    this->Internal->head = nullptr;
    }

  this->Internal->head = new Header;

  this->Internal->par->setImageFile(file);
  string outputFolder = this->GetMRMLScene()->GetCacheManager()->GetRemoteCacheDirectory();
  outputFolder += "/";
  this->Internal->par->setOutfolder(outputFolder);
  this->Internal->par->setVerbosity(true);
  this->Internal->par->setShowbar(false);
  this->Internal->par->getParSE().flagSearch = false;
  this->Internal->par->getParGF().flagGALFIT = true;

  if (!strcmp(pnode->GetMode(), "Manual"))
    {
    if (fabs(pnode->GetNumberOfRings() - 1) > 0.001)
      {
      this->Internal->par->getParGF().NRADII = pnode->GetNumberOfRings();
      }
    else if (pnode->GetNumberOfRings() > 0 && fabs(pnode->GetNumberOfRings() - 1) < 0.001 )
      {
      pnode->SetStatus(100);
      pnode->SetFitSuccess(false);
      inputVolumeDisplay->SetOpticalVelocityDefinition(false);
      vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " insufficient number of rings."
                    " 3DBarolo needs at least two rings!");
      return 0;
      }
    else
      {
      this->Internal->par->getParGF().NRADII = -1;
      }

    if (pnode->GetRadSep() > -1)
      {
      this->Internal->par->getParGF().RADSEP = pnode->GetRadSep();
      }
    else
      {
      this->Internal->par->getParGF().RADSEP = -1;
      }

    if (pnode->GetXCenter() > -1)
      {
      this->Internal->par->getParGF().XPOS = DoubleToString(pnode->GetXCenter());
      }
    else
      {
      this->Internal->par->getParGF().XPOS = "-1";
      }

    if (pnode->GetYCenter() > -1)
      {
      this->Internal->par->getParGF().YPOS = DoubleToString(pnode->GetYCenter());
      }
    else
      {
      this->Internal->par->getParGF().YPOS = "-1";
      }

    if (pnode->GetSystemicVelocity() > -1)
      {
      this->Internal->par->getParGF().VSYS = DoubleToString(pnode->GetSystemicVelocity());
      }
    else
      {
      this->Internal->par->getParGF().VSYS = "-1";
      }

    if (pnode->GetRotationVelocity() > -1)
      {
      this->Internal->par->getParGF().VROT = DoubleToString(pnode->GetRotationVelocity());
      }
    else
      {
      this->Internal->par->getParGF().VROT = "-1";
      }

    this->Internal->par->getParGF().VRAD = DoubleToString(pnode->GetRadialVelocity());
    this->Internal->par->getParGF().VVERT = DoubleToString(pnode->GetVerticalVelocity());
    this->Internal->par->getParGF().DVDZ = DoubleToString(pnode->GetVerticalRotationalGradient());
    this->Internal->par->getParGF().ZCYL = DoubleToString(pnode->GetVerticalRotationalGradientHeight());

    if (pnode->GetVelocityDispersion() > -1)
      {
      this->Internal->par->getParGF().VDISP = DoubleToString(pnode->GetVelocityDispersion());
      }
    else
      {
      this->Internal->par->getParGF().VDISP = "-1";
      }

    if (pnode->GetInclination() > -1)
      {
      this->Internal->par->getParGF().INC = DoubleToString(pnode->GetInclination());
      }
    else
      {
      this->Internal->par->getParGF().INC = "-1";
      }

    this->Internal->par->getParGF().DELTAINC = pnode->GetInclinationError();

    if (pnode->GetPositionAngle() > -1)
      {
      this->Internal->par->getParGF().PHI = DoubleToString(pnode->GetPositionAngle());
      }
    else
      {
      this->Internal->par->getParGF().PHI = "-1";
      }

    this->Internal->par->getParGF().DELTAPHI = pnode->GetPositionAngleError();

    if (pnode->GetScaleHeight() > -1)
      {
      this->Internal->par->getParGF().Z0 = DoubleToString(pnode->GetScaleHeight());
      }
    else
      {
      this->Internal->par->getParGF().Z0 = "-1";
      }

    if (pnode->GetDistance() > -1)
      {
      this->Internal->par->getParGF().DISTANCE = pnode->GetDistance();
      }
    else
      {
      this->Internal->par->getParGF().DISTANCE = -1;
      }

    if (pnode->GetColumnDensity() > -1)
      {
      this->Internal->par->getParGF().DENS = DoubleToString(pnode->GetColumnDensity());
      }
    else
      {
      this->Internal->par->getParGF().DENS = "-1";
      }

    if (pnode->GetNumberOfClounds() > -1)
      {
      this->Internal->par->getParGF().NV = pnode->GetNumberOfClounds();
      }
    else
      {
      this->Internal->par->getParGF().NV = -1;
      }

    string freeParameters;

    if (pnode->GetPositionAngleFit())
      {
      freeParameters += "PA";
      }

    if (pnode->GetRotationVelocityFit())
      {
      freeParameters += "VROT";
      }

    if (pnode->GetRadialVelocityFit())
      {
      freeParameters += "VRAD";
      }

    if (pnode->GetVelocityDispersionFit())
      {
      freeParameters += "VDISP";
      }

    if (pnode->GetInclinationFit())
      {
      freeParameters += "INC";
      }

    if (pnode->GetXCenterFit())
      {
      freeParameters += "XPOS";
      }

    if (pnode->GetYCenterFit())
      {
      freeParameters += "YPOS";
      }

    if (pnode->GetSystemicVelocityFit())
      {
      freeParameters += "VSYS";
      }

    if (pnode->GetScaleHeightFit())
      {
      freeParameters += "Z0";
      }

    this->Internal->par->getParGF().FREE = freeParameters;
    this->Internal->par->getParGF().LTYPE = pnode->GetLayerType() + 1;
    this->Internal->par->getParGF().FTYPE = pnode->GetFittingFunction() + 1;
    this->Internal->par->getParGF().WFUNC = pnode->GetWeightingFunction();
    this->Internal->par->getParGF().CDENS = pnode->GetCloudsColumnDensity();

    if (pnode->GetNumberOfClounds() > 0)
      {
      this->Internal->par->getParGF().NV = pnode->GetNumberOfClounds();
      }
    else
      {
      this->Internal->par->getParGF().NV = -1;
      }
    }
  else
    {
    this->Internal->par->getParGF().NRADII = -1;
    this->Internal->par->getParGF().RADSEP = -1;
    this->Internal->par->getParGF().XPOS = "-1";
    this->Internal->par->getParGF().YPOS = "-1";
    this->Internal->par->getParGF().VSYS = "-1";
    this->Internal->par->getParGF().VROT = "-1";
    this->Internal->par->getParGF().VRAD = "0";
    this->Internal->par->getParGF().PHI = "-1";
    this->Internal->par->getParGF().INC = "-1";
    this->Internal->par->getParGF().Z0 = "-1";
    this->Internal->par->getParGF().DISTANCE = -1;
    this->Internal->par->getParGF().VDISP = "-1";
    this->Internal->par->getParGF().DENS = "-1";
    this->Internal->par->getParGF().NV = -1;
    }

  if (maskActive)
    {
    if (this->Internal->par->getParGF().NRADII == -1 ||
        this->Internal->par->getParGF().RADSEP == -1 ||
        this->Internal->par->getParGF().XPOS == "-1" ||
        this->Internal->par->getParGF().YPOS == "-1" ||
        this->Internal->par->getParGF().VSYS == "-1" ||
        this->Internal->par->getParGF().VROT == "-1" ||
        this->Internal->par->getParGF().PHI == "-1" ||
        this->Internal->par->getParGF().INC == "-1")
      // the rings and the geometry are needed; the distance (from the systemic
      // velocity), the clouds, the scale height, the dispersion and the column
      // density have defaults in 3DBarolo (-1)
      {
      this->Internal->par->getParSE().flagSearch = true;
      }
    else
      {
      this->Internal->par->getParSE().flagSearch = false;
      }
    }
  else
    {
    this->Internal->par->getParSE().flagSearch = true;
    }

  this->Internal->par->getParGF().flagADRIFT = pnode->GetADRIFTCorrection();
  this->Internal->par->getParGF().TOL = pnode->GetTollerance();
  this->Internal->par->getParGF().NORM = pnode->GetNormalize();
  ApplyBBaroloParameters(this->Internal->par, pnode);

  // set header
  this->Internal->head->setBitpix(StringToInt(inputVolume->GetAttribute("SlicerAstro.BITPIX")));
  int numAxes = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  this->Internal->head->setNumAx(numAxes);
  string inputString;

  for (int ii = 0; ii < numAxes; ii++)
    {
    inputString = "SlicerAstro.NAXIS";
    inputString += IntToString(ii + 1);
    this->Internal->head->setDimAx(ii,StringToLong(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CRPIX";
    inputString += IntToString(ii + 1);
    this->Internal->head->setCrpix(ii, StringToDouble(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CRVAL";
    inputString += IntToString(ii + 1);
    this->Internal->head->setCrval(ii, StringToDouble(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CDELT";
    inputString += IntToString(ii + 1);
    this->Internal->head->setCdelt(ii, StringToDouble(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CTYPE";
    inputString += IntToString(ii + 1);
    inputString = inputVolume->GetAttribute(inputString.c_str());
    this->Internal->head->setCtype(ii, inputString);
    inputString = "SlicerAstro.CUNIT";
    inputString += IntToString(ii + 1);
    inputString = inputVolume->GetAttribute(inputString.c_str());
    this->Internal->head->setCunit(ii, inputString);
    }

  this->Internal->head->setBmaj(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BMAJ")));
  this->Internal->head->setBmin(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BMIN")));
  this->Internal->head->setBpa(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BPA")));
  this->Internal->head->setBzero(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BZERO")));
  this->Internal->head->setBscale(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BSCALE")));
  this->Internal->head->setBlank(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BLANK")));
  this->Internal->head->setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA2")));
  this->Internal->head->setEpoch(StringToFloat(inputVolume->GetAttribute("SlicerAstro.EPOCH")));
  this->Internal->head->setFreq0(StringToDouble(inputVolume->GetAttribute("SlicerAstro.RESTFREQ")));
  inputString = inputVolume->GetAttribute("SlicerAstro.BUNIT");
  this->Internal->head->setBunit(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.BTYPE");
  this->Internal->head->setBtype(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.OBJECT");
  this->Internal->head->setName(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.TELESCOP");
  this->Internal->head->setTelesc(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.DUNIT3");
  this->Internal->head->setDunit3(inputString);
  this->Internal->head->setDrval3(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DRVAL3")));
  this->Internal->head->setDataMin(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMIN")));
  this->Internal->head->setDataMax(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMAX")));
  if(!this->Internal->head->saveWCSStruct(inputVolume->GetAstroVolumeDisplayNode()->GetWCSStruct()))
    {
    pnode->SetStatus(100);
    pnode->SetFitSuccess(false);
    inputVolumeDisplay->SetOpticalVelocityDefinition(false);
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : the WCS copy failed!");
    return 0;
    }

  this->Internal->head->calcArea();

  if (pnode->GetStatus() == -1 || this->Internal->AbortRequested)
    {
    this->cleanPointers();
    pnode->SetStatus(100);
    pnode->SetFitSuccess(false);
    return 0;
    }
  SetProgress(pnode, 5);

  int *dims = inputVolume->GetImageData()->GetDimensions();
  int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  vtkIdType numElements = static_cast<vtkIdType>(dims[0]) * dims[1] * dims[2] * numComponents;

  switch (DataType)
    {
    case VTK_FLOAT:
      {
      if (this->Internal->cubeF != nullptr)
        {
        delete this->Internal->cubeF;
        this->Internal->cubeF = nullptr;
        }

      this->Internal->cubeF = new Cube<float>;

      this->Internal->cubeF->saveParam(*this->Internal->par);
      this->Internal->cubeF->saveHead(*this->Internal->head);
      this->Internal->cubeF->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA2")));
      float *inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer());
      if (!inFPixel)
        {
        this->cleanPointers();
        pnode->SetStatus(100);
        pnode->SetFitSuccess(false);
        inputVolumeDisplay->SetOpticalVelocityDefinition(false);
        vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                      "Unable to find inFPixel pointer.");
        return 0;
        }
      this->Internal->cubeF->setCube(inFPixel, dims);

      // Feed segmentation mask to cube
      if (maskActive)
        {
        short* segmentationMaskPointer = static_cast<short*> (maskVolume->GetImageData()->GetScalarPointer());
        if (!segmentationMaskPointer)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find segmentationMask.");
          return 0;
          }
        // Fill the BBarolo mask in place (no intermediate copy)
        bool* mask = this->Internal->cubeF->allocateMask();
        for (vtkIdType ii = 0; ii < numElements; ii++)
          {
          mask[ii] = segmentationMaskPointer[ii] > 0;
          }
        }

      // Searching stuff if the user has not provided a mask
      SetProgress(pnode, 10);
      if (this->Internal->par->getParSE().flagSearch)
        {
        this->Internal->cubeF->search();
        }
      if (this->Internal->AbortRequested)
        {
        this->cleanPointers();
        pnode->SetStatus(100);
        pnode->SetFitSuccess(false);
        return 0;
        }
      SetProgress(pnode, 30);

      // Cube Fitting
      if (this->Internal->par->getflagGalFit())
        {
        if (this->Internal->fitF != nullptr)
          {
          delete this->Internal->fitF;
          this->Internal->fitF = nullptr;
          }

        // Setup, initial estimates and rings. Fatal errors (e.g. no source found,

        // not enough rings) are reported by BBarolo as bbarolo::Error.

        this->Internal->fitF = new Model::Galfit<float>(this->Internal->cubeF);
        SetProgress(pnode, 40);

        *this->Internal->par = this->Internal->cubeF->pars();

        wasModifying = pnode->StartModify();

        // Initial parameters (given or estimated by 3DBarolo): BBarolo 1.8
        // keeps the estimates in the initial rings of Galfit (the parameter
        // strings keep the input, e.g. -1 for the estimated ones). The rings
        // can differ (e.g. the position angles refined ring by ring with a 2D
        // tilted-ring model): the UI shows their median.
        Rings<float>* initialRings = this->Internal->fitF->Inrings();
        // Guards against meaningless initial rings, for Estimate and Create
        // (the model uses the initial rings, no fit corrects them). A fit
        // starts from the initial rings of 3DBarolo, as upstream 3DBarolo.
        if (pnode->GetOperation() != vtkMRMLAstroModelingParametersNode::FIT)
          {
          RejectOutlierInitialRings(initialRings);
          RejectOutlierInitialRings(this->Internal->fitF->Outrings());
          ExtrapolateInnerRotationVelocity(initialRings);
          ExtrapolateInnerRotationVelocity(this->Internal->fitF->Outrings());
          }
        // Manual mode: Create and Fit keep the values of the user (the rings
        // of 3DBarolo can differ, e.g. refined by its 2D tilted-ring fit and
        // by the guards above); only the values to estimate (-1) are filled
        const bool keepUserValues = !strcmp(pnode->GetMode(), "Manual") &&
          pnode->GetOperation() != vtkMRMLAstroModelingParametersNode::ESTIMATE;
        if (initialRings && initialRings->nr > 0)
          {
          pnode->SetNumberOfRings(initialRings->nr);
          pnode->SetRadSep(initialRings->radsep > 0 ? initialRings->radsep :
            (initialRings->nr > 1 ? initialRings->radii[1] - initialRings->radii[0] : initialRings->radii[0]));
          pnode->SetXCenter(UserOrInitialValue(keepUserValues, pnode->GetXCenter(), MedianOfRings(initialRings->xpos)));
          pnode->SetYCenter(UserOrInitialValue(keepUserValues, pnode->GetYCenter(), MedianOfRings(initialRings->ypos)));
          pnode->SetSystemicVelocity(UserOrInitialValue(keepUserValues, pnode->GetSystemicVelocity(), MedianOfRings(initialRings->vsys)));
          pnode->SetRotationVelocity(UserOrInitialValue(keepUserValues, pnode->GetRotationVelocity(), MedianOfRings(initialRings->vrot)));
          pnode->SetRadialVelocity(GivenOrInitialValue(keepUserValues, pnode->GetRadialVelocity(), MedianOfRings(initialRings->vrad)));
          pnode->SetVerticalVelocity(GivenOrInitialValue(keepUserValues, pnode->GetVerticalVelocity(), MedianOfRings(initialRings->vvert)));
          pnode->SetVerticalRotationalGradient(GivenOrInitialValue(keepUserValues, pnode->GetVerticalRotationalGradient(), MedianOfRings(initialRings->dvdz)));
          pnode->SetVerticalRotationalGradientHeight(GivenOrInitialValue(keepUserValues, pnode->GetVerticalRotationalGradientHeight(), MedianOfRings(initialRings->zcyl)));
          pnode->SetPositionAngle(UserOrInitialValue(keepUserValues, pnode->GetPositionAngle(), MedianOfRings(initialRings->phi)));
          pnode->SetInclination(UserOrInitialValue(keepUserValues, pnode->GetInclination(), MedianOfRings(initialRings->inc)));
          pnode->SetScaleHeight(UserOrInitialValue(keepUserValues, pnode->GetScaleHeight(), MedianOfRings(initialRings->z0)));
          pnode->SetVelocityDispersion(UserOrInitialValue(keepUserValues, pnode->GetVelocityDispersion(), MedianOfRings(initialRings->vdisp)));
          pnode->SetColumnDensity(UserOrInitialValue(keepUserValues, pnode->GetColumnDensity(), MedianOfRings(initialRings->dens)));
          }
        else
          {
          pnode->SetNumberOfRings(this->Internal->par->getParGF().NRADII);
          pnode->SetRadSep(this->Internal->par->getParGF().RADSEP);
          pnode->SetXCenter(StringToDouble(this->Internal->par->getParGF().XPOS.c_str()));
          pnode->SetYCenter(StringToDouble(this->Internal->par->getParGF().YPOS.c_str()));
          pnode->SetSystemicVelocity(StringToDouble(this->Internal->par->getParGF().VSYS.c_str()));
          pnode->SetRotationVelocity(StringToDouble(this->Internal->par->getParGF().VROT.c_str()));
          pnode->SetRadialVelocity(StringToDouble(this->Internal->par->getParGF().VRAD.c_str()));
          pnode->SetVerticalVelocity(StringToDouble(this->Internal->par->getParGF().VVERT.c_str()));
          pnode->SetVerticalRotationalGradient(StringToDouble(this->Internal->par->getParGF().DVDZ.c_str()));
          pnode->SetVerticalRotationalGradientHeight(StringToDouble(this->Internal->par->getParGF().ZCYL.c_str()));
          pnode->SetPositionAngle(StringToDouble(this->Internal->par->getParGF().PHI.c_str()));
          pnode->SetInclination(StringToDouble(this->Internal->par->getParGF().INC.c_str()));
          pnode->SetScaleHeight(StringToDouble(this->Internal->par->getParGF().Z0.c_str()));
          pnode->SetDistance(this->Internal->par->getParGF().DISTANCE);
          pnode->SetVelocityDispersion(StringToDouble(this->Internal->par->getParGF().VDISP.c_str()));
          pnode->SetColumnDensity(StringToDouble(this->Internal->par->getParGF().DENS.c_str()));
          pnode->SetCloudsColumnDensity((double) this->Internal->par->getParGF().CDENS);
          pnode->SetNumberOfClounds(this->Internal->par->getParGF().NV);
          }
        pnode->SetDistance(this->Internal->par->getParGF().DISTANCE);
        pnode->SetCloudsColumnDensity((double) this->Internal->par->getParGF().CDENS);
        pnode->SetNumberOfClounds(this->Internal->par->getParGF().NV);

        pnode->EndModify(wasModifying);

        if (pnode->GetOperation() == vtkMRMLAstroModelingParametersNode::ESTIMATE)
          {
          // Exit (success). Only the estimation of the parameters was requested.
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          return 1;
          }

        if (pnode->GetOperation() == vtkMRMLAstroModelingParametersNode::FIT)
          {
          RunGalfitStage(this->Internal->fitF, pnode, false, 40,
                         this->Internal->par->getParGF().TWOSTAGE ? 70 : 85,
                         this->Internal->AbortRequested);

          if (pnode->GetStatus() == -1 || this->Internal->AbortRequested)
            {
            this->cleanPointers();
            pnode->SetStatus(100);
            pnode->SetFitSuccess(false);
            return 0;
            }


          if (this->Internal->par->getParGF().TWOSTAGE)
            {
            RunGalfitStage(this->Internal->fitF, pnode, true, 70, 85, this->Internal->AbortRequested);
            if (pnode->GetStatus() == -1 || this->Internal->AbortRequested)
              {
              this->cleanPointers();
              pnode->SetStatus(100);
              pnode->SetFitSuccess(false);
              return 0;
              }
            }
          }

        const bool fitOperation = pnode->GetOperation() == vtkMRMLAstroModelingParametersNode::FIT;
        SetProgress(pnode, fitOperation ? 86 : 50);

        // final model: radial profile and normalization (AZIM, LOCAL, NONE) of
        // 3DBarolo, as in its Galfit::writeModel
        this->Internal->modF = BuildFinalModel(this->Internal->fitF, pnode, fitOperation ? 88 : 60, fitOperation ? 92 : 90);
        SetProgress(pnode, fitOperation ? 92 : 90);
        if (!this->Internal->modF || !this->Internal->modF->Out())
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find modF.");
          return 0;
          }

        this->Internal->modF->Out()->Head().setMinMax(0.,0.);
        float *outarray = this->Internal->modF->Out()->Array();
        if (!outarray)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find modF array.");
          return 0;
          }

        float *outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer());
        if (!outFPixel)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find outFPixel pointer.");
          return 0;
          }
        for (vtkIdType ii = 0; ii < numElements; ii++)
          {
          *(outFPixel + ii) = *(outarray + ii);
          }

        float *residualFPixel = static_cast<float*> (residualVolume->GetImageData()->GetScalarPointer());
        if (!residualFPixel)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find residualFPixel pointer.");
          return 0;
          }

        for (vtkIdType ii = 0; ii < numElements; ii++)
          {
          if (*(outFPixel + ii) < 1.E-6)
            {
            *(residualFPixel + ii) = *(inFPixel + ii);
            }
          else
            {
            *(residualFPixel + ii) = 0.;
            }
          }

        outarray = nullptr;
        if (this->Internal->modF != nullptr)
          {
          delete this->Internal->modF;
          this->Internal->modF = nullptr;
          }
        outFPixel = nullptr;
        inFPixel = nullptr;
        residualFPixel = nullptr;
        }
      break;
      }
    case VTK_DOUBLE:
      {
      if (this->Internal->cubeD != nullptr)
        {
        delete this->Internal->cubeD;
        this->Internal->cubeD = nullptr;
        }

      this->Internal->cubeD = new Cube<double>;

      this->Internal->cubeD->saveParam(*this->Internal->par);
      this->Internal->cubeD->saveHead(*this->Internal->head);
      this->Internal->cubeD->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA2")));
      double *inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer());
      if (!inDPixel)
        {
        this->cleanPointers();
        pnode->SetStatus(100);
        pnode->SetFitSuccess(false);
        inputVolumeDisplay->SetOpticalVelocityDefinition(false);
        vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                      "Unable to find inDPixel pointer.");
        return 0;
        }
      this->Internal->cubeD->setCube(inDPixel, dims);

      // Feed segmentation mask to cube
      if (maskActive)
        {
        short* segmentationMaskPointer = static_cast<short*> (maskVolume->GetImageData()->GetScalarPointer());
        if (!segmentationMaskPointer)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find segmentationMask.");
          return 0;
          }
        // Fill the BBarolo mask in place (no intermediate copy)
        bool* mask = this->Internal->cubeD->allocateMask();
        for (vtkIdType ii = 0; ii < numElements; ii++)
          {
          mask[ii] = segmentationMaskPointer[ii] > 0;
          }
        }

      // Searching stuff if the user has not provided a mask
      SetProgress(pnode, 10);
      if (this->Internal->par->getParSE().flagSearch)
        {
        this->Internal->cubeD->search();
        }
      if (this->Internal->AbortRequested)
        {
        this->cleanPointers();
        pnode->SetStatus(100);
        pnode->SetFitSuccess(false);
        return 0;
        }
      SetProgress(pnode, 30);

      // Cube Fitting
      if (this->Internal->par->getflagGalFit())
        {
        if (this->Internal->fitD != nullptr)
          {
          delete this->Internal->fitD;
          this->Internal->fitD = nullptr;
          }

        // Setup, initial estimates and rings. Fatal errors (e.g. no source found,

        // not enough rings) are reported by BBarolo as bbarolo::Error.

        this->Internal->fitD = new Model::Galfit<double>(this->Internal->cubeD);
        SetProgress(pnode, 40);

        *this->Internal->par = this->Internal->cubeD->pars();

        wasModifying = pnode->StartModify();

        // Initial parameters (given or estimated by 3DBarolo): BBarolo 1.8
        // keeps the estimates in the initial rings of Galfit (the parameter
        // strings keep the input, e.g. -1 for the estimated ones). The rings
        // can differ (e.g. the position angles refined ring by ring with a 2D
        // tilted-ring model): the UI shows their median.
        Rings<double>* initialRings = this->Internal->fitD->Inrings();
        // Guards against meaningless initial rings, for Estimate and Create
        // (the model uses the initial rings, no fit corrects them). A fit
        // starts from the initial rings of 3DBarolo, as upstream 3DBarolo.
        if (pnode->GetOperation() != vtkMRMLAstroModelingParametersNode::FIT)
          {
          RejectOutlierInitialRings(initialRings);
          RejectOutlierInitialRings(this->Internal->fitD->Outrings());
          ExtrapolateInnerRotationVelocity(initialRings);
          ExtrapolateInnerRotationVelocity(this->Internal->fitD->Outrings());
          }
        // Manual mode: Create and Fit keep the values of the user (the rings
        // of 3DBarolo can differ, e.g. refined by its 2D tilted-ring fit and
        // by the guards above); only the values to estimate (-1) are filled
        const bool keepUserValues = !strcmp(pnode->GetMode(), "Manual") &&
          pnode->GetOperation() != vtkMRMLAstroModelingParametersNode::ESTIMATE;
        if (initialRings && initialRings->nr > 0)
          {
          pnode->SetNumberOfRings(initialRings->nr);
          pnode->SetRadSep(initialRings->radsep > 0 ? initialRings->radsep :
            (initialRings->nr > 1 ? initialRings->radii[1] - initialRings->radii[0] : initialRings->radii[0]));
          pnode->SetXCenter(UserOrInitialValue(keepUserValues, pnode->GetXCenter(), MedianOfRings(initialRings->xpos)));
          pnode->SetYCenter(UserOrInitialValue(keepUserValues, pnode->GetYCenter(), MedianOfRings(initialRings->ypos)));
          pnode->SetSystemicVelocity(UserOrInitialValue(keepUserValues, pnode->GetSystemicVelocity(), MedianOfRings(initialRings->vsys)));
          pnode->SetRotationVelocity(UserOrInitialValue(keepUserValues, pnode->GetRotationVelocity(), MedianOfRings(initialRings->vrot)));
          pnode->SetRadialVelocity(GivenOrInitialValue(keepUserValues, pnode->GetRadialVelocity(), MedianOfRings(initialRings->vrad)));
          pnode->SetVerticalVelocity(GivenOrInitialValue(keepUserValues, pnode->GetVerticalVelocity(), MedianOfRings(initialRings->vvert)));
          pnode->SetVerticalRotationalGradient(GivenOrInitialValue(keepUserValues, pnode->GetVerticalRotationalGradient(), MedianOfRings(initialRings->dvdz)));
          pnode->SetVerticalRotationalGradientHeight(GivenOrInitialValue(keepUserValues, pnode->GetVerticalRotationalGradientHeight(), MedianOfRings(initialRings->zcyl)));
          pnode->SetPositionAngle(UserOrInitialValue(keepUserValues, pnode->GetPositionAngle(), MedianOfRings(initialRings->phi)));
          pnode->SetInclination(UserOrInitialValue(keepUserValues, pnode->GetInclination(), MedianOfRings(initialRings->inc)));
          pnode->SetScaleHeight(UserOrInitialValue(keepUserValues, pnode->GetScaleHeight(), MedianOfRings(initialRings->z0)));
          pnode->SetVelocityDispersion(UserOrInitialValue(keepUserValues, pnode->GetVelocityDispersion(), MedianOfRings(initialRings->vdisp)));
          pnode->SetColumnDensity(UserOrInitialValue(keepUserValues, pnode->GetColumnDensity(), MedianOfRings(initialRings->dens)));
          }
        else
          {
          pnode->SetNumberOfRings(this->Internal->par->getParGF().NRADII);
          pnode->SetRadSep(this->Internal->par->getParGF().RADSEP);
          pnode->SetXCenter(StringToDouble(this->Internal->par->getParGF().XPOS.c_str()));
          pnode->SetYCenter(StringToDouble(this->Internal->par->getParGF().YPOS.c_str()));
          pnode->SetSystemicVelocity(StringToDouble(this->Internal->par->getParGF().VSYS.c_str()));
          pnode->SetRotationVelocity(StringToDouble(this->Internal->par->getParGF().VROT.c_str()));
          pnode->SetRadialVelocity(StringToDouble(this->Internal->par->getParGF().VRAD.c_str()));
          pnode->SetVerticalVelocity(StringToDouble(this->Internal->par->getParGF().VVERT.c_str()));
          pnode->SetVerticalRotationalGradient(StringToDouble(this->Internal->par->getParGF().DVDZ.c_str()));
          pnode->SetVerticalRotationalGradientHeight(StringToDouble(this->Internal->par->getParGF().ZCYL.c_str()));
          pnode->SetPositionAngle(StringToDouble(this->Internal->par->getParGF().PHI.c_str()));
          pnode->SetInclination(StringToDouble(this->Internal->par->getParGF().INC.c_str()));
          pnode->SetScaleHeight(StringToDouble(this->Internal->par->getParGF().Z0.c_str()));
          pnode->SetDistance(this->Internal->par->getParGF().DISTANCE);
          pnode->SetVelocityDispersion(StringToDouble(this->Internal->par->getParGF().VDISP.c_str()));
          pnode->SetColumnDensity(StringToDouble(this->Internal->par->getParGF().DENS.c_str()));
          pnode->SetCloudsColumnDensity((double) this->Internal->par->getParGF().CDENS);
          pnode->SetNumberOfClounds(this->Internal->par->getParGF().NV);
          }
        pnode->SetDistance(this->Internal->par->getParGF().DISTANCE);
        pnode->SetCloudsColumnDensity((double) this->Internal->par->getParGF().CDENS);
        pnode->SetNumberOfClounds(this->Internal->par->getParGF().NV);

        pnode->EndModify(wasModifying);

        if (pnode->GetOperation() == vtkMRMLAstroModelingParametersNode::ESTIMATE)
          {
          // Exit (success). Only the estimation of the parameters was requested.
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          return 1;
          }

        if (pnode->GetOperation() == vtkMRMLAstroModelingParametersNode::FIT)
          {
          RunGalfitStage(this->Internal->fitD, pnode, false, 40,
                         this->Internal->par->getParGF().TWOSTAGE ? 70 : 85,
                         this->Internal->AbortRequested);

          if (pnode->GetStatus() == -1 || this->Internal->AbortRequested)
            {
            this->cleanPointers();
            pnode->SetStatus(100);
            pnode->SetFitSuccess(false);
            return 0;
            }


          if (this->Internal->par->getParGF().TWOSTAGE)
            {
            RunGalfitStage(this->Internal->fitD, pnode, true, 70, 85, this->Internal->AbortRequested);
            if (pnode->GetStatus() == -1 || this->Internal->AbortRequested)
              {
              this->cleanPointers();
              pnode->SetStatus(100);
              pnode->SetFitSuccess(false);
              return 0;
              }
            }
          }

        const bool fitOperation = pnode->GetOperation() == vtkMRMLAstroModelingParametersNode::FIT;
        SetProgress(pnode, fitOperation ? 86 : 50);

        // Calculate the total flux inside last ring in data
        // final model: radial profile and normalization (AZIM, LOCAL, NONE) of
        // 3DBarolo, as in its Galfit::writeModel
        this->Internal->modD = BuildFinalModel(this->Internal->fitD, pnode, fitOperation ? 88 : 60, fitOperation ? 92 : 90);
        SetProgress(pnode, fitOperation ? 92 : 90);
        if (!this->Internal->modD || !this->Internal->modD->Out())
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find modF.");
          return 0;
          }
        this->Internal->modD->Out()->Head().setMinMax(0.,0.);
        double *outarray = this->Internal->modD->Out()->Array();
        if (!outarray)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find modF array.");
          return 0;
          }

        double *outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer());
        if (!outDPixel)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find outDPixel pointer.");
          return 0;
          }
        for (vtkIdType ii = 0; ii < numElements; ii++)
          {
          *(outDPixel + ii) = *(outarray + ii);
          }

        double *residualDPixel = static_cast<double*> (residualVolume->GetImageData()->GetScalarPointer());
        if (!residualDPixel)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find residualDPixel pointer.");
          return 0;
          }

        for (vtkIdType ii = 0; ii < numElements; ii++)
          {
          if (*(outDPixel + ii) < 1.E-6)
            {
            *(residualDPixel + ii) = *(inDPixel + ii);
            }
          else
            {
            *(residualDPixel + ii) = 0.;
            }
          }

        outarray = nullptr;
        if (this->Internal->modD != nullptr)
          {
          delete this->Internal->modD;
          this->Internal->modD = nullptr;
          }
        outDPixel = nullptr;
        inDPixel = nullptr;
        residualDPixel = nullptr;
        }
      break;
      }
    }

  // the velocity definition of the input is restored by velocityDefinitionGuard

  SetProgress(pnode, 95);

  if (!tnode)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                  "Unable to find the internal parameters table node.");
    return 0;
    }

  wasModifying = tnode->StartModify();

  vtkTable* paramsTable = tnode->GetTable();
  if (!paramsTable)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                  "Unable to find the parameters table.");
    return 0;
    }

  vtkDoubleArray* XPos = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("XPos"));
  XPos->Initialize();
  vtkDoubleArray* YPos = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("YPos"));
  YPos->Initialize();
  vtkDoubleArray* VSys = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("VSys"));
  VSys->Initialize();
  vtkDoubleArray* Radii = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("Radii"));
  Radii->Initialize();
  vtkDoubleArray* VRot = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("VRot"));
  VRot->Initialize();
  vtkDoubleArray* VRad = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("VRad"));
  VRad->Initialize();
  vtkDoubleArray* VDisp = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("VDisp"));
  VDisp->Initialize();
  vtkDoubleArray* Dens = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("Dens"));
  Dens->Initialize();
  vtkDoubleArray* Z0 = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("Z0"));
  Z0->Initialize();
  vtkDoubleArray* Inc = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("Inc"));
  Inc->Initialize();
  vtkDoubleArray* Phi = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("Phi"));
  Phi->Initialize();

  if (!XPos || !YPos || !VSys || !Radii ||
      !VRot || !VRad || !VDisp || !Dens ||
      !Z0 || !Inc || !Phi)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                  "Unable to find one or more table columns.");
    return 0;
    }

  // one row per output ring of 3DBarolo
  const int numberOfRings = this->Internal->fitF ? NumberOfCompleteRings(this->Internal->fitF->Outrings()) :
    (this->Internal->fitD ? NumberOfCompleteRings(this->Internal->fitD->Outrings()) : 0);
  if (numberOfRings != pnode->GetNumberOfRings())
    {
    vtkModelingWarningMacro("vtkSlicerAstroModelingLogic::OperateModel : 3DBarolo returned "
                            << numberOfRings << " rings (" << pnode->GetNumberOfRings() << " requested).");
    pnode->SetNumberOfRings(numberOfRings);
    }

  // rings whose fit did not converge: 3DBarolo sets their density to 0 (no
  // emission in the model) and leaves them out of its ring files
  if (pnode->GetOperation() == vtkMRMLAstroModelingParametersNode::FIT)
    {
    const std::string unconverged = this->Internal->fitF ?
      UnconvergedRings(this->Internal->fitF->Outrings(), pnode->GetNumberOfRings()) :
      (this->Internal->fitD ? UnconvergedRings(this->Internal->fitD->Outrings(), pnode->GetNumberOfRings()) : std::string());
    if (!unconverged.empty())
      {
      vtkModelingWarningMacro("vtkSlicerAstroModelingLogic::OperateModel : the fit of ring(s) "
                              << unconverged << " did not converge: their column density is 0"
                              " (no emission in the model).");
      }
    }

  for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
    {
    tnode->AddEmptyRow();
    }

  if(this->Internal->fitF && this->Internal->fitF->Outrings())
    {
    if (this->Internal->fitF->Outrings()->xpos.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        XPos->SetValue(ii, this->Internal->fitF->Outrings()->xpos[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->ypos.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        YPos->SetValue(ii, this->Internal->fitF->Outrings()->ypos[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->vsys.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VSys->SetValue(ii, this->Internal->fitF->Outrings()->vsys[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->radii.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Radii->SetValue(ii, this->Internal->fitF->Outrings()->radii[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->vrot.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VRot->SetValue(ii, this->Internal->fitF->Outrings()->vrot[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->vrad.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VRad->SetValue(ii, this->Internal->fitF->Outrings()->vrad[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->vdisp.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VDisp->SetValue(ii, this->Internal->fitF->Outrings()->vdisp[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->dens.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Dens->SetValue(ii, this->Internal->fitF->Outrings()->dens[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->z0.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Z0->SetValue(ii, this->Internal->fitF->Outrings()->z0[ii]
                         * KpcPerArc(this->Internal->par->getDistance()));
        }
      }

    if (this->Internal->fitF->Outrings()->inc.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Inc->SetValue(ii, this->Internal->fitF->Outrings()->inc[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->phi.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Phi->SetValue(ii, this->Internal->fitF->Outrings()->phi[ii]);
        }
      }
    }
  else if (this->Internal->fitD && this->Internal->fitD->Outrings())
    {
    if (this->Internal->fitD->Outrings()->xpos.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        XPos->SetValue(ii, this->Internal->fitD->Outrings()->xpos[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->ypos.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        YPos->SetValue(ii, this->Internal->fitD->Outrings()->ypos[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->vsys.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VSys->SetValue(ii, this->Internal->fitD->Outrings()->vsys[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->radii.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Radii->SetValue(ii, this->Internal->fitD->Outrings()->radii[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->vrot.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VRot->SetValue(ii, this->Internal->fitD->Outrings()->vrot[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->vrad.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VRad->SetValue(ii, this->Internal->fitD->Outrings()->vrad[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->vdisp.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VDisp->SetValue(ii, this->Internal->fitD->Outrings()->vdisp[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->dens.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Dens->SetValue(ii, this->Internal->fitD->Outrings()->dens[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->z0.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Z0->SetValue(ii, this->Internal->fitD->Outrings()->z0[ii]
                         * KpcPerArc(this->Internal->par->getDistance()));
        }
      }

    if (this->Internal->fitD->Outrings()->inc.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Inc->SetValue(ii, this->Internal->fitD->Outrings()->inc[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->phi.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Phi->SetValue(ii, this->Internal->fitD->Outrings()->phi[ii]);
        }
      }
    }
  else
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " galfit not found!");
    return 0;
    }

  tnode->EndModify(wasModifying);

  this->cleanPointers();
  pnode->SetStatus(100);
  pnode->SetFitSuccess(true);

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroModelingLogic::UpdateModelFromTableInternal(vtkMRMLAstroModelingParametersNode *pnode,
                                                              vtkMRMLTableNode *tableNode)
{
  if(!pnode)
    {
    vtkModelingWarningMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " parameter node not found!");
    return 0;
    }

  if (!this->GetMRMLScene())
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                  " scene not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume)
    {
    vtkModelingWarningMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " inputVolume not found!");
    return 0;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if(!outputVolume)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                  " outputVolume not found!");
    return 0;
    }

  vtkMRMLAstroVolumeNode *residualVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetResidualVolumeNodeID()));
  if(!residualVolume)
    {
    vtkModelingWarningMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " residualVolume not found!");
    }

  vtkImageData* imageData = inputVolume->GetImageData();
  if (!imageData)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                  " input imageData nullptr!");
    return 0;
    }

  vtkPointData* pointData = imageData->GetPointData();
  if (!pointData)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                  " input pointData nullptr!");
    return 0;
    }

  vtkDataArray *dataArray = pointData->GetScalars();
  if (!dataArray)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                  " input dataArray nullptr!");
    return 0;
    }

  vtkMRMLAstroVolumeDisplayNode* inputVolumeDisplay =
    inputVolume->GetAstroVolumeDisplayNode();
  if (!inputVolumeDisplay)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                  " inputVolumeDisplay nullptr!");
    return 0;
    }

  // 3DBarolo works on input with the optical velocity definition (the
  // definition of the input is restored at the end)
  VelocityDefinitionGuard velocityDefinitionGuard(inputVolumeDisplay);
  wcsprm* WCS = inputVolumeDisplay->GetWCSStruct();
  if (!WCS)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                  " WCS nullptr!");
    return 0;
    }

  if (!strncmp(WCS->ctype[2], "VRAD", 4))
    {
    if (!inputVolumeDisplay->SetOpticalVelocityDefinition(false))
      {
      vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " Conversion to radio velocity definition failed!");
      return 0;
      }
    }

  const int DataType = dataArray->GetDataType();

  int *dims = inputVolume->GetImageData()->GetDimensions();
  int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  vtkIdType numElements = static_cast<vtkIdType>(dims[0]) * dims[1] * dims[2] * numComponents;

  if (!this->Internal->fitF && !this->Internal->fitD)
    {
    short* maskPointer = nullptr;
    if (pnode->GetMaskActive())
      {
      vtkMRMLAstroLabelMapVolumeNode *maskVolume =
        vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
          (this->GetMRMLScene()->GetNodeByID(pnode->GetMaskVolumeNodeID()));
      if (!maskVolume || !maskVolume->GetImageData() ||
          !maskVolume->GetImageData()->GetScalarPointer())
        {
        vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                      " maskVolume not found!");
        return 0;
        }
      maskPointer = static_cast<short*> (maskVolume->GetImageData()->GetScalarPointer());
      }

    if (this->Internal->par != nullptr)
      {
      delete this->Internal->par;
      this->Internal->par = nullptr;
      }

    this->Internal->par = new Param;
    ApplyThreadsFromEnvironment(this->Internal->par);

    if (this->Internal->head != nullptr)
      {
      delete this->Internal->head;
      this->Internal->head = nullptr;
      }

    this->Internal->head = new Header;

    string file = inputVolume->GetName();
    this->Internal->par->setImageFile(file);
    string outputFolder = this->GetMRMLScene()->GetCacheManager()->GetRemoteCacheDirectory();
    outputFolder += "/";
    this->Internal->par->setOutfolder(outputFolder);
    this->Internal->par->setVerbosity(false);
    this->Internal->par->setShowbar(false);
    this->Internal->par->getParGF().flagGALFIT = true;

    if (fabs(pnode->GetNumberOfRings() - 1) > 0.001)
      {
      this->Internal->par->getParGF().NRADII = pnode->GetNumberOfRings();
      }
    else if (pnode->GetNumberOfRings() > 0 && fabs(pnode->GetNumberOfRings() - 1) < 0.001 )
      {
      pnode->SetStatus(100);
      pnode->SetFitSuccess(false);
      inputVolumeDisplay->SetOpticalVelocityDefinition(false);
      vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " insufficient number of rings."
                    " 3DBarolo needs at least two rings!");
      return 0;
      }
    else
      {
      this->Internal->par->getParGF().NRADII = -1;
      }

    if (pnode->GetRadSep() > -1)
      {
      this->Internal->par->getParGF().RADSEP = pnode->GetRadSep();
      }
    else
      {
      this->Internal->par->getParGF().RADSEP = -1;
      }

    if (pnode->GetXCenter() > -1)
      {
      this->Internal->par->getParGF().XPOS = DoubleToString(pnode->GetXCenter());
      }
    else
      {
      this->Internal->par->getParGF().XPOS = "-1";
      }

    if (pnode->GetYCenter() > -1)
      {
      this->Internal->par->getParGF().YPOS = DoubleToString(pnode->GetYCenter());
      }
    else
      {
      this->Internal->par->getParGF().YPOS = "-1";
      }

    if (pnode->GetSystemicVelocity() > -1)
      {
      this->Internal->par->getParGF().VSYS = DoubleToString(pnode->GetSystemicVelocity());
      }
    else
      {
      this->Internal->par->getParGF().VSYS = "-1";
      }

    if (pnode->GetRotationVelocity() > -1)
      {
      this->Internal->par->getParGF().VROT = DoubleToString(pnode->GetRotationVelocity());
      }
    else
      {
      this->Internal->par->getParGF().VROT = "-1";
      }

    this->Internal->par->getParGF().VRAD = DoubleToString(pnode->GetRadialVelocity());
    this->Internal->par->getParGF().VVERT = DoubleToString(pnode->GetVerticalVelocity());
    this->Internal->par->getParGF().DVDZ = DoubleToString(pnode->GetVerticalRotationalGradient());
    this->Internal->par->getParGF().ZCYL = DoubleToString(pnode->GetVerticalRotationalGradientHeight());

    if (pnode->GetVelocityDispersion() > -1)
      {
      this->Internal->par->getParGF().VDISP = DoubleToString(pnode->GetVelocityDispersion());
      }
    else
      {
      this->Internal->par->getParGF().VDISP = "-1";
      }

    if (pnode->GetInclination() > -1)
      {
      this->Internal->par->getParGF().INC = DoubleToString(pnode->GetInclination());
      }
    else
      {
      this->Internal->par->getParGF().INC = "-1";
      }

    this->Internal->par->getParGF().DELTAINC = pnode->GetInclinationError();

    if (pnode->GetPositionAngle() > -1)
      {
      this->Internal->par->getParGF().PHI = DoubleToString(pnode->GetPositionAngle());
      }
    else
      {
      this->Internal->par->getParGF().PHI = "-1";
      }

    this->Internal->par->getParGF().DELTAPHI = pnode->GetPositionAngleError();

    if (pnode->GetScaleHeight() > -1)
      {
      this->Internal->par->getParGF().Z0 = DoubleToString(pnode->GetScaleHeight());
      }
    else
      {
      this->Internal->par->getParGF().Z0 = "-1";
      }

    if (pnode->GetDistance() > -1)
      {
      this->Internal->par->getParGF().DISTANCE = pnode->GetDistance();
      }
    else
      {
      this->Internal->par->getParGF().DISTANCE = -1;
      }

    if (pnode->GetColumnDensity() > -1)
      {
      this->Internal->par->getParGF().DENS = DoubleToString(pnode->GetColumnDensity());
      }
    else
      {
      this->Internal->par->getParGF().DENS = "-1";
      }

    if (pnode->GetNumberOfClounds() > -1)
      {
      this->Internal->par->getParGF().NV = pnode->GetNumberOfClounds();
      }
    else
      {
      this->Internal->par->getParGF().NV = -1;
      }

    string freeParameters;

    if (pnode->GetPositionAngleFit())
      {
      freeParameters += "PA";
      }

    if (pnode->GetRotationVelocityFit())
      {
      freeParameters += "VROT";
      }

    if (pnode->GetRadialVelocityFit())
      {
      freeParameters += "VRAD";
      }

    if (pnode->GetVelocityDispersionFit())
      {
      freeParameters += "VDISP";
      }

    if (pnode->GetInclinationFit())
      {
      freeParameters += "INC";
      }

    if (pnode->GetXCenterFit())
      {
      freeParameters += "XPOS";
      }

    if (pnode->GetYCenterFit())
      {
      freeParameters += "YPOS";
      }

    if (pnode->GetSystemicVelocityFit())
      {
      freeParameters += "VSYS";
      }

    if (pnode->GetScaleHeightFit())
      {
      freeParameters += "Z0";
      }

    if (this->Internal->par->getParGF().NRADII == -1 ||
        this->Internal->par->getParGF().RADSEP == -1 ||
        this->Internal->par->getParGF().XPOS == "-1" ||
        this->Internal->par->getParGF().YPOS == "-1" ||
        this->Internal->par->getParGF().VSYS == "-1" ||
        this->Internal->par->getParGF().VROT == "-1" ||
        this->Internal->par->getParGF().PHI == "-1" ||
        this->Internal->par->getParGF().INC == "-1")
      // the rings and the geometry are needed; the distance (from the systemic
      // velocity), the clouds, the scale height, the dispersion and the column
      // density have defaults in 3DBarolo (-1)
      {
      vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                    "Run again the estimation of the input parameters. ");
      return 0;
      }

    this->Internal->par->getParGF().FREE = freeParameters;
    this->Internal->par->getParGF().LTYPE = pnode->GetLayerType() + 1;
    this->Internal->par->getParGF().FTYPE = pnode->GetFittingFunction() + 1;
    this->Internal->par->getParGF().WFUNC = pnode->GetWeightingFunction();
    this->Internal->par->getParGF().CDENS = pnode->GetCloudsColumnDensity();

    this->Internal->par->getParGF().flagADRIFT = pnode->GetADRIFTCorrection();
    // same mask as the fit (OperateModel): the user's mask, otherwise the
    // source search (3DBarolo's default SEARCH mask). The LOCAL and AZIM
    // normalizations of the model depend on it.
    this->Internal->par->getParSE().flagSearch = false;
    this->Internal->par->setBeamFWHM(this->Internal->par->getBeamFWHM()/3600.);
    this->Internal->par->getParGF().TOL = pnode->GetTollerance();

    this->Internal->par->getParGF().NORM = pnode->GetNormalize();
    ApplyBBaroloParameters(this->Internal->par, pnode);

    // set header
    this->Internal->head->setBitpix(StringToInt(inputVolume->GetAttribute("SlicerAstro.BITPIX")));
    int numAxes = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
    this->Internal->head->setNumAx(numAxes);
    string inputString;

    for (int ii = 0; ii < numAxes; ii++)
      {
      inputString = "SlicerAstro.NAXIS";
      inputString += IntToString(ii + 1);
      this->Internal->head->setDimAx(ii,StringToLong(inputVolume->GetAttribute(inputString.c_str())));
      inputString = "SlicerAstro.CRPIX";
      inputString += IntToString(ii + 1);
      this->Internal->head->setCrpix(ii, StringToDouble(inputVolume->GetAttribute(inputString.c_str())));
      inputString = "SlicerAstro.CRVAL";
      inputString += IntToString(ii + 1);
      this->Internal->head->setCrval(ii, StringToDouble(inputVolume->GetAttribute(inputString.c_str())));
      inputString = "SlicerAstro.CDELT";
      inputString += IntToString(ii + 1);
      this->Internal->head->setCdelt(ii, StringToDouble(inputVolume->GetAttribute(inputString.c_str())));
      inputString = "SlicerAstro.CTYPE";
      inputString += IntToString(ii + 1);
      inputString = inputVolume->GetAttribute(inputString.c_str());
      this->Internal->head->setCtype(ii, inputString);
      inputString = "SlicerAstro.CUNIT";
      inputString += IntToString(ii + 1);
      inputString = inputVolume->GetAttribute(inputString.c_str());
      this->Internal->head->setCunit(ii, inputString);
      }

    this->Internal->head->setBmaj(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BMAJ")));
    this->Internal->head->setBmin(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BMIN")));
    this->Internal->head->setBpa(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BPA")));
    this->Internal->head->setBzero(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BZERO")));
    this->Internal->head->setBscale(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BSCALE")));
    this->Internal->head->setBlank(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BLANK")));
    this->Internal->head->setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA2")));
    this->Internal->head->setEpoch(StringToFloat(inputVolume->GetAttribute("SlicerAstro.EPOCH")));
    this->Internal->head->setFreq0(StringToDouble(inputVolume->GetAttribute("SlicerAstro.RESTFREQ")));
    inputString = inputVolume->GetAttribute("SlicerAstro.BUNIT");
    this->Internal->head->setBunit(inputString);
    inputString = inputVolume->GetAttribute("SlicerAstro.BTYPE");
    this->Internal->head->setBtype(inputString);
    inputString = inputVolume->GetAttribute("SlicerAstro.OBJECT");
    this->Internal->head->setName(inputString);
    inputString = inputVolume->GetAttribute("SlicerAstro.TELESCOP");
    this->Internal->head->setTelesc(inputString);
    inputString = inputVolume->GetAttribute("SlicerAstro.DUNIT3");
    this->Internal->head->setDunit3(inputString);
    this->Internal->head->setDrval3(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DRVAL3")));
    this->Internal->head->setDataMin(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMIN")));
    this->Internal->head->setDataMax(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMAX")));
    if(!this->Internal->head->saveWCSStruct(inputVolume->GetAstroVolumeDisplayNode()->GetWCSStruct()))
      {
      vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : the WCS copy failed!");
      return 0;
      }

    this->Internal->head->calcArea();

    switch (DataType)
      {
      case VTK_FLOAT:
        {
        if (!this->Internal->cubeF)
          {
          this->Internal->cubeF = new Cube<float>;
          }

        this->Internal->cubeF->saveParam(*this->Internal->par);
        this->Internal->cubeF->saveHead(*this->Internal->head);
        this->Internal->cubeF->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA2")));
        this->Internal->cubeF->setCube(static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer()), dims);
        if (maskPointer)
          {
          bool* mask = this->Internal->cubeF->allocateMask();
          for (vtkIdType ii = 0; ii < numElements; ii++)
            {
            mask[ii] = maskPointer[ii] > 0;
            }
          }

        this->Internal->fitF = new Model::Galfit<float>(this->Internal->cubeF);
        break;
        }
      case VTK_DOUBLE:
        {
        if (!this->Internal->cubeD)
          {
          this->Internal->cubeD = new Cube<double>;
          }

        this->Internal->cubeD->saveParam(*this->Internal->par);
        this->Internal->cubeD->saveHead(*this->Internal->head);
        this->Internal->cubeD->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA2")));
        this->Internal->cubeD->setCube(static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer()), dims);
        if (maskPointer)
          {
          bool* mask = this->Internal->cubeD->allocateMask();
          for (vtkIdType ii = 0; ii < numElements; ii++)
            {
            mask[ii] = maskPointer[ii] > 0;
            }
          }

        this->Internal->fitD = new Model::Galfit<double>(this->Internal->cubeD);
        break;
        }
      }
    }

  vtkMRMLTableNode* paramsTableNode = tableNode ? tableNode : pnode->GetParamsTableNode();
  if (!paramsTableNode)
    {
    vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                  "Unable to find the parameters table.");
    return 0;
    }

  switch (DataType)
    {
    case VTK_FLOAT:
      {
      if (!this->Internal->fitF || !this->Internal->fitF->Outrings())
        {
        vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find fitF pointer.");
        return 0;
        }

      float *inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer());
      if (!inFPixel)
        {
        vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find inFPixel pointer.");
        return 0;
        }

      this->Internal->fitF->Outrings()->xpos.clear();
      this->Internal->fitF->Outrings()->ypos.clear();
      this->Internal->fitF->Outrings()->vsys.clear();
      this->Internal->fitF->Outrings()->radii.clear();
      this->Internal->fitF->Outrings()->vrot.clear();
      this->Internal->fitF->Outrings()->vdisp.clear();
      this->Internal->fitF->Outrings()->dens.clear();
      this->Internal->fitF->Outrings()->z0.clear();
      this->Internal->fitF->Outrings()->inc.clear();
      this->Internal->fitF->Outrings()->phi.clear();
      this->Internal->fitF->Outrings()->pa.clear();
      this->Internal->fitF->Outrings()->vvert.clear();
      this->Internal->fitF->Outrings()->dvdz.clear();
      this->Internal->fitF->Outrings()->zcyl.clear();
      this->Internal->fitF->Outrings()->nr = pnode->GetNumberOfRings();
      this->Internal->fitF->Outrings()->vrad.clear();

      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        this->Internal->fitF->Outrings()->xpos.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnXPos));
        this->Internal->fitF->Outrings()->ypos.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnYPos));
        this->Internal->fitF->Outrings()->vsys.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVSys));
        this->Internal->fitF->Outrings()->radii.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
        this->Internal->fitF->Outrings()->vrot.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVRot));
        this->Internal->fitF->Outrings()->vrad.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVRad));
        this->Internal->fitF->Outrings()->vdisp.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVDisp));
        this->Internal->fitF->Outrings()->dens.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnDens));
        this->Internal->fitF->Outrings()->z0.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnZ0) /
                    KpcPerArc(this->Internal->par->getDistance()));
        this->Internal->fitF->Outrings()->inc.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnInc));
        this->Internal->fitF->Outrings()->phi.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnPhi));
        this->Internal->fitF->Outrings()->pa.push_back(0.);
        this->Internal->fitF->Outrings()->vvert.push_back(pnode->GetVerticalVelocity());
        this->Internal->fitF->Outrings()->dvdz.push_back(pnode->GetVerticalRotationalGradient());
        this->Internal->fitF->Outrings()->zcyl.push_back(pnode->GetVerticalRotationalGradientHeight());
        }

      // Check if Radii are in ascending order
      for (int ii = 1; ii < pnode->GetNumberOfRings(); ii++)
        {
        if (this->Internal->fitF->Outrings()->radii[ii] < this->Internal->fitF->Outrings()->radii[ii-1])
          {
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                        "Radii are not in ascending order.");
          return 0;
          }
        }

      this->Internal->fitF->In()->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA2")));
      if (this->Internal->AbortRequested)
        {
        this->cleanPointers();
        pnode->SetStatus(100);
        return 0;
        }
      // final model: radial profile and normalization (AZIM, LOCAL, NONE) of
      // 3DBarolo, as in its Galfit::writeModel
      this->Internal->modF = BuildFinalModel(this->Internal->fitF, pnode, 40, 90);
      SetProgress(pnode, 90);
      if (!this->Internal->modF || !this->Internal->modF->Out())
        {
        vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find modF pointer.");
        return 0;
        }

      this->Internal->modF->Out()->Head().setMinMax(0.,0.);
      float *outarray = this->Internal->modF->Out()->Array();
      if (!outarray)
        {
        vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find outarray pointer.");
        return 0;
        }

      float *outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer());
      if (!outFPixel)
        {
        vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find outFPixel pointer.");
        return 0;
        }
      for (vtkIdType ii = 0; ii < numElements; ii++)
        {
        *(outFPixel + ii) = *(outarray + ii);
        }

      if (inputVolume && residualVolume)
        {
        float *residualFPixel = static_cast<float*> (residualVolume->GetImageData()->GetScalarPointer());
        if (!residualFPixel)
          {
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                        "Unable to find residualFPixel pointer.");
          return 0;
          }
        for (vtkIdType ii = 0; ii < numElements; ii++)
          {
          if (*(outFPixel + ii) < 1.E-6)
            {
            *(residualFPixel + ii) = *(inFPixel + ii);
            }
          else
            {
            *(residualFPixel + ii) = 0.;
            }
          }
        residualFPixel = nullptr;
        }

      outarray = nullptr;
      if (this->Internal->modF != nullptr)
        {
        delete this->Internal->modF;
        this->Internal->modF = nullptr;
        }
      outFPixel = nullptr;
      inFPixel = nullptr;
      break;
      }
    case VTK_DOUBLE:
      {
      if (!this->Internal->fitD || !this->Internal->fitD->Outrings())
        {
        vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find fitD pointer.");
        return 0;
        }

      double *inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer());
      if (!inDPixel)
        {
        vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find inDPixel pointer.");
        return 0;
        }

      this->Internal->fitD->Outrings()->xpos.clear();
      this->Internal->fitD->Outrings()->ypos.clear();
      this->Internal->fitD->Outrings()->vsys.clear();
      this->Internal->fitD->Outrings()->radii.clear();
      this->Internal->fitD->Outrings()->vrot.clear();
      this->Internal->fitD->Outrings()->vdisp.clear();
      this->Internal->fitD->Outrings()->dens.clear();
      this->Internal->fitD->Outrings()->z0.clear();
      this->Internal->fitD->Outrings()->inc.clear();
      this->Internal->fitD->Outrings()->phi.clear();
      this->Internal->fitD->Outrings()->pa.clear();
      this->Internal->fitD->Outrings()->vvert.clear();
      this->Internal->fitD->Outrings()->dvdz.clear();
      this->Internal->fitD->Outrings()->zcyl.clear();
      this->Internal->fitD->Outrings()->nr = pnode->GetNumberOfRings();
      this->Internal->fitD->Outrings()->vrad.clear();

      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        this->Internal->fitD->Outrings()->xpos.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnXPos));
        this->Internal->fitD->Outrings()->ypos.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnYPos));
        this->Internal->fitD->Outrings()->vsys.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVSys));
        this->Internal->fitD->Outrings()->radii.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
        this->Internal->fitD->Outrings()->vrot.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVRot));
        this->Internal->fitD->Outrings()->vrad.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVRad));
        this->Internal->fitD->Outrings()->vdisp.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVDisp));
        this->Internal->fitD->Outrings()->dens.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnDens));
        this->Internal->fitD->Outrings()->z0.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnZ0) /
                    KpcPerArc(this->Internal->par->getDistance()));
        this->Internal->fitD->Outrings()->inc.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnInc));
        this->Internal->fitD->Outrings()->phi.push_back(
            TableValue(paramsTableNode, ii, vtkMRMLAstroModelingParametersNode::ParamsColumnPhi));
        this->Internal->fitD->Outrings()->pa.push_back(0.);
        this->Internal->fitD->Outrings()->vvert.push_back(pnode->GetVerticalVelocity());
        this->Internal->fitD->Outrings()->dvdz.push_back(pnode->GetVerticalRotationalGradient());
        this->Internal->fitD->Outrings()->zcyl.push_back(pnode->GetVerticalRotationalGradientHeight());
        }

      // Check if Radii are in ascending order
      for (int ii = 1; ii < pnode->GetNumberOfRings(); ii++)
        {
        if (this->Internal->fitD->Outrings()->radii[ii] < this->Internal->fitD->Outrings()->radii[ii-1])
          {
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                        "Radii are not in ascending order.");
          return 0;
          }
        }

      this->Internal->fitD->In()->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA2")));
      if (this->Internal->AbortRequested)
        {
        this->cleanPointers();
        pnode->SetStatus(100);
        return 0;
        }
      // final model: radial profile and normalization (AZIM, LOCAL, NONE) of
      // 3DBarolo, as in its Galfit::writeModel
      this->Internal->modD = BuildFinalModel(this->Internal->fitD, pnode, 40, 90);
      SetProgress(pnode, 90);
      if (!this->Internal->modD || !this->Internal->modD->Out())
        {
        vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find modF pointer.");
        return 0;
        }

      this->Internal->modD->Out()->Head().setMinMax(0.,0.);
      double *outarray = this->Internal->modD->Out()->Array();
      if (!outarray)
        {
        vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find outarray pointer.");
        return 0;
        }

      double *outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer());
      if (!outDPixel)
        {
        vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find outDPixel pointer.");
        return 0;
        }
      for (vtkIdType ii = 0; ii < numElements; ii++)
        {
        *(outDPixel + ii) = *(outarray + ii);
        }

      if (inputVolume && residualVolume)
        {
        double *residualDPixel = static_cast<double*> (residualVolume->GetImageData()->GetScalarPointer());
        if (!residualDPixel)
          {
          vtkModelingErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                        "Unable to find residualDPixel pointer.");
          return 0;
          }
        for (vtkIdType ii = 0; ii < numElements; ii++)
          {
          if (*(outDPixel + ii) < 1.E-6)
            {
            *(residualDPixel + ii) = *(inDPixel + ii);
            }
          else
            {
            *(residualDPixel + ii) = 0.;
            }
          }
        residualDPixel = nullptr;
        }

      outarray = nullptr;
      if (this->Internal->modD != nullptr)
        {
        delete this->Internal->modD;
        this->Internal->modD = nullptr;
        }
      outDPixel = nullptr;
      inDPixel = nullptr;
      break;
      }
    }

  return 1;
}
