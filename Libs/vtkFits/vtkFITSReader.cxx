/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkNRRDReader.cxx,v $
  Date:      $Date: 2007/06/12 19:13:58 $
  Version:   $Revision: 1.7.2.1 $

=========================================================================auto=*/
/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkNRRDReader.cxx,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// vtkTeem includes
#include "vtkFITSReader.h"

// VTK includes
#include "vtkBitArray.h"
#include "vtkCharArray.h"
#include "vtkDoubleArray.h"
#include "vtkFloatArray.h"
#include "vtkImageData.h"
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include "vtkIntArray.h"
#include "vtkLongArray.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkShortArray.h"
#include <vtkStreamingDemandDrivenPipeline.h>
#include "vtkUnsignedCharArray.h"
#include "vtkUnsignedShortArray.h"
#include "vtkUnsignedIntArray.h"
#include "vtkUnsignedLongArray.h"
#include <vtksys/SystemTools.hxx>


vtkStandardNewMacro(vtkFITSReader);

vtkFITSReader::vtkFITSReader()
{
  RasToIjkMatrix = NULL;
  FITSWorldToRasMatrix = NULL;
  HeaderKeys = NULL;
  CurrentFileName = NULL;
  UseNativeOrigin = true;
  cube_ptr = NULL;
  fptr = NULL;
  ReadStatus = 0;
}

vtkFITSReader::~vtkFITSReader()
{
  if (RasToIjkMatrix) {
    RasToIjkMatrix->Delete();
    RasToIjkMatrix = NULL;
  }

  if (FITSWorldToRasMatrix) {
    FITSWorldToRasMatrix->Delete();
    FITSWorldToRasMatrix = NULL;
  }

  if (HeaderKeys) {
    delete [] HeaderKeys;
    HeaderKeys = NULL;
  }

  if (CurrentFileName) {
    delete [] CurrentFileName;
    CurrentFileName = NULL;
  }

  if (cube_ptr) {
    delete [] cube_ptr;
    cube_ptr = NULL;
  }

  if (fptr) {
    delete [] fptr;
    fptr = NULL;
  }

}

vtkMatrix4x4* vtkFITSReader::GetRasToIjkMatrix()
{
  this->ExecuteInformation();
  return this->RasToIjkMatrix;
}


char* vtkFITSReader::GetHeaderKeys()
{
  std::string keys;
  for (std::map<std::string,std::string>::iterator i = HeaderKeyValue.begin();
       i != HeaderKeyValue.end(); i++) {
    std::string s = static_cast<std::string> (i->first);
    if (i != HeaderKeyValue.begin()){
      keys = keys + " ";
    }
    keys = keys + s;
  }
  if (HeaderKeys) {
    delete [] HeaderKeys;
  }
  HeaderKeys = NULL;

  if (keys.size() > 0) {
    HeaderKeys = new char[keys.size()+1];
    strcpy(HeaderKeys, keys.c_str());
  }
  return HeaderKeys;
}

std::vector<std::string> vtkFITSReader::GetHeaderKeysVector()
{
  std::vector<std::string> keys;

  for (std::map<std::string,std::string>::iterator i = HeaderKeyValue.begin();
       i != HeaderKeyValue.end(); i++)
    {
    keys.push_back( i->first );
    }
  return keys;
}

const char* vtkFITSReader::GetHeaderValue(const char *key)
{
  std::map<std::string,std::string>::iterator i = HeaderKeyValue.find(key);
  if (i != HeaderKeyValue.end()) {
    return (i->second.c_str());
  }
  else {
    return NULL;
  }
}

int vtkFITSReader::CanReadFile(const char* filename)
{

  // Check the extension first to avoid opening files that do not
  // look like fits.  The file must have an appropriate extension to be
  // recognized.
  std::string fname = filename;
  if(  fname == "" )
    {
    vtkDebugMacro(<<"No filename specified.");
    return false;
    }

  // We'll assume we can read from stdin (don't try to read the header though)
  //if ( fname == "-" )
  //  {
  //  return true;
  //  }

  std::string extension = vtksys::SystemTools::LowerCase( vtksys::SystemTools::GetFilenameLastExtension(fname) );
  if (extension != ".fits")
    {
    vtkDebugMacro(<<"The filename extension is not recognized");
    return false;
    }

  // We have the correct extension, so now check for the Fits magic.
  std::ifstream inputStream;

  inputStream.open( filename, std::ios::in | std::ios::binary );

  if( inputStream.fail() )
    {
    return false;
    }


  char magic[5] = {'\0','\0','\0','\0','\0'};
  inputStream.read(magic,4*sizeof(char));

  if( inputStream.eof() )
    {
    inputStream.close();
    return false;
    }


  if( strcmp(magic,"SIMP")==0 )
    {
    inputStream.close();
    return true;
    }

  inputStream.close();
  return false;
}




