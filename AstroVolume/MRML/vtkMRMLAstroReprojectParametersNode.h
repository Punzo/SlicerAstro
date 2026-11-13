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

#ifndef __vtkMRMLAstroReprojectParametersNode_h
#define __vtkMRMLAstroReprojectParametersNode_h

// STD includes
#include <string>

// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLNode.h>

// Export includes
#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkMRMLMarkupsROINode;
class vtkMRMLTableNode;
class vtkMRMLTransformNode;

/// \brief MRML parameter node for the AstroReproject module.
///
/// \ingroup SlicerAstro_QtModules_AstroReproject
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroReprojectParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroReprojectParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroReprojectParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  virtual vtkMRMLNode* CreateNodeInstance() override;

  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) override;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) override;

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() override {return "AstroReprojectParameters";};

  /// Set/Get the InputVolumeNodeID.
  /// \sa SetInputVolumeNodeID(), GetInputVolumeNodeID()
  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  /// Set/Get the ReferenceVolumeNodeID.
  /// \sa SetReferenceVolumeNodeID(), GetReferenceVolumeNodeID()
  vtkSetStringMacro(ReferenceVolumeNodeID);
  vtkGetStringMacro(ReferenceVolumeNodeID);

  /// Set/Get the OutputVolumeNodeID.
  /// \sa SetOutputVolumeNodeID(), GetOutputVolumeNodeID()
  vtkSetStringMacro(OutputVolumeNodeID);
  vtkGetStringMacro(OutputVolumeNodeID);

  /// Set/Get the ReprojectRotation.
  /// \sa SetReprojectRotation(), GetReprojectRotation()
  vtkSetMacro(ReprojectRotation,bool);
  vtkGetMacro(ReprojectRotation,bool);
  vtkBooleanMacro(ReprojectRotation,bool);

  /// Set/Get the ReprojectData.
  /// \sa SetReprojectData(), GetReprojectData()
  vtkSetMacro(ReprojectData,bool);
  vtkGetMacro(ReprojectData,bool);
  vtkBooleanMacro(ReprojectData,bool);

  /// Set/Get the OutputSerial.
  /// \sa SetOutputSerial(), GetOutputSerial()
  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

  /// Set/Get the InterpolationOrder.
  /// Default is 1 (Bilinear)
  /// \sa SetInterpolationOrder(), GetInterpolationOrder()
  vtkSetMacro(InterpolationOrder,int);
  vtkGetMacro(InterpolationOrder,int);

  /// spatial resampling (vtkAstroReprojector::SpatialMethod)
  enum ORDER
  {
    NearestNeighbour = 0,
    Bilinear,
    Bicubic,
    Lanczos3,
    AreaAverage,
    ExactOverlap
  };

  /// Spatial grid of the output
  enum SpatialGridModes
  {
    SpatialReference = 0,  ///< grid of the reference volume
    SpatialInput,          ///< grid of the (first) input
    SpatialResample,       ///< grid of the input with pixels SpatialResampleFactor larger
    SpatialUnion,          ///< covering all the inputs (Projection, CelestialFrame, PixelSizeX)
    SpatialIntersection,   ///< common area of the inputs
    SpatialManual          ///< CelestialFrame, Projection, Center*, PixelSize*, Rotation, Size*
  };

  /// Spectral grid of the output
  enum SpectralGridModes
  {
    SpectralInput = 0,     ///< channels of the (first) input
    SpectralReference,     ///< channels of the reference volume
    SpectralResample,      ///< channels of the input SpectralResampleFactor wider
    SpectralUnion,         ///< covering all the inputs (SpectralType, SpectralFrame, RestFrequency, SpectralStep)
    SpectralIntersection,  ///< common range of the inputs
    SpectralManual         ///< SpectralType, SpectralFrame, RestFrequency, SpectralStart, SpectralStep, SpectralCount
  };

  /// Spectral resampling (SpectralMethod, as vtkAstroReprojector::SpectralMethod)
  enum SpectralMethods
  {
    SpectralNearest = 0,
    SpectralLinear,
    SpectralCubic,
    SpectralOverlap,       ///< channel overlap (flux conserving)
    SpectralAuto           ///< overlap for wider output channels, cubic otherwise
  };

  /// Channels matched by frequency or by velocity (each input with its rest
  /// frequency, e.g. HI and CO data), as vtkAstroReprojector::SpectralMatching
  enum SpectralMatchings
  {
    MatchFrequency = 0,
    MatchVelocity
  };

  /// Combination of several inputs (Combination, as vtkAstroReprojector::Combination)
  enum Combinations
  {
    CombineMean = 0,
    CombineOptimal,        ///< sum(A I w) / sum(A^2 w) (Sault et al. 1996)
    CombinePrimaryBeam     ///< sum(I w) / sum(A w)
  };

  /// Primary beam treatment of the inputs (InputCorrection)
  enum InputCorrections
  {
    FluxCorrect = 0,              ///< primary beam corrected
    PrimaryBeamWeighted,          ///< not corrected (flat noise)
    PrimaryBeamSquaredWeighted
  };

  /// Primary beam model (PrimaryBeamModel)
  enum PrimaryBeamModels
  {
    NoPrimaryBeam = 0,
    GaussianPrimaryBeam,
    AiryPrimaryBeam,
    VolumePrimaryBeam      ///< PrimaryBeamVolumeNodeIDs
  };

  /// Resolution of the output
  enum ResolutionModes
  {
    ResolutionKeep = 0,
    ResolutionCommonBeam,
    ResolutionTarget
  };

  /// Input volumes (IDs separated by ';'); empty: InputVolumeNodeID
  vtkSetStdStringFromCharMacro(InputVolumeNodeIDs);
  vtkGetCharFromStdStringMacro(InputVolumeNodeIDs);

  /// Primary beam volumes of the inputs (IDs separated by ';', same order, empty entries allowed)
  vtkSetStdStringFromCharMacro(PrimaryBeamVolumeNodeIDs);
  vtkGetCharFromStdStringMacro(PrimaryBeamVolumeNodeIDs);

  /// Noise of the inputs (values separated by ';'; empty entries: SlicerAstro.DisplayThreshold)
  vtkSetStdStringFromCharMacro(InputNoises);
  vtkGetCharFromStdStringMacro(InputNoises);

  /// Output: weight volume
  vtkSetStdStringFromCharMacro(WeightVolumeNodeID);
  vtkGetCharFromStdStringMacro(WeightVolumeNodeID);

  /// Output: sensitivity (noise) volume
  vtkSetStdStringFromCharMacro(SensitivityVolumeNodeID);
  vtkGetCharFromStdStringMacro(SensitivityVolumeNodeID);

  /// Output: Sault-weighted volume
  vtkSetStdStringFromCharMacro(SaultVolumeNodeID);
  vtkGetCharFromStdStringMacro(SaultVolumeNodeID);

  /// Manual grid: celestial frame (ICRS, FK5, FK4, GALACTIC, SUPERGALACTIC)
  vtkSetStdStringFromCharMacro(CelestialFrame);
  vtkGetCharFromStdStringMacro(CelestialFrame);

  /// Manual/union grid: projection code (TAN, SIN, ARC, ZEA, CAR, AIT, SFL, ...)
  vtkSetStdStringFromCharMacro(Projection);
  vtkGetCharFromStdStringMacro(Projection);

  /// Manual spectral grid: type (FREQ, VRAD, VOPT, WAVE, VELO)
  vtkSetStdStringFromCharMacro(SpectralType);
  vtkGetCharFromStdStringMacro(SpectralType);

  /// Spectral grid: frame (SPECSYS) of the output
  vtkSetStdStringFromCharMacro(SpectralFrame);
  vtkGetCharFromStdStringMacro(SpectralFrame);

  /// Spatial grid of the output (SpatialGridModes)
  vtkSetMacro(SpatialGridMode, int);
  vtkGetMacro(SpatialGridMode, int);

  /// Spectral grid of the output (SpectralGridModes)
  vtkSetMacro(SpectralGridMode, int);
  vtkGetMacro(SpectralGridMode, int);

  /// Manual grid: pixels along x
  vtkSetMacro(SizeX, int);
  vtkGetMacro(SizeX, int);

  /// Manual grid: pixels along y
  vtkSetMacro(SizeY, int);
  vtkGetMacro(SizeY, int);

  /// Manual spectral grid: channels
  vtkSetMacro(SpectralCount, int);
  vtkGetMacro(SpectralCount, int);

  /// Spectral resampling (vtkAstroReprojector::SpectralMethod: nearest, linear, cubic, overlap, auto)
  vtkSetMacro(SpectralMethod, int);
  vtkGetMacro(SpectralMethod, int);

  /// Spectral matching: 0 frequency, 1 velocity (rest frequency of each input)
  vtkSetMacro(SpectralMatching, int);
  vtkGetMacro(SpectralMatching, int);

  /// Combination of the inputs (vtkAstroReprojector::Combination: mean, optimal, primary beam)
  vtkSetMacro(Combination, int);
  vtkGetMacro(Combination, int);

  /// Inputs: 0 primary beam corrected, 1 not corrected (flat noise), 2 weighted by the primary beam squared
  vtkSetMacro(InputCorrection, int);
  vtkGetMacro(InputCorrection, int);

  /// Primary beam (vtkAstroReprojector::PrimaryBeamModel: none, Gaussian, Airy, volume)
  vtkSetMacro(PrimaryBeamModel, int);
  vtkGetMacro(PrimaryBeamModel, int);

  /// Resolution: 0 keep, 1 common beam of the inputs, 2 target beam
  vtkSetMacro(ResolutionMode, int);
  vtkGetMacro(ResolutionMode, int);

  /// Area average: subpixels per axis (0: automatic)
  vtkSetMacro(Supersampling, int);
  vtkGetMacro(Supersampling, int);

  /// Weights 1 / noise^2 (1) or 1 (0)
  vtkSetMacro(NoiseWeighting, int);
  vtkGetMacro(NoiseWeighting, int);

  /// Create the weight volume
  vtkSetMacro(ComputeWeight, int);
  vtkGetMacro(ComputeWeight, int);

  /// Create the sensitivity volume
  vtkSetMacro(ComputeSensitivity, int);
  vtkGetMacro(ComputeSensitivity, int);

  /// Create the Sault-weighted volume
  vtkSetMacro(SaultImage, int);
  vtkGetMacro(SaultImage, int);

  /// Manual grid: equinox (FK5: Julian, FK4: Besselian)
  vtkSetMacro(Equinox, double);
  vtkGetMacro(Equinox, double);

  /// Manual grid: longitude of the centre (degrees)
  vtkSetMacro(CenterLongitude, double);
  vtkGetMacro(CenterLongitude, double);

  /// Manual grid: latitude of the centre (degrees)
  vtkSetMacro(CenterLatitude, double);
  vtkGetMacro(CenterLatitude, double);

  /// Manual/union grid: pixel size along x (arcsec; <= 0: smallest of the inputs)
  vtkSetMacro(PixelSizeX, double);
  vtkGetMacro(PixelSizeX, double);

  /// Manual grid: pixel size along y (arcsec)
  vtkSetMacro(PixelSizeY, double);
  vtkGetMacro(PixelSizeY, double);

  /// Manual grid: rotation (CROTA2, degrees)
  vtkSetMacro(Rotation, double);
  vtkGetMacro(Rotation, double);

  /// Resample: pixel size factor (> 1: larger pixels)
  vtkSetMacro(SpatialResampleFactor, double);
  vtkGetMacro(SpatialResampleFactor, double);

  /// Spectral grid: rest frequency (Hz)
  vtkSetMacro(RestFrequency, double);
  vtkGetMacro(RestFrequency, double);

  /// Manual spectral grid: value of the first channel (SI: Hz, m/s, m)
  vtkSetMacro(SpectralStart, double);
  vtkGetMacro(SpectralStart, double);

  /// Manual/union spectral grid: channel width (SI; union: <= 0 for the narrowest of the inputs)
  vtkSetMacro(SpectralStep, double);
  vtkGetMacro(SpectralStep, double);

  /// Resample: channel width factor (> 1: wider channels)
  vtkSetMacro(SpectralResampleFactor, double);
  vtkGetMacro(SpectralResampleFactor, double);

  /// Primary beam models: dish diameter (m)
  vtkSetMacro(DishDiameter, double);
  vtkGetMacro(DishDiameter, double);

  /// Gaussian primary beam: FWHM in lambda / D
  vtkSetMacro(PrimaryBeamFactor, double);
  vtkGetMacro(PrimaryBeamFactor, double);

  /// Primary beam level below which the inputs are not used
  vtkSetMacro(PrimaryBeamCutoff, double);
  vtkGetMacro(PrimaryBeamCutoff, double);

  /// Fraction of an output pixel covered by data
  vtkSetMacro(MinimumCoverage, double);
  vtkGetMacro(MinimumCoverage, double);

  /// Target beam: FWHM major axis (arcsec)
  vtkSetMacro(TargetBeamMajor, double);
  vtkGetMacro(TargetBeamMajor, double);

  /// Target beam: FWHM minor axis (arcsec)
  vtkSetMacro(TargetBeamMinor, double);
  vtkGetMacro(TargetBeamMinor, double);

  /// Target beam: position angle (degrees)
  vtkSetMacro(TargetBeamPA, double);
  vtkGetMacro(TargetBeamPA, double);

  /// Sault image: flux correct above this fraction of the peak weight
  vtkSetMacro(SaultFraction, double);
  vtkGetMacro(SaultFraction, double);


  /// Set/Get the Cores.
  /// Default is 0 (all the free cores will be used)
  /// \sa SetCores(), GetCores()
  vtkSetMacro(Cores,int);
  vtkGetMacro(Cores,int);

  /// Set/Get the Status.
  /// \sa SetStatus(), GetStatus()
  vtkSetMacro(Status,int);
  vtkGetMacro(Status,int);

