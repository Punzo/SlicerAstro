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
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroReprojectLogic.h>

// SlicerAstro includes
#include <vtkAstroBeams.h>
#include <vtkAstroReprojector.h>
#include <vtkFITSReader.h>

// MRML includes
#include <vtkMRMLAstroReprojectParametersNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>

// WCSLIB includes
#include <wcslib.h>

// STD includes
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <sstream>

//----------------------------------------------------------------------------
class vtkSlicerAstroReprojectLogic::vtkInternal
{
public:
  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
};

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroReprojectLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroReprojectLogic::vtkSlicerAstroReprojectLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerAstroReprojectLogic::~vtkSlicerAstroReprojectLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroReprojectLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerAstroReprojectLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic.GetPointer();
}

//----------------------------------------------------------------------------
void vtkSlicerAstroReprojectLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkSlicerAstroReprojectLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }
  vtkMRMLAstroReprojectParametersNode* pNode = vtkMRMLAstroReprojectParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

namespace
{
const double DEG2RAD = 3.14159265358979323846 / 180.;

//----------------------------------------------------------------------------
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

//----------------------------------------------------------------------------
double AttributeDouble(vtkMRMLNode* node, const char* key, double defaultValue)
{
  const char* value = node ? node->GetAttribute(key) : nullptr;
  if (!value || !*value || !strcmp(value, "UNDEFINED"))
    {
    return defaultValue;
    }
  char* end = nullptr;
  const double result = std::strtod(value, &end);
  return end != value ? result : defaultValue;
}

//----------------------------------------------------------------------------
/// wcsprm owned by a unique_ptr (vtkAstroReprojector::FreeWCS)
struct WCSDeleter
{
  void operator()(wcsprm* wcs) const { vtkAstroReprojector::FreeWCS(wcs); }
};
typedef std::unique_ptr<wcsprm, WCSDeleter> WCSPointer;

WCSPointer NewWCS(int naxis)
{
  wcsprm* wcs = static_cast<wcsprm*>(calloc(1, sizeof(wcsprm)));
  wcs->flag = -1;
  if (wcsini(1, naxis, wcs))
    {
    free(wcs);
    return WCSPointer();
    }
  return WCSPointer(wcs);
}

//----------------------------------------------------------------------------
/// Axes (1-based indices of source) of a WCS, in the PC form (CDELT and
/// PCi_j, from CDi_j or CROTAi if needed)
WCSPointer ExtractAxes(const wcsprm* source, const std::vector<int>& axes)
{
  if (!source)
    {
    return WCSPointer();
    }
  wcsprm* wcs = static_cast<wcsprm*>(calloc(1, sizeof(wcsprm)));
  wcs->flag = -1;
  int nsub = static_cast<int>(axes.size());
  std::vector<int> axisList(axes);
  if (wcssub(1, const_cast<wcsprm*>(source), &nsub, axisList.data(), wcs) || wcsset(wcs))
    {
    wcsfree(wcs);
    free(wcs);
    return WCSPointer();
    }
  if (wcs->altlin & 6)
    {
    // the linear transformation of wcsset (pixel steps and PC matrix)
    const int n = wcs->naxis;
    for (int i = 0; i < n; i++)
      {
      wcs->cdelt[i] = wcs->lin.cdelt[i];
      for (int j = 0; j < n; j++)
        {
        wcs->pc[i * n + j] = wcs->lin.pc[i * n + j];
        }
      }
    wcs->altlin = 1;
    wcs->flag = 0;
    wcsset(wcs);
    }
  return WCSPointer(wcs);
}

//----------------------------------------------------------------------------
/// One spectral axis of a spectral grid
WCSPointer MakeSpectralWCS(const vtkAstroReprojector::SpectralGrid& grid)
{
  WCSPointer wcs = NewWCS(1);
  if (!wcs)
    {
    return wcs;
    }
  std::string type = grid.Type.substr(0, 4);
  std::transform(type.begin(), type.end(), type.begin(), ::toupper);
  std::snprintf(wcs->ctype[0], 72, "%s", type.c_str());
  std::snprintf(wcs->cunit[0], 72, "%s", type == "FREQ" ? "Hz" : (type == "WAVE" || type == "AWAV") ? "m" : "m/s");
  wcs->crval[0] = grid.Start;
  wcs->cdelt[0] = grid.Step;
  wcs->crpix[0] = 1.;
  if (grid.RestFrequency > 0.)
    {
    wcs->restfrq = grid.RestFrequency;
    }
  std::snprintf(wcs->specsys, 72, "%s", grid.SpecSys.c_str());
  if (wcsset(wcs.get()))
    {
    return WCSPointer();
    }
  return wcs;
}

//----------------------------------------------------------------------------
/// celestial (2 axes) and spectral (1 axis, optional) -> output WCS
WCSPointer CombineWCS(const wcsprm* celestial, const wcsprm* spectral)
{
  if (!celestial || celestial->naxis != 2)
    {
    return WCSPointer();
    }
  const int n = spectral ? 3 : 2;
  WCSPointer wcs = NewWCS(n);
  if (!wcs)
    {
    return wcs;
    }
  for (int i = 0; i < 2; i++)
    {
    std::snprintf(wcs->ctype[i], 72, "%s", celestial->ctype[i]);
    std::snprintf(wcs->cunit[i], 72, "%s", celestial->cunit[i]);
    wcs->crval[i] = celestial->crval[i];
    wcs->cdelt[i] = celestial->cdelt[i];
    wcs->crpix[i] = celestial->crpix[i];
    for (int j = 0; j < 2; j++)
      {
      wcs->pc[i * n + j] = celestial->pc[i * 2 + j];
      }
    }
  for (int k = 0; k < celestial->npv && wcs->npv < wcs->npvmax; k++)
    {
    wcs->pv[wcs->npv++] = celestial->pv[k];
    }
  wcs->lonpole = celestial->lonpole;
  wcs->latpole = celestial->latpole;
  wcs->equinox = celestial->equinox;
  std::snprintf(wcs->radesys, 72, "%s", celestial->radesys);
  std::snprintf(wcs->dateobs, 72, "%s", celestial->dateobs);
  wcs->mjdobs = celestial->mjdobs;
  wcs->mjdavg = celestial->mjdavg;
  for (int index = 0; index < 6; index++)
    {
    wcs->obsgeo[index] = celestial->obsgeo[index];
    }
  if (spectral)
    {
    std::snprintf(wcs->ctype[2], 72, "%s", spectral->ctype[0]);
    std::snprintf(wcs->cunit[2], 72, "%s", spectral->cunit[0]);
    wcs->crval[2] = spectral->crval[0];
    wcs->cdelt[2] = spectral->cdelt[0];
    wcs->crpix[2] = spectral->crpix[0];
    wcs->restfrq = spectral->restfrq;
    wcs->restwav = spectral->restwav;
    std::snprintf(wcs->specsys, 72, "%s", spectral->specsys);
    std::snprintf(wcs->ssysobs, 72, "%s", spectral->ssysobs);
    wcs->velosys = spectral->velosys;
    for (int k = 0; k < spectral->npv && wcs->npv < wcs->npvmax; k++)
      {
      pvcard card = spectral->pv[k];
      card.i = 3;
      wcs->pv[wcs->npv++] = card;
      }
    if (!(wcs->mjdobs > 0.) && spectral->mjdobs > 0.)
      {
      wcs->mjdobs = spectral->mjdobs;
      std::snprintf(wcs->dateobs, 72, "%s", spectral->dateobs);
      for (int index = 0; index < 6; index++)
        {
        wcs->obsgeo[index] = spectral->obsgeo[index];
        }
      }
    }
  wcs->altlin = 1;
  if (wcsset(wcs.get()))
    {
    return WCSPointer();
    }
  return wcs;
}

//----------------------------------------------------------------------------
/// Resampled axes: pixel steps times factor (same field: size n / factor)
void ResampleAxes(wcsprm* wcs, const std::vector<int>& axes, double factor, std::vector<int>& sizes)
{
  for (std::size_t index = 0; index < axes.size(); index++)
    {
    const int axis = axes[index];
    const int size = sizes[index];
    const int newSize = std::max(1, static_cast<int>(std::ceil(size / factor - 1.e-9)));
    // the edges of the first pixels coincide: (crpix - 0.5) / factor + 0.5
    wcs->crpix[axis] = (wcs->crpix[axis] - 0.5) / factor + 0.5;
    wcs->cdelt[axis] *= factor;
    sizes[index] = newSize;
    }
  wcs->flag = 0;
  wcsset(wcs);
}

//----------------------------------------------------------------------------
std::string FormatAngle(double degrees)
{
  std::ostringstream text;
  text.precision(3);
  if (degrees >= 1.)
    {
    text << degrees << " deg";
    }
  else if (degrees >= 1. / 60.)
    {
    text << degrees * 60. << "'";
    }
  else
    {
    text << degrees * 3600. << "\"";
    }
  return text.str();
}

//----------------------------------------------------------------------------
/// Why the WCS of a volume can not be used for a reprojection (empty if it can)
std::string WCSIssue(vtkMRMLAstroVolumeNode* volume)
{
  if (!volume)
    {
    return "volume not found";
    }
  const std::string name = volume->GetName() ? volume->GetName() : "";
  const char* dss = volume->GetAttribute("SlicerAstro.DSS");
  const char* cdelt1 = volume->GetAttribute("SlicerAstro.CDELT1");
  const char* cdelt2 = volume->GetAttribute("SlicerAstro.CDELT2");
  if (dss && cdelt1 && cdelt2 && !strcmp(dss, "1") && !strcmp(cdelt1, "1.0") && !strcmp(cdelt2, "1.0"))
    {
    return name + " has a WCS from the old plate solution of the DSS (PPO, AMDX, AMDY), "
                  "which is not supported";
    }
  vtkMRMLAstroVolumeDisplayNode* display = volume->GetAstroVolumeDisplayNode();
  if (!volume->GetImageData() || !display || !display->GetWCSStruct() || !display->GetSpace() ||
      !strcmp(display->GetSpace(), "IJK") || display->GetWCSStruct()->lng < 0)
    {
    return name + ": no data or no celestial coordinates (WCS)";
    }
  if (volume->GetImageData()->GetNumberOfScalarComponents() != 1)
    {
    return name + ": data with more than one component";
    }
  return std::string();
}

//----------------------------------------------------------------------------
/// Inputs of the parameters node as reprojector inputs
bool MakeInputs(vtkMRMLScene* scene, vtkMRMLAstroReprojectParametersNode* pnode,
                const std::vector<vtkMRMLAstroVolumeNode*>& volumes,
                std::vector<vtkAstroReprojector::Input>& inputs, std::string& error)
{
  inputs.clear();
  const std::vector<std::string> beams = SplitList(pnode->GetPrimaryBeamVolumeNodeIDs());
  const std::vector<std::string> noises = SplitList(pnode->GetInputNoises());
  for (std::size_t index = 0; index < volumes.size(); index++)
    {
    vtkMRMLAstroVolumeNode* volume = volumes[index];
    error = WCSIssue(volume);
    if (!error.empty())
      {
      return false;
      }
    vtkMRMLAstroVolumeDisplayNode* display = volume->GetAstroVolumeDisplayNode();
    vtkAstroReprojector::Input input;
    input.Data = volume->GetImageData();
    input.WCS = display->GetWCSStruct();
    input.Name = volume->GetName();
    const char* bunit = volume->GetAttribute("SlicerAstro.BUNIT");
    input.BUnit = bunit ? bunit : "";
    input.Beam[0] = AttributeDouble(volume, "SlicerAstro.BMAJ", 0.);
    input.Beam[1] = AttributeDouble(volume, "SlicerAstro.BMIN", 0.);
    input.Beam[2] = AttributeDouble(volume, "SlicerAstro.BPA", 0.);
    if (!(input.Beam[0] > 0.) || !(input.Beam[1] > 0.))
      {
      input.Beam[0] = input.Beam[1] = input.Beam[2] = 0.;
      }
    input.Noise = AttributeDouble(volume, "SlicerAstro.DisplayThreshold", 0.);
    if (index < noises.size() && !noises[index].empty())
      {
      input.Noise = std::atof(noises[index].c_str());
      }
    if (index < beams.size() && !beams[index].empty() && scene)
      {
      vtkMRMLVolumeNode* beam = vtkMRMLVolumeNode::SafeDownCast(scene->GetNodeByID(beams[index].c_str()));
      input.PrimaryBeam = beam ? beam->GetImageData() : nullptr;
      }
    // pointing centre: OBSRA/OBSDEC if in the header (the data centre otherwise)
    input.Pointing[0] = AttributeDouble(volume, "SlicerAstro.OBSRA", std::numeric_limits<double>::quiet_NaN());
    input.Pointing[1] = AttributeDouble(volume, "SlicerAstro.OBSDEC", std::numeric_limits<double>::quiet_NaN());
    input.DishDiameter = pnode->GetDishDiameter();
    input.Frequency = AttributeDouble(volume, "SlicerAstro.ObservedFrequency", 0.);
    inputs.push_back(input);
    }
  if (inputs.empty())
    {
    error = "no input volume";
    return false;
    }
  return true;
}

//----------------------------------------------------------------------------
/// FITS header cards of a WCS (wcshdo): keyword -> value (strings unquoted)
std::map<std::string, std::string> HeaderKeywords(wcsprm* wcs)
{
  std::map<std::string, std::string> keywords;
  int nkeyrec = 0;
  char* header = nullptr;
  if (wcshdo(WCSHDO_P17, wcs, &nkeyrec, &header) || !header)
    {
    return keywords;
    }
  for (int record = 0; record < nkeyrec; record++)
    {
    const std::string card(header + 80 * record, 80);
    if (card.size() < 10 || card.compare(8, 2, "= ") != 0)
      {
      continue;
      }
    std::string key = card.substr(0, 8);
    key.erase(key.find_last_not_of(' ') + 1);
    std::string value = card.substr(10);
    const std::size_t firstQuote = value.find('\'');
    if (firstQuote != std::string::npos)
      {
      const std::size_t lastQuote = value.find('\'', firstQuote + 1);
      value = value.substr(firstQuote + 1, lastQuote == std::string::npos ? std::string::npos : lastQuote - firstQuote - 1);
      value.erase(value.find_last_not_of(' ') + 1);
      }
    else
      {
      const std::size_t slash = value.find('/');
      if (slash != std::string::npos)
        {
        value = value.substr(0, slash);
        }
      value.erase(0, value.find_first_not_of(' '));
      value.erase(value.find_last_not_of(' ') + 1);
      }
    keywords[key] = value;
    }
  wcsdealloc(header);
  return keywords;
}

//----------------------------------------------------------------------------
bool IsWCSKeyword(const std::string& key)
{
  static const char* prefixes[] = {"CTYPE", "CRVAL", "CDELT", "CRPIX", "CUNIT", "CROTA", "CNAME", "CRDER",
                                   "CSYER", "LONPOLE", "LATPOLE", "RESTFRQ", "RESTFREQ", "RESTWAV", "SPECSYS",
                                   "SSYSOBS", "SSYSSRC", "VELOSYS", "ZSOURCE", "EQUINOX", "EPOCH", "RADESYS",
                                   "RADECSYS", "WCSNAME", "WCSAXES", "VELREF", "ALTRVAL", "ALTRPIX", "NAXIS",
                                   "DATAMIN", "DATAMAX", "DRVAL", "DUNIT"};
  for (const char* prefix : prefixes)
    {
    if (key.compare(0, std::strlen(prefix), prefix) == 0)
      {
      return true;
      }
    }
  // PCi_j, CDi_j, PVi_m, PSi_m (and the old PC00i00j)
  return key.size() > 2 && (key.compare(0, 2, "PC") == 0 || key.compare(0, 2, "CD") == 0 ||
                            key.compare(0, 2, "PV") == 0 || key.compare(0, 2, "PS") == 0) &&
         std::isdigit(static_cast<unsigned char>(key[2]));
}

//----------------------------------------------------------------------------
std::string NumberString(double value)
{
  std::ostringstream text;
  text << std::setprecision(17) << value;
  return text.str();
}

//----------------------------------------------------------------------------
/// Velocity type for the display of a spectral axis (vtkFITSReader)
std::string DisplayVelocityType(const std::string& ctype)
{
  const std::string type = ctype.substr(0, 4);
  if (type == "VRAD" || type == "VOPT")
    {
    return type;
    }
  if (type == "FREQ" || type == "AFRQ" || type == "ENER" || type == "WAVN")
    {
    return "VRAD";
    }
  return "VOPT";
}

//----------------------------------------------------------------------------
/// Output data: image, WCS (display: spectral axis as a velocity, as
/// vtkFITSReader; the keywords of the output spectral axis are written back
/// when the volume is saved), attributes, geometry
bool SetOutputVolume(vtkMRMLAstroVolumeNode* volume, vtkImageData* image, const wcsprm* outputWCS,
                     const std::string& bunit, const double beam[3], std::string& error)
{
  vtkMRMLAstroVolumeDisplayNode* display = volume->GetAstroVolumeDisplayNode();
  if (!display)
    {
    error = "the output volume has no display node";
    return false;
    }
  int dims[3];
  image->GetDimensions(dims);
  const int naxis = outputWCS->naxis;

  // display WCS: spectral axis as a velocity
  std::vector<int> allAxes;
  for (int axis = 1; axis <= naxis; axis++)
    {
    allAxes.push_back(axis);
    }
  WCSPointer displayWCS = ExtractAxes(outputWCS, allAxes);
  WCSPointer savedWCS = ExtractAxes(outputWCS, allAxes);
  if (!displayWCS || !savedWCS)
    {
    error = "invalid output WCS";
    return false;
    }
  bool convertedSpectral = false;
  if (naxis > 2 && displayWCS->spec >= 0)
    {
    const std::string ctype = displayWCS->ctype[displayWCS->spec];
    const std::string velocityType = DisplayVelocityType(ctype);
    if (ctype.compare(0, 4, velocityType) != 0 && displayWCS->restfrq > 0.)
      {
      int index = displayWCS->spec;
      char ctypeS[9];
      std::snprintf(ctypeS, sizeof(ctypeS), "%s-???", velocityType.c_str());
      if (!wcssptr(displayWCS.get(), &index, ctypeS) && !wcsset(displayWCS.get()))
        {
        convertedSpectral = true;
        }
      }
    }

  // attributes: remove the WCS keywords of the clone, set the new ones
  std::vector<std::string> names = volume->GetAttributeNames();
  for (const std::string& name : names)
    {
    if ((name.compare(0, 12, "SlicerAstro.") == 0 && IsWCSKeyword(name.substr(12))) ||
        name.compare(0, 13, "FITSOriginal.") == 0 || name.compare(0, 14, "FITSConverted.") == 0)
      {
      volume->RemoveAttribute(name.c_str());
      }
    }
  const std::map<std::string, std::string> keywords = HeaderKeywords(displayWCS.get());
  for (const std::pair<const std::string, std::string>& keyword : keywords)
    {
    if (keyword.first == "WCSAXES" || keyword.first == "RESTFRQ" || keyword.first == "MJDREF")
      {
      continue;
      }
    volume->SetAttribute(("SlicerAstro." + keyword.first).c_str(), keyword.second.c_str());
    }
  // all the axis keywords (WCSLIB omits the default values)
  for (int axis = 0; axis < naxis; axis++)
    {
    const std::string n = std::to_string(axis + 1);
    volume->SetAttribute(("SlicerAstro.CDELT" + n).c_str(), NumberString(displayWCS->cdelt[axis]).c_str());
    volume->SetAttribute(("SlicerAstro.CRPIX" + n).c_str(), NumberString(displayWCS->crpix[axis]).c_str());
    volume->SetAttribute(("SlicerAstro.CRVAL" + n).c_str(), NumberString(displayWCS->crval[axis]).c_str());
    volume->SetAttribute(("SlicerAstro.CTYPE" + n).c_str(), displayWCS->ctype[axis]);
    volume->SetAttribute(("SlicerAstro.CUNIT" + n).c_str(), displayWCS->cunit[axis]);
    for (int other = 0; other < naxis; other++)
      {
      volume->SetAttribute(("SlicerAstro.PC" + n + "_" + std::to_string(other + 1)).c_str(),
                           NumberString(displayWCS->pc[axis * naxis + other]).c_str());
      }
    }
  volume->SetAttribute("SlicerAstro.NAXIS", std::to_string(naxis).c_str());
  for (int axis = 0; axis < naxis; axis++)
    {
    volume->SetAttribute(("SlicerAstro.NAXIS" + std::to_string(axis + 1)).c_str(), std::to_string(dims[axis]).c_str());
    }
  if (displayWCS->restfrq > 0.)
    {
    volume->SetAttribute("SlicerAstro.RESTFREQ", NumberString(displayWCS->restfrq).c_str());
    }
  // rotation of the celestial axes (CROTA2, used by SlicerAstro), from the
  // linear transformation, as for the files read by vtkFITSReader
  if (displayWCS->lng >= 0 && displayWCS->lat >= 0)
    {
    const int lng = displayWCS->lng, lat = displayWCS->lat;
    const double* pc = displayWCS->lin.pc;
    const double* cdelt = displayWCS->lin.cdelt;
    double rotation = vtkFITSReader::ComputeCelestialRotation(
      cdelt[lng] * pc[lng * naxis + lng], cdelt[lng] * pc[lng * naxis + lat],
      cdelt[lat] * pc[lat * naxis + lng], cdelt[lat] * pc[lat * naxis + lat]);
    if (std::fabs(rotation) < 1.e-10)
      {
      rotation = 0.;
      }
    volume->SetAttribute("SlicerAstro.CROTA2", NumberString(rotation).c_str());
    }
  if (convertedSpectral)
    {
    // the spectral axis of the output is written back when saved
    // (vtkMRMLAstroVolumeStorageNode, as for the axes converted by the reader)
    const std::map<std::string, std::string> saved = HeaderKeywords(savedWCS.get());
    const std::string spectralAxis = std::to_string(savedWCS->spec + 1);
    const std::vector<std::string> keys = vtkFITSReader::GetSpectralAxisKeywords();
    for (const std::string& key : keys)
      {
      std::string savedKey = key;
      if (key == "RESTFREQ")
        {
        savedKey = "RESTFRQ";
        }
      const std::map<std::string, std::string>::const_iterator value = saved.find(savedKey);
      if (key.compare(0, 6, "CDELT3") == 0)
        {
        volume->SetAttribute(("FITSOriginal." + key).c_str(), NumberString(savedWCS->cdelt[savedWCS->spec]).c_str());
        }
      else if (key == "CRPIX3")
        {
        volume->SetAttribute(("FITSOriginal." + key).c_str(), NumberString(savedWCS->crpix[savedWCS->spec]).c_str());
        }
      else if (key == "CRVAL3")
        {
        volume->SetAttribute(("FITSOriginal." + key).c_str(), NumberString(savedWCS->crval[savedWCS->spec]).c_str());
        }
      else if (key == "CTYPE3")
        {
        volume->SetAttribute(("FITSOriginal." + key).c_str(), savedWCS->ctype[savedWCS->spec]);
        }
      else if (key == "CUNIT3")
        {
        volume->SetAttribute(("FITSOriginal." + key).c_str(), savedWCS->cunit[savedWCS->spec]);
        }
      else if (key == "PC3_3")
        {
        volume->SetAttribute(("FITSOriginal." + key).c_str(), "1");
        }
      else if (value != saved.end() && key != "RESTFRQ")
        {
        volume->SetAttribute(("FITSOriginal." + key).c_str(), value->second.c_str());
        }
      const char* current = volume->GetAttribute(("SlicerAstro." + key).c_str());
      volume->SetAttribute(("FITSConverted." + key).c_str(), current);
      }
    }

  // units and beam
  volume->SetAttribute("SlicerAstro.BUNIT", bunit.c_str());
  if (beam[0] > 0. && beam[1] > 0.)
    {
    volume->SetAttribute("SlicerAstro.BMAJ", NumberString(beam[0]).c_str());
    volume->SetAttribute("SlicerAstro.BMIN", NumberString(beam[1]).c_str());
    volume->SetAttribute("SlicerAstro.BPA", NumberString(beam[2]).c_str());
    }

  // geometry (as vtkFITSReader, centred)
  const double cdelt[3] = {displayWCS->cdelt[0], displayWCS->cdelt[1], naxis > 2 ? displayWCS->cdelt[2] : 0.};
  vtkNew<vtkMatrix4x4> rasToIjk;
  vtkFITSReader::ComputeRasToIjkMatrix(naxis, dims, cdelt, displayWCS->crval[1], true, rasToIjk);
  image->SetSpacing(1., 1., 1.);
  image->SetOrigin(0., 0., 0.);
  volume->SetRASToIJKMatrix(rasToIjk);
  volume->SetAndObserveImageData(image);

  display->SetWCSStruct(displayWCS.get());
  volume->UpdateRangeAttributes();
  volume->UpdateDisplayThresholdAttributes();
  volume->Modified();
  return true;
}
} // end of anonymous namespace

