#include <cstdlib>

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

// STD includes
#include <sstream>

vtkStandardNewMacro(vtkFITSReader);

vtkFITSReader::vtkFITSReader()
{
  RasToIjkMatrix = NULL;
  HeaderKeys = NULL;
  CurrentFileName = NULL;
  UseNativeOrigin = true;
  fptr = NULL;
  ReadStatus = 0;
  WCS = NULL;
  NWCS = 0;
  WCSStatus = 0;
}

vtkFITSReader::~vtkFITSReader()
{
  if (RasToIjkMatrix)
    {
    RasToIjkMatrix->Delete();
    RasToIjkMatrix = NULL;
    }

  if (HeaderKeys)
    {
    delete [] HeaderKeys;
    HeaderKeys = NULL;
    }

  if (CurrentFileName)
    {
    delete [] CurrentFileName;
    CurrentFileName = NULL;
    }

  if(WCS)
    {
    if((WCSStatus = wcsvfree(&NWCS, &WCS)))
      {
      vtkErrorMacro("wcsfree ERROR "<<WCSStatus<<": "<<wcshdr_errmsg[WCSStatus]<<"\n");
      }
    delete [] WCS;
    WCS = NULL;
    }
}

namespace
{
//----------------------------------------------------------------------------
template <typename T> T StringToNumber(const char* num)
{
  std::stringstream ss;
  ss << num;
  T result;
  return ss >> result ? result : 0;
}

//----------------------------------------------------------------------------
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
}

//----------------------------------------------------------------------------
float StringToFloat(const char* str)
{
  return StringToNumber<float>(str);
}

//----------------------------------------------------------------------------
template <typename T> std::string NumberToString(T V)
{
  std::string stringValue;
  std::stringstream strstream;
  strstream << V;
  strstream >> stringValue;
  return stringValue;
}

//----------------------------------------------------------------------------
std::string IntToString(int Value)
{
  return NumberToString<int>(Value);
}
}// end namespace

vtkMatrix4x4* vtkFITSReader::GetRasToIjkMatrix()
{
  this->ExecuteInformation();
  return this->RasToIjkMatrix;
}

//----------------------------------------------------------------------------
char* vtkFITSReader::GetHeaderKeys()
{
  std::string keys;
  for (std::map<std::string,std::string>::iterator i = HeaderKeyValue.begin();
       i != HeaderKeyValue.end(); i++)
    {
    std::string s = static_cast<std::string> (i->first);
    if (i != HeaderKeyValue.begin())
      {
      keys = keys + " ";
      }
    keys = keys + s;
    }
  if (HeaderKeys)
    {
    delete [] HeaderKeys;
    }
  HeaderKeys = NULL;

  if (keys.size() > 0)
    {
    HeaderKeys = new char[keys.size()+1];
    strcpy(HeaderKeys, keys.c_str());
    }
  return HeaderKeys;
}

//----------------------------------------------------------------------------
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

//----------------------------------------------------------------------------
const char* vtkFITSReader::GetHeaderValue(const char *key)
{
  std::map<std::string,std::string>::iterator i = HeaderKeyValue.find(key);
  if (i != HeaderKeyValue.end())
    {
    return (i->second.c_str());
    }
  else
    {
    return NULL;
    }
}

//----------------------------------------------------------------------------
struct wcsprm* vtkFITSReader::GetWCSStruct()
{
  this->ExecuteInformation();
  return this->WCS;
}

//----------------------------------------------------------------------------
int vtkFITSReader::CanReadFile(const char* filename)
{

  // Check the extension first to avoid opening files that do not
  // look like fits.  The file must have an appropriate extension to be
  // recognized.
  std::string fname = filename;
  if (fname == "")
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

  if (inputStream.fail())
    {
    return false;
    }

  char magic[5] = {'\0','\0','\0','\0','\0'};
  inputStream.read(magic,4*sizeof(char));

  if (inputStream.eof())
    {
    inputStream.close();
    return false;
    }

  if (strcmp(magic,"SIMP")==0)
    {
    inputStream.close();
    return true;
    }

  inputStream.close();
  return false;
}