void vtkFITSReader::ExecuteInformation()
{

    // This method determines the following and sets the appropriate value in
   // the parent IO class:
   //
   // binary/ascii file type
   // endianness
   // pixel type
   // pixel component type
   // number of pixel components
   // number of image dimensions
   // image spacing
   // image origin
   // meta data dictionary information

   int i;

   // save the Fits struct for the current file and
   // don't re-execute the read unless the filename changes
   if ( this->CurrentFileName != NULL &&
            !strcmp (this->CurrentFileName, this->GetFileName()) )
   {
       // filename hasn't changed, don't re-execute
       return;
   }

   if ( this->CurrentFileName != NULL )
   {
        delete [] this->CurrentFileName;
   }

   this->CurrentFileName = new char[1 + strlen(this->GetFileName())];
   strcpy (this->CurrentFileName, this->GetFileName());

   if(fits_open_data(&fptr, this->GetFileName(), READONLY, &ReadStatus)){
       vtkErrorMacro("ERROR IN CFITSIO! Error reading "<< this->GetFileName() << ":\n");
       fits_report_error(stderr, ReadStatus);
       return;
   }

   HeaderKeyValue.clear();

   if (this->RasToIjkMatrix) {
     this->RasToIjkMatrix->Delete();
   }
   this->RasToIjkMatrix = vtkMatrix4x4::New();
   vtkMatrix4x4* IjkToRasMatrix = vtkMatrix4x4::New();

   this->RasToIjkMatrix->Identity();
   IjkToRasMatrix->Identity();

   if (FITSWorldToRasMatrix) {
      FITSWorldToRasMatrix->Delete();
   }
   FITSWorldToRasMatrix = vtkMatrix4x4::New();
   FITSWorldToRasMatrix->Identity();


   // Set the number of image dimensions
   unsigned int naxes;
   if (fits_read_key(fptr, TINT,"NAXIS", &naxes, NULL, &ReadStatus)) fits_report_error(stderr, ReadStatus);

   this->SetPointDataType(vtkDataSetAttributes::SCALARS);
   this->SetNumberOfComponents(1);

   // Set type information
   this->SetDataType( VTK_FLOAT );
#if (VTK_MAJOR_VERSION > 5)
   this->SetDataScalarType( VTK_FLOAT );
#endif

   // Set axis information
   int dataExtent[6];
   double spacings[3];
   double spacing;
   double origin[3];


   if(naxes != 3){ vtkErrorMacro("fits file is not 3D");
   }else{
     long int naxe[3];
     int nfound;

     if(fits_read_keys_lng(fptr, "NAXIS",1,3,naxe, &nfound, &ReadStatus)) fits_report_error(stderr, ReadStatus);

     int spaceDir[3];

     spaceDir[0]= 1;
     spaceDir[1]= 1;
     spaceDir[2]= 1;

     for (unsigned int axii=0; axii < naxes; axii++){

   //calculate the dataExtent

       dataExtent[2*axii] = 0;
       dataExtent[2*axii+1] = static_cast<int>(naxe[axii]) - 1;

   //calculate the spacing
       double spacing = 1.;
       spacings[axii]=spacing;

   //calculate the origin

       origin[axii] = 0.0;

       /*for (int j=0; j < naxes; j++)
         {
          IjkToRasMatrix->SetElement(j, axii , spaceDir[j]*spacing);
         }*/

     }

   }
   //set properly RasToIjkMatrix for world coordinates!!!!
   /*
   if (this->UseNativeOrigin) {
        for (i=0; i < 3; i++) {
            IjkToRasMatrix->SetElement(i, 3, origin[i]);
        }
        vtkMatrix4x4::Invert(IjkToRasMatrix, this->RasToIjkMatrix);
    } else {
        vtkMatrix4x4::Invert(IjkToRasMatrix, this->RasToIjkMatrix);
        for (i=0; i < 3; i++) {
            this->RasToIjkMatrix->SetElement(i, 3, (dataExtent[2*i+1] - dataExtent[2*i])/2.0);
        }
    }

   this->RasToIjkMatrix->SetElement(1,1,-1.0);
    this->RasToIjkMatrix->SetElement(3,3,1.0);

   cout<<IjkToRasMatrix->GetElement(1,1)<<" "<<IjkToRasMatrix->GetElement(1,2)<<" "<<IjkToRasMatrix->GetElement(1,3)<<endl;
   cout<<IjkToRasMatrix->GetElement(2,1)<<" "<<IjkToRasMatrix->GetElement(2,2)<<" "<<IjkToRasMatrix->GetElement(2,3)<<endl;
   cout<<IjkToRasMatrix->GetElement(3,1)<<" "<<IjkToRasMatrix->GetElement(3,2)<<" "<<IjkToRasMatrix->GetElement(3,3)<<endl;

   cout<<this->RasToIjkMatrix->GetElement(1,1)<<" "<< this->RasToIjkMatrix->GetElement(1,2)<<" "<< this->RasToIjkMatrix->GetElement(1,3)<<endl;
   cout<<this->RasToIjkMatrix->GetElement(2,1)<<" "<< this->RasToIjkMatrix->GetElement(2,2)<<" "<< this->RasToIjkMatrix->GetElement(2,3)<<endl;
   cout<<this->RasToIjkMatrix->GetElement(3,1)<<" "<< this->RasToIjkMatrix->GetElement(3,2)<<" "<< this->RasToIjkMatrix->GetElement(3,3)<<endl;
*/

   //this->RasToIjkMatrix->SetElement(1,1,-1.0);
    IjkToRasMatrix->Delete();

   this->SetDataExtent(dataExtent);
   this->SetDataSpacing(spacings);
   this->SetDataOrigin(origin);


   // Push extra key/value pair data into std::map
   this->AllocateHeader();

   this->vtkImageReader2::ExecuteInformation();
   if (fits_close_file(fptr, &ReadStatus)) fits_report_error(stderr, ReadStatus);

}

