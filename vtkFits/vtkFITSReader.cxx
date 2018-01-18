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

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <string>
#include <zlib.h>

// vtkASTRO includes
#include <vtkFITSReader.h>

// Qt includes
#include <QFileInfo>
#include <QRegExp>

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkInformation.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkShortArray.h>
#include <vtksys/SystemTools.hxx>
#include <vtkStreamingDemandDrivenPipeline.h>

// Slicer includes
#include "vtkMRMLVolumeArchetypeStorageNode.h"

// STD includes
#include <sstream>

vtkStandardNewMacro(vtkFITSReader);

vtkFITSReader::vtkFITSReader()
{
  RasToIjkMatrix = NULL;
  HeaderKeys = NULL;
  CurrentFileName = NULL;
  UseNativeOrigin = true;
  Compression = false;
  fptr = NULL;
  ReadStatus = 0;
  WCS = NULL;
  NWCS = 0;
  WCSStatus = 0;
  wcserr_enable(1);
  FixGipsyHeaderOn = false;
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
      vtkErrorMacro("vtkFITSReader::~vtkFITSReader: wcsfree ERROR "<<WCSStatus<<": "<<wcshdr_errmsg[WCSStatus]<<"\n");
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

//----------------------------------------------------------------------------
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
}

std::string ZeroPadNumber(int num)
{
  std::stringstream ss;

  // the number is converted to string with the help of stringstream
  ss << num;
  std::string ret;
  ss >> ret;

  // Append zero chars
  int str_length = ret.length();
  for (int ii = 0; ii < 7 - str_length; ii++)
    ret = "0" + ret;
  return ret;
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

// Utility function to decompress files with zlib
//----------------------------------------------------------------------------
bool vtkFITSReader::decompress_one_file(const char *infilename, const char *outfilename)
{
  gzFile infile = gzopen(infilename, "rb");
  FILE *outfile = fopen(outfilename, "wb");

  if (!infile || !outfile)
    {
    return false;
    }

  char buffer[128];
  int num_read = 0;
  while ((num_read = gzread(infile, buffer, sizeof(buffer))) > 0)
    {
    fwrite(buffer, 1, num_read, outfile);
    }

  gzclose(infile);
  fclose(outfile);

  return true;
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
    vtkDebugMacro(<<"vtkFITSReader::CanReadFile: No filename specified.");
    return false;
    }

  std::string extension = vtksys::SystemTools::LowerCase( vtksys::SystemTools::GetFilenameLastExtension(fname) );
  if (extension != ".fits" && extension != ".gz")
    {
    vtkDebugMacro(<<"vtkFITSReader::CanReadFile: The filename extension is not recognized.");
    return false;
    }

  std::string FileName = filename;
  std::string gz = ".gz";
  std::size_t found =  FileName.find(gz);
  if (found != std::string::npos)
    {
    FileName.replace(FileName.find(gz),gz.length(),"");
    if (!vtkFITSReader::decompress_one_file(filename, FileName.c_str()))
      {
      vtkErrorMacro(<<"vtkFITSReader::CanReadFile: Decompression failed.");
      }
    this->SetFileName(FileName.c_str());
    this->SetCompression(true);
    }
  // We have the correct extension, so now check for the Fits magic.
  std::ifstream inputStream;

  inputStream.open(FileName.c_str(), std::ios::in | std::ios::binary );

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
    vtkErrorMacro("vtkFITSReader::ExecuteInformation: ERROR IN CFITSIO! Error reading"
                  " "<< this->GetFileName() << ": \n");
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
    vtkErrorMacro("vtkFITSReader::ExecuteInformation: Failed to allocateFitsHeader. \n")
    return;
    }

  // Push FITS header key/value pair data into std::map
  if(!this->FixGipsyHeader())
    {
    vtkErrorMacro("vtkFITSReader::ExecuteInformation: Failed to FixGipsyHeader. \n")
    return;
    }

  // Push FITS header into WCS struct
  if(!this->AllocateWCS())
    {
    vtkErrorMacro("vtkFITSReader::ExecuteInformation: Failed to allocateWCS. \n")
    }

  // Set type information
  std::string dataModel = this->GetHeaderValue("SlicerAstro.DATAMODEL");

  if (!dataModel.compare("MASK"))
    {
    this->SetDataType( VTK_SHORT );
    this->SetDataScalarType( VTK_SHORT );
    }
  else if (!dataModel.compare("DATA") ||
           !dataModel.compare("MODEL") ||
           !dataModel.compare("ZEROMOMENTMAP") ||
           !dataModel.compare("FIRSTMOMENTMAP") ||
           !dataModel.compare("SECONDMOMENTMAP") ||
           !dataModel.compare("PROFILE"))
    {
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
        vtkErrorMacro("vtkFITSReader::ExecuteInformation: Could not allocate data type. \n");
        return;
      }
    }
  else
    {
    vtkErrorMacro("vtkFITSReader::ExecuteInformation: Could not find the DATAMODEL keyword. \n");
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
   int histCont = 0, commCont = 0;
   for (ii = 1; ii <= nkeys; ii++)
     {
     if (fits_read_record(fptr, ii, card, &ReadStatus))break;
     if (fits_get_keyname(card, key, &keylen, &ReadStatus)) break;
     std::string strkey(key);
     if (fits_parse_value(card, val, com, &ReadStatus)) break;

     std::string str(val);
     std::string temp = "D+";
     size_t found = str.find(temp);
     while (found!=std::string::npos)
       {
       str.replace(found, temp.size(), "E+");
       found = str.find(temp);
       }
     temp = "D-";
     found = str.find(temp);
     while (found!=std::string::npos)
       {
       str.replace(found, temp.size(), "E-");
       found = str.find(temp);
       }

     if (std::string::npos != str.find_first_of("'"))
       {
       str.erase(0,1);
       str.erase(str.size()-1, str.size());
       }

     if (!strkey.compare("COMMENT"))
       {
       str = card;
       str = str.substr(8);

       if (str.find_first_of('@') != std::string::npos)
         {
         std::replace(str.begin(), str.end(), '@', 'a');
         }
       if (str.find_first_of('$') != std::string::npos)
         {
         std::replace(str.begin(), str.end(), '$', 's');
         }
       if (str.find_first_of('%') != std::string::npos)
         {
         std::replace(str.begin(), str.end(), '%', 't');
         }
       if (str.find_first_of('&') != std::string::npos)
         {
         std::replace(str.begin(), str.end(), '&', 'e');
         }
       if (str.find_first_of('"') != std::string::npos)
         {
         std::replace(str.begin(), str.end(), '"', '|');
         }

       commCont++;
       strkey = "SlicerAstro._" + strkey + ZeroPadNumber(commCont);
       HeaderKeyValue[strkey] = str;
       }
     else if (!strkey.compare("HISTORY"))
       {
       str = card;
       str = str.substr(8);

       if (str.find_first_of('@') != std::string::npos)
         {
         std::replace(str.begin(), str.end(), '@', 'a');
         }
       if (str.find_first_of('$') != std::string::npos)
         {
         std::replace(str.begin(), str.end(), '$', 's');
         }
       if (str.find_first_of('%') != std::string::npos)
         {
         std::replace(str.begin(), str.end(), '%', 't');
         }
       if (str.find_first_of('&') != std::string::npos)
         {
         std::replace(str.begin(), str.end(), '&', 'e');
         }
       if (str.find_first_of('"') != std::string::npos)
         {
         std::replace(str.begin(), str.end(), '"', '|');
         }

       histCont++;
       strkey = "SlicerAstro._" + strkey + ZeroPadNumber(histCont);
       HeaderKeyValue[strkey] = str;
       }
     else
       {
       str.erase(std::remove_if(str.begin(), str.end(), isspace), str.end());
       if (str.empty())
         {
         continue;
         }
       strkey = "SlicerAstro." + strkey;
       HeaderKeyValue[strkey] = str;
       }
     }

   if(HeaderKeyValue.count("SlicerAstro.NAXIS") == 0)
     {
     vtkErrorMacro("vtkFITSReader::ExecuteInformation:"
                   " The fits header is missing the NAXIS keyword. It is not possible to load the datacube. \n");
     return false;
     }

   int n = StringToInt((HeaderKeyValue.at("SlicerAstro.NAXIS")).c_str());

   if(n == 4 && !(HeaderKeyValue.count("SlicerAstro.NAXIS4")) == 0)
     {
     int n4 = StringToInt((HeaderKeyValue.at("SlicerAstro.NAXIS4")).c_str());
     if(n4 == 1)
       {
       HeaderKeyValue["SlicerAstro.NAXIS"] = "3";
       HeaderKeyValue.erase("SlicerAstro.NAXIS4");
       if (!(HeaderKeyValue.count("SlicerAstro.CDELT4")) == 0)
         {
         HeaderKeyValue.erase("SlicerAstro.CDELT4");
         }
       if (!(HeaderKeyValue.count("SlicerAstro.CRPIX4")) == 0)
         {
         HeaderKeyValue.erase("SlicerAstro.CRPIX4");
         }
       if (!(HeaderKeyValue.count("SlicerAstro.CRVAL4")) == 0)
         {
         HeaderKeyValue.erase("SlicerAstro.CRVAL4");
         }
       if (!(HeaderKeyValue.count("SlicerAstro.CTYPE4")) == 0)
         {
         HeaderKeyValue.erase("SlicerAstro.CTYPE4");
         }
       if (!(HeaderKeyValue.count("SlicerAstro.CUNIT4")) == 0)
         {
         HeaderKeyValue.erase("SlicerAstro.CUNIT4");
         }
       vtkWarningMacro("vtkFITSReader::ExecuteInformation:"
                       " the 4th dimension keywords have been removed. \n");

       n = 3;
       }
     else
       {
       vtkErrorMacro("vtkFITSReader::ExecuteInformation: \n"
                     "Datacube with polarization (NAXIS=4 and NAXIS4>1) at the moment are not supported. \n"<<
                     "If you want to visulize such kind of datacube: please, contact me. \n"<<
                     "Davide Punzo, punzodavide@hotmail.it \n");
       }
     }

   if(n == 3 && !(HeaderKeyValue.count("SlicerAstro.NAXIS3")) == 0)
     {
     int n3 = StringToInt((HeaderKeyValue.at("SlicerAstro.NAXIS3")).c_str());
     if(n3 == 1)
       {
       HeaderKeyValue["SlicerAstro.NAXIS"] = "2";
       n = 2;
       }
     }

   if(n == 2 && !(HeaderKeyValue.count("SlicerAstro.NAXIS2")) == 0)
     {
     int n2 = StringToInt((HeaderKeyValue.at("SlicerAstro.NAXIS2")).c_str());
     if(n2 == 1)
       {
       HeaderKeyValue["SlicerAstro.NAXIS"] = "1";
       n = 1;
       }
     }


   std::string temp = "SlicerAstro.NAXIS";

   if(n > 3)
     {
     vtkErrorMacro("vtkFITSReader::ExecuteInformation:"
                   " SlicerAstro, currently, can't load datacube with NAXIS > 3. \n");
     return false;
     }

   for(ii = 1; ii <= n; ii++)
     {
     temp += IntToString(ii);

     if(HeaderKeyValue.count(temp.c_str()) == 0)
       {
       vtkErrorMacro("vtkFITSReader::ExecuteInformation:"
                     " The fits header is missing the NAXIS" << ii <<
                     " keyword. It is not possible to load the datacube. \n");
       return false;
       }
       temp.erase(temp.size()-1);
     }

   if(HeaderKeyValue.count("SlicerAstro.CDELT1") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation:"
                     " The fits header is missing the CDELT1 keyword. \n");
     HeaderKeyValue["SlicerAstro.CDELT1"] = "UNDEFINED";
     }

   if(HeaderKeyValue.count("SlicerAstro.CDELT2") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation:"
                     " The fits header is missing the CDELT2 keyword. \n");
     HeaderKeyValue["SlicerAstro.CDELT2"] = "UNDEFINED";
     }

   if(HeaderKeyValue.count("SlicerAstro.CDELT3") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation:"
                     " The fits header is missing the CDELT3 keyword. \n");
     HeaderKeyValue["SlicerAstro.CDELT3"] = "UNDEFINED";
     }

   if(HeaderKeyValue.count("SlicerAstro.CRPIX1") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the CRPIX1 keyword. \n");
     HeaderKeyValue["SlicerAstro.CRPIX1"] = "UNDEFINED";
     }

   if(HeaderKeyValue.count("SlicerAstro.CRPIX2") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the CRPIX2 keyword. \n");
     HeaderKeyValue["SlicerAstro.CRPIX2"] = "UNDEFINED";
     }

   if(HeaderKeyValue.count("SlicerAstro.CRPIX3") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the CRPIX3 keyword. \n");
     HeaderKeyValue["SlicerAstro.CRPIX3"] = "UNDEFINED";
     }

   if(HeaderKeyValue.count("SlicerAstro.CRVAL1") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the CRVAL1 keyword. \n");
     HeaderKeyValue["SlicerAstro.CRVAL1"] = "UNDEFINED";
     }

   if(HeaderKeyValue.count("SlicerAstro.CRVAL2") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the CRVAL2 keyword. \n");
     HeaderKeyValue["SlicerAstro.CRVAL2"] = "UNDEFINED";
     }

   if(HeaderKeyValue.count("SlicerAstro.CRVAL3") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the CRVAL3 keyword. \n");
     HeaderKeyValue["SlicerAstro.CRVAL3"] = "UNDEFINED";
     }

   if(HeaderKeyValue.count("SlicerAstro.CTYPE1") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the CTYPE1 keyword. \n");
     HeaderKeyValue["SlicerAstro.CTYPE1"] = "NONE";
     }

   if(HeaderKeyValue.count("SlicerAstro.CTYPE2") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the CTYPE2 keyword. \n");
     HeaderKeyValue["SlicerAstro.CTYPE2"] = "NONE";
     }

   if(HeaderKeyValue.count("SlicerAstro.CTYPE3") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the CTYPE3 keyword. \n");
     HeaderKeyValue["SlicerAstro.CTYPE3"] = "NONE";
     }

   if(HeaderKeyValue.count("SlicerAstro.CUNIT1") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the CUNIT1 keyword. Assuming degree. \n");
     HeaderKeyValue["SlicerAstro.CUNIT1"] = "DEGREE";
     }

   if(HeaderKeyValue.count("SlicerAstro.CUNIT2") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the CUNIT2 keyword. Assuming degree. \n");
     HeaderKeyValue["SlicerAstro.CUNIT2"] = "DEGREE";
     }

   if(HeaderKeyValue.count("SlicerAstro.CUNIT3") == 0)
     {
     std::string ctype3 = HeaderKeyValue.at("SlicerAstro.CTYPE3");
     if(!(ctype3.compare(0,4,"FREQ")))
       {
       vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                       " The fits header is missing the CUNIT3 keyword. Assuming Hz. \n");
       HeaderKeyValue["SlicerAstro.CUNIT3"] = "Hz";
       }
     else
       {
       vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                       " The fits header is missing the CUNIT3 keyword. Assuming km/s. \n");
       HeaderKeyValue["SlicerAstro.CUNIT3"] = "km/s";
       }
     }

   if(HeaderKeyValue.count("SlicerAstro.CROTA") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the CROTA keyword. Assuming a value equal to zero. \n");
     HeaderKeyValue["SlicerAstro.CROTA"] = "0.";
     }

   if(HeaderKeyValue.count("SlicerAstro.BITPIX") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the BITPIX keyword. Using in default 32 (float). \n");
     HeaderKeyValue["SlicerAstro.BITPIX"] = "32";
     }

   if(HeaderKeyValue.count("SlicerAstro.BTYPE") == 0)
     {
     HeaderKeyValue["SlicerAstro.BTYPE"] = "NONE";
     }

   if(HeaderKeyValue.count("SlicerAstro.BUNIT") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the BUNIT keyword. Assuming JY/BEAM \n");
     HeaderKeyValue["SlicerAstro.BUNIT"] = "JY/BEAM";
     }

   if(HeaderKeyValue.count("SlicerAstro.BMAJ") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the BMAJ keyword. \n");
     HeaderKeyValue["SlicerAstro.BMAJ"] = "UNDEFINED";
     }

   if (!strcmp(HeaderKeyValue["SlicerAstro.BMAJ"].c_str(), "UNDEFINED"))
     {
     // 3DBAROLO
     if (HeaderKeyValue.count("SlicerAstro.BBMAJ") != 0)
       {
       HeaderKeyValue["SlicerAstro.BMAJ"] = HeaderKeyValue["SlicerAstro.BBMAJ"];
       vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                        " Beam information recovered from alternative keywords (3DBAROLO) \n"
                        << " BMAJ = " << HeaderKeyValue["SlicerAstro.BMAJ"] << " DEGREE. \n"
                        << " It is recommended to check the value.");
       }
     // GIPSY
     else if (HeaderKeyValue.count("SlicerAstro.BMMAJ") != 0)
       {
       double BMMAJ = StringToDouble(HeaderKeyValue["SlicerAstro.BMMAJ"].c_str());
       BMMAJ /= 3600.;
       HeaderKeyValue["SlicerAstro.BMAJ"] = DoubleToString(BMMAJ);
       vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                        " Beam information recovered from alternative keywords (GIPSY) \n"
                        << " BMAJ = " << HeaderKeyValue["SlicerAstro.BMAJ"] << " DEGREE. \n"
                        << " It is recommended to check the value.");
       }
     else
       {
       for (std::map<std::string,std::string>::iterator it = HeaderKeyValue.begin(); it != HeaderKeyValue.end(); ++it)
         {
         // CASA
         //> restoration: 37.5693 by 19.9686 (arcsec) at pa -54.7538 (deg)
         std::string HistoryKeyword = it->second;
         size_t found = HistoryKeyword.find("restoration:");
         if (found != std::string::npos)
           {
           std::string BMAJString = HistoryKeyword.substr(found + 12, HistoryKeyword.find("by") - found - 12);
           double BMAJ = StringToDouble(BMAJString.c_str());
           if (HistoryKeyword.find("arcsec") != std::string::npos)
             {
             BMAJ /= 3600.;
             }
           HeaderKeyValue["SlicerAstro.BMAJ"] = DoubleToString(BMAJ);
           vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                            " Beam information recovered from alternative keywords (CASA) \n"
                            << " BMAJ = " << HeaderKeyValue["SlicerAstro.BMAJ"] << " DEGREE. \n"
                            << " It is recommended to check the value.");
           break;
           }

         // MIRIAD
         // RESTOR: Beam =  5.303E+01 x  3.266E+01 arcsec, pa =  6.485E+00 degrees
         found = HistoryKeyword.find("RESTOR: Beam");
         if (found != std::string::npos)
           {
           std::string BMAJString = HistoryKeyword.substr(HistoryKeyword.find("Beam =") + 6,
                                                          HistoryKeyword.find("x") - HistoryKeyword.find("Beam =") - 6);
           double BMAJ = StringToDouble(BMAJString.c_str());
           if (HistoryKeyword.find("arcsec") != std::string::npos)
             {
             BMAJ /= 3600.;
             }
           HeaderKeyValue["SlicerAstro.BMAJ"] = DoubleToString(BMAJ);
           vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                            " Beam information recovered from alternative keywords (MIRIAD) \n"
                            << " BMAJ = " << HeaderKeyValue["SlicerAstro.BMAJ"] << " DEGREE. \n"
                            << " It is recommended to check the value.");
           break;
           }

         // AIPS
         // AIPS   CLEAN BMAJ=  8.3333E-03 BMIN=  8.3333E-03 BPA=   0.00
         found = HistoryKeyword.find("BMAJ=");
         if (found != std::string::npos)
           {
           std::string BMAJString = HistoryKeyword.substr(found + 5, HistoryKeyword.find("BMIN") - found - 5);
           double BMAJ = StringToDouble(BMAJString.c_str());
           if (HistoryKeyword.find("arcsec") != std::string::npos)
             {
             BMAJ /= 3600.;
             }
           HeaderKeyValue["SlicerAstro.BMAJ"] = DoubleToString(BMAJ);
           vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                            " Beam information recovered from alternative keywords (MIRIAD) \n"
                            << " BMAJ = " << HeaderKeyValue["SlicerAstro.BMAJ"] << " DEGREE. \n"
                            << " It is recommended to check the value.");
           break;
           }
         }
       }
     }

   if(HeaderKeyValue.count("SlicerAstro.BMIN") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the BMIN keyword. \n");
     HeaderKeyValue["SlicerAstro.BMIN"] = "UNDEFINED";
     }

   if (!strcmp(HeaderKeyValue["SlicerAstro.BMIN"].c_str(), "UNDEFINED"))
     {
     // 3DBAROLO
     if (HeaderKeyValue.count("SlicerAstro.BBMIN") != 0)
       {
       HeaderKeyValue["SlicerAstro.BMIN"] = HeaderKeyValue["SlicerAstro.BBMIN"];
       vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                        " Beam information recovered from alternative keywords (3DBAROLO) \n"
                        << " BMIN = " << HeaderKeyValue["SlicerAstro.BMIN"] << " DEGREE. \n"
                        << " It is recommended to check the value.");
       }
     // GIPSY
     else if (HeaderKeyValue.count("SlicerAstro.BMMIN") != 0)
       {
       double BMMIN = StringToDouble(HeaderKeyValue["SlicerAstro.BMMIN"].c_str());
       BMMIN /= 3600.;
       HeaderKeyValue["SlicerAstro.BMIN"] = DoubleToString(BMMIN);
       vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                        " Beam information recovered from alternative keywords (GIPSY) \n"
                        << " BMIN = " << HeaderKeyValue["SlicerAstro.BMIN"] << " DEGREE. \n"
                        << " It is recommended to check the value.");
       }
     else
       {
       for (std::map<std::string,std::string>::iterator it = HeaderKeyValue.begin(); it != HeaderKeyValue.end(); ++it)
         {
         // CASA
         //> restoration: 37.5693 by 19.9686 (arcsec) at pa -54.7538 (deg)
         std::string HistoryKeyword = it->second;
         size_t found = HistoryKeyword.find("restoration:");
         if (found != std::string::npos)
           {
           std::string BMINString = HistoryKeyword.substr(HistoryKeyword.find("by") + 2,
                                                          HistoryKeyword.find_first_of("(") - HistoryKeyword.find("by") - 2);
           double BMIN = StringToDouble(BMINString.c_str());
           if (HistoryKeyword.find("arcsec") != std::string::npos)
             {
             BMIN /= 3600.;
             }
           HeaderKeyValue["SlicerAstro.BMIN"] = DoubleToString(BMIN);
           vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                            " Beam information recovered from alternative keywords (CASA) \n"
                            << " BMIN = " << HeaderKeyValue["SlicerAstro.BMIN"] << " DEGREE. \n"
                            << " It is recommended to check the value.");
           break;
           }

         // MIRIAD
         // RESTOR: Beam =  5.303E+01 x  3.266E+01 arcsec, pa =  6.485E+00 degrees
         found = HistoryKeyword.find("RESTOR: Beam");
         if (found != std::string::npos)
           {
           std::string BMINString = HistoryKeyword.substr(HistoryKeyword.find("x") + 1,
                                                          HistoryKeyword.find(",") - 7 - HistoryKeyword.find("x") - 1);
           double BMIN = StringToDouble(BMINString.c_str());
           if (HistoryKeyword.find("arcsec") != std::string::npos)
             {
             BMIN /= 3600.;
             }
           HeaderKeyValue["SlicerAstro.BMIN"] = DoubleToString(BMIN);
           vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                            " Beam information recovered from alternative keywords (MIRIAD) \n"
                            << " BMIN = " << HeaderKeyValue["SlicerAstro.BMIN"] << " DEGREE. \n"
                            << " It is recommended to check the value.");
           break;
           }

         // AIPS
         // AIPS   CLEAN BMAJ=  8.3333E-03 BMIN=  8.3333E-03 BPA=   0.00
         found = HistoryKeyword.find("BMIN=");
         if (found != std::string::npos)
           {
           std::string BMINString = HistoryKeyword.substr(found + 5, HistoryKeyword.find("BPA=") - found - 5);
           double BMIN = StringToDouble(BMINString.c_str());
           if (HistoryKeyword.find("arcsec") != std::string::npos)
             {
             BMIN /= 3600.;
             }
           HeaderKeyValue["SlicerAstro.BMIN"] = DoubleToString(BMIN);
           vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                            " Beam information recovered from alternative keywords (MIRIAD) \n"
                            << " BMIN = " << HeaderKeyValue["SlicerAstro.BMIN"] << " DEGREE. \n"
                            << " It is recommended to check the value.");
           break;
           }
         }
       }
     }

   if(HeaderKeyValue.count("SlicerAstro.BPA") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the BPA keyword. \n");
     HeaderKeyValue["SlicerAstro.BPA"] = "UNDEFINED";
     }

   if (!strcmp(HeaderKeyValue["SlicerAstro.BPA"].c_str(), "UNDEFINED"))
     {
     double RadToDeg = 45. / atan(1.);
     // 3DBAROLO
     if (HeaderKeyValue.count("SlicerAstro.BBPA") != 0)
       {
       HeaderKeyValue["SlicerAstro.BPA"] = HeaderKeyValue["SlicerAstro.BBPA"];
       vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                        " Beam information recovered from alternative keywords (3DBAROLO) \n"
                        << " BPA = " << HeaderKeyValue["SlicerAstro.BPA"] << " DEGREE. \n"
                        << " It is recommended to check the value.");
       }
     // GIPSY
     else if (HeaderKeyValue.count("SlicerAstro.BMPA") != 0)
       {
       double BMPA = StringToDouble(HeaderKeyValue["SlicerAstro.BMPA"].c_str());
       BMPA /= 3600.;
       HeaderKeyValue["SlicerAstro.BPA"] = DoubleToString(BMPA);
       vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                        " Beam information recovered from alternative keywords (GIPSY) \n"
                        << " BPA = " << HeaderKeyValue["SlicerAstro.BPA"] << " DEGREE. \n"
                        << " It is recommended to check the value.");
       }
     else
       {
       for (std::map<std::string,std::string>::iterator it = HeaderKeyValue.begin(); it != HeaderKeyValue.end(); ++it)
         {
         // CASA
         //> restoration: 37.5693 by 19.9686 (arcsec) at pa -54.7538 (deg)
         std::string HistoryKeyword = it->second;
         size_t found = HistoryKeyword.find("restoration:");
         if (found != std::string::npos)
           {
           std::string BPAString = HistoryKeyword.substr(HistoryKeyword.find("pa") + 2,
                                                         HistoryKeyword.find_last_of("(") - HistoryKeyword.find("pa") - 2);
           double BPA = StringToDouble(BPAString.c_str());
           if (HistoryKeyword.find("rad") != std::string::npos)
             {
             BPA *= RadToDeg;
             }
           HeaderKeyValue["SlicerAstro.BPA"] = DoubleToString(BPA);
           vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                            " Beam information recovered from alternative keywords (CASA) \n"
                            << " BPA = " << HeaderKeyValue["SlicerAstro.BPA"] << " DEGREE. \n"
                            << " It is recommended to check the value.");
           break;
           }

         // MIRIAD
         // RESTOR: Beam =  5.303E+01 x  3.266E+01 arcsec, pa =  6.485E+00 degrees
         found = HistoryKeyword.find("RESTOR: Beam");
         if (found != std::string::npos)
           {
           std::string BPAString = HistoryKeyword.substr(HistoryKeyword.find("pa =") + 4);
           double BPA = StringToDouble(BPAString.c_str());
           if (HistoryKeyword.find("rad") != std::string::npos)
             {
             BPA *= RadToDeg;
             }
           HeaderKeyValue["SlicerAstro.BPA"] = DoubleToString(BPA);
           vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                            " Beam information recovered from alternative keywords (MIRIAD) \n"
                            << " BPA = " << HeaderKeyValue["SlicerAstro.BPA"] << " DEGREE. \n"
                            << " It is recommended to check the value.");
           break;
           }

         // AIPS
         // AIPS   CLEAN BMAJ=  8.3333E-03 BMIN=  8.3333E-03 BPA=   0.00
         found = HistoryKeyword.find("BPA=");
         if (found != std::string::npos)
           {
           std::string BPAString = HistoryKeyword.substr(found + 4);
           double BPA = StringToDouble(BPAString.c_str());
           if (HistoryKeyword.find("rad") != std::string::npos)
             {
             BPA *= RadToDeg;
             }
           HeaderKeyValue["SlicerAstro.BPA"] = DoubleToString(BPA);
           vtkWarningMacro( "vtkFITSReader::ExecuteInformation: "
                            " Beam information recovered from alternative keywords (MIRIAD) \n"
                            << " BPA = " << HeaderKeyValue["SlicerAstro.BPA"] << " DEGREE. \n"
                            << " It is recommended to check the value.");
           break;
           }
         }
       }
     }

   if(HeaderKeyValue.count("SlicerAstro.BZERO") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the BZERO keyword. Assuming a value equal to zero. \n");
     HeaderKeyValue["SlicerAstro.BZERO"] = "0.";
     }

   if(HeaderKeyValue.count("SlicerAstro.BSCALE") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the BSCALE keyword. Assuming a value equal to 1. \n");
     HeaderKeyValue["SlicerAstro.BSCALE"] = "1.";
     }

   if(HeaderKeyValue.count("SlicerAstro.BLANK") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the BLANK keyword. Assuming a value equal to zero. \n");
     HeaderKeyValue["SlicerAstro.BLANK"] = "0.";
     }

   if(HeaderKeyValue.count("SlicerAstro.DATAMODEL") == 0)
     {
     bool onlyNumberInExtension = false;
     vtkNew<vtkMRMLVolumeArchetypeStorageNode> snode;
     QFileInfo fileInfo(this->CurrentFileName);
     QString fileBaseName = fileInfo.baseName();
     if (fileInfo.isFile())
       {
       std::string fileNameStd = fileInfo.fileName().toStdString();
       std::string filenameWithoutExtension = snode->GetFileNameWithoutExtension(fileNameStd.c_str());
       fileBaseName = QString(filenameWithoutExtension.c_str());
       fileInfo.suffix().toInt(&onlyNumberInExtension);
       }

     QRegExp labelMapName("(\\b|_)([Ll]abel(s)?)(\\b|_)");
     QRegExp segName("(\\b|_)([Ss]eg)(\\b|_)");
     QRegExp maskName("(\\b|_)([Mm]ask)(\\b|_)");
     QRegExp modelName("(\\b|_)([Mm]odel)(\\b|_)");
     QRegExp modelNamesShort("(\\b|_)([Mm]od)(\\b|_)");
     QRegExp profileName("(\\b|_)([Pp]rofile)(\\b|_)");
     QRegExp zeroMomentMapName("(\\b|_)(0(th)?[Mm]omentMap)(\\b|_)");
     QRegExp zeroMomentMapNameShort("(\\b|_)([Mm]om0(th)?)(\\b|_)");
     QRegExp firstMomentMapName("(\\b|_)(1(st)?[Mm]omentMap)(\\b|_)");
     QRegExp firstMomentMapNameShort("(\\b|_)([Mm]om1(st)?)(\\b|_)");
     QRegExp secondMomentMapName("(\\b|_)(2(nd)?[Mm]omentMap)(\\b|_)");
     QRegExp secondMomentMapNameShort("(\\b|_)([Mm]om2(nd)?)(\\b|_)");

     if (fileInfo.baseName().contains(labelMapName) ||
         fileInfo.baseName().contains(segName) ||
         fileInfo.baseName().contains(maskName))
       {
       HeaderKeyValue["SlicerAstro.DATAMODEL"] = "MASK";
       }
     else if (fileInfo.baseName().contains(modelName) ||
              fileInfo.baseName().contains(modelNamesShort))
       {
       HeaderKeyValue["SlicerAstro.DATAMODEL"] = "MODEL";
       }
     else if (fileInfo.baseName().contains(profileName))
       {
       HeaderKeyValue["SlicerAstro.DATAMODEL"] = "PROFILE";
       }
     else if (fileInfo.baseName().contains(zeroMomentMapName) ||
              fileInfo.baseName().contains(zeroMomentMapNameShort))
       {
       HeaderKeyValue["SlicerAstro.DATAMODEL"] = "ZEROMOMENTMAP";
       }
     else if (fileInfo.baseName().contains(firstMomentMapName) ||
              fileInfo.baseName().contains(firstMomentMapNameShort))
       {
       HeaderKeyValue["SlicerAstro.DATAMODEL"] = "FIRSTMOMENTMAP";
       }
     else if (fileInfo.baseName().contains(secondMomentMapName) ||
              fileInfo.baseName().contains(secondMomentMapNameShort))
       {
       HeaderKeyValue["SlicerAstro.DATAMODEL"] = "SECONDMOMENTMAP";
       }
     else
       {
       HeaderKeyValue["SlicerAstro.DATAMODEL"] = "DATA";
       }
     }

   if(HeaderKeyValue.count("SlicerAstro.DATAMAX") == 0)
     {
     HeaderKeyValue["SlicerAstro.DATAMAX"] = "0.";
     }

   if(HeaderKeyValue.count("SlicerAstro.DATAMIN") == 0)
     {
     HeaderKeyValue["SlicerAstro.DATAMIN"] = "0.";
     }

   HeaderKeyValue["SlicerAstro.3DDisplayThreshold"] = "0.";
   HeaderKeyValue["SlicerAstro.3DDisplayThresholdMean"] = "0.";

   HeaderKeyValue["SlicerAstro.HistoMinSel"] = "0.";
   HeaderKeyValue["SlicerAstro.HistoMaxSel"] = "0.";

   if(HeaderKeyValue.count("SlicerAstro.DUNIT3") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the DUNIT3 keyword. \n");
     HeaderKeyValue["SlicerAstro.DUNIT3"] = "NONE";
     }

   if(HeaderKeyValue.count("SlicerAstro.DRVAL3") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the DRVAL3 keyword. \n");
     HeaderKeyValue["SlicerAstro.DRVAL3"] = "0.";
     }

   if(HeaderKeyValue.count("SlicerAstro.RESTFREQ") == 0)
     {
     if (!(HeaderKeyValue.count("SlicerAstro.FREQ0") == 0))
       {
       HeaderKeyValue["SlicerAstro.RESTFREQ"] = HeaderKeyValue.at("SlicerAstro.FREQ0");
       }
     else
       {
       std::string dunit3 = HeaderKeyValue.at("SlicerAstro.DUNIT3");
       std::string cunit3 = HeaderKeyValue.at("SlicerAstro.CUNIT3");
       double drval3 = StringToDouble(HeaderKeyValue.at("SlicerAstro.DRVAL3").c_str());
       double crval3 = StringToDouble(HeaderKeyValue.at("SlicerAstro.CRVAL3").c_str());
       if (!dunit3.compare("NONE") || fabs(drval3) < 1.E-06 ||
           !cunit3.compare("NONE") || fabs(crval3) < 1.E-06)
         {
         vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                         " The fits header is missing the RESTFREQ keyword. "
                         " Assuming HI data, i.e. RESTFREQ = 1.420405752E+09. \n");
         HeaderKeyValue["SlicerAstro.RESTFREQ"] = "1.420405752E+09";
         }
       else
         {
         double drval3ms = 0.;
         double crval3hz = 0.;
         if (!dunit3.compare("KM/S") || !dunit3.compare("km/s"))
           {
           drval3ms = drval3 * 1000;
           }
         else if (!dunit3.compare("M/S") || !dunit3.compare("m/s"))
           {
           drval3ms = drval3;
           }
         if (!cunit3.compare("HZ") || !cunit3.compare("hz") || !cunit3.compare("Hz"))
           {
           crval3hz = crval3;
           }
         else if (!cunit3.compare("MHZ") || !cunit3.compare("MHz") || !cunit3.compare("Mhz") || !cunit3.compare("mhz"))
           {
           crval3hz = crval3 * 1.E06;
           }
         else if (!cunit3.compare("GHZ") || !cunit3.compare("GHz") || !cunit3.compare("Ghz") || !cunit3.compare("ghz"))
           {
           crval3hz = crval3 * 1.E09;
           }
         double freq0 = crval3hz * sqrt((299792458. + drval3ms) / (299792458. - drval3ms));
         HeaderKeyValue["SlicerAstro.RESTFREQ"] = DoubleToString(freq0);
         }
       }
     }

   if (StringToDouble(HeaderKeyValue.at("SlicerAstro.RESTFREQ").c_str()) == 0.)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the RESTFREQ keyword. "
                     " Assuming HI data, i.e. RESTFREQ = 1.420405752E+09. \n");
     HeaderKeyValue["SlicerAstro.RESTFREQ"] = "1.420405752E+09";
     }

   if(HeaderKeyValue.count("SlicerAstro.DATE-OBS") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the DATE-OBS keyword. \n");
     HeaderKeyValue["SlicerAstro.DATE-OBS"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.EPOCH") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is also missing the EPOCH keyword. Assuming JD2000. \n");
     HeaderKeyValue["SlicerAstro.EPOCH"] = "2000.";
     }

   if(HeaderKeyValue.count("SlicerAstro.CELLSCAL") == 0)
     {
     HeaderKeyValue["SlicerAstro.CELLSCAL"] = "";
     }

   if(HeaderKeyValue.count("SlicerAstro.TELESCOP") == 0)
     {
     if (!(HeaderKeyValue.count("SlicerAstro.TELESC") == 0))
       {
       HeaderKeyValue["SlicerAstro.TELESCOP"] = HeaderKeyValue.at("SlicerAstro.TELESC");
       }
     else
       {
       vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                       " The fits header is missing the TELESCOP keyword. \n");
       HeaderKeyValue["SlicerAstro.TELESCOP"] = "";
       }
     }

   if(HeaderKeyValue.count("SlicerAstro.OBJECT") == 0)
     {
     vtkWarningMacro("vtkFITSReader::ExecuteInformation: "
                     " The fits header is missing the OBJECT keyword. \n");
     HeaderKeyValue["SlicerAstro.OBJECT"] = "";
     }

   if (ReadStatus) fits_report_error(stderr, ReadStatus); /* print any error message */

   return true;
}

