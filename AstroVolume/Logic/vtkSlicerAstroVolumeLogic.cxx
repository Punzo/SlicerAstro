/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkSlicerVolumesLogic.cxx,v $
  Date:      $Date: 2006/01/06 17:56:48 $
  Version:   $Revision: 1.58 $

=========================================================================auto=*/


// Volumes includes
#include "vtkSlicerAstroVolumeLogic.h"

// MRML nodes includes

#include "vtkMRMLAstroVolumeStorageNode.h"
#include "vtkCacheManager.h"
#include "vtkDataIOManager.h"
#include "vtkMRMLLabelMapVolumeDisplayNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLVolumeArchetypeStorageNode.h"
#include "vtkMRMLTransformNode.h"
#include "vtkMRMLScalarVolumeNode.h"
#include "vtkMRMLScalarVolumeDisplayNode.h"
#include "vtkMRMLNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLVolumeNode.h"
#include "vtkMRMLColorLogic.h"


// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkImageData.h>
#include <vtkImageThreshold.h>
#include <vtkMathUtilities.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtksys/SystemTools.hxx>
#include <vtkVersion.h>
#include <vtkWeakPointer.h>
#include <vtkImageReslice.h>
#include <vtkTransform.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroVolumeLogic);

//----------------------------------------------------------------------------

namespace
{
//----------------------------------------------------------------------------

ArchetypeVolumeNodeSet AstroScalarVolumeNodeSetFactory(std::string& volumeName, vtkMRMLScene* scene, int options)
{
  ArchetypeVolumeNodeSet nodeSet(scene);

  // set up the scalar node's support nodes
  vtkNew<vtkMRMLScalarVolumeNode> scalarNode;
  scalarNode->SetName(volumeName.c_str());
  scalarNode->SetLabelMap(0);
  nodeSet.Scene->AddNode(scalarNode.GetPointer());

  vtkNew<vtkMRMLScalarVolumeDisplayNode> sdisplayNode;
  nodeSet.Scene->AddNode(sdisplayNode.GetPointer());
  scalarNode->SetAndObserveDisplayNodeID(sdisplayNode->GetID());

  vtkNew<vtkMRMLAstroVolumeStorageNode> storageNode;
  storageNode->SetCenterImage(options & vtkSlicerAstroVolumeLogic::CenterImage);
  nodeSet.Scene->AddNode(storageNode.GetPointer());
  scalarNode->SetAndObserveStorageNodeID(storageNode->GetID());

  nodeSet.StorageNode = storageNode.GetPointer();
  nodeSet.DisplayNode = sdisplayNode.GetPointer();
  nodeSet.Node = scalarNode.GetPointer();

  return nodeSet;
}

}//end of anonyous namespace


//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic::vtkSlicerAstroVolumeLogic()
{
  this->RegisterArchetypeVolumeNodeSetFactory( AstroScalarVolumeNodeSetFactory );

  this->SetCompareVolumeGeometryEpsilon(0.000001);
}


vtkSlicerAstroVolumeLogic::~vtkSlicerAstroVolumeLogic()
{
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::ProcessMRMLNodesEvents(vtkObject *vtkNotUsed(caller),
                                            unsigned long event,
                                            void *callData)
{
  if (event ==  vtkCommand::ProgressEvent)
    {
    this->InvokeEvent ( vtkCommand::ProgressEvent,callData );
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroVolumeLogic::SetColorLogic(vtkMRMLColorLogic *colorLogic)
{
  if (this->ColorLogic == colorLogic)
    {
    return;
    }
  this->ColorLogic = colorLogic;
  this->Modified();
}

void vtkSlicerAstroVolumeLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);

  os << indent << "vtkSlicerAstroVolumesLogic:             " << this->GetClassName() << "\n";

  os << indent << "ActiveVolumeNode: " <<
    (this->ActiveVolumeNode ? this->ActiveVolumeNode->GetName() : "(none)") << "\n";
}
