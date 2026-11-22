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

// Operations of the Modeling module (start in the background, results) and
// the display of the results: contours, PV slices along the axes of the
// rings with the points of the rotation curve, plot of the rings.
// Included by vtkSlicerAstroModelingLogic.cxx (part of the logic class; the
// Python wrapping takes one source file per class).

// Logic includes
#include "vtkSlicerAstroModelingLogic.h"
#include "vtkSlicerAstroVolumeLogic.h"
#include <vtkSlicerSegmentationsModuleLogic.h>

// MRML includes
#include <vtkMRMLAstroModelingParametersNode.h>
#include <vtkMRMLAstroTerminology.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLMarkupsDisplayNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLPlotChartNode.h>
#include <vtkMRMLPlotSeriesNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTableNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

// Segmentations includes
#include <vtkOrientedImageData.h>
#include <vtkSegmentation.h>

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkImageThreshold.h>
#include <vtkIntArray.h>
#include <vtkMath.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkTable.h>
#include <vtkTransform.h>
#include <vtkVector.h>

// STD includes
#include <algorithm>
#include <cmath>
#include <functional>
#include <cstring>
#include <memory>
#include <sstream>

namespace ModelingResults
{
//----------------------------------------------------------------------------
const char* OperationLabel(int operation)
{
  switch (operation)
    {
    case vtkMRMLAstroModelingParametersNode::ESTIMATE:
      return "3DBarolo: estimating the parameters...";
    case vtkMRMLAstroModelingParametersNode::CREATE:
      return "3DBarolo: creating the model...";
    case vtkMRMLAstroModelingParametersNode::FIT:
      return "3DBarolo: fitting the model...";
    default:
      return "3DBarolo: calculating the model...";
    }
}

//----------------------------------------------------------------------------
vtkDoubleArray* Column(vtkMRMLTableNode* table, const char* name)
{
  return (table && table->GetTable()) ? vtkDoubleArray::SafeDownCast(table->GetTable()->GetColumnByName(name)) :
                                        nullptr;
}

//----------------------------------------------------------------------------
vtkMRMLSliceNode* SliceNode(vtkMRMLScene* scene, const char* name)
{
  return scene ? vtkMRMLSliceNode::SafeDownCast(scene->GetNodeByID((std::string("vtkMRMLSliceNode") + name).c_str())) :
                 nullptr;
}

//----------------------------------------------------------------------------
void SetOrientationPreset(vtkMRMLSliceNode* sliceNode, const char* name, vtkMatrix3x3* orientation)
{
  if (!sliceNode)
    {
    return;
    }
  if (sliceNode->HasSliceOrientationPreset(name))
    {
    sliceNode->GetSliceOrientationPreset(name)->DeepCopy(orientation);
    }
  else
    {
    sliceNode->AddSliceOrientationPreset(name, orientation);
    }
}

//----------------------------------------------------------------------------
/// The XZ orientation of a slice node rotated about its vertical (velocity)
/// axis by angle degrees
void RotatedXZ(vtkMRMLSliceNode* sliceNode, double angle, vtkMatrix3x3* orientation)
{
  vtkNew<vtkMatrix4x4> base;
  vtkMatrix3x3* xz = sliceNode->HasSliceOrientationPreset("XZ") ? sliceNode->GetSliceOrientationPreset("XZ") :
                                                                 nullptr;
  if (xz)
    {
    for (int row = 0; row < 3; row++)
      {
      for (int column = 0; column < 3; column++)
        {
        base->SetElement(row, column, xz->GetElement(row, column));
        }
      }
    }
  else
    {
    // RAS: east-west along R, velocity along A, declination along S
    base->SetElement(0, 0, -1.);
    base->SetElement(1, 1, 0.);
    base->SetElement(1, 2, 1.);
    base->SetElement(2, 1, 1.);
    base->SetElement(2, 2, 0.);
    }
  vtkNew<vtkTransform> transform;
  transform->SetMatrix(base);
  transform->RotateY(angle);
  for (int row = 0; row < 3; row++)
    {
    for (int column = 0; column < 3; column++)
      {
      orientation->SetElement(row, column, transform->GetMatrix()->GetElement(row, column));
      }
    }
}

//----------------------------------------------------------------------------
struct RingColumn
{
  const char* Name;
  const char* Title;
  const char* Unit;
};

//----------------------------------------------------------------------------
const RingColumn RingColumns[] = {
  {"Radii", "Radius", "arcsec"},
  {"VRot", "Rotational velocity", "km/s"},
  {"VRad", "Radial velocity", "km/s"},
  {"Inc", "Inclination", "degree"},
  {"Phi", "Position angle", "degree"},
  {"VSys", "Systemic velocity", "km/s (Velocity Definition: Optical)"},
  {"VDisp", "Dispersion velocity", "km/s"},
  {"Dens", "Column density", "10^20 cm^-2"},
  {"Z0", "Scale height", "Kpc"},
  {"XPos", "X center", "pixels"},
  {"YPos", "Y center", "pixels"},
};
} // end of namespace ModelingResults

using namespace ModelingResults;