//----------------------------------------------------------------------------
bool vtkFITSReader::FixGipsyHeader()
{
  std::vector<std::string> GipsyKeywords;
  GipsyKeywords.push_back("FREQ-OHEL");
  GipsyKeywords.push_back("FREQ-OLSR");
  GipsyKeywords.push_back("FREQ-RHEL");
  GipsyKeywords.push_back("FREQ-RLSR");

  if (!HeaderKeyValue.at("SlicerAstro.CTYPE3").compare(GipsyKeywords[0].c_str()) ||
      !HeaderKeyValue.at("SlicerAstro.CTYPE3").compare(GipsyKeywords[1].c_str()) ||
      !HeaderKeyValue.at("SlicerAstro.CTYPE3").compare(GipsyKeywords[2].c_str()) ||
      !HeaderKeyValue.at("SlicerAstro.CTYPE3").compare(GipsyKeywords[3].c_str()))
    {
    double c = 299792458.0;
    double vel = 0.;

    // Get Velocity reference info
    if(!HeaderKeyValue.count("SlicerAstro.VELR") == 0)
      {
      vel = StringToDouble(HeaderKeyValue.at("SlicerAstro.VELR").c_str());
      }
    else
      {
      if (!HeaderKeyValue.at("SlicerAstro.DRVAL3").compare("0.") ||
          !HeaderKeyValue.at("SlicerAstro.DUNIT3").compare("NONE"))
        {
        vtkErrorMacro("vtkFITSReader::FixGipsyHeader: could not find DRVAL3 or DUNIT3. \n");
        return false;
        }
      else
        {
        vel = StringToDouble(HeaderKeyValue.at("SlicerAstro.DRVAL3").c_str());
        if (!HeaderKeyValue.at("SlicerAstro.DUNIT3").compare("km/s") ||
            !HeaderKeyValue.at("SlicerAstro.DUNIT3").compare("KM/S"))
          {
          vel *= 1000.;
          }
        }
      }

    if (fabs(vel) < 1.E-06)
      {
      vtkErrorMacro("vtkFITSReader::FixGipsyHeader: could not find velocity information. \n");
      return false;
      }

    // Convert reference frequency to Hz
    double freq  = StringToDouble(HeaderKeyValue.at("SlicerAstro.CRVAL3").c_str());
    double dfreq = StringToDouble(HeaderKeyValue.at("SlicerAstro.CDELT3").c_str());
    std::string unit  = HeaderKeyValue.at("SlicerAstro.CUNIT3");
    std::vector<std::string> unitKeywords;
    unitKeywords.push_back("HZ");
    unitKeywords.push_back("Hz");
    unitKeywords.push_back("hz");
    unitKeywords.push_back("KHZ");
    unitKeywords.push_back("KHz");
    unitKeywords.push_back("Khz");
    unitKeywords.push_back("khz");
    unitKeywords.push_back("MHZ");
    unitKeywords.push_back("MHz");
    unitKeywords.push_back("Mhz");
    unitKeywords.push_back("mhz");
    unitKeywords.push_back("GHZ");
    unitKeywords.push_back("GHz");
    unitKeywords.push_back("Ghz");
    unitKeywords.push_back("ghz");

    bool found = false;
    for (unsigned int ii = 0; ii < unitKeywords.size(); ii++)
      {
      if (!unit.compare(unitKeywords[ii]))
        {
        double multi;
        if (ii > 10)
          {
          multi = 1.E+09;
          }
        else if (ii > 6)
          {
          multi = 1.E+06;
          }
        else if (ii > 2)
          {
          multi = 1.E+03;
          }
        else
          {
          multi = 1.;
          }
        freq  *= multi;
        dfreq *= multi;
        found = true;
        break;
        }
      }

    if (!found)
      {
      vtkErrorMacro("vtkFITSReader::FixGipsyHeader : Freq unit not found. \n");
      return false;
      }

    // Need rest frequency for conversion
    double freq0;
    if (!HeaderKeyValue.at("SlicerAstro.RESTFREQ").compare("0."))
      {
      vtkErrorMacro("vtkFITSReader::FixGipsyHeader: could not find RESTFREQ. \n");
      return false;
      }
    else
      {
      freq0 = StringToDouble(HeaderKeyValue.at("SlicerAstro.RESTFREQ").c_str());
      }

    // Calculate reference frequency in the barycentric system
    double freqB;
    if (!HeaderKeyValue.at("SlicerAstro.CTYPE3").compare(GipsyKeywords[0].c_str()) ||
        !HeaderKeyValue.at("SlicerAstro.CTYPE3").compare(GipsyKeywords[1].c_str()))
      {
      freqB = freq0 * (1. + (vel / c));
      }
    else
      {
      freqB = freq0 * (1. - (vel / c));
      }

    // Calculate topocentric velocity
    double velT = c * (((freqB * freqB) - (freq * freq)) / ((freqB * freqB) + (freq * freq)));
    double dfreqB = dfreq * (c - velT) / sqrt((c * c) - (velT * velT));

    HeaderKeyValue["SlicerAstro.CTYPE3"] = "FREQ";
    HeaderKeyValue["SlicerAstro.CUNIT3"] = "HZ";
    HeaderKeyValue["SlicerAstro.CRVAL3"] = DoubleToString(freqB);
    HeaderKeyValue["SlicerAstro.CDELT3"] = DoubleToString(dfreqB);

    vtkWarningMacro("vtkFITSReader::FixGipsyHeader: found Gipsy header keywords."
                    " CTYPE3, CUNIT3, CRVAL3 and CDELT3 has been automatically converted. \n");
    FixGipsyHeaderOn = true;
    }

  return true;
}