void *vtkFITSReader::AllocateHeader(){

    //do it
   /*    char *key;
    char *val;
    //if (fits_read_key(fptr, TINT,"NAXIS", &naxes, NULL, &ReadStatus)) fits_report_error(stderr, ReadStatus);
    HeaderKeyValue[std::string(key)] = std::string(val);
    free(key);
    free(val);
    key = val = NULL;*/
}


#if (VTK_MAJOR_VERSION <= 5)
vtkImageData *vtkFITSReader::AllocateOutputData(vtkDataObject *out) {
#else
vtkImageData *vtkFITSReader::AllocateOutputData(vtkDataObject *out, vtkInformation* outInfo){
#endif
 vtkImageData *res = vtkImageData::SafeDownCast(out);
  if (!res)
    {
    vtkWarningMacro("Call to AllocateOutputData with non vtkImageData output");
    return NULL;
    }

  // I would like to eliminate this method which requires extra "information"
  // That is not computed in the graphics pipeline.
  // Until I can eliminate the method, I will reexecute the ExecuteInformation
  // before the execute.
  this->ExecuteInformation();

#if (VTK_MAJOR_VERSION <= 5)
  res->SetExtent(res->GetUpdateExtent());
  this->AllocatePointData(res);
#else
  res->SetExtent(this->GetUpdateExtent());
  this->AllocatePointData(res, outInfo);
#endif

  return res;

}

#if (VTK_MAJOR_VERSION <= 5)
void vtkFITSReader::AllocatePointData(vtkImageData *out) {
#else
void vtkFITSReader::AllocatePointData(vtkImageData *out, vtkInformation* outInfo) {
#endif

 vtkDataArray *pd = NULL;
 int Extent[6];
 out->GetExtent(Extent);

  // if the scalar type has not been set then we have a problem
  if (this->DataType == VTK_VOID)
    {
    vtkErrorMacro("Attempt to allocate scalars before scalar type was set!.");
    return;
    }

  // if we currently have scalars then just adjust the size
  pd = out->GetPointData()->GetScalars();

  if (pd && pd->GetDataType() == this->DataType
      && pd->GetReferenceCount() == 1)
    {
    pd->SetNumberOfComponents(this->GetNumberOfComponents());
    pd->SetNumberOfTuples((Extent[1] - Extent[0] + 1)*
                               (Extent[3] - Extent[2] + 1)*
                               (Extent[5] - Extent[4] + 1));
    // Since the execute method will be modifying the scalars
    // directly.
    pd->Modified();
    return;
    }

  // allocate the new scalars
  switch (this->DataType)
    {
    case VTK_BIT:
      pd = vtkBitArray::New();
      break;
    case VTK_UNSIGNED_CHAR:
      pd = vtkUnsignedCharArray::New();
      break;
    case VTK_CHAR:
      pd = vtkCharArray::New();
      break;
    case VTK_UNSIGNED_SHORT:
      pd = vtkUnsignedShortArray::New();
      break;
    case VTK_SHORT:
      pd = vtkShortArray::New();
      break;
    case VTK_UNSIGNED_INT:
      pd = vtkUnsignedIntArray::New();
      break;
    case VTK_INT:
      pd = vtkIntArray::New();
      break;
    case VTK_UNSIGNED_LONG:
      pd = vtkUnsignedLongArray::New();
      break;
    case VTK_LONG:
      pd = vtkLongArray::New();
      break;
    case VTK_DOUBLE:
      pd = vtkDoubleArray::New();
      break;
    case VTK_FLOAT:
      pd = vtkFloatArray::New();
      break;
    default:
      vtkErrorMacro("Could not allocate data type.");
      return;
    }
#if (VTK_MAJOR_VERSION <= 5)
  out->SetScalarType(this->DataType);
#else
  vtkDataObject::SetPointDataActiveScalarInfo(outInfo,
    this->DataType, this->GetNumberOfComponents());
#endif
  pd->SetNumberOfComponents(this->GetNumberOfComponents());

  // allocate enough memors
  pd->SetNumberOfTuples((Extent[1] - Extent[0] + 1)*
                      (Extent[3] - Extent[2] + 1)*
                      (Extent[5] - Extent[4] + 1));


  out->GetPointData()->SetScalars(pd);
#if (VTK_MAJOR_VERSION <= 5)
  out->SetNumberOfScalarComponents(this->GetNumberOfComponents());
#else
  vtkDataObject::SetPointDataActiveScalarInfo(outInfo,
         this->DataType, this->GetNumberOfComponents());
#endif

  pd->Delete();
}



//----------------------------------------------------------------------------
// This function reads a data from a file.  The datas extent/axes
// are assumed to be the same as the file extent/order.
#if (VTK_MAJOR_VERSION <= 5)
void vtkFITSReader::ExecuteData(vtkDataObject *output)
{
  output->SetUpdateExtentToWholeExtent();
  vtkImageData *data = this->(output);
#else
void vtkFITSReader::ExecuteDataWithInformation(vtkDataObject *output, vtkInformation* outInfo)
{
  this->SetUpdateExtentToWholeExtent();
  vtkImageData *data = this->AllocateOutputData(output, outInfo);
#endif

  if (this->GetFileName() == NULL)
    {
    vtkErrorMacro(<< "Either a FileName or FilePrefix must be specified.");
    return;
    }


  // Read in the fits.  Yes, this means that the header is being read
  // twice: once by ExecuteInformation, and once here

  if(fits_open_data(&fptr, this->GetFileName(), READONLY, &ReadStatus)){
      vtkErrorMacro("ERROR IN CFITSIO! Error reading "<< this->GetFileName() << ":\n");
      fits_report_error(stderr, ReadStatus);
      return;
  }

  void *ptr = NULL;

  data->GetPointData()->GetScalars()->SetName("FITSImage");
  //get pointer
  ptr = data->GetPointData()->GetScalars()->GetVoidPointer(0);

  this->ComputeDataIncrements();

  int naxe[3];
  int nfound;

  if(fits_read_keys_log(fptr, "NAXIS",1,3,naxe, &nfound, &ReadStatus)) fits_report_error(stderr, ReadStatus);

  data->GetDimensions(naxe);

  int dim = naxe[0]*naxe[1]*naxe[2];
  float nullval = NAN;
  int anynull;

  if(fits_read_img(fptr, TFLOAT, 1, dim, &nullval, ptr, &anynull, &ReadStatus))
  {
      fits_report_error(stderr, ReadStatus);
      vtkErrorMacro(<< "data is null.");
      return;
  }

  if (fits_close_file(fptr, &ReadStatus)) fits_report_error(stderr, ReadStatus);

}


//----------------------------------------------------------------------------
void vtkFITSReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

