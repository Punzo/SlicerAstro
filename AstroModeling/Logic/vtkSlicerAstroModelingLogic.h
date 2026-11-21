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

#ifndef __vtkSlicerAstroModelingLogic_h
#define __vtkSlicerAstroModelingLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"

// MRML includes
#include <vtkMRMLAstroModelingParametersNode.h>
#include <vtkMRMLTableNode.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>
class vtkImageData;
class vtkMRMLAstroVolumeNode;
class vtkMRMLMarkupsFiducialNode;
class vtkMRMLPlotChartNode;
class vtkMRMLSegmentationNode;
class vtkMRMLSliceNode;
class vtkMRMLTableNode;
class vtkMRMLVolumeNode;
class vtkSlicerAstroVolumeLogic;
class vtkSlicerMarkupsLogic;

// AstroModelings includes
#include "vtkSlicerAstroModelingModuleLogicExport.h"
class vtkMRMLAstroModelingParametersNode;
class vtkMRMLAstroVolumeDisplayNode;

/// \class vtkSlicerAstroModelingLogic
/// \brief This is a C++ SlicerAstro wrapping of 3DBAROLO,
/// a 3D algorithm to derive rotation curves of galaxies,
/// developed by Enrico di Teodoro.
///
/// \ingroup SlicerAstro_QtModules_AstroModeling
#include <string>

