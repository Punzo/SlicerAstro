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

#ifndef __vtkFITSWriter_h
#define __vtkFITSWriter_h

// FITS includes
#include "fitsio.h"

// VTK includes
#include "vtkWriter.h"

// VTK declaration
class vtkDoubleArray;
class vtkMatrix4x4;
class vtkImageData;
class AttributeMapType;

#include "vtkFitsWin32Header.h"

/// \brief Writes fits files.
///
/// vtkFITSWriter writes FITS files.
///
/// \sa vtkFITSReader
class VTK_FITS_EXPORT vtkFITSWriter : public vtkWriter
{
public:

  vtkTypeMacro(vtkFITSWriter,vtkWriter);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  static vtkFITSWriter *New();

  ///
  /// Get the input to this writer.
  vtkImageData* GetInput();
  vtkImageData* GetInput(int port);

  ///
  /// Specify file name of vtk polygon data file to write.
  vtkSetStringMacro(FileName);
  vtkGetStringMacro(FileName);

  vtkSetMacro(UseCompression,int);
  vtkGetMacro(UseCompression,int);
  vtkBooleanMacro(UseCompression,int);

  vtkSetClampMacro(FileType,int,VTK_ASCII,VTK_BINARY);
  vtkGetMacro(FileType,int);
  void SetFileTypeToASCII() {this->SetFileType(VTK_ASCII);};
  void SetFileTypeToBinary() {this->SetFileType(VTK_BINARY);};

  vtkBooleanMacro(WriteError, int);
  vtkSetMacro(WriteError, int);
  vtkGetMacro(WriteError, int);

  /// Method to set an attribute that will be passed into the FITS
  /// file on write
  void SetAttribute(const std::string& name, const std::string& value);
  const char* GetAttribute(const std::string& key);

protected:
  vtkFITSWriter();
  ~vtkFITSWriter();

  virtual int FillInputPortInformation(int port, vtkInformation *info) VTK_OVERRIDE;

  ///
  /// Write method. It is called by vtkWriter::Write();
  void WriteData() VTK_OVERRIDE;

  ///
  /// Flag to set to on when a write error occured
  int WriteError;

  char *FileName;

  int UseCompression;
  int FileType;

  AttributeMapType *Attributes;

  fitsfile *fptr;
  int WriteStatus;

  static bool compress_one_file(const char *infilename, const char *outfilename);

private:
  vtkFITSWriter(const vtkFITSWriter&);  /// Not implemented.
  void operator=(const vtkFITSWriter&);  /// Not implemented.

};

#endif