//----------------------------------------------------------------------------
vtkMRMLTableNode* vtkSlicerAstroModelingLogic::EnsureParamsTable(vtkMRMLAstroModelingParametersNode* pnode,
                                                                bool forceNew)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene || !pnode)
    {
    return nullptr;
    }
  if (!forceNew)
    {
    vtkMRMLTableNode* table = pnode->GetParamsTableNode();
    if (!table)
      {
      std::vector<vtkMRMLNode*> tables;
      scene->GetNodesByClass("vtkMRMLTableNode", tables);
      for (std::vector<vtkMRMLNode*>::reverse_iterator it = tables.rbegin(); it != tables.rend() && !table; ++it)
        {
        if ((*it)->GetName() && std::string((*it)->GetName()).find("ModelingParamsTable") != std::string::npos)
          {
          table = vtkMRMLTableNode::SafeDownCast(*it);
          }
        }
      }
    if (table)
      {
      pnode->SetParamsTableNode(table);
      return table;
      }
    }
  vtkMRMLTableNode* table = vtkMRMLTableNode::SafeDownCast(
    scene->AddNewNodeByClass("vtkMRMLTableNode", scene->GenerateUniqueName("ModelingParamsTable")));
  if (!table)
    {
    return nullptr;
    }
  const int wasModifying = table->StartModify();
  table->RemoveAllColumns();
  table->SetUseColumnTitleAsColumnHeader(true);
  table->SetDefaultColumnType("double");
  for (const RingColumn& column : RingColumns)
    {
    vtkAbstractArray* array = table->AddColumn();
    if (!array)
      {
      continue;
      }
    array->SetName(column.Name);
    table->SetColumnUnitLabel(column.Name, column.Unit);
    table->SetColumnTitle(column.Name, column.Title);
    }
  table->EndModify(wasModifying);
  pnode->SetParamsTableNode(table);
  return table;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::ResetFitParameters(vtkMRMLAstroModelingParametersNode* pnode)
{
  if (!pnode)
    {
    return;
    }
  const int wasModifying = pnode->StartModify();
  pnode->SetNumberOfRings(-1);
  pnode->SetRadSep(-1);
  pnode->SetXCenter(-1);
  pnode->SetYCenter(-1.);
  pnode->SetSystemicVelocity(-1);
  pnode->SetRotationVelocity(-1);
  pnode->SetVelocityDispersion(-1);
  pnode->SetRadialVelocity(0.);
  pnode->SetVerticalVelocity(0.);
  pnode->SetVerticalRotationalGradient(0.);
  pnode->SetVerticalRotationalGradientHeight(0.);
  pnode->SetInclination(-1);
  pnode->SetInclinationError(5.);
  pnode->SetPositionAngle(-1);
  pnode->SetPositionAngleError(15.);
  pnode->SetScaleHeight(-1);
  pnode->SetColumnDensity(1.);
  pnode->SetDistance(-1);
  pnode->SetPositionAngleFit(true);
  pnode->SetRotationVelocityFit(true);
  pnode->SetRadialVelocityFit(false);
  pnode->SetVelocityDispersionFit(true);
  pnode->SetInclinationFit(true);
  pnode->SetXCenterFit(false);
  pnode->SetYCenterFit(false);
  pnode->SetSystemicVelocityFit(false);
  pnode->SetScaleHeightFit(false);
  pnode->SetADRIFTCorrection(false);
  pnode->SetLayerType(0);
  pnode->SetFittingFunction(1);
  pnode->SetWeightingFunction(1);
  pnode->SetNumberOfClounds(-1);
  pnode->SetCloudsColumnDensity(10.);
  pnode->SetTollerance(0.001);
  pnode->EndModify(wasModifying);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroModelingLogic::SelectMaskSegment(vtkMRMLAstroModelingParametersNode* pnode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLAstroVolumeNode* cube = (scene && pnode && pnode->GetInputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetInputVolumeNodeID())) : nullptr;
  vtkMRMLSegmentationNode* mask = (cube && this->GetAstroVolumeLogic()) ?
    this->GetAstroVolumeLogic()->GetMaskSegmentationForVolume(cube, false) : nullptr;
  vtkSegmentation* segmentation = mask ? mask->GetSegmentation() : nullptr;
  if (!segmentation || segmentation->GetNumberOfSegments() < 1)
    {
    this->LastError = "The mask of the data has no segments: draw the mask (Mask tab), or let 3DBarolo find "
                      "the source (no input mask).";
    return false;
    }
  const char* segmentID = pnode->GetMaskSegmentID();
  const bool sameMask = pnode->GetMaskSegmentationNodeID() &&
    !strcmp(pnode->GetMaskSegmentationNodeID(), mask->GetID());
  std::string selected = (sameMask && segmentID && segmentation->GetSegment(segmentID)) ? segmentID :
    segmentation->GetNthSegmentID(0);
  const int wasModifying = pnode->StartModify();
  pnode->SetMaskSegmentationNodeID(mask->GetID());
  pnode->SetMaskSegmentID(selected.c_str());
  pnode->EndModify(wasModifying);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroModelingLogic::CreateOutputVolumes(vtkMRMLAstroModelingParametersNode* pnode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkSlicerAstroVolumeLogic* astroVolumeLogic = this->GetAstroVolumeLogic();
  vtkMRMLAstroVolumeNode* cube = (scene && pnode && pnode->GetInputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetInputVolumeNodeID())) : nullptr;
  if (!cube || !astroVolumeLogic)
    {
    return false;
    }
  const int serial = pnode->GetOutputSerial();
  // the model (and the residuals of scenes of earlier versions) of the
  // previous operation of the module are replaced; the data in the region
  // of the model are not made any more (Masking: blank with the model region)
  const char* previousIDs[2] = {pnode->GetOutputVolumeNodeID(), pnode->GetResidualVolumeNodeID()};
  const char* tokens[2] = {"_model_", "_maskedByModel_"};
  for (int index = 0; index < 2; index++)
    {
    vtkMRMLAstroVolumeNode* previous = previousIDs[index] ?
      vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(previousIDs[index])) : nullptr;
    if (previous && previous != cube && previous->GetName() &&
        std::string(previous->GetName()).find(tokens[index]) != std::string::npos)
      {
      scene->RemoveNode(previous);
      }
    }
  const std::string name = std::string(cube->GetName() ? cube->GetName() : "") + "_model_" + std::to_string(serial);
  vtkMRMLAstroVolumeNode* model =
    vtkMRMLAstroVolumeNode::SafeDownCast(astroVolumeLogic->CloneVolume(scene, cube, name.c_str()));
  if (!model)
    {
    return false;
    }
  vtkMRMLNode* noPreset = nullptr;
  model->SetPresetNode(noPreset);
  for (int display = model->GetNumberOfDisplayNodes() - 1; display >= 0; display--)
    {
    if (vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(model->GetNthDisplayNode(display)))
      {
      model->RemoveNthDisplayNodeID(display);
      }
    }
  vtkNew<vtkMatrix4x4> rasToIJK;
  cube->GetRASToIJKMatrix(rasToIJK);
  model->SetRASToIJKMatrix(rasToIJK);
  model->SetAndObserveTransformNodeID(cube->GetTransformNodeID());
  const int wasModifying = pnode->StartModify();
  pnode->SetOutputVolumeNodeID(model->GetID());
  pnode->SetResidualVolumeNodeID(nullptr);
  pnode->SetOutputSerial(serial + 1);
  pnode->EndModify(wasModifying);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroModelingLogic::StartOperation(vtkMRMLAstroModelingParametersNode* pnode, int operation,
                                                 bool synchronous)
{
  this->LastError.clear();
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkSlicerAstroVolumeLogic* astroVolumeLogic = this->GetAstroVolumeLogic();
  if (this->OperationRunning)
    {
    this->LastError = "Another operation of 3DBarolo is running.";
    return false;
    }
  if (!scene || !pnode || !astroVolumeLogic)
    {
    this->LastError = "No scene, parameters or AstroVolume logic.";
    return false;
    }
  vtkMRMLAstroVolumeNode* cube = pnode->GetInputVolumeNodeID() ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetInputVolumeNodeID())) : nullptr;
  if (!cube || !cube->GetImageData() || !cube->GetAstroVolumeDisplayNode())
    {
    this->LastError = "No data cube.";
    return false;
    }
  if (cube->GetImageData()->GetDimensions()[2] < 2)
    {
    this->LastError = "3DBarolo models data cubes (two axes on the sky and the spectral axis).";
    return false;
    }
  // running from here: the preparation changes the scene (e.g. new cubes),
  // and the observers (panels following the active volume) must not change
  // the parameters meanwhile
  this->OperationRunning = true;
  std::shared_ptr<bool> prepared = std::make_shared<bool>(false);
  std::unique_ptr<void, std::function<void(void*)> > runningGuard(this, [this, prepared](void*)
    {
    if (!*prepared)
      {
      this->OperationRunning = false;
      }
    });
  const std::string cubeID = cube->GetID();

  // the beam: from the header, or estimated from the noise (saved with the
  // data)
  double beam[3] = {0., 0., 0.};
  if (!vtkSlicerAstroVolumeLogic::GetBeam(cube, beam))
    {
    if (!vtkSlicerAstroVolumeLogic::EstimateBeamFromNoise(cube, beam) ||
        !vtkSlicerAstroVolumeLogic::SetBeam(cube, beam))
      {
      this->LastError = "The beam of the data is needed: it is not in the FITS header and could not be "
                        "estimated from the noise (enter it in the Info section).";
      return false;
      }
    this->AppendLog("The FITS header has no beam: beam estimated from the noise.\n");
    }
  if (pnode->GetMaskActive() && !this->SelectMaskSegment(pnode))
    {
    return false;
    }
  vtkMRMLTableNode* table = this->EnsureParamsTable(pnode);
  if (!table)
    {
    this->LastError = "The table of the rings could not be created.";
    return false;
    }
  vtkMRMLAstroVolumeNode* model = pnode->GetOutputVolumeNodeID() ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetOutputVolumeNodeID())) : nullptr;
  if (operation != vtkMRMLAstroModelingParametersNode::UPDATE || !model || model == cube)
    {
    if (operation != vtkMRMLAstroModelingParametersNode::UPDATE && table->GetNumberOfRows() > 0)
      {
      // a new fit: a new table (the previous one stays with its model)
      table = this->EnsureParamsTable(pnode, true);
      }
    if (!this->CreateOutputVolumes(pnode))
      {
      this->LastError = "The model cube could not be created.";
      return false;
      }
    }
  if (operation == vtkMRMLAstroModelingParametersNode::UPDATE && table->GetNumberOfRows() < 1)
    {
    this->LastError = "The table has no rings.";
    return false;
    }
  // the model on the voxels of the cube (same type and size: 3DBarolo
  // writes it with the type of the cube)
  if (!pnode->GetInputVolumeNodeID() || cubeID != pnode->GetInputVolumeNodeID())
    {
    this->LastError = "The data cube changed while the operation was prepared.";
    return false;
    }
  model = vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  vtkMRMLAstroVolumeNode* residual = pnode->GetResidualVolumeNodeID() ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetResidualVolumeNodeID())) : nullptr;
  for (vtkMRMLAstroVolumeNode* output : {model, residual})
    {
    if (output == residual && !residual)
      {
      continue;
      }
    int cubeDims[3] = {0, 0, 0}, outputDims[3] = {0, 0, 0};
    cube->GetImageData()->GetDimensions(cubeDims);
    if (output && output->GetImageData())
      {
      output->GetImageData()->GetDimensions(outputDims);
      }
    if (!output || !output->GetImageData() || output->GetImageData()->GetScalarType() !=
        cube->GetImageData()->GetScalarType() || !std::equal(cubeDims, cubeDims + 3, outputDims))
      {
      this->LastError = "The model cube does not match the data cube.";
      return false;
      }
    }

  // the work uses private copies of the parameters and of the table and the
  // private volume nodes of PrepareWorkerData (voxels shared, no copy)
  const int wasModifying = pnode->StartModify();
  pnode->SetOperation(operation);
  pnode->SetStatus(1);
  if (operation != vtkMRMLAstroModelingParametersNode::UPDATE)
    {
    pnode->SetFitSuccess(false);
    }
  pnode->EndModify(wasModifying);
  this->OperationParameters = vtkSmartPointer<vtkMRMLAstroModelingParametersNode>::New();
  this->OperationParameters->Copy(pnode);
  this->OperationTable = vtkSmartPointer<vtkMRMLTableNode>::New();
  this->OperationTable->Copy(table);
  std::string error;
  if (!this->PrepareWorkerData(this->OperationParameters, error))
    {
    this->LastError = "The operation could not start (" + error + ").";
    this->OperationParameters = nullptr;
    this->OperationTable = nullptr;
    pnode->SetStatus(0);
    return false;
    }
  this->OperationSceneParameters = pnode;
  *prepared = true;
  this->OperationCancelled = false;
  this->LastOperationResult = 0;
  std::shared_ptr<int> result = std::make_shared<int>(0);
  vtkMRMLAstroModelingParametersNode* parameters = this->OperationParameters;
  vtkMRMLTableNode* operationTable = this->OperationTable;
  std::function<void()> work = [this, parameters, operationTable, operation, result]()
    {
    *result = operation == vtkMRMLAstroModelingParametersNode::UPDATE ?
      this->UpdateModelFromTable(parameters, operationTable) : this->OperateModel(parameters, operationTable);
    };
  std::function<void()> finished = [this, parameters, result]()
    {
    const bool cancelled = this->OperationCancelled || parameters->GetStatus() == -1;
    this->FinishOperation(cancelled ? -1 : (*result ? 1 : 0));
    };
  astroVolumeLogic->StartInBackground(OperationLabel(operation), work,
    [parameters]() { return std::max(0, std::min(100, parameters->GetStatus())); },
    [this]() { this->CancelOperation(); }, finished, synchronous);
  return true;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::SetMRMLSceneInternal(vtkMRMLScene* newScene)
{
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAboutToBeRemovedEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events);
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::ProcessMRMLSceneEvents(vtkObject* caller, unsigned long event, void* callData)
{
  if (event == vtkMRMLScene::NodeAboutToBeRemovedEvent)
    {
    this->OnNodeAboutToBeRemoved(reinterpret_cast<vtkMRMLNode*>(callData));
    return;
    }
  this->Superclass::ProcessMRMLSceneEvents(caller, event, callData);
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::OnNodeAboutToBeRemoved(vtkMRMLNode* node)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!node || !node->GetID() || !scene)
    {
    return;
    }
  const std::string nodeID = node->GetID();
  // a node used by the running operation: cancelled (the work keeps its own
  // references to the voxels until it ends)
  vtkMRMLAstroModelingParametersNode* running = this->OperationParameters;
  if (this->OperationRunning && running)
    {
    const char* usedIDs[4] = {running->GetInputVolumeNodeID(), running->GetOutputVolumeNodeID(),
                              running->GetResidualVolumeNodeID(), running->GetMaskSegmentationNodeID()};
    bool used = node == this->OperationSceneParameters.GetPointer();
    for (const char* usedID : usedIDs)
      {
      used = used || (usedID && nodeID == usedID);
      }
    if (used)
      {
      this->AppendLog("The operation is cancelled: " + std::string(node->GetName() ? node->GetName() : nodeID) +
                      " is removed from the scene.\n");
      this->CancelOperation();
      }
    }
  if (scene->IsClosing() || scene->IsImporting())
    {
    return;
    }
  // the data cube removed: the results of the module made from it cleared
  std::vector<vtkMRMLNode*> parametersNodes;
  scene->GetNodesByClass("vtkMRMLAstroModelingParametersNode", parametersNodes);
  for (vtkMRMLNode* candidate : parametersNodes)
    {
    vtkMRMLAstroModelingParametersNode* pnode = vtkMRMLAstroModelingParametersNode::SafeDownCast(candidate);
    if (!pnode || !pnode->GetInputVolumeNodeID() || nodeID != pnode->GetInputVolumeNodeID())
      {
      continue;
      }
    vtkMRMLTableNode* table = pnode->GetParamsTableNode();
    if (table && table->GetTable() && table->GetTable()->GetNumberOfRows() > 0)
      {
      table->GetTable()->SetNumberOfRows(0);
      table->GetTable()->Modified();
      table->Modified();
      }
    this->ClearResults(pnode);
    vtkMRMLSegmentationNode* contours = this->GetResultContours();
    if (contours && contours->GetSegmentation() && contours->GetSegmentation()->GetNumberOfSegments() > 0)
      {
      contours->GetSegmentation()->RemoveAllSegments();
      }
    }
}