class VTK_SLICERASTRO_ASTROMODELING_MODULE_LOGIC_EXPORT vtkSlicerAstroModelingLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroModelingLogic *New();
  vtkTypeMacro(vtkSlicerAstroModelingLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Set AstroVolume module logic
  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);

  /// Get AstroVolume module logic
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  /// Set Markups module logic
  void SetMarkupsLogic(vtkSlicerMarkupsLogic* logic);

  /// Get Markups module logic
  vtkSlicerMarkupsLogic* GetMarkupsLogic();

  /// Register MRML Node classes to Scene.
  /// Gets called automatically when the MRMLScene is attached to this logic class
  virtual void RegisterNodes() override;

  /// Run Estimation, Crate Model or Fitting Model
  /// \param MRML parameter node
  /// \param MRML table node
  /// \return Success flag
  int OperateModel(vtkMRMLAstroModelingParametersNode *pnode,
                   vtkMRMLTableNode *tnode);

  /// Abort the operation in progress (estimate, model or fit), from any
  /// thread: the fit stops at the next iteration of the minimization, the
  /// other steps at the end of the current step.
  void RequestAbort();

  /// Recalculate the model from new parameters in the mrml table
  /// \param MRML parameter node
  /// \param MRML table node
  /// \return Success flag
  /// The parameters of the rings are read from tableNode (default: the
  /// parameters table of pnode, which must be in the scene).
  int UpdateModelFromTable(vtkMRMLAstroModelingParametersNode *pnode,
                           vtkMRMLTableNode *tableNode = nullptr);

  /// An operation in a worker thread (OperateModel, UpdateModelFromTable)
  /// must not look up or change the nodes of the scene: the GUI thread
  /// changes the scene meanwhile (views, panels, the user).
  /// PrepareWorkerData (GUI thread, before the worker starts) gives the
  /// operation private nodes: the input, model and residual volumes (name,
  /// attributes, geometry; their voxels are shared with the volumes of the
  /// scene, no copy of the data, and kept alive until the operation ends even
  /// if a volume is removed), a copy of the display node of the input (own
  /// WCS: 3DBarolo converts it to the optical velocity definition, the views
  /// keep using theirs) and the input mask. The operation uses only these.
  /// FinishWorkerData (GUI thread, after the worker ended) marks the model
  /// and residual voxels modified (if apply) and releases the private nodes.
  /// Without PrepareWorkerData the operations use the scene (e.g. scripts
  /// running them in the GUI thread).
  bool PrepareWorkerData(vtkMRMLAstroModelingParametersNode* pnode, std::string& error);
  void FinishWorkerData(bool apply);
  bool IsWorkerDataPrepared() const;

  /// Clean all the pointers of the instantiated calsses of 3DBarolo
  void cleanPointers();

  /// \name Operations of the module (estimate, create, fit, model of the table)
  /// Behaviour of the Modeling panel, usable from scripts.
  //@{
  enum
    {
    /// invoked (GUI thread) when an operation started by StartOperation ends
    OperationFinishedEvent = vtkCommand::UserEvent + 7301
    };

  /// Start an operation of 3DBarolo on the cube of the parameters node:
  /// vtkMRMLAstroModelingParametersNode::ESTIMATE, CREATE, FIT or UPDATE (the
  /// model of the rings of the parameters table). Prepares it in the calling
  /// (GUI) thread: checks (GetLastError()), beam, model and residual cubes
  /// ("<cube>_model_N", "<cube>_maskedByModel_N", replacing the previous
  /// ones of the module), mask (SelectMaskSegment), optical velocities. The
  /// work runs in the background (AstroVolume StartInBackground) on private
  /// copies of the parameters and of the table, and FinishOperation applies
  /// the results in the GUI thread (parameters, table, results display,
  /// OperationFinishedEvent). synchronous: returns when done (scripts).
  /// Returns false if the operation could not start.
  bool StartOperation(vtkMRMLAstroModelingParametersNode* pnode, int operation, bool synchronous = false);
  /// True while an operation started by StartOperation runs
  bool IsOperationRunning() const { return this->OperationRunning; }
  /// Cancel the running operation (the fit stops at the next iteration)
  void CancelOperation();
  /// Result of the last operation: 1 done, 0 failed, -1 cancelled
  int GetLastOperationResult() const { return this->LastOperationResult; }
  /// Why the last StartOperation (or other call) failed ("" if it did not)
  const char* GetLastError() const { return this->LastError.c_str(); }
  /// Progress of the running operation (status of the private parameters,
  /// 0-100)
  int GetOperationProgress();

  /// The mask of the fit: a segment of the mask of the cube (the segment of
  /// the parameters node if it is in the mask, otherwise its first one).
  /// False (GetLastError()) if the mask has no segments.
  bool SelectMaskSegment(vtkMRMLAstroModelingParametersNode* pnode);

  /// The table of the parameters of the rings of the parameters node
  /// (columns Radii, VRot, VRad, Inc, Phi, VSys, VDisp, Dens, Z0, XPos,
  /// YPos with titles and units): the referenced one, the last
  /// "ModelingParamsTable" of the scene, or a new one (forceNew: always new)
  vtkMRMLTableNode* EnsureParamsTable(vtkMRMLAstroModelingParametersNode* pnode, bool forceNew = false);

  /// Reset the parameters of the rings and of the fit to the automatic
  /// values of 3DBarolo (Automatic mode)
  static void ResetFitParameters(vtkMRMLAstroModelingParametersNode* pnode);

  /// Centre (mean X/YPos of the rings: XPos/YPosCenterIJK and RAS at the
  /// central channel) and PVPhi (-(mean PA - 90)) from the parameters table
  bool UpdateResultsGeometry(vtkMRMLAstroModelingParametersNode* pnode);

  /// Contours of the results (segmentation GetResultContoursName()): the
  /// data and the model at ContourLevel times the noise of the cube, and the
  /// region of the model. Created if needed.
  bool UpdateResultContours(vtkMRMLAstroModelingParametersNode* pnode);
  vtkMRMLSegmentationNode* GetResultContours();
  static const char* GetResultContoursName() { return "AstroModelingContoursSeg"; }
  static const char* GetDataContoursSegmentID() { return "ModelingDataContours"; }
  static const char* GetModelContoursSegmentID() { return "ModelingModelContours"; }
  static const char* GetModelMaskSegmentID() { return "ModelingModelMask"; }
  /// Visibility of a segment of the result contours (2D and 3D as set by
  /// UpdateResultContours)
  bool SetResultContoursVisibility(const char* segmentID, bool visible);
  bool GetResultContoursVisibility(const char* segmentID);

  /// PV slices of the results: the Yellow view along the major axis (mean
  /// PA of the rings plus YellowRotValue, degrees) and the Green view along
  /// the minor axis (plus GreenRotValue), both through the centre of the
  /// rings; absolute orientations (presets "PVMajor", "PVMinor"), set
  /// again identically after a scene is loaded. Then UpdateRingMarkers.
  bool SetPVSlices(vtkMRMLAstroModelingParametersNode* pnode);
  /// Points of the rotation curve on the PV slices: for each ring, the two
  /// points where the ring crosses the plane of the Yellow (major axis) or
  /// Green (minor axis) slice, at the velocity of the model there (as in
  /// Galmod). Points of rings that do not cross the slice are hidden.
  bool UpdateRingMarkers(vtkMRMLAstroModelingParametersNode* pnode);
  vtkMRMLMarkupsFiducialNode* GetRingMarkers(bool major);
  static const char* GetRingMarkersName(bool major)
    {
    return major ? "MarkupsFiducialsMajor" : "MarkupsFiducialsMinor";
    }
  /// Remove the points of the rotation curve and reset the geometry of the
  /// results (e.g. a failed operation)
  void ClearResults(vtkMRMLAstroModelingParametersNode* pnode);

  /// Show the results: the cube, the model and the residuals prepared for
  /// the 3D views (the cube shown), the PV slices; with setLayout, the
  /// layout with a plot view (classic Slicer; the Workbench keeps its own).
  bool ShowResults(vtkMRMLAstroModelingParametersNode* pnode, bool setLayout);

  /// Plot chart of a parameter of the rings (a column of the parameters
  /// table, e.g. "VRot") against the radius: chart "ModelingRingsChart"
  /// with one series. Returns the chart (nullptr if the column is unknown).
  vtkMRMLPlotChartNode* UpdateRingPlot(vtkMRMLAstroModelingParametersNode* pnode, const char* columnName);
  //@}

  /// Output of the operations since the last call: what 3DBarolo prints
  /// (standard output and error, also still printed in the terminal) and
  /// the errors and warnings of the operations. Thread safe.
  std::string TakeLog();

  /// Append text to the output of the operations. Thread safe.
  void AppendLog(const std::string& text);