//----------------------------------------------------------------------------
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
  if (this->CurrentFileName != NULL &&
       !strcmp (this->CurrentFileName, this->GetFileName()))
    {
    // filename hasn't changed, don't re-execute
    return;
    }

  if (this->CurrentFileName != NULL)
    {
    delete [] this->CurrentFileName;
    }

  this->CurrentFileName = new char[1 + strlen(this->GetFileName())];
  strcpy (this->CurrentFileName, this->GetFileName());

  if(fits_open_data(&fptr, this->GetFileName(), READONLY, &ReadStatus))
    {
    vtkErrorMacro("ERROR IN CFITSIO! Error reading "<< this->GetFileName() << ":\n");
    fits_report_error(stderr, ReadStatus);
    return;
    }

  HeaderKeyValue.clear();

  if (this->RasToIjkMatrix)
    {
    this->RasToIjkMatrix->Delete();
    }
  this->RasToIjkMatrix = vtkMatrix4x4::New();
  this->RasToIjkMatrix->Identity();

  this->SetPointDataType(vtkDataSetAttributes::SCALARS);
  this->SetNumberOfComponents(1);

  // Push FITS header key/value pair data into std::map
  this->AllocateHeader();

  // Push FITS header into WCS struct
  this->AllocateWCS();

  // Set type information
  switch(StringToInt(this->GetHeaderValue("SlicerAstro.BITPIX")))
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
  unsigned int naxes = StringToInt(this->GetHeaderValue("SlicerAstro.NAXIS"));

  if(naxes > 3)
    {
    vtkErrorMacro("SlicerAstro, currently, can't load datacube with NAXIS > 3");
    return;
    }

  //calculate the dataExtent and setting the Spacings and Origin
  for (unsigned int axii=0; axii < naxes; axii++)
    {
    dataExtent[2*axii] = 0;
    dataExtent[2*axii+1] = static_cast<int>(StringToInt(this->GetHeaderValue(("SlicerAstro.NAXIS"+IntToString(axii+1)).c_str())) - 1);
    origin[axii] = 0.0;
    spacings[axii] = 1.0;
    }


  float theta1 = (StringToFloat(this->GetHeaderValue("SlicerAstro.CDELT1")) > 0.) ? 0. : M_PI;
  float theta2 = (StringToFloat(this->GetHeaderValue("SlicerAstro.CDELT2")) > 0.) ? 0. : M_PI;
  float theta3 = (StringToFloat(this->GetHeaderValue("SlicerAstro.CDELT3")) > 0.) ? 0. : M_PI;
  theta1 += M_PI/2.;

  this->RasToIjkMatrix->SetElement(0, 0, cos(theta2) * cos(theta3));
  this->RasToIjkMatrix->SetElement(0, 1, cos(theta1) * sin(theta3) + sin(theta1) * sin(theta2) * cos(theta3));
  this->RasToIjkMatrix->SetElement(0, 2, sin(theta1) * sin(theta3) - cos(theta1) * sin(theta2) * cos(theta3));
  this->RasToIjkMatrix->SetElement(1, 0, -cos(theta2) * sin(theta3));
  this->RasToIjkMatrix->SetElement(1, 1, cos(theta1) * cos(theta3) - sin(theta1) * sin(theta2) * sin(theta3));
  this->RasToIjkMatrix->SetElement(1, 2, sin(theta1) * cos(theta3) + cos(theta1) * sin(theta2) * sin(theta3));
  this->RasToIjkMatrix->SetElement(2, 0, sin(theta2));
  this->RasToIjkMatrix->SetElement(2, 1, -sin(theta1) * cos(theta2));
  this->RasToIjkMatrix->SetElement(2, 2, cos(theta1) * cos(theta2));

  if (this->UseNativeOrigin)
    {
    for (unsigned int i=0; i < 3; i++)
      {
      this->RasToIjkMatrix->SetElement(i, 3, origin[i]);
      }
    }
  else
    {
    for (unsigned int i=0; i < 3; i++)
      {
      this->RasToIjkMatrix->SetElement(i, 3, (dataExtent[2*i+1] - dataExtent[2*i])/2.0);
      }
    }

  this->SetDataExtent(dataExtent);
  this->SetDataSpacing(spacings);
  this->SetDataOrigin(origin);

  this->vtkImageReader2::ExecuteInformation();

  if (fits_close_file(fptr, &ReadStatus))
    {
    fits_report_error(stderr, ReadStatus);
    }
}

