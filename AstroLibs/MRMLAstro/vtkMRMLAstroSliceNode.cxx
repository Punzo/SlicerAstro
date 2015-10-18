// MRML includes
#include "vtkMRMLScene.h"
#include "vtkMRMLAstroSliceNode.h"
#include "vtkMRMLTransformNode.h"
#include "vtkMRMLVolumeNode.h"

// VTK includes
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkVector.h>

// VNL includes
#include <vnl/vnl_double_3.h>

// STL includes
#include <algorithm>

//------------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroSliceNode);

//----------------------------------------------------------------------------
// Constructor
vtkMRMLAstroSliceNode::vtkMRMLAstroSliceNode()
{
 //this->SetOrientationToXY();
}

//----------------------------------------------------------------------------
vtkMRMLAstroSliceNode::~vtkMRMLAstroSliceNode()
{
}

/*
//----------------------------------------------------------------------------
void vtkMRMLAstroSliceNode::SetOrientation(const char* orientation)
{
  if (!orientation)
    {
    return;
    }
 if (!strcmp(orientation, "XY"))
    {
    this->SetOrientationToXY();
    }
  else if (!strcmp(orientation, "XZ"))
    {
    this->SetOrientationToXZ();
    }
  else if (!strcmp(orientation, "ZY"))
    {
    this->SetOrientationToZY();
    }
  else if (!strcmp(orientation, "Reformat"))
    {
    this->SetOrientationToReformat();
    }
  else
    {
    vtkErrorMacro("SetOrientation: invalid orientation: " << orientation);
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSliceNode::SetOrientationToReformat()
{
    // Don't need to do anything.  Leave the matrices where they were
    // so the reformat starts where you were.

    this->SetOrientationString( "Reformat" );
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSliceNode::SetOrientationToXZ()
{
    this->SliceToRAS->SetElement(0, 0, -1.0);
    this->SliceToRAS->SetElement(1, 0,  0.0);
    this->SliceToRAS->SetElement(2, 0,  0.0);
    this->SliceToRAS->SetElement(0, 1,  0.0);
    this->SliceToRAS->SetElement(1, 1,  1.0);
    this->SliceToRAS->SetElement(2, 1,  0.0);
    this->SliceToRAS->SetElement(0, 2,  0.0);
    this->SliceToRAS->SetElement(1, 2,  0.0);
    this->SliceToRAS->SetElement(2, 2,  1.0);

    this->SetOrientationString( "XZ" );
    this->SetOrientationReference( "XZ" );
    this->UpdateMatrices();
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSliceNode::SetOrientationToZY()
{
    this->SliceToRAS->SetElement(0, 0,  0.0);
    this->SliceToRAS->SetElement(1, 0, -1.0);
    this->SliceToRAS->SetElement(2, 0,  0.0);
    this->SliceToRAS->SetElement(0, 1,  0.0);
    this->SliceToRAS->SetElement(1, 1,  0.0);
    this->SliceToRAS->SetElement(2, 1,  1.0);
    this->SliceToRAS->SetElement(0, 2,  1.0);
    this->SliceToRAS->SetElement(1, 2,  0.0);
    this->SliceToRAS->SetElement(2, 2,  0.0);

    this->SetOrientationString( "ZY" );
    this->SetOrientationReference( "ZY" );
    this->UpdateMatrices();
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSliceNode::SetOrientationToXY()
{
    this->SliceToRAS->SetElement(0, 0, -1.0);
    this->SliceToRAS->SetElement(1, 0,  0.0);
    this->SliceToRAS->SetElement(2, 0,  0.0);
    this->SliceToRAS->SetElement(0, 1,  0.0);
    this->SliceToRAS->SetElement(1, 1,  0.0);
    this->SliceToRAS->SetElement(2, 1,  1.0);
    this->SliceToRAS->SetElement(0, 2,  0.0);
    this->SliceToRAS->SetElement(1, 2,  1.0);
    this->SliceToRAS->SetElement(2, 2,  0.0);

    this->SetOrientationString( "XY" );
    this->SetOrientationReference( "XY" );
    this->UpdateMatrices();
}


//----------------------------------------------------------------------------
//  Calculate XYToSlice and XYToRAS
//  Inputs: Dimenionss, FieldOfView, SliceToRAS
//
void vtkMRMLAstroSliceNode::UpdateMatrices()
{

  if (this->IsUpdatingMatrices)
    {
    return;
    }
  else
    {
    this->IsUpdatingMatrices = 1;
    }
  double spacing[3];
  unsigned int i;
  vtkNew<vtkMatrix4x4> xyToSlice;
  vtkNew<vtkMatrix4x4> xyToRAS;

  int disabledModify = this->StartModify();

  // the mapping from XY output slice pixels to Slice Plane coordinate
  xyToSlice->Identity();
  if (this->Dimensions[0] > 0 &&
      this->Dimensions[1] > 0 &&
      this->Dimensions[2] > 0)
    {
    for (i = 0; i < 3; i++)
      {
      spacing[i] = this->FieldOfView[i] / this->Dimensions[i];
      xyToSlice->SetElement(i, i, spacing[i]);
      xyToSlice->SetElement(i, 3, -this->FieldOfView[i] / 2. + this->XYZOrigin[i]);
      }
    //vtkWarningMacro( << "FieldOfView[2] = " << this->FieldOfView[2] << ", Dimensions[2] = " << this->Dimensions[2] );
    //xyToSlice->SetElement(2, 2, 1.);

    xyToSlice->SetElement(2, 3, 0.);
    }

    // the mapping from slice plane coordinates to RAS
    // (the Orienation as in Axial, Sagittal, Coronal)
    //
    // The combined transform:
    //
    // | R | = [Slice to RAS ] [ XY to Slice ]  | X |
    // | A |                                    | Y |
    // | S |                                    | Z |
    // | 1 |                                    | 1 |
    //
    // or
    //
    // RAS = XYToRAS * XY
    //
    vtkMatrix4x4::Multiply4x4(this->SliceToRAS, xyToSlice.GetPointer(), xyToRAS.GetPointer());

    bool modified = false;

    // check to see if the matrix actually changed
    if ( !Matrix4x4AreEqual (xyToRAS.GetPointer(), this->XYToRAS) )
      {
      this->XYToSlice->DeepCopy(xyToSlice.GetPointer());
      this->XYToRAS->DeepCopy(xyToRAS.GetPointer());
      modified = true;
      }


    // the mapping from XY output slice pixels to Slice Plane coordinate
    this->UVWToSlice->Identity();
    if (this->UVWDimensions[0] > 0 &&
        this->UVWDimensions[1] > 0 &&
        this->UVWDimensions[2] > 0)
      {
      for (i = 0; i < 2; i++)
        {
        spacing[i] = this->UVWExtents[i] / (this->UVWDimensions[i]);
        this->UVWToSlice->SetElement(i, i, spacing[i]);
        this->UVWToSlice->SetElement(i, 3, -this->UVWExtents[i] / 2. + this->UVWOrigin[i]);
        }
      this->UVWToSlice->SetElement(2, 2, 1.0);
      this->UVWToSlice->SetElement(2, 3, 0.);
      }

    vtkNew<vtkMatrix4x4> uvwToRAS;

    vtkMatrix4x4::Multiply4x4(this->SliceToRAS, this->UVWToSlice, uvwToRAS.GetPointer());

    if (!Matrix4x4AreEqual(uvwToRAS.GetPointer(), this->UVWToRAS))
      {
      this->UVWToRAS->DeepCopy(uvwToRAS.GetPointer());
      modified = true;
      }

    if (modified)
      {
      this->Modified();
      }

    const char *orientationString = "Reformat";
    if ( this->SliceToRAS->GetElement(0, 0) == -1.0 &&
         this->SliceToRAS->GetElement(1, 0) ==  0.0 &&
         this->SliceToRAS->GetElement(2, 0) ==  0.0 &&
         this->SliceToRAS->GetElement(0, 1) ==  0.0 &&
         this->SliceToRAS->GetElement(1, 1) ==  1.0 &&
         this->SliceToRAS->GetElement(2, 1) ==  0.0 &&
         this->SliceToRAS->GetElement(0, 2) ==  0.0 &&
         this->SliceToRAS->GetElement(1, 2) ==  0.0 &&
         this->SliceToRAS->GetElement(2, 2) ==  1.0 )
      {
        orientationString = "XZ";
      }

    if ( this->SliceToRAS->GetElement(0, 0) ==  0.0 &&
         this->SliceToRAS->GetElement(1, 0) == -1.0 &&
         this->SliceToRAS->GetElement(2, 0) ==  0.0 &&
         this->SliceToRAS->GetElement(0, 1) ==  0.0 &&
         this->SliceToRAS->GetElement(1, 1) ==  0.0 &&
         this->SliceToRAS->GetElement(2, 1) ==  1.0 &&
         this->SliceToRAS->GetElement(0, 2) ==  1.0 &&
         this->SliceToRAS->GetElement(1, 2) ==  0.0 &&
         this->SliceToRAS->GetElement(2, 2) ==  0.0 )
      {
        orientationString = "ZY";
      }

    if ( this->SliceToRAS->GetElement(0, 0) == -1.0 &&
         this->SliceToRAS->GetElement(1, 0) ==  0.0 &&
         this->SliceToRAS->GetElement(2, 0) ==  0.0 &&
         this->SliceToRAS->GetElement(0, 1) ==  0.0 &&
         this->SliceToRAS->GetElement(1, 1) ==  0.0 &&
         this->SliceToRAS->GetElement(2, 1) ==  1.0 &&
         this->SliceToRAS->GetElement(0, 2) ==  0.0 &&
         this->SliceToRAS->GetElement(1, 2) ==  1.0 &&
         this->SliceToRAS->GetElement(2, 2) ==  0.0 )
      {
        orientationString = "XY";
      }

    this->SetOrientationString( orientationString );

    // as UpdateMatrices can be called with DisableModifiedEvent
    // (typically when the scene is closed, slice nodes are reset but shouldn't
    // fire events. We should respect the modifiedWasDisabled flag.
    this->EndModify(disabledModify);

    this->IsUpdatingMatrices = 0;
}


//----------------------------------------------------------------------------
void vtkMRMLAstroSliceNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  Superclass::ReadXMLAttributes(atts);

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSliceNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, SliceID
void vtkMRMLAstroSliceNode::Copy(vtkMRMLNode *anode)
{
  if (!anode)
    {
    return;
    }
  int disabledModify = this->StartModify();

  Superclass::Copy(anode);

  this->EndModify(disabledModify);

}

//----------------------------------------------------------------------------
void vtkMRMLAstroSliceNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroSliceNode::RotateToVolumePlane(vtkMRMLVolumeNode *volumeNode)
{
  if ( volumeNode == NULL )
    {
    return;
    }

  vtkNew<vtkMatrix4x4> ijkToRAS;
  volumeNode->GetIJKToRASMatrix(ijkToRAS.GetPointer());

  // apply the transform
  vtkMRMLTransformNode *transformNode  = volumeNode->GetParentTransformNode();
  if ( transformNode != NULL )
    {
    if ( transformNode->IsTransformToWorldLinear() )
      {
      vtkNew<vtkMatrix4x4> rasToRAS;
      transformNode->GetMatrixTransformToWorld(rasToRAS.GetPointer());
      rasToRAS->Multiply4x4( rasToRAS.GetPointer(), ijkToRAS.GetPointer(), ijkToRAS.GetPointer());
      }
    else
      {
      vtkErrorMacro( "Cannot handle non-linear transforms" );
      }
    }

  // calculate vectors indicating transformed axis directions in RAS space (normalized)
  // e.g. toRAS[0] is the three-vector in RAS space that points along the row axis in ijk space
  // (toRAS[1] is the column, and toRAS[2] is slice)
  double toRAS[3][3];

  double len[3]; // length of each column vector
  double ele;
  int col, row;
  for (col = 0; col < 3; col++)
    {
    len[col] = 0;
    for (row = 0; row < 3; row++)
      {
      ele = ijkToRAS->GetElement(row, col);
      len[col] += ele*ele;
      }
    len[col] = sqrt(len[col]);
    for (row = 0; row < 3; row++)
      {
      toRAS[col][row] = ijkToRAS->GetElement( row, col ) / len[col];
      }
    }


  //
  // find the closest direction for each of the major axes
  //

  // define major directions
  double directions [6][3] = {
                   {  1,  0,  0 },   // right
                   { -1,  0,  0 },   // left
                   {  0,  1,  0 },   // anterior
                   {  0, -1,  0 },   // posterior
                   {  0,  0,  1 },   // superior
                   {  0,  0, -1 } }; // inferior

  int closestAxis[3] = {0, 0, 0};
  double closestDot[3] = {-1., -1., -1.};

  int direction;
  for (direction = 0; direction < 6; direction++)
    {
    double dot[3];
    for (col = 0; col < 3; col++)
      {
      dot[col] = 0;
      int i;
      for (i = 0; i < 3; i++)
        {
        dot[col] += toRAS[col][i] * directions[direction][i];
        }
      if (dot[col] > closestDot[col])
        {
        closestDot[col] = dot[col];
        closestAxis[col] = direction;
        }
      }
    }

  //
  // assign the vectors that correspond to each major direction
  //
  double alignedRAS[6][3] = {{0., 0., 0.},{0., 0., 0.},{0., 0., 0.},
                             {0., 0., 0.},{0., 0., 0.},{0., 0., 0.}};
  for (col = 0; col < 3; col++)
    {
    for (row = 0; row < 3; row++)
      {
      switch (closestAxis[col])
        {
        default:
        case 0:  // R
          alignedRAS[0][row] =  toRAS[col][row];
          alignedRAS[1][row] = -toRAS[col][row];
          break;
        case 1:  // L
          alignedRAS[0][row] = -toRAS[col][row];
          alignedRAS[1][row] =  toRAS[col][row];
          break;
        case 2:  // A
          alignedRAS[2][row] =  toRAS[col][row];
          alignedRAS[3][row] = -toRAS[col][row];
          break;
        case 3:  // P
          alignedRAS[2][row] = -toRAS[col][row];
          alignedRAS[3][row] =  toRAS[col][row];
          break;
        case 4:  // S
          alignedRAS[4][row] =  toRAS[col][row];
          alignedRAS[5][row] = -toRAS[col][row];
          break;
        case 5:  // I
          alignedRAS[4][row] = -toRAS[col][row];
          alignedRAS[5][row] =  toRAS[col][row];
          break;
        }
      }
    }


  //
  // plug vectors into slice matrix to best approximate requested orientation
  //

  for (row = 0; row < 3; row++)
    {
    if ( !strcmp(this->GetOrientationReference(), "YZ") )
      {
      this->SliceToRAS->SetElement(row, 0, alignedRAS[3][row]);
      this->SliceToRAS->SetElement(row, 1, alignedRAS[4][row]);
      this->SliceToRAS->SetElement(row, 2, alignedRAS[0][row]);
      }
    else if ( !strcmp(this->GetOrientationReference(), "XY") )
      {
      this->SliceToRAS->SetElement(row, 0, alignedRAS[1][row]);
      this->SliceToRAS->SetElement(row, 1, alignedRAS[4][row]);
      this->SliceToRAS->SetElement(row, 2, alignedRAS[2][row]);
      }
    else if ( !strcmp(this->GetOrientationReference(), "XZ") )
      {
      this->SliceToRAS->SetElement(row, 0, alignedRAS[1][row]);
      this->SliceToRAS->SetElement(row, 1, alignedRAS[2][row]);
      this->SliceToRAS->SetElement(row, 2, alignedRAS[4][row]);
      }
    else
      {
      this->SliceToRAS->SetElement(row, 0, alignedRAS[1][row]);
      this->SliceToRAS->SetElement(row, 1, alignedRAS[2][row]);
      this->SliceToRAS->SetElement(row, 2, alignedRAS[4][row]);
      }
    }

  //
  // If two colums project to the same axis, then there will be
  // a column of all zeros in the SliceToRAS matrix - if this happens replace this
  // with the cross product of the other columns
  //
  int nullColumn = -1;
  for (col = 0; col < 3; col++)
    {
    int row;
    bool isNull = true;
    for (row = 0; row < 3; row++)
      {
      if (this->SliceToRAS->GetElement(row, col) != 0.0)
        {
        isNull = false;
        }
      }
    if (isNull)
      {
      nullColumn = col;
      }
    }
  if (nullColumn != -1)
    {
    vtkVector3<double> A(
      this->SliceToRAS->GetElement(0, (nullColumn+1)%3),
      this->SliceToRAS->GetElement(1, (nullColumn+1)%3),
      this->SliceToRAS->GetElement(2, (nullColumn+1)%3));
    vtkVector3<double> B(
      this->SliceToRAS->GetElement(0, (nullColumn+2)%3),
      this->SliceToRAS->GetElement(1, (nullColumn+2)%3),
      this->SliceToRAS->GetElement(2, (nullColumn+2)%3));
    vtkVector3<double> C = A.Cross(B);
    this->SliceToRAS->SetElement(0, nullColumn, C.GetX());
    this->SliceToRAS->SetElement(1, nullColumn, C.GetY());
    this->SliceToRAS->SetElement(2, nullColumn, C.GetZ());
    }

  this->SetOrientationToReformat(); // just sets the string - indicates that this is not patient aligned

  this->UpdateMatrices();
}

*/
