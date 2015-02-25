/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkNRRDReader.h,v $
  Date:      $Date: 2007/06/12 19:13:59 $
  Version:   $Revision: 1.3.2.1 $

=========================================================================auto=*/
/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkFITSReader.h,v $

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef __vtkFITSReader_h
#define __vtkFITSReader_h

#include <string>
#include <map>
#include <iostream>

#include "vtkMedicalImageReader2.h"

//#include "vtkDataObject.h"
//#include "vtkImageData.h"

#include <vtkMatrix4x4.h>
#include <vtkPointData.h>
#include <vtkVersion.h>

#include "/media/Storage/PhD_Groningen/code/cfitsio/fitsio.h"

#define VTK_Fits_EXPORT

/// \brief Reads FITS files.
///
/// Reads FITS using the CFITSIO library
//
/// \sa vtkImageReader2
class VTK_Fits_EXPORT vtkFITSReader : public vtkMedicalImageReader2
{
public:
  static vtkFITSReader *New();

  vtkTypeMacro(vtkFITSReader,vtkMedicalImageReader2);

  ///
  /// Returns a IJK to RAS transformation matrix
  vtkMatrix4x4* GetRasToIjkMatrix();

  ///
  /// Get a space separated list of all keys in the header
  /// the string is allocated and deleted in this object.  This method
  /// does not support spaces in key names.
  char* GetHeaderKeys();

  ///
  /// Get a list of keys in the header. Preferred method to use as it
  /// supports spaces in key names.
  std::vector<std::string> GetHeaderKeysVector();

  ///
  /// Get a value given a key in the header
  const char* GetHeaderValue(const char *key);

  virtual void PrintSelf(ostream& os, vtkIndent indent);

  ///  is the given file name a NRRD file?
  virtual int CanReadFile(const char* filename);

  ///
  /// Valid extentsions
  virtual const char* GetFileExtensions()
    {
      return ".fits";
    }

  ///
  /// A descriptive name for this format
  virtual const char* GetDescriptiveName()
    {
      return "FITS - Flexible Image Transport System";
    }

  //Description:
  /// Report the status of the reading process.
  /// If this is different than zero, there have been some error
  /// parsing the complete header information.
  vtkGetMacro(ReadStatus,int);

  ///
  /// Point data field type
  vtkSetMacro(PointDataType,int);
  vtkGetMacro(PointDataType,int);

  ///
  /// Set the data type: int, float....
  vtkSetMacro(DataType,int);
  vtkGetMacro(DataType,int);

  ///
  //Number of components
  vtkSetMacro(NumberOfComponents,int);
  vtkGetMacro(NumberOfComponents,int);


  ///
  /// Use image origin from the file
  void SetUseNativeOriginOn()
  {
    UseNativeOrigin = true;
  }

  ///
  /// Use image center as origin
  void SetUseNativeOriginOff()
  {
    UseNativeOrigin = false;
  }

#if (VTK_MAJOR_VERSION <= 5)
virtual vtkImageData * AllocateOutputData(vtkDataObject *out);
virtual void AllocateOutputData(vtkImageData *out, int *uExtent)
    { Superclass::AllocateOutputData(out, uExtent); }
void AllocatePointData(vtkImageData *out);
#else
virtual vtkImageData * AllocateOutputData(vtkDataObject *out, vtkInformation* outInfo);
virtual void AllocateOutputData(vtkImageData *out, vtkInformation* outInfo, int *uExtent)
    { Superclass::AllocateOutputData(out, outInfo, uExtent); }
void AllocatePointData(vtkImageData *out, vtkInformation* outInfo);
#endif

protected:
  vtkFITSReader();
  ~vtkFITSReader();

  vtkMatrix4x4* RasToIjkMatrix;
  vtkMatrix4x4* FITSWorldToRasMatrix;

  char* HeaderKeys;
  char* CurrentFileName;

  int PointDataType;
  int DataType;
  int NumberOfComponents;
  bool UseNativeOrigin;

  float *cube_ptr;
  fitsfile *fptr;
  int ReadStatus;

  std::map <std::string, std::string> HeaderKeyValue;

  virtual void ExecuteInformation();
#if (VTK_MAJOR_VERSION <= 5)
  virtual void ExecuteData(vtkDataObject *out);
#else
  virtual void ExecuteDataWithInformation(vtkDataObject *output, vtkInformation* outInfo);
#endif

  void *AllocateHeader();

private:
  vtkFITSReader(const vtkFITSReader&);  /// Not implemented.
  void operator=(const vtkFITSReader&);  /// Not implemented.

};

#endif
