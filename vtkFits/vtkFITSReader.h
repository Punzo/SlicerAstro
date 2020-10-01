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

#ifndef __vtkFITSReader_h
#define __vtkFITSReader_h

// std includes
#include <map>
#include <vector>

// VTK includes
#include "vtkMedicalImageReader2.h"

// VTK decleration
class vtkMatrix4x4;

// FITS includes
#include "fitsio.h"

// WCS includes
#include "wcslib.h"
#include "getwcstab.h"

#include "vtkFitsWin32Header.h"

/// \brief Reads FITS files.
///
/// Reads FITS using the CFITSIO library
//
/// \sa vtkImageReader2
class VTK_FITS_EXPORT vtkFITSReader : public vtkMedicalImageReader2
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

  ///
  /// Get WCSstruct
  ///
  struct wcsprm* GetWCSStruct();

  virtual void PrintSelf(ostream& os, vtkIndent indent) override;

  ///  is the given file name a FITS file?
  virtual int CanReadFile(const char* filename) override;

  ///
  /// Valid extentsions
  virtual const char* GetFileExtensions() override
    {
    return ".fits .fits.gz";
    }

  ///
  /// A descriptive name for this format
  virtual const char* GetDescriptiveName() override
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
  /// Number of components
  vtkSetMacro(NumberOfComponents,int);
  vtkGetMacro(NumberOfComponents,int);

  ///
  /// WCSStatus
  vtkSetMacro(WCSStatus,int);
  vtkGetMacro(WCSStatus,int);

  ///
  /// Compression
  vtkSetMacro(Compression,bool);
  vtkGetMacro(Compression,bool);

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

  virtual vtkImageData * AllocateOutputData(vtkDataObject *out, vtkInformation* outInfo) override;

  virtual void AllocateOutputData(vtkImageData *out, vtkInformation* outInfo, int *uExtent) override
    { Superclass::AllocateOutputData(out, outInfo, uExtent); }

  bool AllocatePointData(vtkImageData *out, vtkInformation* outInfo);

protected:
  vtkFITSReader();
  ~vtkFITSReader();

  vtkMatrix4x4* RasToIjkMatrix;

  char* HeaderKeys;
  char* CurrentFileName;

  int PointDataType;
  int DataType;
  int NumberOfComponents;
  bool Compression;
  bool UseNativeOrigin;

  fitsfile *fptr;
  int ReadStatus;

  struct wcsprm *WCS;
  struct wcsprm *ReadWCS;
  int WCSStatus;
  int NWCS;
  struct wcserr info[NWCSFIX];

  std::map <std::string, std::string> HeaderKeyValue;

  virtual void ExecuteInformation() override;
  virtual bool AstroExecuteInformation();
  virtual void ExecuteDataWithInformation(vtkDataObject *output, vtkInformation* outInfo) override;

  // SlicerAstro can read up to NAXIS = 3 and it assumes
  // the first 2 axes are the spatial (celestial) and
  // the third axis is the spectral one.
  bool AllocateHeader();
  // Fix the third spectral axis for datacube generated with Gipsy
  int FixGipsyHeader();
  bool AllocateWCS();

  bool FixGipsyHeaderOn;

  static bool decompress_one_file(const char *infilename, const char *outfilename);

private:
  vtkFITSReader(const vtkFITSReader&);  /// Not implemented.
  void operator=(const vtkFITSReader&);  /// Not implemented.

};

#endif