//----------------------------------------------------------------------------
bool vtkFITSReader::AllocateWCS(){
  char *header;
  int  i, nkeyrec, nreject, stat[NWCSFIX];

  if ((WCSStatus = fits_hdr2str(fptr, 1, NULL, 0, &header, &nkeyrec, &WCSStatus)))
    {
    fits_report_error(stderr, WCSStatus);
    }

  std::string stdHeader(header);

  size_t found;

  if (FixGipsyHeaderOn)
    {
    found = stdHeader.find("CTYPE3");
    if (found != std::string::npos)
      {
      size_t first = stdHeader.find("=", found);
      if (first != std::string::npos)
        {
        std::string blankString = "                                                                       ";
        std::string replaceString = "'" + HeaderKeyValue.at("SlicerAstro.CTYPE3") + "'";
        blankString.replace(1, replaceString.size(), replaceString);
        stdHeader.replace(first + 1, blankString.size(), blankString);
        }
      else
        {
        vtkErrorMacro("vtkFITSReader::AllocateWCS: Gipsy CTYPE3 keyword could not be replaced. \n");
        return false;
        }
      }

    found = stdHeader.find("CUNIT3");
    if (found != std::string::npos)
      {
      size_t first = stdHeader.find("=", found);
      if (first != std::string::npos)
        {
        std::string blankString = "                                                                       ";
        std::string replaceString = "'" + HeaderKeyValue.at("SlicerAstro.CUNIT3") + "'";
        blankString.replace(1, replaceString.size(), replaceString);
        stdHeader.replace(first + 1, blankString.size(), blankString);
        }
      else
        {
        vtkErrorMacro("vtkFITSReader::AllocateWCS: Gipsy CUNIT3 keyword could not be replaced. \n");
        return false;
        }
      }

    found = stdHeader.find("CRVAL3");
    if (found != std::string::npos)
      {
      size_t first = stdHeader.find("=", found);
      if (first != std::string::npos)
        {
        std::string blankString = "                                                                       ";
        std::string replaceString = HeaderKeyValue.at("SlicerAstro.CRVAL3");
        blankString.replace(1, replaceString.size(), replaceString);
        stdHeader.replace(first + 1, blankString.size(), blankString);
        }
      else
        {
        vtkErrorMacro("vtkFITSReader::AllocateWCS: Gipsy CRVAL3 keyword could not be replaced. \n");
        return false;
        }
      }

    found = stdHeader.find("CDELT3");
    if (found != std::string::npos)
      {
      size_t first = stdHeader.find("=", found);
      if (first != std::string::npos)
        {
        std::string blankString = "                                                                       ";
        std::string replaceString = HeaderKeyValue.at("SlicerAstro.CDELT3");
        blankString.replace(1, replaceString.size(), replaceString);
        stdHeader.replace(first + 1, blankString.size(), blankString);
        }
      else
        {
        vtkErrorMacro("vtkFITSReader::AllocateWCS: Gipsy CDELT3 keyword could not be replaced. \n");
        return false;
        }
      }
    }

  found = stdHeader.find("RESTFREQ");
  if (found != std::string::npos)
    {
    size_t first = stdHeader.find("=", found);
    if (first != std::string::npos)
      {
      std::string blankString = "                                                                       ";
      std::string replaceString = HeaderKeyValue.at("SlicerAstro.RESTFREQ");
      blankString.replace(1, replaceString.size(), replaceString);
      stdHeader.replace(first + 1, blankString.size(), blankString);
      }
    }
  else
    {
    std::string addString = "RESTFREQ=                                                                       ";
    stdHeader.insert(0, addString);
    found = stdHeader.find("RESTFREQ");
    if (found != std::string::npos)
      {
      size_t first = stdHeader.find("=", found);
      if (first != std::string::npos)
        {
        std::string blankString = "                                                                       ";
        std::string replaceString = HeaderKeyValue.at("SlicerAstro.RESTFREQ");
        blankString.replace(1, replaceString.size(), replaceString);
        stdHeader.replace(first + 1, blankString.size(), blankString);
        }
      }
    }

  header = (char *)malloc(((int)(stdHeader.size())+1)*sizeof(char));
  std::strcpy(header, stdHeader.c_str());

  if ((WCSStatus = wcspih(header, nkeyrec, WCSHDR_all, 2, &nreject, &NWCS, &WCS)))
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: wcspih ERROR "<<WCSStatus<<":\n"<<
                  "Message from "<<WCS->err->function<<
                  "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                  ": \n"<<WCS->err->msg<<"\n");
    return false;
    }

  if (NWCS > 1)
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: the volume has more than one WCS, "
                  "SlicerAstro assume only one WCS per volume.")
    }

  if ((WCSStatus = wcsfixi(7, 0, WCS, stat, info)))
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: wcsfix error: "<<WCSStatus<<"\n");
    }

  std::string print = "vtkFITSReader::AllocateWCS: wcsfix status returns: (";
  for (i = 0; i < NWCSFIX; i++)
    {
    print += IntToString(stat[i])+",";
    }
  print += ")";

  vtkDebugMacro(<<print);

  for (i = 0; i < NWCSFIX; i++)
    {
    if (info[i].status < -1 || 0 < info[i].status)
      {
      vtkWarningMacro("wcsfix INFORMATIVE message from "<<info[i].function<<
                    "at line "<<info[i].line_no<<" of file "<<info[i].file<<
                    ": \n"<< info[i].msg<<"\n");
      }
    }

  if ((WCSStatus = wcsset(WCS)))
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: wcsset ERROR "<<WCSStatus<<":\n"<<
                  "Message from "<<WCS->err->function<<
                  "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                  ": \n"<<WCS->err->msg<<"\n");
    }

  std::string ctype2 = WCS->ctype[2];
  if (strncmp(WCS->ctype[2], "VOPT", 4))
    {
    int index = 2;
    char ctypeS[9];
    strcpy(ctypeS, "VOPT-???");

    if ((WCSStatus = wcssptr(WCS, &index, ctypeS)))
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: wcssptr ERROR "<<WCSStatus<<":\n"<<
                    "Message from "<<WCS->err->function<<
                    "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                    ": \n"<<WCS->err->msg<<"\n");
      }
    else
      {
      vtkWarningMacro("vtkFITSReader::AllocateWCS: 3rd WCS axes has been converted "
                      "from "<<ctype2<< " to "<<WCS->ctype[2]<<". \n");
      }

    if ((WCSStatus = wcsset(WCS)))
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: wcsset ERROR "<<WCSStatus<<":\n"<<
                    "Message from "<<WCS->err->function<<
                    "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                    ": \n"<<WCS->err->msg<<"\n");
      }
    }

  if (WCSStatus != 0)
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: WCSlib failed to create WCSstruct."<< "\n"<<
                  "World coordinates will not be displayed. "<< "\n");
    return false;
    }
    free(header);
    return true;
}