void vtkFITSReader::AllocateHeader()
{
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
     std::string strkey(key);
     if (strkey.compare(0,7,"HISTORY") == 0) continue;
     if (strkey.compare(0,7,"COMMENT") == 0) continue;
     if (fits_parse_value(card, val, com, &ReadStatus)) break;

     std::string str(val);
     if (std::string::npos != str.find_first_of("'")){
       str.erase(0,1);
       str.erase(str.size()-1, str.size());
     }
     std::string strkey1 = "SlicerAstro." + strkey;
     HeaderKeyValue[strkey1] = str;

   }

   if(HeaderKeyValue.count("SlicerAstro.NAXIS") == 0){
       vtkErrorMacro("The fits header is missing the NAXIS keyword. It is not possible to load the datacube!");
       return;
   }

   int n = StringToInt((HeaderKeyValue.at("SlicerAstro.NAXIS")).c_str());
   std::string temp = "SlicerAstro.NAXIS";

   for(ii = 1; ii <= n; ii++){
       temp += IntToString(ii);

       if(HeaderKeyValue.count(temp.c_str()) == 0){
           vtkErrorMacro("The fits header is missing the NAXIS" << ii <<
                         " keyword. It is not possible to load the datacube!");
           return;
       }
       temp.erase(temp.size()-1);
   }

   if(HeaderKeyValue.count("SlicerAstro.BITPIX") == 0){
       vtkWarningMacro("The fits header is missing the BITPIX keyword. Using in default 64 (double). Odd behaviors may show up!");
       HeaderKeyValue["SlicerAstro.BITPIX"] = "64";
   }

   if(HeaderKeyValue.count("SlicerAstro.BUNIT") == 0){
       vtkWarningMacro("The fits header is missing the BUNIT keyword. Odd behaviors may show up!");
       HeaderKeyValue["SlicerAstro.BUNIT"] = "";
   }

   if(HeaderKeyValue.count("SlicerAstro.DATAMAX") == 0)
       HeaderKeyValue["SlicerAstro.DATAMAX"] = "0.";

   if(HeaderKeyValue.count("SlicerAstro.DATAMIN") == 0)
       HeaderKeyValue["SlicerAstro.DATAMIN"] = "0.";

   if (ReadStatus) fits_report_error(stderr, ReadStatus); /* print any error message */

}

//----------------------------------------------------------------------------
void vtkFITSReader::AllocateWCS(){
  char *header;
  int  i, nkeyrec, nreject, stat[NWCSFIX];

  if ((WCSStatus = fits_hdr2str(fptr, 1, NULL, 0, &header, &nkeyrec, &WCSStatus)))
    {
    fits_report_error(stderr, WCSStatus);
    }
  if ((WCSStatus = wcspih(header, nkeyrec, WCSHDR_all, 2, &nreject, &NWCS, &WCS)))
    {
    vtkErrorMacro("wcspih ERROR "<<WCSStatus<<": "<<wcshdr_errmsg[WCSStatus]<<"\n");
    }
  if (NWCS > 1)
    {
    vtkErrorMacro("The volume has more than one WCS, SlicerAstro assume only one WCS per volume!")
    }
  if ((WCSStatus = wcsfix(7, 0, WCS, stat)))
    {
    for (i = 0; i < NWCSFIX; i++)
      {
      if (stat[i] > 0)
        {
        vtkErrorMacro("wcsfix ERROR "<<WCSStatus<<": "<<wcshdr_errmsg[WCSStatus]<<"\n");
        }
      }
    }
  if ((WCSStatus = wcsset(WCS)))
    {
    vtkErrorMacro("wcsset ERROR "<<WCSStatus<<": "<<wcshdr_errmsg[WCSStatus]<<"\n");
    }
  if (WCSStatus!=0)
    {
    vtkWarningMacro("The fits header is missing essential keywords."<< "\n"<<
                    "World coordinates wil no be displayed. "<< "\n"<<
                    "In addition, odd behaviors may show up!"<< "\n");
    }
    free(header);
}

//----------------------------------------------------------------------------
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

//----------------------------------------------------------------------------
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
  if(fits_open_data(&fptr, this->GetFileName(), READONLY, &ReadStatus))
    {
    vtkErrorMacro("ERROR IN CFITSIO! Error reading "<< this->GetFileName() << ":\n");
    fits_report_error(stderr, ReadStatus);
    return;
    }

  data->GetPointData()->GetScalars()->SetName("FITSImage");
  //get pointer
  void *ptr = NULL;
  ptr = data->GetPointData()->GetScalars()->GetVoidPointer(0);
  this->ComputeDataIncrements();
  unsigned int naxes = data->GetDataDimension();
  int naxe[naxes];
  data->GetDimensions(naxe);
  int dim = 1;
  for (unsigned int axii=0; axii < naxes; axii++)
    {
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
  if (fits_close_file(fptr, &ReadStatus))
    {
    fits_report_error(stderr, ReadStatus);
    }
}


//----------------------------------------------------------------------------
void vtkFITSReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