//----------------------------------------------------------------------------
int vtkSlicerAstroModelingLogic::GetOperationProgress()
{
  return this->OperationParameters ? std::max(0, std::min(100, this->OperationParameters->GetStatus())) : 0;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::CancelOperation()
{
  if (!this->OperationRunning)
    {
    return;
    }
  this->OperationCancelled = true;
  this->RequestAbort();
  if (this->OperationParameters)
    {
    this->OperationParameters->SetStatus(-1);
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::FinishOperation(int result)
{
  this->FinishWorkerData(result == 1);
  vtkMRMLAstroModelingParametersNode* pnode = this->OperationSceneParameters;
  vtkMRMLScene* scene = this->GetMRMLScene();
  const int operation = this->OperationParameters ? this->OperationParameters->GetOperation() :
                                                    vtkMRMLAstroModelingParametersNode::UPDATE;
  if (pnode && scene && pnode->GetScene() == scene && this->OperationParameters)
    {
    // the parameters found by 3DBarolo (and the rings, except for the model
    // of the table, which keeps its table)
    vtkMRMLTableNode* table = pnode->GetParamsTableNode();
    pnode->Copy(this->OperationParameters);
    pnode->SetParamsTableNode(table);
    if (result == 1 && operation != vtkMRMLAstroModelingParametersNode::UPDATE && table && table->GetTable())
      {
      table->GetTable()->DeepCopy(this->OperationTable->GetTable());
      table->Modified();
      }
    vtkMRMLAstroVolumeNode* cube = vtkMRMLAstroVolumeNode::SafeDownCast(
      scene->GetNodeByID(pnode->GetInputVolumeNodeID() ? pnode->GetInputVolumeNodeID() : ""));
    vtkMRMLAstroVolumeNode* model = vtkMRMLAstroVolumeNode::SafeDownCast(
      scene->GetNodeByID(pnode->GetOutputVolumeNodeID() ? pnode->GetOutputVolumeNodeID() : ""));
    vtkMRMLAstroVolumeNode* residual = vtkMRMLAstroVolumeNode::SafeDownCast(
      scene->GetNodeByID(pnode->GetResidualVolumeNodeID() ? pnode->GetResidualVolumeNodeID() : ""));
    const bool success = result == 1 &&
      (operation == vtkMRMLAstroModelingParametersNode::UPDATE || pnode->GetFitSuccess());
    if (success && model)
      {
      vtkMRMLAstroVolumeNode* volumes[2] = {model, residual};
      const char* dataModels[2] = {"MODEL", "DATA"};
      for (int index = 0; index < 2; index++)
        {
        if (!volumes[index])
          {
          continue;
          }
        const int wasModifying = volumes[index]->StartModify();
        volumes[index]->UpdateRangeAttributes();
        volumes[index]->UpdateDisplayThresholdAttributes();
        volumes[index]->SetAttribute("SlicerAstro.DATAMODEL", dataModels[index]);
        volumes[index]->EndModify(wasModifying);
        }
      // the input mask hidden: the model and its contours are compared with
      // the data
      vtkMRMLSegmentationNode* inputMask = (pnode->GetMaskActive() && pnode->GetMaskSegmentationNodeID()) ?
        vtkMRMLSegmentationNode::SafeDownCast(scene->GetNodeByID(pnode->GetMaskSegmentationNodeID())) : nullptr;
      if (inputMask && inputMask->GetDisplayNode() && operation != vtkMRMLAstroModelingParametersNode::UPDATE &&
          operation != vtkMRMLAstroModelingParametersNode::ESTIMATE)
        {
        inputMask->GetDisplayNode()->SetVisibility(false);
        }
      this->UpdateResultsGeometry(pnode);
      this->UpdateResultContours(pnode);
      }
    else if (operation != vtkMRMLAstroModelingParametersNode::UPDATE)
      {
      // no model: the cubes of the operation removed
      for (vtkMRMLAstroVolumeNode* volume : {model, residual})
        {
        if (volume && volume != cube)
          {
          scene->RemoveNode(volume);
          }
        }
      const int wasModifying = pnode->StartModify();
      pnode->SetOutputVolumeNodeID(nullptr);
      pnode->SetResidualVolumeNodeID(nullptr);
      pnode->EndModify(wasModifying);
      this->ClearResults(pnode);
      if (cube)
        {
        cube->SetDisplayVisibility(1);
        }
      }
    pnode->SetStatus(0);
    }
  this->OperationParameters = nullptr;
  this->OperationTable = nullptr;
  this->OperationSceneParameters = nullptr;
  this->OperationRunning = false;
  this->LastOperationResult = result;
  this->InvokeEvent(OperationFinishedEvent);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroModelingLogic::UpdateResultsGeometry(vtkMRMLAstroModelingParametersNode* pnode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLTableNode* table = pnode ? pnode->GetParamsTableNode() : nullptr;
  vtkDoubleArray* phi = Column(table, "Phi");
  vtkDoubleArray* xPos = Column(table, "XPos");
  vtkDoubleArray* yPos = Column(table, "YPos");
  vtkMRMLAstroVolumeNode* cube = (scene && pnode && pnode->GetInputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetInputVolumeNodeID())) : nullptr;
  if (!phi || !xPos || !yPos || phi->GetNumberOfValues() < 1 || !cube || !cube->GetImageData())
    {
    return false;
    }
  double phiMean = 0., xMean = 0., yMean = 0.;
  const vtkIdType rings = phi->GetNumberOfValues();
  for (vtkIdType ring = 0; ring < rings; ring++)
    {
    phiMean += phi->GetValue(ring);
    xMean += xPos->GetValue(ring);
    yMean += yPos->GetValue(ring);
    }
  phiMean /= rings;
  xMean /= rings;
  yMean /= rings;
  // the centre at the central channel
  int dims[3] = {0, 0, 0};
  cube->GetImageData()->GetDimensions(dims);
  vtkNew<vtkMatrix4x4> ijkToRAS;
  cube->GetIJKToRASMatrix(ijkToRAS);
  const double ijk[4] = {xMean, yMean, static_cast<double>(static_cast<int>(dims[2] * 0.5)), 1.};
  double ras[4] = {0., 0., 0., 1.};
  ijkToRAS->MultiplyPoint(ijk, ras);
  const int wasModifying = pnode->StartModify();
  pnode->SetXPosCenterIJK(xMean);
  pnode->SetYPosCenterIJK(yMean);
  pnode->SetXPosCenterRAS(ras[0]);
  pnode->SetYPosCenterRAS(ras[1]);
  pnode->SetZPosCenterRAS(ras[2]);
  pnode->SetPVPhi(-(phiMean - 90.));
  pnode->EndModify(wasModifying);
  return true;
}

//----------------------------------------------------------------------------
vtkMRMLSegmentationNode* vtkSlicerAstroModelingLogic::GetResultContours()
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  return scene ? vtkMRMLSegmentationNode::SafeDownCast(scene->GetFirstNodeByName(GetResultContoursName())) : nullptr;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroModelingLogic::UpdateResultContours(vtkMRMLAstroModelingParametersNode* pnode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLAstroVolumeNode* cube = (scene && pnode && pnode->GetInputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetInputVolumeNodeID())) : nullptr;
  vtkMRMLAstroVolumeNode* model = (scene && pnode && pnode->GetOutputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetOutputVolumeNodeID())) : nullptr;
  if (!cube || !model || !cube->GetImageData() || !model->GetImageData() || model == cube)
    {
    return false;
    }
  vtkMRMLSegmentationNode* contours = this->GetResultContours();
  if (!contours)
    {
    vtkSmartPointer<vtkMRMLSegmentationNode> node = vtkSmartPointer<vtkMRMLSegmentationNode>::Take(
      vtkMRMLSegmentationNode::SafeDownCast(scene->CreateNodeByClass("vtkMRMLSegmentationNode")));
    node->SetName(GetResultContoursName());
    node->SetAttribute(vtkSlicerAstroVolumeLogic::GetContoursAttributeName(), "true");
    scene->AddNode(node);
    contours = node;
    }
  if (!contours->GetDisplayNode())
    {
    contours->CreateDefaultDisplayNodes();
    }
  vtkSegmentation* segmentation = contours->GetSegmentation();
  // one segment per kind of contours (scenes of SlicerAstro 1 had one per
  // level or per model)
  std::vector<std::string> segmentIDs;
  segmentation->GetSegmentIDs(segmentIDs);
  for (const std::string& segmentID : segmentIDs)
    {
    if (segmentID != GetDataContoursSegmentID() && segmentID != GetModelContoursSegmentID() &&
        segmentID != GetModelMaskSegmentID())
      {
      segmentation->RemoveSegment(segmentID);
      }
    }

  const double level = pnode->GetContourLevel();
  const double threshold = StringToDouble(cube->GetAttribute("SlicerAstro.DisplayThreshold")) * level;
  std::ostringstream suffix;
  suffix << "_" << level << "RMS";
  struct ContourSegment
    {
    const char* ID;
    std::string Name;
    double Color[3];
    vtkMRMLAstroVolumeNode* Volume;
    double Lower;
    bool Model;
    };
  const ContourSegment segments[3] = {
    {GetDataContoursSegmentID(), std::string(cube->GetName()) + suffix.str(), {0.5, 0.68, 0.5}, cube, threshold, false},
    {GetModelContoursSegmentID(), std::string(model->GetName()) + suffix.str(), {1., 0.9, 0.13}, model, threshold,
     true},
    // the region of the model (independent of the contour level)
    {GetModelMaskSegmentID(), std::string(model->GetName()) + "_asMask", {0.73, 0.06, 0.59}, model, 1e-6, true}};
  vtkSlicerAstroVolumeLogic::PrepareContoursUpdate(contours);
  for (const ContourSegment& entry : segments)
    {
    vtkSegment* segment = segmentation->GetSegment(entry.ID);
    if (!segment)
      {
      double color[3] = {entry.Color[0], entry.Color[1], entry.Color[2]};
      segmentation->AddEmptySegment(entry.ID, entry.Name, color);
      segment = segmentation->GetSegment(entry.ID);
      if (segment)
        {
        segment->SetTerminology(entry.Model ? vtkMRMLAstroTerminology::GetModelEntry() :
                                              vtkMRMLAstroTerminology::GetContourEntry());
        }
      }
    else
      {
      segment->SetName(entry.Name.c_str());
      }
    vtkNew<vtkImageThreshold> imageThreshold;
    imageThreshold->SetInputData(entry.Volume->GetImageData());
    imageThreshold->ThresholdBetween(entry.Lower,
                                     std::max(entry.Lower, StringToDouble(entry.Volume->GetAttribute("SlicerAstro.DATAMAX"))));
    imageThreshold->SetInValue(1);
    imageThreshold->SetOutValue(0);
    imageThreshold->SetOutputScalarType(VTK_SHORT);
    imageThreshold->Update();
    vtkNew<vtkOrientedImageData> labelmap;
    labelmap->ShallowCopy(imageThreshold->GetOutput());
    vtkNew<vtkMatrix4x4> ijkToRAS;
    entry.Volume->GetIJKToRASMatrix(ijkToRAS);
    labelmap->SetGeometryFromImageToWorldMatrix(ijkToRAS);
    if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(labelmap, contours, entry.ID,
                                                                       vtkSlicerSegmentationsModuleLogic::MODE_REPLACE))
      {
      vtkErrorMacro("UpdateResultContours: the segment " << entry.ID << " could not be updated");
      return false;
      }
    }
  for (int index = 0; index < contours->GetNumberOfDisplayNodes(); index++)
    {
    vtkMRMLSegmentationDisplayNode* display =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(contours->GetNthDisplayNode(index));
    if (!display)
      {
      continue;
      }
    const int wasModifying = display->StartModify();
    // data and model contours in 2D (outlines), model and its region in 3D
    display->SetSegmentVisibility3D(GetDataContoursSegmentID(), false);
    display->SetSegmentVisibility3D(GetModelContoursSegmentID(), true);
    display->SetSegmentVisibility3D(GetModelMaskSegmentID(), true);
    display->SetSegmentVisibility2DOutline(GetDataContoursSegmentID(), true);
    display->SetSegmentVisibility2DOutline(GetModelContoursSegmentID(), true);
    display->SetSegmentVisibility2DOutline(GetModelMaskSegmentID(), false);
    for (const ContourSegment& entry : segments)
      {
      display->SetSegmentVisibility2DFill(entry.ID, false);
      }
    display->EndModify(wasModifying);
    }
  vtkSlicerAstroVolumeLogic::SetContoursDisplay(contours);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroModelingLogic::SetResultContoursVisibility(const char* segmentID, bool visible)
{
  vtkMRMLSegmentationNode* contours = this->GetResultContours();
  if (!contours || !segmentID || !contours->GetSegmentation()->GetSegment(segmentID))
    {
    return false;
    }
  for (int index = 0; index < contours->GetNumberOfDisplayNodes(); index++)
    {
    vtkMRMLSegmentationDisplayNode* display =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(contours->GetNthDisplayNode(index));
    if (display)
      {
      display->SetSegmentVisibility(segmentID, visible);
      }
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroModelingLogic::GetResultContoursVisibility(const char* segmentID)
{
  vtkMRMLSegmentationNode* contours = this->GetResultContours();
  vtkMRMLSegmentationDisplayNode* display = contours ?
    vtkMRMLSegmentationDisplayNode::SafeDownCast(contours->GetDisplayNode()) : nullptr;
  return display && segmentID && contours->GetSegmentation()->GetSegment(segmentID) &&
         display->GetVisibility() && display->GetSegmentVisibility(segmentID);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroModelingLogic::SetPVSlices(vtkMRMLAstroModelingParametersNode* pnode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLSliceNode* yellow = SliceNode(scene, "Yellow");
  vtkMRMLSliceNode* green = SliceNode(scene, "Green");
  vtkMRMLSliceNode* red = SliceNode(scene, "Red");
  vtkMRMLTableNode* table = pnode ? pnode->GetParamsTableNode() : nullptr;
  if (!yellow || !green || !table || table->GetNumberOfRows() < 1)
    {
    return false;
    }
  // absolute orientations: the axes of the rings plus the offsets of the
  // user (no incremental rotation, the same after a scene is loaded)
  vtkNew<vtkMatrix3x3> major;
  vtkNew<vtkMatrix3x3> minor;
  RotatedXZ(yellow, pnode->GetPVPhi() + pnode->GetYellowRotValue(), major);
  RotatedXZ(green, pnode->GetPVPhi() + 90. + pnode->GetGreenRotValue(), minor);
  for (vtkMRMLSliceNode* sliceNode : {yellow, green, red})
    {
    if (sliceNode)
      {
      SetOrientationPreset(sliceNode, "PVMajor", major);
      SetOrientationPreset(sliceNode, "PVMinor", minor);
      }
    }
  const double center[3] = {pnode->GetXPosCenterRAS(), pnode->GetYPosCenterRAS(), pnode->GetZPosCenterRAS()};
  const char* presets[2] = {"PVMajor", "PVMinor"};
  vtkMRMLSliceNode* slices[2] = {yellow, green};
  for (int index = 0; index < 2; index++)
    {
    const int wasModifying = slices[index]->StartModify();
    slices[index]->SetOrientation(presets[index]);
    for (int axis = 0; axis < 3; axis++)
      {
      slices[index]->GetSliceToRAS()->SetElement(axis, 3, center[axis]);
      }
    slices[index]->UpdateMatrices();
    slices[index]->EndModify(wasModifying);
    }
  return this->UpdateRingMarkers(pnode);
}

//----------------------------------------------------------------------------
vtkMRMLMarkupsFiducialNode* vtkSlicerAstroModelingLogic::GetRingMarkers(bool major)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  return scene ? vtkMRMLMarkupsFiducialNode::SafeDownCast(scene->GetFirstNodeByName(GetRingMarkersName(major))) :
                 nullptr;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroModelingLogic::UpdateRingMarkers(vtkMRMLAstroModelingParametersNode* pnode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLTableNode* table = pnode ? pnode->GetParamsTableNode() : nullptr;
  vtkMRMLAstroVolumeNode* cube = (scene && pnode && pnode->GetInputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetInputVolumeNodeID())) : nullptr;
  vtkMRMLAstroVolumeDisplayNode* cubeDisplay = cube ? cube->GetAstroVolumeDisplayNode() : nullptr;
  vtkDoubleArray* radii = Column(table, "Radii");
  vtkDoubleArray* vRot = Column(table, "VRot");
  vtkDoubleArray* vRad = Column(table, "VRad");
  vtkDoubleArray* inc = Column(table, "Inc");
  vtkDoubleArray* phi = Column(table, "Phi");
  vtkDoubleArray* vSys = Column(table, "VSys");
  vtkDoubleArray* xPos = Column(table, "XPos");
  vtkDoubleArray* yPos = Column(table, "YPos");
  if (!cubeDisplay || !cube->GetImageData() || !radii || !vRot || !vRad || !inc || !phi || !vSys || !xPos || !yPos)
    {
    return false;
    }
  const vtkIdType rings = radii->GetNumberOfValues();
  for (vtkDoubleArray* array : {vRot, vRad, inc, phi, vSys, xPos, yPos})
    {
    if (array->GetNumberOfValues() < rings)
      {
      return false;
      }
    }

  // geometry of the rings as in the model of 3DBarolo (Galmod): pixel steps
  // in arcsec (signed), position angle plus the rotation of the image
  const double deg2rad = vtkMath::Pi() / 180.;
  const double cdelt1 = StringToDouble(cube->GetAttribute("SlicerAstro.CDELT1")) * 3600.;
  const double cdelt2 = StringToDouble(cube->GetAttribute("SlicerAstro.CDELT2")) * 3600.;
  const double crota = StringToDouble(cube->GetAttribute("SlicerAstro.CROTA2")) * deg2rad;
  if (cdelt1 == 0. || cdelt2 == 0.)
    {
    return false;
    }
  // the velocities of 3DBarolo are optical; the spectral axis of the cube
  // may be shown as radio velocity (same frequency: vrad = c vopt / (c + vopt))
  const bool radioVelocity = !strncmp(cubeDisplay->GetVelocityDefinition().c_str(), "VRAD", 4);
  const double speedOfLight = 299792.458;
  vtkNew<vtkMatrix4x4> rasToIJK;
  cube->GetRASToIJKMatrix(rasToIJK);
  vtkNew<vtkMatrix4x4> ijkToRAS;
  cube->GetIJKToRASMatrix(ijkToRAS);

  for (bool major : {true, false})
    {
    vtkMRMLSliceNode* sliceNode = SliceNode(scene, major ? "Yellow" : "Green");
    if (!sliceNode)
      {
      continue;
      }
    vtkMRMLMarkupsFiducialNode* points = this->GetRingMarkers(major);
    if (!points)
      {
      points = vtkMRMLMarkupsFiducialNode::SafeDownCast(
        scene->AddNewNodeByClass("vtkMRMLMarkupsFiducialNode", GetRingMarkersName(major)));
      if (!points)
        {
        continue;
        }
      points->SetHideFromEditors(true);
      }
    if (!points->GetDisplayNode())
      {
      points->CreateDefaultDisplayNodes();
      }
    vtkMRMLMarkupsDisplayNode* display = points->GetMarkupsDisplayNode();
    if (display)
      {
      const int wasModifyingDisplay = display->StartModify();
      display->SetGlyphScale(1.5);
      display->SetTextScale(0.);
      if (!major)
        {
        display->SetColor(1., 1., 0.44);
        }
      display->RemoveAllViewNodeIDs();
      display->AddViewNodeID(sliceNode->GetID());
      display->EndModify(wasModifyingDisplay);
      }

    // the slice in the pixels (IJK) of the cube: its centre and its
    // direction on the sky
    vtkMatrix4x4* sliceToRAS = sliceNode->GetSliceToRAS();
    double centerRAS[4] = {sliceToRAS->GetElement(0, 3), sliceToRAS->GetElement(1, 3),
                           sliceToRAS->GetElement(2, 3), 1.};
    double centerIJK[4] = {0., 0., 0., 1.};
    rasToIJK->MultiplyPoint(centerRAS, centerIJK);
    double direction[2] = {0., 0.};
    for (int axis = 0; axis < 2 && std::hypot(direction[0], direction[1]) < 1e-9; axis++)
      {
      double axisRAS[4] = {centerRAS[0] + sliceToRAS->GetElement(0, axis),
                           centerRAS[1] + sliceToRAS->GetElement(1, axis),
                           centerRAS[2] + sliceToRAS->GetElement(2, axis), 1.};
      double axisIJK[4] = {0., 0., 0., 1.};
      rasToIJK->MultiplyPoint(axisRAS, axisIJK);
      direction[0] = axisIJK[0] - centerIJK[0];
      direction[1] = axisIJK[1] - centerIJK[1];
      }
    const double directionNorm = std::hypot(direction[0], direction[1]);
    if (directionNorm < 1e-9)
      {
      continue;
      }
    direction[0] /= directionNorm;
    direction[1] /= directionNorm;

    points->GlobalWarningDisplayOff();
    const int wasModifying = points->StartModify();
    if (points->GetNumberOfControlPoints() != rings * 2)
      {
      points->RemoveAllControlPoints();
      for (vtkIdType ring = 0; ring < rings; ring++)
        {
        for (int side = 0; side < 2; side++)
          {
          const std::string label = std::string(side == 0 ? "" : "-") + (major ? "RMajor" : "RMinor") +
                                    std::to_string(ring);
          const int index = points->AddControlPoint(vtkVector3d(0., 0., 0.), label);
          points->SetNthControlPointSelected(index, false);
          points->SetNthControlPointLocked(index, true);
          }
        }
      }
    for (vtkIdType ring = 0; ring < rings; ring++)
      {
      const double radius = radii->GetValue(ring);
      const double inclination = inc->GetValue(ring) * deg2rad;
      const double pa = phi->GetValue(ring) * deg2rad + crota;
      const double spa = std::sin(pa);
      const double cpa = std::cos(pa);
      const double x0 = xPos->GetValue(ring);
      const double y0 = yPos->GetValue(ring);
      // point of the ring at the azimuth az (from the receding major axis,
      // in the plane of the disk): x = R cos(az), y = R sin(az) cos(inc),
      // pixel = (x0 + (x spa - y cpa) / cdelt1, y0 + (x cpa + y spa) / cdelt2).
      // On the line of the slice: A + B cos(az) + E sin(az) = 0.
      const double ai = x0 - centerIJK[0];
      const double aj = y0 - centerIJK[1];
      const double bi = radius * spa / cdelt1;
      const double ei = -radius * std::cos(inclination) * cpa / cdelt1;
      const double bj = radius * cpa / cdelt2;
      const double ej = radius * std::cos(inclination) * spa / cdelt2;
      const double a = ai * direction[1] - aj * direction[0];
      const double b = bi * direction[1] - bj * direction[0];
      const double e = ei * direction[1] - ej * direction[0];
      const double rho = std::hypot(b, e);
      const int firstIndex = static_cast<int>(ring) * 2;
      if (radius <= 0. || rho < 1e-12 || std::fabs(a) > rho)
        {
        // the ring does not cross the slice
        points->SetNthControlPointVisibility(firstIndex, false);
        points->SetNthControlPointVisibility(firstIndex + 1, false);
        continue;
        }
      const double base = std::atan2(e, b);
      const double delta = std::acos(-a / rho);
      for (int side = 0; side < 2; side++)
        {
        const double az = base + (side == 0 ? delta : -delta);
        const double x = radius * std::cos(az);
        const double y = radius * std::sin(az) * std::cos(inclination);
        double ijk[3] = {x0 + (x * spa - y * cpa) / cdelt1, y0 + (x * cpa + y * spa) / cdelt2, 0.};
        double velocity = vSys->GetValue(ring) +
          (vRot->GetValue(ring) * std::cos(az) + vRad->GetValue(ring) * std::sin(az)) * std::sin(inclination);
        if (radioVelocity)
          {
          velocity = speedOfLight * velocity / (speedOfLight + velocity);
          }
        double world[3] = {0., 0., 0.}, velocityIJK[3] = {0., 0., 0.};
        cubeDisplay->GetReferenceSpace(ijk, world);
        world[2] = velocity * 1000.;
        cubeDisplay->GetIJKSpace(world, velocityIJK);
        const double pointIJK[4] = {ijk[0], ijk[1], velocityIJK[2], 1.};
        double pointRAS[4] = {0., 0., 0., 1.};
        ijkToRAS->MultiplyPoint(pointIJK, pointRAS);
        points->SetNthControlPointPosition(firstIndex + side, pointRAS[0], pointRAS[1], pointRAS[2]);
        points->SetNthControlPointVisibility(firstIndex + side, true);
        }
      }
    points->EndModify(wasModifying);
    points->GlobalWarningDisplayOn();
    }
  return true;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::ClearResults(vtkMRMLAstroModelingParametersNode* pnode)
{
  for (bool major : {true, false})
    {
    vtkMRMLMarkupsFiducialNode* points = this->GetRingMarkers(major);
    if (points)
      {
      points->GlobalWarningDisplayOff();
      points->RemoveAllControlPoints();
      points->GlobalWarningDisplayOn();
      }
    }
  if (!pnode)
    {
    return;
    }
  const int wasModifying = pnode->StartModify();
  pnode->SetXPosCenterIJK(0.);
  pnode->SetYPosCenterIJK(0.);
  pnode->SetPVPhi(0.);
  pnode->SetYellowRotValue(0.);
  pnode->SetGreenRotValue(0.);
  pnode->EndModify(wasModifying);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroModelingLogic::ShowResults(vtkMRMLAstroModelingParametersNode* pnode, bool setLayout)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkSlicerAstroVolumeLogic* astroVolumeLogic = this->GetAstroVolumeLogic();
  vtkMRMLAstroVolumeNode* cube = (scene && pnode && pnode->GetInputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetInputVolumeNodeID())) : nullptr;
  vtkMRMLAstroVolumeNode* model = (scene && pnode && pnode->GetOutputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(pnode->GetOutputVolumeNodeID())) : nullptr;
  if (!cube || !cube->GetImageData() || !model || !model->GetImageData() || !astroVolumeLogic)
    {
    return false;
    }
  if (setLayout)
    {
    vtkMRMLLayoutNode* layoutNode = vtkMRMLLayoutNode::SafeDownCast(scene->GetFirstNodeByClass("vtkMRMLLayoutNode"));
    const int arrangement = layoutNode ? layoutNode->GetViewArrangement() : 0;
    if (layoutNode && arrangement != vtkMRMLLayoutNode::SlicerLayoutConventionalPlotView &&
        arrangement != vtkMRMLLayoutNode::SlicerLayoutFourUpPlotView &&
        arrangement != vtkMRMLLayoutNode::SlicerLayoutFourUpPlotTableView &&
        arrangement != vtkMRMLLayoutNode::SlicerLayoutOneUpPlotView &&
        arrangement != vtkMRMLLayoutNode::SlicerLayoutThreeOverThreePlotView)
      {
      layoutNode->SetViewArrangement(vtkMRMLLayoutNode::SlicerLayoutConventionalPlotView);
      }
    }
  // the model ready for the 3D views, the cube shown in 3D
  const bool autoShow = astroVolumeLogic->GetAutoShowActiveVolume();
  astroVolumeLogic->SetAutoShowActiveVolume(false);
  astroVolumeLogic->PrepareVolumeRendering(model);
  astroVolumeLogic->SetVolumeRenderingVisibility(model, false, false);
  astroVolumeLogic->PrepareVolumeRendering(cube);
  astroVolumeLogic->SetVolumeRenderingVisibility(cube, true, true);
  vtkMRMLSelectionNode* selection = vtkMRMLSelectionNode::SafeDownCast(
    scene->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selection)
    {
    selection->SetSecondaryVolumeID(model->GetID());
    selection->SetActiveVolumeID(cube->GetID());
    }
  astroVolumeLogic->SetAutoShowActiveVolume(autoShow);
  // the cube in the 2D views (not other data, e.g. moment maps): the red
  // view on the sky, the yellow and green views the PV slices
  for (const char* name : {"Red", "Yellow", "Green"})
    {
    vtkMRMLSliceCompositeNode* composite = vtkMRMLSliceCompositeNode::SafeDownCast(
      scene->GetNodeByID((std::string("vtkMRMLSliceCompositeNode") + name).c_str()));
    if (composite)
      {
      const int wasModifying = composite->StartModify();
      composite->SetBackgroundVolumeID(cube->GetID());
      composite->SetForegroundVolumeID(nullptr);
      composite->SetLabelVolumeID(nullptr);
      composite->EndModify(wasModifying);
      }
    }
  vtkMRMLSliceNode* red = SliceNode(scene, "Red");
  if (red && strcmp(red->GetOrientationString(), "XY"))
    {
    red->SetOrientation("XY");
    }
  this->UpdateResultContours(pnode);
  this->SetResultsShown(pnode, true);
  return this->SetPVSlices(pnode);
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::SetRingMarkersVisibility(vtkMRMLAstroModelingParametersNode* pnode, bool visible)
{
  if (pnode)
    {
    pnode->SetAttribute("SlicerAstro.RingMarkersVisible", visible ? "true" : "false");
    }
  for (bool major : {true, false})
    {
    vtkMRMLMarkupsFiducialNode* points = this->GetRingMarkers(major);
    if (points && points->GetDisplayNode())
      {
      points->GetDisplayNode()->SetVisibility(visible);
      }
    }
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroModelingLogic::GetRingMarkersVisibility(vtkMRMLAstroModelingParametersNode* pnode)
{
  const char* value = pnode ? pnode->GetAttribute("SlicerAstro.RingMarkersVisible") : nullptr;
  return !value || strcmp(value, "false");
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::SetResultsShown(vtkMRMLAstroModelingParametersNode* pnode, bool shown)
{
  vtkMRMLSegmentationNode* contours = this->GetResultContours();
  if (contours && contours->GetDisplayNode())
    {
    contours->GetDisplayNode()->SetVisibility(shown);
    }
  const bool markers = shown && this->GetRingMarkersVisibility(pnode);
  for (bool major : {true, false})
    {
    vtkMRMLMarkupsFiducialNode* points = this->GetRingMarkers(major);
    if (points && points->GetDisplayNode())
      {
      points->GetDisplayNode()->SetVisibility(markers);
      }
    }
}

//----------------------------------------------------------------------------
vtkMRMLPlotChartNode* vtkSlicerAstroModelingLogic::UpdateRingPlot(vtkMRMLAstroModelingParametersNode* pnode,
                                                                 const char* columnName)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLTableNode* table = pnode ? pnode->GetParamsTableNode() : nullptr;
  const RingColumn* column = nullptr;
  for (const RingColumn& entry : RingColumns)
    {
    if (columnName && !strcmp(entry.Name, columnName))
      {
      column = &entry;
      }
    }
  if (!scene || !table || !column || !Column(table, column->Name))
    {
    return nullptr;
    }
  vtkMRMLPlotSeriesNode* series = vtkMRMLPlotSeriesNode::SafeDownCast(
    scene->GetFirstNode("ModelingRingsSeries", "vtkMRMLPlotSeriesNode"));
  if (!series)
    {
    series = vtkMRMLPlotSeriesNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLPlotSeriesNode",
                                                                          "ModelingRingsSeries"));
    series->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
    series->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
    series->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
    series->SetMarkerSize(9);
    series->SetLineWidth(3);
    series->SetColor(0.96, 0.65, 0.14);
    }
  series->SetAndObserveTableNodeID(table->GetID());
  series->SetXColumnName("Radii");
  series->SetYColumnName(column->Name);
  vtkMRMLPlotChartNode* chart = vtkMRMLPlotChartNode::SafeDownCast(
    scene->GetFirstNode("ModelingRingsChart", "vtkMRMLPlotChartNode"));
  if (!chart)
    {
    chart = vtkMRMLPlotChartNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLPlotChartNode",
                                                                        "ModelingRingsChart"));
    chart->SetEnablePointMoveAlongX(false);
    chart->SetLegendVisibility(false);
    }
  chart->SetXAxisTitle("Radius (arcsec)");
  std::string unit = column->Unit;
  const std::string::size_type space = unit.find(' ');
  if (space != std::string::npos)
    {
    unit = unit.substr(0, space);
    }
  chart->SetYAxisTitle((std::string(column->Title) + " (" + unit + ")").c_str());
  chart->SetTitle(column->Title);
  if (!chart->HasPlotSeriesNodeID(series->GetID()))
    {
    chart->RemoveAllPlotSeriesNodeIDs();
    chart->AddAndObservePlotSeriesNodeID(series->GetID());
    }
  vtkMRMLSelectionNode* selection = vtkMRMLSelectionNode::SafeDownCast(
    scene->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selection)
    {
    selection->SetActivePlotChartID(chart->GetID());
    }
  return chart;
}
