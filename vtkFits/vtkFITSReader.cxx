#include <cstdlib>
#include <algorithm>

// vtkASTRO includes
#include <vtkFITSReader.h>

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
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
  wcserr_enable(1);
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
float StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
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
  if(!this->AllocateHeader())
    {
    return;
    }

  // Push FITS header into WCS struct
  this->AllocateWCS();

  // Set type information
  switch(StringToInt(this->GetHeaderValue("SlicerAstro.BITPIX")))
    {
    case 8:
      this->SetDataType( VTK_FLOAT );
      this->SetDataScalarType( VTK_FLOAT );
    case 16:
      this->SetDataType( VTK_FLOAT );
      this->SetDataScalarType( VTK_FLOAT );
      break;
    case 32:
      this->SetDataType( VTK_FLOAT );
      this->SetDataScalarType( VTK_FLOAT );
      break;
    case -32:
      this->SetDataType( VTK_FLOAT );
      this->SetDataScalarType( VTK_FLOAT );
      break;
    case 64:
      this->SetDataType( VTK_DOUBLE );
      this->SetDataScalarType( VTK_DOUBLE );
      break;
    case -64:
      this->SetDataType( VTK_DOUBLE );
      this->SetDataScalarType( VTK_DOUBLE );
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

  //calculate the dataExtent and setting the Spacings and Origin
  for (unsigned int axii=0; axii < naxes; axii++)
    {
    dataExtent[2*axii] = 0;
    dataExtent[2*axii+1] = static_cast<int>(StringToInt(this->GetHeaderValue(("SlicerAstro.NAXIS"+IntToString(axii+1)).c_str())) - 1);
    origin[axii] = 0.0;
    spacings[axii] = 1.0;
    }


  double theta1 = (StringToDouble(this->GetHeaderValue("SlicerAstro.CDELT1")) > 0.) ? 0. : M_PI;
  double theta2 = (StringToDouble(this->GetHeaderValue("SlicerAstro.CDELT2")) > 0.) ? 0. : M_PI;
  double theta3 = (StringToDouble(this->GetHeaderValue("SlicerAstro.CDELT3")) > 0.) ? 0. : M_PI;
  theta1 += M_PI/2.;

  if (StringToDouble(this->GetHeaderValue("SlicerAstro.CRVAL2")) < 0. &&
      StringToDouble(this->GetHeaderValue("SlicerAstro.CDELT3")) > 0.)
    {
    theta3 += M_PI;
    }

  if (StringToDouble(this->GetHeaderValue("SlicerAstro.CRVAL2")) > 0. &&
      StringToDouble(this->GetHeaderValue("SlicerAstro.CDELT3")) > 0.)
    {
    theta3 += M_PI;
    }

  this->RasToIjkMatrix->SetElement(0, 0, cos(theta2) * cos(theta3));
  this->RasToIjkMatrix->SetElement(0, 1, 0.);
  this->RasToIjkMatrix->SetElement(0, 2, 0.);
  this->RasToIjkMatrix->SetElement(1, 0, 0.);
  this->RasToIjkMatrix->SetElement(1, 1, 0.);
  this->RasToIjkMatrix->SetElement(1, 2, sin(theta1) * cos(theta3));
  this->RasToIjkMatrix->SetElement(2, 0, 0.);
  this->RasToIjkMatrix->SetElement(2, 1, -sin(theta1) * cos(theta2));
  this->RasToIjkMatrix->SetElement(2, 2, 0.);

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

bool vtkFITSReader::AllocateHeader()
{
   char card[FLEN_CARD];/* Standard string lengths defined in fitsio.h */
   char val[FLEN_VALUE];
   char com[FLEN_COMMENT];
   char key[FLEN_KEYWORD];
   int keylen = FLEN_KEYWORD;

   int nkeys, ii;

   fits_get_hdrspace(fptr, &nkeys, NULL, &ReadStatus); /* get # of keywords */

   /* Read and print each keywords */
   for (ii = 1; ii <= nkeys; ii++)
     {

     if (fits_read_record(fptr, ii, card, &ReadStatus))break;
     if (fits_get_keyname(card, key, &keylen, &ReadStatus)) break;
     std::string strkey(key);
     if (strkey.compare(0,7,"HISTORY") == 0) continue;
     if (strkey.compare(0,7,"COMMENT") == 0) continue;
     if (fits_parse_value(card, val, com, &ReadStatus)) break;

     std::string str(val);

     std::replace(str.begin(), str.end(), 'D+', 'E+');

     if (std::string::npos != str.find_first_of("'"))
       {
       str.erase(0,1);
       str.erase(str.size()-1, str.size());
       }
     str.erase(std::remove_if(str.begin(), str.end(), isspace), str.end());
     std::string strkey1 = "SlicerAstro." + strkey;
     HeaderKeyValue[strkey1] = str;

     }

   if(HeaderKeyValue.count("SlicerAstro.NAXIS") == 0)
     {
     vtkErrorMacro("The fits header is missing the NAXIS keyword. It is not possible to load the datacube.");
     return false;
     }

   int n = StringToInt((HeaderKeyValue.at("SlicerAstro.NAXIS")).c_str());

   if(n == 4 && !(HeaderKeyValue.count("SlicerAstro.NAXIS4")) == 0)
     {
     int n4 = StringToInt((HeaderKeyValue.at("SlicerAstro.NAXIS4")).c_str());
     if(n4 == 1)
       {
       HeaderKeyValue["SlicerAstro.NAXIS"] = "3";
       n = 3;
       }
     else
       {
       vtkErrorMacro("Datacube with polarization (NAXIS=4 and NAXIS4>1) at the moment are not supported."<<
                     "If you want to visulize such kind of datacube: please, contact me."<<
                     "Davide Punzo, punzodavide@hotmail.it");
       }
     }

   std::string temp = "SlicerAstro.NAXIS";

   if(n > 3)
     {
     vtkErrorMacro("SlicerAstro, currently, can't load datacube with NAXIS > 3");
     return false;
     }

   for(ii = 1; ii <= n; ii++)
     {
     temp += IntToString(ii);

     if(HeaderKeyValue.count(temp.c_str()) == 0)
       {
       vtkErrorMacro("The fits header is missing the NAXIS" << ii <<
                       " keyword. It is not possible to load the datacube.");
       return false;
       }
       temp.erase(temp.size()-1);
     }

   if(HeaderKeyValue.count("SlicerAstro.CDELT1") == 0)
     {
     vtkWarningMacro("The fits header is missing the CDELT1 keyword. Odd behaviors may show up.");
     HeaderKeyValue["SlicerAstro.CDELT1"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.CDELT2") == 0)
     {
     vtkWarningMacro("The fits header is missing the CDELT2 keyword. Odd behaviors may show up.");
     HeaderKeyValue["SlicerAstro.CDELT2"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.CDELT3") == 0)
     {
     vtkWarningMacro("The fits header is missing the CDELT3 keyword. Odd behaviors may show up.");
     HeaderKeyValue["SlicerAstro.CDELT3"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.CRPIX1") == 0)
     {
     vtkWarningMacro("The fits header is missing the CRPIX1 keyword. Odd behaviors may show up.");
     HeaderKeyValue["SlicerAstro.CRPIX1"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.CRPIX2") == 0)
     {
     vtkWarningMacro("The fits header is missing the CRPIX2 keyword. Odd behaviors may show up.");
     HeaderKeyValue["SlicerAstro.CRPIX2"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.CRPIX3") == 0)
     {
     vtkWarningMacro("The fits header is missing the CRPIX3 keyword. Odd behaviors may show up.");
     HeaderKeyValue["SlicerAstro.CRPIX3"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.CRVAL1") == 0)
     {
     vtkWarningMacro("The fits header is missing the CRVAL1 keyword. Odd behaviors may show up.");
     HeaderKeyValue["SlicerAstro.CRVAL1"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.CRVAL2") == 0)
     {
     vtkWarningMacro("The fits header is missing the CRVAL2 keyword. Odd behaviors may show up.");
     HeaderKeyValue["SlicerAstro.CRVAL2"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.CRVAL3") == 0)
     {
     vtkWarningMacro("The fits header is missing the CRVAL3 keyword. Odd behaviors may show up.");
     HeaderKeyValue["SlicerAstro.CRVAL3"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.CTYPE1") == 0)
     {
     vtkWarningMacro("The fits header is missing the CTYPE1 keyword. Odd behaviors may show up.");
     HeaderKeyValue["SlicerAstro.CTYPE1"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.CTYPE2") == 0)
     {
     vtkWarningMacro("The fits header is missing the CTYPE2 keyword. Odd behaviors may show up.");
     HeaderKeyValue["SlicerAstro.CTYPE2"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.CTYPE3") == 0)
     {
     vtkWarningMacro("The fits header is missing the CTYPE3 keyword. Odd behaviors may show up.");
     HeaderKeyValue["SlicerAstro.CTYPE3"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.CUNIT1") == 0)
     {
     vtkWarningMacro("The fits header is missing the CUNIT1 keyword. Assuming degree.");
     HeaderKeyValue["SlicerAstro.CUNIT1"] = "DEGREE";
     }

   if(HeaderKeyValue.count("SlicerAstro.CUNIT2") == 0)
     {
     vtkWarningMacro("The fits header is missing the CUNIT2 keyword. Assuming degree.");
     HeaderKeyValue["SlicerAstro.CUNIT2"] = "DEGREE";
     }

   if(HeaderKeyValue.count("SlicerAstro.CUNIT3") == 0)
     {
     std::string ctype3 = HeaderKeyValue.at("SlicerAstro.CTYPE3");
     if(!(ctype3.compare(0,4,"FREQ")))
       {
       vtkWarningMacro("The fits header is missing the CUNIT3 keyword. Assuming Hz.");
       HeaderKeyValue["SlicerAstro.CUNIT3"] = "Hz";
       }
     else if (!(ctype3.compare(0,4,"VELO")))
       {
       vtkWarningMacro("The fits header is missing the CUNIT3 keyword. Assuming km/s.");
       HeaderKeyValue["SlicerAstro.CUNIT3"] = "km/s";
       }
     }


   if(HeaderKeyValue.count("SlicerAstro.BITPIX") == 0)
     {
     vtkWarningMacro("The fits header is missing the BITPIX keyword. Using in default 32 (float).");
     HeaderKeyValue["SlicerAstro.BITPIX"] = "32";
     }

   if(HeaderKeyValue.count("SlicerAstro.BTYPE") == 0)
     {
     HeaderKeyValue["SlicerAstro.BTYPE"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.BUNIT") == 0)
     {
     vtkWarningMacro("The fits header is missing the BUNIT keyword.");
     HeaderKeyValue["SlicerAstro.BUNIT"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.BMAJ") == 0)
     {
     vtkWarningMacro("The fits header is missing the BMAJ keyword.");
     HeaderKeyValue["SlicerAstro.BMAJ"] = "-1.";
     }

   if(HeaderKeyValue.count("SlicerAstro.BMIN") == 0)
     {
     vtkWarningMacro("The fits header is missing the BMIN keyword.");
     HeaderKeyValue["SlicerAstro.BMIN"] = "-1.";
     }

   if(HeaderKeyValue.count("SlicerAstro.BPA") == 0)
     {
     vtkWarningMacro("The fits header is missing the BPA keyword.");
     HeaderKeyValue["SlicerAstro.BPA"] = "0.";
     }

   if(HeaderKeyValue.count("SlicerAstro.DATAMAX") == 0)
     {
     HeaderKeyValue["SlicerAstro.DATAMAX"] = "0.";
     }

   if(HeaderKeyValue.count("SlicerAstro.DATAMIN") == 0)
     {
     HeaderKeyValue["SlicerAstro.DATAMIN"] = "0.";
     }

   if(!(HeaderKeyValue.count("SlicerAstro.NOISE") == 0))
     {
     HeaderKeyValue["SlicerAstro.RMS"] = HeaderKeyValue.at("SlicerAstro.NOISE");
     HeaderKeyValue.erase("SlicerAstro.NOISE");
     }

   if(HeaderKeyValue.count("SlicerAstro.RMS") == 0)
     {
     HeaderKeyValue["SlicerAstro.RMS"] = "0.";
     }

   if(HeaderKeyValue.count("SlicerAstro.NOISEMEAN") == 0)
     {
     HeaderKeyValue["SlicerAstro.NOISEMEAN"] = "0.";
     }

   if(HeaderKeyValue.count("SlicerAstro.RESTFREQ") == 0)
     {
     if (!(HeaderKeyValue.count("SlicerAstro.FREQ0") == 0))
       {
       HeaderKeyValue["SlicerAstro.RESTFREQ"] = HeaderKeyValue.at("SlicerAstro.FREQ0");
       }
     else
       {
       vtkWarningMacro("The fits header is missing the RESTFREQ keyword. Assuming HI data, i.e. RESTFREQ = 1.420405752E+09");
       HeaderKeyValue["SlicerAstro.RESTFREQ"] = "1.420405752E+09";
       }
     }

   if(HeaderKeyValue.count("SlicerAstro.DATE-OBS") == 0)
     {
     vtkWarningMacro("The fits header is missing the DATE-OBS keyword. Odd behaviors may show up.");
     HeaderKeyValue["SlicerAstro.DATE-OBS"] = "";

     if(HeaderKeyValue.count("SlicerAstro.EPOCH") == 0)
       {
       vtkWarningMacro("The fits header is also missing the EPOCH keyword. Assuming JD2000.");
       HeaderKeyValue["SlicerAstro.EPOCH"] = "2000.";
       }
     }

   if(HeaderKeyValue.count("SlicerAstro.CELLSCAL") == 0)
     {
     HeaderKeyValue["SlicerAstro.CELLSCAL"] = "";
     }

   if (ReadStatus) fits_report_error(stderr, ReadStatus); /* print any error message */

   return true;
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
    vtkErrorMacro("wcspih ERROR "<<WCSStatus<<":\n"<<
                  "Message from "<<WCS->err->function<<
                  "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                  ": \n"<<WCS->err->msg<<"\n");
    }

  if (NWCS > 1)
    {
    vtkErrorMacro("The volume has more than one WCS, SlicerAstro assume only one WCS per volume.")
    }

  if ((WCSStatus = wcsfixi(7, 0, WCS, stat, info)))
    {
    vtkErrorMacro("wcsfix error: "<<WCSStatus<<":\n"<<
                  "Message from "<<WCS->err->function<<
                  "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                  ": \n"<<WCS->err->msg<<"\n");
    }

  std::string print = "wcsfix status returns: (";
  for (i = 0; i < NWCSFIX; i++)
    {
    print += IntToString(stat[i])+",";
    }
  print += ")";

  vtkDebugMacro(<<print);

  for (i = 0; i < NWCSFIX; i++) {
    if (info[i].status < -1 || 0 < info[i].status) {
      vtkErrorMacro("wcsfix INFORMATIVE message from "<<info[i].function<<
                    "at line "<<info[i].line_no<<" of file "<<info[i].file<<
                    ": \n"<< info[i].msg<<"\n");
    }
  }

  if ((WCSStatus = wcsset(WCS)))
    {
    vtkErrorMacro("wcsset ERROR "<<WCSStatus<<":\n"<<
                  "Message from "<<WCS->err->function<<
                  "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                  ": \n"<<WCS->err->msg<<"\n");
    }

  if (WCSStatus!=0)
    {
    vtkErrorMacro("WCSlib failed to create WCSstruct."<< "\n"<<
                  "World coordinates will not be displayed. "<< "\n"<<
                  "In addition, odd behaviors may show up."<< "\n");
    }
    free(header);
}

//----------------------------------------------------------------------------
vtkImageData *vtkFITSReader::AllocateOutputData(vtkDataObject *out, vtkInformation* outInfo){
  vtkImageData *res = vtkImageData::SafeDownCast(out);
  if (!res)
    {
    vtkWarningMacro("Call to AllocateOutputData with non vtkImageData output");
    return NULL;
    }

  this->ExecuteInformation();

  res->SetExtent(this->GetUpdateExtent());
  this->AllocatePointData(res, outInfo);

  return res;
}

//----------------------------------------------------------------------------
void vtkFITSReader::AllocatePointData(vtkImageData *out, vtkInformation* outInfo) {

  vtkDataArray *pd = NULL;
  int Extent[6];
  out->GetExtent(Extent);

  // if the scalar type has not been set then we have a problem
  if (this->DataType == VTK_VOID)
    {
    vtkErrorMacro("Attempt to allocate scalars before scalar type was set.");
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
  vtkDataObject::SetPointDataActiveScalarInfo(outInfo,
    this->DataType, this->GetNumberOfComponents());
  pd->SetNumberOfComponents(this->GetNumberOfComponents());

  // allocate enough memors
  pd->SetNumberOfTuples((Extent[1] - Extent[0] + 1)*
                      (Extent[3] - Extent[2] + 1)*
                      (Extent[5] - Extent[4] + 1));


  out->GetPointData()->SetScalars(pd);
  vtkDataObject::SetPointDataActiveScalarInfo(outInfo,
         this->DataType, this->GetNumberOfComponents());

  pd->Delete();
}



//----------------------------------------------------------------------------
// This function reads a data from a file.  The datas extent/axes
// are assumed to be the same as the file extent/order.
void vtkFITSReader::ExecuteDataWithInformation(vtkDataObject *output, vtkInformation* outInfo)
{
  this->SetUpdateExtentToWholeExtent();
  vtkImageData *data = this->AllocateOutputData(output, outInfo);

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

