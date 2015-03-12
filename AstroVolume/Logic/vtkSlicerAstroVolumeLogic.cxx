/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkSlicerAstroVolumeLogic.cxx,v $
  Date:      $Date: 2006/01/06 17:56:48 $
  Version:   $Revision: 1.58 $

=========================================================================auto=*/

// STD includes
#include <algorithm>

// AstroVolume includes
#include "vtkSlicerAstroVolumeLogic.h"

// MRML logic includes
#include "vtkMRMLColorLogic.h"

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
#include <vtkGeneralTransform.h>
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

  // set up the Astro scalar node's support nodes
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

}
//end of anonyous namespace


//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic::vtkSlicerAstroVolumeLogic()
{
  this->RegisterArchetypeVolumeNodeSetFactory( AstroScalarVolumeNodeSetFactory );

  this->SetCompareVolumeGeometryEpsilon(0.000001);
}


vtkSlicerAstroVolumeLogic::~vtkSlicerAstroVolumeLogic()
{
}


void vtkSlicerAstroVolumeLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);

  os << indent << "vtkSlicerAstroVolumeLogic:             " << this->GetClassName() << "\n";

  os << indent << "ActiveVolumeNode: " <<
        (this->ActiveVolumeNode ? this->ActiveVolumeNode->GetName() : "(none)") << "\n";
}

int vtkSlicerAstroVolumeLogic::SaveArchetypeVolume(const char *filename, vtkMRMLVolumeNode *volumeNode)
{
  if (volumeNode == NULL || filename == NULL)
    {
    return 0;
    }

  vtkMRMLAstroVolumeStorageNode *storageNode1 = NULL;
  vtkMRMLStorageNode *storageNode = NULL;
  vtkMRMLStorageNode *snode = volumeNode->GetStorageNode();

  if (snode != NULL)
    {
    storageNode1 = vtkMRMLAstroVolumeStorageNode::SafeDownCast(snode);
    }

  bool useURI = false;
  if (this->GetMRMLScene() &&
      this->GetMRMLScene()->GetCacheManager())
    {
    useURI = this->GetMRMLScene()->GetCacheManager()->IsRemoteReference(filename);
    }

  // Use FITS writer if we are dealing with Astro volumes

  if (volumeNode->IsA("vtkMRMLScalarVolumeNode"))
    {

    if (storageNode1 == NULL)
      {
      storageNode1 = vtkMRMLAstroVolumeStorageNode::New();
      storageNode1->SetScene(this->GetMRMLScene());
      this->GetMRMLScene()->AddNode(storageNode1);
      volumeNode->SetAndObserveStorageNodeID(storageNode1->GetID());
      storageNode1->Delete();
      }
    if (useURI)
      {
      storageNode1->SetURI(filename);
      }
    else
      {
      storageNode1->SetFileName(filename);
      }
    storageNode = storageNode1;
    }
  else vtkErrorMacro("FITSWriter handle only Scalar Volumes");

  int res = storageNode->WriteData(volumeNode);
  return res;
}

