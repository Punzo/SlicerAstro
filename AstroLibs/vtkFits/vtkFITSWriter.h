#ifndef __vtkFITSWriter_h
#define __vtkFITSWriter_h

#include "vtkWriter.h"

#include "vtkMatrix4x4.h"
#include "vtkDoubleArray.h"
#include "fitsio.h"

class vtkImageData;
class AttributeMapType;

#define VTK_FITS_EXPORT

/// \brief Writes fits files.
///
/// vtkFITSWriter writes FITS files.
///
/// \sa vtkFITSReader
class VTK_FITS_EXPORT vtkFITSWriter : public vtkWriter
{
public:

  vtkTypeMacro(vtkFITSWriter,vtkWriter);
  void PrintSelf(ostream& os, vtkIndent indent);

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

  virtual int FillInputPortInformation(int port, vtkInformation *info);

  ///
  /// Write method. It is called by vtkWriter::Write();
  void WriteData();

  ///
  /// Flag to set to on when a write error occured
  int WriteError;

  char *FileName;

  //vtkMatrix4x4 *IJKToRASMatrix;

  int UseCompression;
  int FileType;

  AttributeMapType *Attributes;

  fitsfile *fptr;
  int WriteStatus;

private:
  vtkFITSWriter(const vtkFITSWriter&);  /// Not implemented.
  void operator=(const vtkFITSWriter&);  /// Not implemented.

};

#endif