//----------------------------------------------------------------------------
vtkImageData *vtkFITSReader::AllocateOutputData(vtkDataObject *out, vtkInformation* outInfo){
  vtkImageData *res = vtkImageData::SafeDownCast(out);
  if (!res)
    {
    vtkErrorMacro("vtkFITSReader::AllocateOutputData: Call to AllocateOutputData with"
                    " non vtkImageData output.");
    return NULL;
    }

  this->ExecuteInformation();

  res->SetExtent(this->GetUpdateExtent());

  if (!this->AllocatePointData(res, outInfo))
    {
    vtkErrorMacro("vtkFITSReader::AllocateOutputData: AllocatePointData failed.");
    return NULL;
    }

  return res;
}

//----------------------------------------------------------------------------
bool vtkFITSReader::AllocatePointData(vtkImageData *out, vtkInformation* outInfo) {

  vtkDataArray *pd = NULL;
  int Extent[6];
  out->GetExtent(Extent);

  // if the scalar type has not been set then we have a problem
  if (this->DataType == VTK_VOID)
    {
    vtkErrorMacro("vtkFITSReader::AllocatePointData:"
                  " attempt to allocate void scalars.");
    return false;
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
    return true;
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
    case VTK_SHORT:
      pd = vtkShortArray::New();
      break;
    default:
      vtkErrorMacro("vtkFITSReader::AllocatePointData: Could not allocate data type.");
      return false;
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
  return true;
}

//----------------------------------------------------------------------------
// This function reads a data from a file.  The datas extent/axes
// are assumed to be the same as the file extent/order.
void vtkFITSReader::ExecuteDataWithInformation(vtkDataObject *output, vtkInformation* outInfo)
{
  if (this->GetOutputInformation(0))
    {
    this->GetOutputInformation(0)->Set(
      vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(),
    this->GetOutputInformation(0)->Get(
      vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT()), 6);
    }
  vtkImageData *data = this->AllocateOutputData(output, outInfo);

  if (data == NULL)
    {
    vtkErrorMacro(<< "vtkFITSReader::ExecuteDataWithInformation: "
                     "data not allocated.");
    return;
    }

  if (this->GetFileName() == NULL)
    {
    vtkErrorMacro(<< "vtkFITSReader::ExecuteDataWithInformation: "
                     "Either a FileName or FilePrefix must be specified.");
    return;
    }

  // Open the fits.  Yes, this means that the file is being opened
  // twice: once by ExecuteInformation, and once here
  if(fits_open_data(&fptr, this->GetFileName(), READONLY, &ReadStatus))
    {
    vtkErrorMacro("vtkFITSReader::ExecuteDataWithInformation: "
                  "ERROR IN CFITSIO! Error reading "<< this->GetFileName() << ":\n");
    fits_report_error(stderr, ReadStatus);
    return;
    }

  data->GetPointData()->GetScalars()->SetName("FITSImage");
  // Get data pointer
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
        vtkErrorMacro(<< "vtkFITSReader::ExecuteDataWithInformation: data is null.");
        return;
        }
      break;
    case VTK_FLOAT:
      if(fits_read_img(fptr, TFLOAT, 1, dim, &nullval, ptr, &anynull, &ReadStatus))
        {
        fits_report_error(stderr, ReadStatus);
        vtkErrorMacro(<< "vtkFITSReader::ExecuteDataWithInformation: data is null.");
        return;
        }
      break;
  case VTK_SHORT:
    if(fits_read_img(fptr, TSHORT, 1, dim, &nullval, ptr, &anynull, &ReadStatus))
      {
      fits_report_error(stderr, ReadStatus);
      vtkErrorMacro(<< "vtkFITSReader::ExecuteDataWithInformation: data is null.");
      return;
      }
    break;
    default:
      vtkErrorMacro("vtkFITSReader::ExecuteDataWithInformation: Could not load data");
      return;
    }

  if (fits_close_file(fptr, &ReadStatus))
    {
    vtkErrorMacro("vtkFITSReader::ExecuteDataWithInformation: ERROR IN CFITSIO! Error closing "
                  << this->GetFileName() << ":\n");
    fits_report_error(stderr, ReadStatus);
    }

  if (this->GetCompression())
    {
    if (remove(this->GetFileName()) != 0)
      {
      vtkErrorMacro("vtkFITSReader::ExecuteDataWithInformation: "
                    "Error deleting the decompressed file: "<< this->GetFileName() << ":\n");
      }
    }
}


//----------------------------------------------------------------------------
void vtkFITSReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