protected:
  vtkMRMLAstroReprojectParametersNode();
  ~vtkMRMLAstroReprojectParametersNode() override;

  vtkMRMLAstroReprojectParametersNode(const vtkMRMLAstroReprojectParametersNode&);
  void operator=(const vtkMRMLAstroReprojectParametersNode&);

  char *InputVolumeNodeID;
  char *ReferenceVolumeNodeID;
  char *OutputVolumeNodeID;

  bool ReprojectRotation;
  bool ReprojectData;

  int OutputSerial;

  int InterpolationOrder;

  int Cores;

  int Status;

  std::string InputVolumeNodeIDs;
  std::string PrimaryBeamVolumeNodeIDs;
  std::string InputNoises;
  std::string WeightVolumeNodeID;
  std::string SensitivityVolumeNodeID;
  std::string SaultVolumeNodeID;
  std::string CelestialFrame;
  std::string Projection;
  std::string SpectralType;
  std::string SpectralFrame;
  int SpatialGridMode;
  int SpectralGridMode;
  int SizeX;
  int SizeY;
  int SpectralCount;
  int SpectralMethod;
  int SpectralMatching;
  int Combination;
  int InputCorrection;
  int PrimaryBeamModel;
  int ResolutionMode;
  int Supersampling;
  int NoiseWeighting;
  int ComputeWeight;
  int ComputeSensitivity;
  int SaultImage;
  double Equinox;
  double CenterLongitude;
  double CenterLatitude;
  double PixelSizeX;
  double PixelSizeY;
  double Rotation;
  double SpatialResampleFactor;
  double RestFrequency;
  double SpectralStart;
  double SpectralStep;
  double SpectralResampleFactor;
  double DishDiameter;
  double PrimaryBeamFactor;
  double PrimaryBeamCutoff;
  double MinimumCoverage;
  double TargetBeamMajor;
  double TargetBeamMinor;
  double TargetBeamPA;
  double SaultFraction;
};

#endif
