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
#include<cstdlib>

// vtkASTRO includes
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
  HeaderKeys = NULL;
  CurrentFileName = NULL;
  UseNativeOrigin = true;
  ptr = NULL;
  fptr = NULL;
  ReadStatus = 0;
}

vtkFITSReader::~vtkFITSReader()
{
  if (RasToIjkMatrix) {
    RasToIjkMatrix->Delete();
    RasToIjkMatrix = NULL;
  }

  if (HeaderKeys) {
    delete [] HeaderKeys;
    HeaderKeys = NULL;
  }

  if (CurrentFileName) {
    delete [] CurrentFileName;
    CurrentFileName = NULL;
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
   this->RasToIjkMatrix->Identity();

   this->SetPointDataType(vtkDataSetAttributes::SCALARS);
   this->SetNumberOfComponents(1);

   // Push FITS header key/value pair data into std::map
   this->AllocateHeader();

   // Set type information
   switch(std::stoi(this->GetHeaderValue("BITPIX")))
   {
     case 8:
       this->SetDataType( VTK_FLOAT );
#if (VTK_MAJOR_VERSION > 5)
       this->SetDataScalarType( VTK_FLOAT );
#endif
     case 16:
       this->SetDataType( VTK_FLOAT );
#if (VTK_MAJOR_VERSION > 5)
       this->SetDataScalarType( VTK_FLOAT );
#endif
       break;
     case 32:
       this->SetDataType( VTK_FLOAT );
#if (VTK_MAJOR_VERSION > 5)
       this->SetDataScalarType( VTK_FLOAT );
#endif
       break;
   case -32:
     this->SetDataType( VTK_FLOAT );
#if (VTK_MAJOR_VERSION > 5)
     this->SetDataScalarType( VTK_FLOAT );
#endif
     break;
     case 64:
          this->SetDataType( VTK_DOUBLE );
#if (VTK_MAJOR_VERSION > 5)
          this->SetDataScalarType( VTK_DOUBLE );
#endif
       break;
   case -64:
        this->SetDataType( VTK_DOUBLE );
#if (VTK_MAJOR_VERSION > 5)
        this->SetDataScalarType( VTK_DOUBLE );
#endif
     break;
     default:
       vtkErrorMacro("Could not allocate data type.");
       return;
   }


   // Set axis information
   int dataExtent[6]={0};
   double spacings[3]={0.};
   double origin[3]={0.};


   unsigned int naxes = std::stoi(this->GetHeaderValue("NAXIS"));

   for (unsigned int axii=0; axii < naxes; axii++){

   //calculate the dataExtent
     dataExtent[2*axii] = 0;
     dataExtent[2*axii+1] = static_cast<int>(std::stoi(this->GetHeaderValue(("NAXIS"+std::to_string(axii+1)).c_str())) - 1);

   //calculate the spacing
     //spacings[axii]= std::stod(this->GetHeaderValue(("CDELT"+std::to_string(axii+1)).c_str()));
     spacings[axii]=1.0;
   //calculate the origin
     //origin[axii] = std::stod(this->GetHeaderValue(("CRVAL"+std::to_string(axii+1)).c_str()));
     origin[axii]=0.0;
   //set RasToIjkMatrix
     this->RasToIjkMatrix->SetElement(axii, axii , spacings[axii]);
   }

   this->RasToIjkMatrix->Invert(this->RasToIjkMatrix,this->RasToIjkMatrix);

   if (this->UseNativeOrigin) {
        for (unsigned int i=0; i < 3; i++) {
            this->RasToIjkMatrix->SetElement(i, 3, origin[i]);
        }
    } else {
        for (unsigned int i=0; i < 3; i++) {
            this->RasToIjkMatrix->SetElement(i, 3, (dataExtent[2*i+1] - dataExtent[2*i])/2.0);
        }
    }

   this->SetDataExtent(dataExtent);
   this->SetDataSpacing(spacings);
   this->SetDataOrigin(origin);

   this->vtkImageReader2::ExecuteInformation();

   if (fits_close_file(fptr, &ReadStatus)) fits_report_error(stderr, ReadStatus);

}

void vtkFITSReader::AllocateHeader(){
   char card[FLEN_CARD];/* Standard string lengths defined in fitsio.h */
   char val[FLEN_VALUE];
   char com[FLEN_COMMENT];
   char key[FLEN_KEYWORD];
   int keylen = FLEN_KEYWORD;

   int nkeys, ii;

   fits_get_hdrspace(fptr, &nkeys, NULL, &ReadStatus); /* get # of keywords */

   for (ii = 1; ii <= nkeys; ii++) { /* Read and print each keywords */

     if (fits_read_record(fptr, ii, card, &ReadStatus))break;
     if (fits_get_keyname(card, key, &keylen, &ReadStatus)) break;
     if (std::string(key).compare(0,7,"HISTORY") == 0) continue;
     if (std::string(key).compare(0,7,"COMMENT") == 0) continue;
     if (fits_parse_value(card, val, com, &ReadStatus)) break;

     std::string str(val);
     if (std::string::npos != str.find_first_of("'")){
       str.erase(0,1);
       str.erase(str.size()-1, str.size());
     }
     HeaderKeyValue[std::string(key)] = str;
   }

   if (ReadStatus) fits_report_error(stderr, ReadStatus); /* print any error message */

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


  // Open the fits.  Yes, this means that the file is being opened
  // twice: once by ExecuteInformation, and once here

  if(fits_open_data(&fptr, this->GetFileName(), READONLY, &ReadStatus)){
      vtkErrorMacro("ERROR IN CFITSIO! Error reading "<< this->GetFileName() << ":\n");
      fits_report_error(stderr, ReadStatus);
      return;
  }

  data->GetPointData()->GetScalars()->SetName("FITSImage");
  //get pointer
  ptr = data->GetPointData()->GetScalars()->GetVoidPointer(0);

  this->ComputeDataIncrements();

  unsigned int naxes = data->GetDataDimension();

  int naxe[naxes];
  data->GetDimensions(naxe);

  int dim = 1;
  for (unsigned int axii=0; axii < naxes; axii++){
    dim *= naxe[axii];
  }

  float nullval = NAN;
  int anynull;

  // load the data
  switch (this->DataType)
    {
    case VTK_DOUBLE:
      if(fits_read_img(fptr, TDOUBLE, 1, dim, &nullval, ptr, &anynull, &ReadStatus))
      {
        fits_report_error(stderr, ReadStatus);
        vtkErrorMacro(<< "data is null.");
        return;
      }
      break;
    case VTK_FLOAT:
      if(fits_read_img(fptr, TFLOAT, 1, dim, &nullval, ptr, &anynull, &ReadStatus))
      {
        fits_report_error(stderr, ReadStatus);
        vtkErrorMacro(<< "data is null.");
        return;
      }
    break;
  default:
    vtkErrorMacro("Could not load data");
    return;
  }

  if (fits_close_file(fptr, &ReadStatus)) fits_report_error(stderr, ReadStatus);

}


//----------------------------------------------------------------------------
void vtkFITSReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