protected:
  /// Implementation of OperateModel and UpdateModelFromTable. They may throw
  /// bbarolo::Error, which the public methods catch and report.
  int OperateModelInternal(vtkMRMLAstroModelingParametersNode *pnode,
                           vtkMRMLTableNode *tnode);
  int UpdateModelFromTableInternal(vtkMRMLAstroModelingParametersNode *pnode,
                                   vtkMRMLTableNode *tableNode);

  /// Node of an operation: the private copy (PrepareWorkerData) or the node
  /// of the scene.
  vtkMRMLAstroVolumeNode* GetOperationVolume(const char* nodeID);
  vtkMRMLAstroVolumeDisplayNode* GetOperationDisplayNode(vtkMRMLAstroVolumeNode* inputVolume);
  /// Input mask of an operation (1 inside) on the grid of inputVolume
  bool GetOperationMask(vtkMRMLAstroModelingParametersNode* pnode, vtkMRMLAstroVolumeNode* inputVolume,
                        vtkImageData* maskImage, std::string& error);
  std::string GetOperationOutputFolder();

  vtkSlicerAstroModelingLogic();
  virtual ~vtkSlicerAstroModelingLogic();

  /// Calculate the wcs velocity in the center of the data-cube
  /// \param MRML astro volume display node
  /// \return value
  double CalculateCentralVelocity(vtkMRMLAstroVolumeDisplayNode* volumeDisplayNode);

  /// Nodes removed from the scene: the data cube of a parameters node (e.g.
  /// a dataset deleted with its products) clears its results (rings,
  /// points of the rotation curve, contours) and cancels its operation; a
  /// node used by the running operation cancels it.
  void SetMRMLSceneInternal(vtkMRMLScene* newScene) override;
  void ProcessMRMLSceneEvents(vtkObject* caller, unsigned long event, void* callData) override;
  void OnNodeAboutToBeRemoved(vtkMRMLNode* node);

  /// Results of the operation started by StartOperation (GUI thread)
  void FinishOperation(int result);
  /// Model and residual cubes of an operation (CREATE, FIT, ESTIMATE)
  bool CreateOutputVolumes(vtkMRMLAstroModelingParametersNode* pnode);

  vtkSmartPointer<vtkMRMLAstroModelingParametersNode> OperationParameters;
  vtkSmartPointer<vtkMRMLTableNode> OperationTable;
  vtkWeakPointer<vtkMRMLAstroModelingParametersNode> OperationSceneParameters;
  bool OperationRunning{false};
  bool OperationCancelled{false};
  int LastOperationResult{0};
  std::string LastError;

private:
  vtkSlicerAstroModelingLogic(const vtkSlicerAstroModelingLogic&); // Not implemented
  void operator=(const vtkSlicerAstroModelingLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