//----------------------------------------------------------------------------
std::vector<vtkMRMLAstroVolumeNode*> vtkSlicerAstroReprojectLogic::GetInputVolumes(vtkMRMLAstroReprojectParametersNode* pnode)
{
  std::vector<vtkMRMLAstroVolumeNode*> volumes;
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene || !pnode)
    {
    return volumes;
    }
  std::vector<std::string> ids = SplitList(pnode->GetInputVolumeNodeIDs());
  if (ids.empty() && pnode->GetInputVolumeNodeID())
    {
    ids.push_back(pnode->GetInputVolumeNodeID());
    }
  for (const std::string& id : ids)
    {
    vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(id.c_str()));
    if (volume && std::find(volumes.begin(), volumes.end(), volume) == volumes.end())
      {
      volumes.push_back(volume);
      }
    }
  return volumes;
}

//----------------------------------------------------------------------------
wcsprm* vtkSlicerAstroReprojectLogic::ComputeOutputGrid(vtkMRMLAstroReprojectParametersNode* pnode, int dimensions[3],
                                                        std::string& error)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  const std::vector<vtkMRMLAstroVolumeNode*> volumes = this->GetInputVolumes(pnode);
  std::vector<vtkAstroReprojector::Input> inputs;
  if (!pnode || !MakeInputs(scene, pnode, volumes, inputs, error))
    {
    return nullptr;
    }
  vtkMRMLAstroVolumeNode* reference = vtkMRMLAstroVolumeNode::SafeDownCast(
    pnode->GetReferenceVolumeNodeID() ? scene->GetNodeByID(pnode->GetReferenceVolumeNodeID()) : nullptr);
  if (reference && !WCSIssue(reference).empty())
    {
    error = WCSIssue(reference);
    return nullptr;
    }
  const wcsprm* referenceWCS = reference ? reference->GetAstroVolumeDisplayNode()->GetWCSStruct() : nullptr;
  int referenceDims[3] = {1, 1, 1};
  if (reference && reference->GetImageData())
    {
    reference->GetImageData()->GetDimensions(referenceDims);
    }
  const wcsprm* firstWCS = inputs[0].WCS;
  int firstDims[3] = {1, 1, 1};
  inputs[0].Data->GetDimensions(firstDims);
  const bool cube = firstWCS->spec >= 0 && firstWCS->naxis > 2 && firstDims[2] > 1;

  // --- celestial axes
  WCSPointer celestial;
  int sizes[2] = {0, 0};
  const int spatialMode = pnode->GetSpatialGridMode();
  if (spatialMode == vtkMRMLAstroReprojectParametersNode::SpatialReference ||
      spatialMode == vtkMRMLAstroReprojectParametersNode::SpatialInput ||
      spatialMode == vtkMRMLAstroReprojectParametersNode::SpatialResample)
    {
    const bool useReference = spatialMode == vtkMRMLAstroReprojectParametersNode::SpatialReference;
    const wcsprm* source = useReference ? referenceWCS : firstWCS;
    const int* sourceDims = useReference ? referenceDims : firstDims;
    if (!source || source->lng < 0)
      {
      error = useReference ? "select a reference volume with celestial coordinates" : "the input has no celestial coordinates";
      return nullptr;
      }
    celestial = ExtractAxes(source, {source->lng + 1, source->lat + 1});
    sizes[0] = sourceDims[source->lng];
    sizes[1] = sourceDims[source->lat];
    if (celestial && spatialMode == vtkMRMLAstroReprojectParametersNode::SpatialResample)
      {
      const double factor = pnode->GetSpatialResampleFactor();
      if (!(factor > 0.))
        {
        error = "invalid pixel size factor";
        return nullptr;
        }
      std::vector<int> axisSizes = {sizes[0], sizes[1]};
      ResampleAxes(celestial.get(), {0, 1}, factor, axisSizes);
      sizes[0] = axisSizes[0];
      sizes[1] = axisSizes[1];
      }
    }
  else
    {
    vtkAstroReprojector::CelestialGrid grid;
    grid.Frame = pnode->GetCelestialFrame();
    grid.Equinox = pnode->GetEquinox();
    grid.Projection = pnode->GetProjection();
    grid.Rotation = pnode->GetRotation();
    if (spatialMode == vtkMRMLAstroReprojectParametersNode::SpatialManual)
      {
      grid.Center[0] = pnode->GetCenterLongitude();
      grid.Center[1] = pnode->GetCenterLatitude();
      grid.PixelSize[0] = pnode->GetPixelSizeX() / 3600.;
      grid.PixelSize[1] = pnode->GetPixelSizeY() / 3600.;
      grid.Size[0] = pnode->GetSizeX();
      grid.Size[1] = pnode->GetSizeY();
      if (!(grid.PixelSize[0] > 0.) || !(grid.PixelSize[1] > 0.) || grid.Size[0] < 1 || grid.Size[1] < 1)
        {
        error = "the manual grid needs positive pixel sizes and sizes";
        return nullptr;
        }
      }
    else if (!vtkAstroReprojector::FitCelestialGrid(inputs,
               spatialMode == vtkMRMLAstroReprojectParametersNode::SpatialUnion,
               pnode->GetPixelSizeX() / 3600., grid, error))
      {
      return nullptr;
      }
    wcsprm* made = vtkAstroReprojector::MakeWCS(grid, nullptr, firstWCS);
    celestial = WCSPointer(made);
    sizes[0] = grid.Size[0];
    sizes[1] = grid.Size[1];
    }
  if (!celestial)
    {
    error = "invalid celestial grid (projection, frame or values)";
    return nullptr;
    }

  // --- spectral axis
  WCSPointer spectral;
  int channels = 1;
  const int spectralMode = pnode->GetSpectralGridMode();
  bool anyCube = false;
  for (const vtkAstroReprojector::Input& input : inputs)
    {
    int dims[3];
    input.Data->GetDimensions(dims);
    anyCube = anyCube || (input.WCS->spec >= 0 && input.WCS->naxis > 2 && dims[2] > 1);
    }
  if (anyCube)
    {
    if (spectralMode == vtkMRMLAstroReprojectParametersNode::SpectralInput ||
        spectralMode == vtkMRMLAstroReprojectParametersNode::SpectralReference ||
        spectralMode == vtkMRMLAstroReprojectParametersNode::SpectralResample)
      {
      const bool useReference = spectralMode == vtkMRMLAstroReprojectParametersNode::SpectralReference;
      const wcsprm* source = useReference ? referenceWCS : firstWCS;
      const int* sourceDims = useReference ? referenceDims : firstDims;
      if (!source || source->spec < 0 || source->naxis < 3)
        {
        error = useReference ? "the reference volume has no spectral axis" : "the first input has no spectral axis";
        return nullptr;
        }
      spectral = ExtractAxes(source, {source->spec + 1});
      channels = sourceDims[source->spec];
      if (spectral && spectralMode == vtkMRMLAstroReprojectParametersNode::SpectralResample)
        {
        const double factor = pnode->GetSpectralResampleFactor();
        if (!(factor > 0.))
          {
          error = "invalid channel width factor";
          return nullptr;
          }
        std::vector<int> axisSizes = {channels};
        ResampleAxes(spectral.get(), {0}, factor, axisSizes);
        channels = axisSizes[0];
        }
      }
    else
      {
      vtkAstroReprojector::SpectralGrid grid;
      grid.Type = pnode->GetSpectralType();
      grid.SpecSys = pnode->GetSpectralFrame();
      grid.RestFrequency = pnode->GetRestFrequency();
      if (spectralMode == vtkMRMLAstroReprojectParametersNode::SpectralManual)
        {
        grid.Start = pnode->GetSpectralStart();
        grid.Step = pnode->GetSpectralStep();
        grid.Count = pnode->GetSpectralCount();
        if (grid.Step == 0. || grid.Count < 1)
          {
          error = "the manual channels need a non-zero width and a number of channels";
          return nullptr;
          }
        }
      else if (!vtkAstroReprojector::FitSpectralGrid(inputs,
                 spectralMode == vtkMRMLAstroReprojectParametersNode::SpectralUnion,
                 pnode->GetSpectralStep(), grid, error))
        {
        return nullptr;
        }
      spectral = MakeSpectralWCS(grid);
      channels = grid.Count;
      }
    if (!spectral)
      {
      error = "invalid spectral axis (type, unit or rest frequency)";
      return nullptr;
      }
    }
  (void)cube;

  WCSPointer combined = CombineWCS(celestial.get(), spectral.get());
  if (!combined)
    {
    error = "the output WCS could not be made";
    return nullptr;
    }
  dimensions[0] = sizes[0];
  dimensions[1] = sizes[1];
  dimensions[2] = channels;
  return combined.release();
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroReprojectLogic::DescribeOutputGrid(vtkMRMLAstroReprojectParametersNode* pnode)
{
  int dims[3] = {0, 0, 0};
  std::string error;
  WCSPointer wcs(this->ComputeOutputGrid(pnode, dims, error));
  if (!wcs)
    {
    this->LastError = error;
    return "Output grid: " + error;
    }
  this->LastError.clear();
  std::ostringstream text;
  vtkAstroReprojector::CelestialGrid celestial;
  vtkAstroReprojector::GetCelestialGrid(wcs.get(), dims, celestial);
  text << "Output: " << dims[0] << " x " << dims[1];
  if (wcs->naxis > 2)
    {
    text << " x " << dims[2];
    }
  text << " voxels; pixels " << FormatAngle(celestial.PixelSize[0]);
  if (std::fabs(celestial.PixelSize[0] - celestial.PixelSize[1]) > 1.e-9 * celestial.PixelSize[0])
    {
    text << " x " << FormatAngle(celestial.PixelSize[1]);
    }
  text << ", " << celestial.Projection << " " << celestial.Frame;
  if (celestial.Equinox > 0.)
    {
    text << " " << (celestial.Frame == "FK5" ? "J" : "B") << celestial.Equinox;
    }
  text.precision(6);
  text << ", centre (" << celestial.Center[0] << ", " << celestial.Center[1] << ") deg";
  if (std::fabs(celestial.Rotation) > 1.e-9)
    {
    text.precision(4);
    text << ", rotation " << celestial.Rotation << " deg";
    }
  vtkAstroReprojector::SpectralGrid spectral;
  if (wcs->naxis > 2 && vtkAstroReprojector::GetSpectralGrid(wcs.get(), dims[2], spectral))
    {
    text.precision(6);
    const bool velocity = spectral.Type != "FREQ" && spectral.Type != "WAVE";
    const double scale = velocity ? 1.e-3 : (spectral.Type == "FREQ" ? 1.e-6 : 1.e9);
    const char* unit = velocity ? "km/s" : (spectral.Type == "FREQ" ? "MHz" : "nm");
    text << "; channels " << std::fabs(spectral.Step) * scale << " " << unit << " from "
         << spectral.Start * scale << " to " << (spectral.Start + (dims[2] - 1) * spectral.Step) * scale << " "
         << unit << " (" << spectral.Type << ", " << (spectral.SpecSys.empty() ? "frame unknown" : spectral.SpecSys) << ")";
    }
  const double megabytes = 4. * dims[0] * dims[1] * std::max(1, dims[2]) / (1024. * 1024.);
  text.precision(4);
  text << "; " << megabytes << " MB";
  return text.str();
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroReprojectLogic::SetManualGridFromInput(vtkMRMLAstroReprojectParametersNode* pnode, bool celestial,
                                                          bool spectral, bool alignNorth)
{
  const std::vector<vtkMRMLAstroVolumeNode*> volumes = this->GetInputVolumes(pnode);
  std::vector<vtkAstroReprojector::Input> inputs;
  std::string error;
  if (!pnode || !MakeInputs(this->GetMRMLScene(), pnode, volumes, inputs, error))
    {
    this->LastError = error;
    return false;
    }
  int dims[3];
  inputs[0].Data->GetDimensions(dims);
  int disabledModify = pnode->StartModify();
  if (celestial)
    {
    vtkAstroReprojector::CelestialGrid grid;
    if (!vtkAstroReprojector::GetCelestialGrid(inputs[0].WCS, dims, grid))
      {
      pnode->EndModify(disabledModify);
      this->LastError = "the input has no celestial coordinates";
      return false;
      }
    pnode->SetCelestialFrame(grid.Frame.c_str());
    pnode->SetEquinox(grid.Equinox);
    pnode->SetProjection(grid.Projection.c_str());
    pnode->SetCenterLongitude(grid.Center[0]);
    pnode->SetCenterLatitude(grid.Center[1]);
    pnode->SetPixelSizeX(grid.PixelSize[0] * 3600.);
    pnode->SetPixelSizeY(grid.PixelSize[1] * 3600.);
    pnode->SetRotation(alignNorth ? 0. : grid.Rotation);
    if (alignNorth && std::fabs(grid.Rotation) > 1.e-9)
      {
      // the rotated field in a north-aligned grid
      const double rotation = grid.Rotation * DEG2RAD;
      const double c = std::fabs(std::cos(rotation)), s = std::fabs(std::sin(rotation));
      pnode->SetSizeX(static_cast<int>(std::ceil(grid.Size[0] * c + grid.Size[1] * s * grid.PixelSize[1] / grid.PixelSize[0])));
      pnode->SetSizeY(static_cast<int>(std::ceil(grid.Size[0] * s * grid.PixelSize[0] / grid.PixelSize[1] + grid.Size[1] * c)));
      }
    else
      {
      pnode->SetSizeX(grid.Size[0]);
      pnode->SetSizeY(grid.Size[1]);
      }
    }
  if (spectral)
    {
    vtkAstroReprojector::SpectralGrid grid;
    if (vtkAstroReprojector::GetSpectralGrid(inputs[0].WCS, dims[2], grid))
      {
      pnode->SetSpectralType(grid.Type.c_str());
      pnode->SetSpectralFrame(grid.SpecSys.c_str());
      if (grid.RestFrequency > 0.)
        {
        pnode->SetRestFrequency(grid.RestFrequency);
        }
      pnode->SetSpectralStart(grid.Start);
      pnode->SetSpectralStep(grid.Step);
      pnode->SetSpectralCount(grid.Count);
      }
    }
  pnode->EndModify(disabledModify);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroReprojectLogic::GetCommonBeam(vtkMRMLAstroReprojectParametersNode* pnode, double beam[3])
{
  const std::vector<vtkMRMLAstroVolumeNode*> volumes = this->GetInputVolumes(pnode);
  std::vector<vtkAstroReprojector::Input> inputs;
  std::string error;
  if (!pnode || !MakeInputs(this->GetMRMLScene(), pnode, volumes, inputs, error))
    {
    this->LastError = error;
    return false;
    }
  vtkNew<vtkDoubleArray> beams;
  beams->SetNumberOfComponents(3);
  for (const vtkAstroReprojector::Input& input : inputs)
    {
    if (!(input.Beam[0] > 0.))
      {
      this->LastError = input.Name + ": no beam (BMAJ, BMIN, BPA)";
      return false;
      }
    beams->InsertNextTuple(input.Beam);
    }
  double common[3];
  if (!vtkAstroBeams::CommonBeam(beams, common))
    {
    this->LastError = "the common beam could not be computed";
    return false;
    }
  beam[0] = common[0] * 3600.;
  beam[1] = common[1] * 3600.;
  beam[2] = common[2];
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroReprojectLogic::Reproject(vtkMRMLAstroReprojectParametersNode* pnode)
{
  this->LastError.clear();
  this->LastWarnings.clear();
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene || !pnode)
    {
    this->LastError = "no scene or parameters";
    return false;
    }
  const std::vector<vtkMRMLAstroVolumeNode*> volumes = this->GetInputVolumes(pnode);
  std::vector<vtkAstroReprojector::Input> inputs;
  std::string error;
  if (!MakeInputs(scene, pnode, volumes, inputs, error))
    {
    this->LastError = error;
    return false;
    }
  int dims[3] = {0, 0, 0};
  WCSPointer outputWCS(this->ComputeOutputGrid(pnode, dims, error));
  if (!outputWCS)
    {
    this->LastError = error;
    return false;
    }

  vtkAstroReprojector::Options options;
  options.Spatial = pnode->GetInterpolationOrder();
  options.Spectral = pnode->GetSpectralMethod();
  options.Matching = pnode->GetSpectralMatching();
  options.Combine = pnode->GetCombination();
  options.Correction = pnode->GetInputCorrection();
  options.PrimaryBeam = pnode->GetPrimaryBeamModel();
  options.NoiseWeighting = pnode->GetNoiseWeighting() != 0;
  options.PrimaryBeamFactor = pnode->GetPrimaryBeamFactor();
  options.PrimaryBeamCutoff = pnode->GetPrimaryBeamCutoff();
  options.MinimumCoverage = pnode->GetMinimumCoverage();
  options.Supersampling = pnode->GetSupersampling();
  options.ComputeWeight = pnode->GetComputeWeight() != 0;
  options.ComputeSensitivity = pnode->GetComputeSensitivity() != 0;
  options.SaultFraction = pnode->GetSaultImage() ? pnode->GetSaultFraction() : 0.;
  if (pnode->GetResolutionMode() == vtkMRMLAstroReprojectParametersNode::ResolutionCommonBeam)
    {
    double beam[3];
    if (!this->GetCommonBeam(pnode, beam))
      {
      return false;
      }
    options.TargetBeamEnabled = true;
    options.TargetBeam[0] = beam[0] / 3600.;
    options.TargetBeam[1] = beam[1] / 3600.;
    options.TargetBeam[2] = beam[2];
    }
  else if (pnode->GetResolutionMode() == vtkMRMLAstroReprojectParametersNode::ResolutionTarget)
    {
    options.TargetBeamEnabled = true;
    options.TargetBeam[0] = std::max(pnode->GetTargetBeamMajor(), pnode->GetTargetBeamMinor()) / 3600.;
    options.TargetBeam[1] = std::min(pnode->GetTargetBeamMajor(), pnode->GetTargetBeamMinor()) / 3600.;
    options.TargetBeam[2] = pnode->GetTargetBeamPA();
    if (!(options.TargetBeam[1] > 0.))
      {
      this->LastError = "the target beam needs positive axes";
      return false;
      }
    }

  // computation (background runner: progress and cancel)
  vtkAstroReprojector::Outputs outputs;
  std::atomic<int> progress{0};
  std::atomic<bool> cancel{false};
  bool success = false;
  std::function<void()> work = [&]()
    {
    success = vtkAstroReprojector::Run(inputs, outputWCS.get(), dims, options, outputs,
                                       [&](double fraction) { progress = static_cast<int>(100. * fraction); },
                                       &cancel);
    };
  vtkSlicerAstroVolumeLogic* astroVolumeLogic = this->GetAstroVolumeLogic();
  pnode->SetStatus(1);
  if (astroVolumeLogic)
    {
    astroVolumeLogic->RunInBackground("Reprojecting " + inputs[0].Name, work,
                                      [&]() { return std::max(1, std::min(99, progress.load())); },
                                      [&]() { cancel = true; });
    }
  else
    {
    work();
    }
  pnode->SetStatus(0);
  for (const std::string& warning : outputs.Warnings)
    {
    this->LastWarnings += warning + "\n";
    vtkWarningMacro("vtkSlicerAstroReprojectLogic::Reproject : " << warning);
    }
  if (!success)
    {
    this->LastError = cancel ? "cancelled" : outputs.Error;
    if (!cancel)
      {
      vtkErrorMacro("vtkSlicerAstroReprojectLogic::Reproject : " << outputs.Error);
      }
    return false;
    }

  // output volumes
  vtkMRMLAstroVolumeNode* firstVolume = volumes[0];
  vtkMRMLAstroVolumeNode* existing = vtkMRMLAstroVolumeNode::SafeDownCast(
    pnode->GetOutputVolumeNodeID() ? scene->GetNodeByID(pnode->GetOutputVolumeNodeID()) : nullptr);
  if (existing && std::find(volumes.begin(), volumes.end(), existing) != volumes.end())
    {
    existing = nullptr;
    }
  std::string name = existing ? existing->GetName() :
    std::string(firstVolume->GetName()) + (volumes.size() > 1 ? "_Mosaic" : "_Reprojected");
  if (!existing)
    {
    name = scene->GenerateUniqueName(name);
    }
  if (!astroVolumeLogic)
    {
    this->LastError = "the AstroVolume logic is not set";
    return false;
    }
  std::function<vtkMRMLAstroVolumeNode*(vtkMRMLAstroVolumeNode*, const std::string&, vtkImageData*, const std::string&)>
    makeVolume = [&](vtkMRMLAstroVolumeNode* previous, const std::string& volumeName, vtkImageData* image,
                     const std::string& unit) -> vtkMRMLAstroVolumeNode*
    {
    if (previous)
      {
      // replaced (same name)
      scene->RemoveNode(previous);
      }
    vtkMRMLAstroVolumeNode* volume = astroVolumeLogic->CloneAstroVolume(scene, firstVolume, nullptr, "_Reprojected",
                                                                        volumeName.c_str(), false);
    if (!volume)
      {
      return nullptr;
      }
    vtkMRMLNode* noPreset = nullptr;
    volume->SetPresetNode(noPreset);
    std::string volumeError;
    if (!SetOutputVolume(volume, image, outputWCS.get(), unit, outputs.Beam, volumeError))
      {
      this->LastError = volumeError;
      scene->RemoveNode(volume);
      return nullptr;
      }
    return volume;
    };
  vtkMRMLAstroVolumeNode* output = makeVolume(existing, name, outputs.Data, outputs.BUnit);
  if (!output)
    {
    if (this->LastError.empty())
      {
      this->LastError = "the output volume could not be created";
      }
    return false;
    }
  pnode->SetOutputVolumeNodeID(output->GetID());
  struct ExtraOutput
    {
    vtkImageData* Image;
    const char* Suffix;
    std::string Unit;
    const char* (vtkMRMLAstroReprojectParametersNode::*Get)();
    void (vtkMRMLAstroReprojectParametersNode::*Set)(const char*);
    };
  const ExtraOutput extras[3] = {
    {outputs.Weight, "_Weight", "", &vtkMRMLAstroReprojectParametersNode::GetWeightVolumeNodeID,
     &vtkMRMLAstroReprojectParametersNode::SetWeightVolumeNodeID},
    {outputs.Sensitivity, "_Sensitivity", outputs.BUnit, &vtkMRMLAstroReprojectParametersNode::GetSensitivityVolumeNodeID,
     &vtkMRMLAstroReprojectParametersNode::SetSensitivityVolumeNodeID},
    {outputs.Sault, "_Sault", outputs.BUnit, &vtkMRMLAstroReprojectParametersNode::GetSaultVolumeNodeID,
     &vtkMRMLAstroReprojectParametersNode::SetSaultVolumeNodeID}};
  for (const ExtraOutput& extraOutput : extras)
    {
    if (!extraOutput.Image)
      {
      continue;
      }
    const char* previousID = (pnode->*extraOutput.Get)();
    vtkMRMLAstroVolumeNode* previous = vtkMRMLAstroVolumeNode::SafeDownCast(
      previousID && *previousID ? scene->GetNodeByID(previousID) : nullptr);
    const std::string extraName = previous ? std::string(previous->GetName()) :
      scene->GenerateUniqueName(std::string(output->GetName()) + extraOutput.Suffix);
    vtkMRMLAstroVolumeNode* volume = makeVolume(previous, extraName, extraOutput.Image, extraOutput.Unit);
    (pnode->*extraOutput.Set)(volume ? volume->GetID() : "");
    }
  return true;
}
