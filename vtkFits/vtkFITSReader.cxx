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

//----------------------------------------------------------------------------
vtkFITSReader::vtkFITSReader()
{
  this->RasToIjkMatrix = nullptr;
  this->HeaderKeys = nullptr;
  this->CurrentFileName = nullptr;
  this->UseNativeOrigin = true;
  this->Compression = false;
  this->fptr = nullptr;
  this->ReadStatus = 0;
  this->WCS = new struct wcsprm;
  this->WCS->flag = -1;
  wcserr_enable(1);
  if((this->WCSStatus = wcsini(1, 0, this->WCS)))
    {
    vtkErrorMacro("wcsini ERROR "<<this->WCSStatus<<":\n"<<
                  "Message from "<<this->WCS->err->function<<
                  "at line "<<this->WCS->err->line_no<<" of file "<<this->WCS->err->file<<
                  ": \n"<<this->WCS->err->msg<<"\n");
    }
  this->ReadWCS = new struct wcsprm;
  this->ReadWCS->flag = -1;
  if((this->WCSStatus = wcsini(1, 0, this->ReadWCS)))
    {
    vtkErrorMacro("wcsini ERROR "<<this->WCSStatus<<":\n"<<
                  "Message from "<<this->ReadWCS->err->function<<
                  "at line "<<this->ReadWCS->err->line_no<<" of file "<<this->ReadWCS->err->file<<
                  ": \n"<<this->ReadWCS->err->msg<<"\n");
    }
  this->NWCS = 0;
  this->WCSStatus = 0;
  this->FixGipsyHeaderOn = false;
}

//----------------------------------------------------------------------------
vtkFITSReader::~vtkFITSReader()
{
  if (this->RasToIjkMatrix)
    {
    this->RasToIjkMatrix->Delete();
    this->RasToIjkMatrix = nullptr;
    }

  if (this->HeaderKeys)
    {
    delete [] this->HeaderKeys;
    this->HeaderKeys = nullptr;
    }

  if (this->CurrentFileName)
    {
    delete [] this->CurrentFileName;
    this->CurrentFileName = nullptr;
    }

  if(this->WCS)
    {
    if((this->WCSStatus = wcsvfree(&this->NWCS, &this->WCS)))
      {
      vtkErrorMacro("vtkFITSReader::~vtkFITSReader: wcsfree ERROR "<<this->WCSStatus<<": "
                    <<wcshdr_errmsg[this->WCSStatus]<<"\n");
      }
    delete [] this->WCS;
    this->WCS = nullptr;
    }

  if(this->ReadWCS)
    {
    if((this->WCSStatus = wcsvfree(&this->NWCS, &this->ReadWCS)))
      {
      vtkErrorMacro("vtkFITSReader::~vtkFITSReader: wcsfree ERROR "<<this->WCSStatus<<": "
                    <<wcshdr_errmsg[this->WCSStatus]<<"\n");
      }
    delete [] this->ReadWCS;
    this->ReadWCS = nullptr;
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
double StringToDouble(const char* str)
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

//----------------------------------------------------------------------------
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

//----------------------------------------------------------------------------
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
       i != this->HeaderKeyValue.end(); i++)
    {
    std::string s = static_cast<std::string> (i->first);
    if (i != this->HeaderKeyValue.begin())
      {
      keys = keys + " ";
      }
    keys = keys + s;
    }
  if (this->HeaderKeys)
    {
    delete [] this->HeaderKeys;
    }
  this->HeaderKeys = nullptr;

  if (keys.size() > 0)
    {
    this->HeaderKeys = new char[keys.size()+1];
    strcpy(this->HeaderKeys, keys.c_str());
    }
  return this->HeaderKeys;
}

//----------------------------------------------------------------------------
std::vector<std::string> vtkFITSReader::GetHeaderKeysVector()
{
  std::vector<std::string> keys;

  for (std::map<std::string,std::string>::iterator i = this->HeaderKeyValue.begin();
       i != this->HeaderKeyValue.end(); i++)
    {
    keys.push_back( i->first );
    }
  return keys;
}

//----------------------------------------------------------------------------
const char* vtkFITSReader::GetHeaderValue(const char *key)
{
  std::map<std::string,std::string>::iterator i = this->HeaderKeyValue.find(key);
  if (i != HeaderKeyValue.end())
    {
    return (i->second.c_str());
    }
  else
    {
    return nullptr;
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
      return false;
      }
    this->SetFileName(FileName.c_str());
    this->SetCompression(true);
    }

  if (this->AstroExecuteInformation())
    {
    return true;
    }

  return false;
}

//----------------------------------------------------------------------------
bool vtkFITSReader::AstroExecuteInformation()
{
  if(fits_open_data(&this->fptr, this->GetFileName(), READONLY, &ReadStatus))
    {
    vtkErrorMacro("vtkFITSReader::AstroExecuteInformation: ERROR IN CFITSIO! Error reading"
                  " "<< this->GetFileName() << ": \n");
    fits_report_error(stderr, this->ReadStatus);
    return false;
    }

  // Push FITS header key/value pair data into std::map
  if(!this->AllocateHeader())
    {
    vtkErrorMacro("vtkFITSReader::AstroExecuteInformation: "
                  "Failed to allocateFitsHeader. The data will not be loaded.");
    return false;
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
           !dataModel.compare("PROFILE") ||
           !dataModel.compare("PVDIAGRAM"))
    {
    switch(StringToInt(this->GetHeaderValue("SlicerAstro.BITPIX")))
      {
      case 8:
        this->SetDataType( VTK_FLOAT );
        this->SetDataScalarType( VTK_FLOAT );
        break;
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
        vtkErrorMacro("vtkFITSReader::AstroExecuteInformation: Could not allocate data type.");
        return false;
      }
    }
  else
    {
    vtkErrorMacro("vtkFITSReader::AstroExecuteInformation: Could not find the DATAMODEL keyword.");
    return false;
    }

  if (fits_close_file(this->fptr, &this->ReadStatus))
    {
    fits_report_error(stderr, this->ReadStatus);
    }

  return true;
}


//----------------------------------------------------------------------------
void vtkFITSReader::ExecuteInformation()
{
  if (this->CurrentFileName != nullptr &&
      !strcmp (this->CurrentFileName, this->GetFileName()))
    {
    return;
    }

  if (this->CurrentFileName != nullptr)
    {
    delete [] this->CurrentFileName;
    }

  this->CurrentFileName = new char[1 + strlen(this->GetFileName())];
  strcpy (this->CurrentFileName, this->GetFileName());

  if(fits_open_data(&this->fptr, this->GetFileName(), READONLY, &ReadStatus))
    {
    vtkErrorMacro("vtkFITSReader::ExecuteInformation: ERROR IN CFITSIO! Error reading"
                  " "<< this->GetFileName() << ": \n");
    fits_report_error(stderr, this->ReadStatus);
    return;
    }

  this->HeaderKeyValue.clear();

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
    vtkErrorMacro("vtkFITSReader::ExecuteInformation: Failed to allocateFitsHeader.");
    return;
    }

  // Push FITS header key/value pair data into std::map
  if(this->FixGipsyHeader() == 0)
    {
    vtkErrorMacro("vtkFITSReader::ExecuteInformation: Failed to FixGipsyHeader.");
    return;
    }

  // Push FITS header into WCS struct
  if(!this->AllocateWCS())
    {
    this->WCSStatus = -1;
    vtkErrorMacro("vtkFITSReader::ExecuteInformation: Failed to allocateWCS.");
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
           !dataModel.compare("PROFILE") ||
           !dataModel.compare("PVDIAGRAM"))
    {
    switch(StringToInt(this->GetHeaderValue("SlicerAstro.BITPIX")))
      {
      case 8:
        this->SetDataType( VTK_FLOAT );
        this->SetDataScalarType( VTK_FLOAT );
        break;
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
        vtkErrorMacro("vtkFITSReader::ExecuteInformation: Could not allocate data type.");
        return;
      }
    }
  else
    {
    vtkErrorMacro("vtkFITSReader::ExecuteInformation: Could not find the DATAMODEL keyword.");
    return;
    }

  // Set axis information
  int dataExtent[6]={0};
  double spacings[3]={0.};
  double origin[3]={0.};
  unsigned int naxes = StringToInt(this->GetHeaderValue("SlicerAstro.NAXIS"));

  //calculate the dataExtent and setting the Spacings and Origin
  for (unsigned int axii = 0; axii < naxes; axii++)
    {
    dataExtent[2*axii] = 0;
    dataExtent[2*axii+1] = StringToInt(this->GetHeaderValue(("SlicerAstro.NAXIS"+IntToString(axii+1)).c_str())) - 1;
    origin[axii] = 0.0;
    spacings[axii] = 1.0;
    }

  double theta1 = (StringToDouble(this->GetHeaderValue("SlicerAstro.CDELT1")) >= 0.) ? 0. : M_PI;
  double theta2 = 0.;
  if (naxes > 1)
    {
    theta2 = (StringToDouble(this->GetHeaderValue("SlicerAstro.CDELT2")) >= 0.) ? 0. : M_PI;
    }
  double theta3 = 0.;
  if (naxes > 2)
    {
    theta3 = (StringToDouble(this->GetHeaderValue("SlicerAstro.CDELT3")) >= 0.) ? 0. : M_PI;
    }
  theta1 += M_PI/2.;

  if (naxes == 2)
    {
    if (StringToDouble(this->GetHeaderValue("SlicerAstro.CDELT1")) < 0.)
      {
      theta3 += M_PI;
      }
    }

  if (naxes > 2)
    {
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
    for (unsigned int ii = 0; ii < 3; ii++)
      {
      this->RasToIjkMatrix->SetElement(ii, 3, origin[ii]);
      }
    }
  else
    {
    for (unsigned int ii = 0; ii < 3; ii++)
      {
      this->RasToIjkMatrix->SetElement(ii, 3, (dataExtent[2*ii+1] - dataExtent[2*ii])/2.0);
      }
    }

  this->SetDataExtent(dataExtent);
  this->SetDataSpacing(spacings);
  this->SetDataOrigin(origin);

  this->vtkImageReader2::ExecuteInformation();

  if (fits_close_file(this->fptr, &this->ReadStatus))
    {
    fits_report_error(stderr, this->ReadStatus);
    }
}

//----------------------------------------------------------------------------
bool vtkFITSReader::AllocateHeader()
{ 
   char card[FLEN_CARD];/* Standard string lengths defined in fitsio.h */
   char val[FLEN_VALUE];
   char com[FLEN_COMMENT];
   char key[FLEN_KEYWORD];
   int keylen = FLEN_KEYWORD;

   int nkeys, ii;

   if (!this->fptr)
     {
     vtkErrorMacro("vtkFITSReader::AllocateHeader :"
                   " fptr file pointer not found.");
     return false;
     }

   fits_get_hdrspace(this->fptr, &nkeys, nullptr, &this->ReadStatus); /* get # of keywords */

   /* Read and print each keywords */
   int histCont = 0, commCont = 0;
   this->HeaderKeyValue["SlicerAstro.DSS"] = "0";
   for (ii = 1; ii <= nkeys; ii++)
     {
     if (fits_read_record(this->fptr, ii, card, &this->ReadStatus))
       {
       continue;
       }
     if (fits_get_keyname(card, key, &keylen, &this->ReadStatus))
       {
       continue;
       }
     std::string strkey(key);
     if (fits_parse_value(card, val, com, &this->ReadStatus))
       {
       continue;
       }

     std::string str(val);
     size_t pos = str.find("D+");
     if (pos != std::string::npos)
       {
       double value = StringToDouble(str.substr(0, pos).c_str());
       double exponent = StringToDouble(str.substr(pos + 2).c_str());
       str = DoubleToString(value * pow(10, exponent));
       }
     pos = str.find("d+");
     if (pos != std::string::npos)
       {
       double value = StringToDouble(str.substr(0, pos).c_str());
       double exponent = StringToDouble(str.substr(pos + 2).c_str());
       str = DoubleToString(value * pow(10, exponent));
       }
     pos = str.find("D-");
     if (pos != std::string::npos)
       {
       double value = StringToDouble(str.substr(0, pos).c_str());
       double exponent = StringToDouble(str.substr(pos + 2).c_str());
       str = DoubleToString(value * pow(10, -exponent));
       }
     pos = str.find("d-");
     if (pos != std::string::npos)
       {
       double value = StringToDouble(str.substr(0, pos).c_str());
       double exponent = StringToDouble(str.substr(pos + 2).c_str());
       str = DoubleToString(value * pow(10, -exponent));
       }
     pos = str.find("E+");
     if (pos != std::string::npos)
       {
       double value = StringToDouble(str.substr(0, pos).c_str());
       double exponent = StringToDouble(str.substr(pos + 2).c_str());
       str = DoubleToString(value * pow(10, exponent));
       }
     pos = str.find("e+");
     if (pos != std::string::npos)
       {
       double value = StringToDouble(str.substr(0, pos).c_str());
       double exponent = StringToDouble(str.substr(pos + 2).c_str());
       str = DoubleToString(value * pow(10, exponent));
       }
     pos = str.find("E-");
     if (pos != std::string::npos)
       {
       double value = StringToDouble(str.substr(0, pos).c_str());
       double exponent = StringToDouble(str.substr(pos + 2).c_str());
       str = DoubleToString(value * pow(10, -exponent));
       }
     pos = str.find("e-");
     if (pos != std::string::npos)
       {
       double value = StringToDouble(str.substr(0, pos).c_str());
       double exponent = StringToDouble(str.substr(pos + 2).c_str());
       str = DoubleToString(value * pow(10, -exponent));
       }

     if (std::string::npos != str.find_first_of("'"))
       {
       str.erase(0,1);
       str.erase(str.size()-1, str.size());
       }

     // WCSLIB can not deal with PPO, AMDX or AMDY matrices
     if(strkey.find("PPO") != std::string::npos ||
        strkey.find("AMDX") != std::string::npos ||
        strkey.find("AMDY") != std::string::npos)
       {
       this->HeaderKeyValue["SlicerAstro.DSS"] = "1";
       }

     if (!strkey.compare("COMMENT"))
       {
       str = card;
       if (str.find("FITS (Flexible Image Transport System) format is defined") != std::string::npos ||
           str.find("volume 376, page 359; bibcode:") != std::string::npos)
         {
         continue;
         }

       if (str.size() < 8)
         {
         continue;
         }
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
       this->HeaderKeyValue[strkey] = str;
       }
     else if (!strkey.compare("HISTORY"))
       {
       str = card;
       if (str.size() < 8)
         {
         continue;
         }
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
       this->HeaderKeyValue[strkey] = str;
       }
     else
       {
       str.erase(std::remove_if(str.begin(), str.end(), isspace), str.end());
       if (str.empty())
         {
         continue;
         }
       strkey = "SlicerAstro." + strkey;
       this->HeaderKeyValue[strkey] = str;
       }
     }

   if (this->HeaderKeyValue.find("SlicerAstro.NAXIS") == this->HeaderKeyValue.end())
     {
     vtkErrorMacro("vtkFITSReader::AllocateHeader :"
                   " The fits header is missing the NAXIS keyword. "
                   "It is not possible to load the datacube.");
     return false;
     }

   int n = StringToInt((this->HeaderKeyValue.at("SlicerAstro.NAXIS")).c_str());

   if (n == 4 && this->HeaderKeyValue.find("SlicerAstro.NAXIS4") != this->HeaderKeyValue.end())
     {
     int n4 = StringToInt((this->HeaderKeyValue.at("SlicerAstro.NAXIS4")).c_str());
     if(n4 == 1)
       {
       this->HeaderKeyValue["SlicerAstro.NAXIS"] = "3";
       this->HeaderKeyValue.erase("SlicerAstro.NAXIS4");
       vtkWarningMacro("vtkFITSReader::AllocateHeader :"
                       " the 4th dimension keywords have been removed.");
       n = 3;
       }
     else
       {
       vtkErrorMacro("vtkFITSReader::AllocateHeader : \n"
                     "Datacube with polarization (NAXIS=4 and NAXIS4>1) are not supported. \n"<<
                     "If you want to visualize 4 dimensional data, contact: \n"<<
                     "Davide Punzo, punzodavide@hotmail.it");
       return false;
       }
     }

   if (this->HeaderKeyValue.find("SlicerAstro.NAXIS4") != this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue.erase("SlicerAstro.NAXIS4");
     }
   if (this->HeaderKeyValue.find("SlicerAstro.CDELT4") != this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue.erase("SlicerAstro.CDELT4");
     }
   if (this->HeaderKeyValue.find("SlicerAstro.CRPIX4") != this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue.erase("SlicerAstro.CRPIX4");
     }
   if (this->HeaderKeyValue.find("SlicerAstro.CRVAL4") != this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue.erase("SlicerAstro.CRVAL4");
     }
   if (this->HeaderKeyValue.find("SlicerAstro.CTYPE4") != this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue.erase("SlicerAstro.CTYPE4");
     }
   if (this->HeaderKeyValue.find("SlicerAstro.CUNIT4") != this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue.erase("SlicerAstro.CUNIT4");
     }
   if (this->HeaderKeyValue.find("SlicerAstro.CROTA4") != this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue.erase("SlicerAstro.CROTA4");
     }

   if (n == 3 && this->HeaderKeyValue.find("SlicerAstro.NAXIS3") != this->HeaderKeyValue.end())
     {
     int n3 = StringToInt((this->HeaderKeyValue.at("SlicerAstro.NAXIS3")).c_str());
     if (n3 == 1)
       {
       this->HeaderKeyValue["SlicerAstro.NAXIS"] = "2";
       n = 2;
       }
     }

   if (n == 2)
     {
     if (this->HeaderKeyValue.find("SlicerAstro.NAXIS3") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.NAXIS3");
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CDELT3") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.CDELT3");
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CRPIX3") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.CRPIX3");
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CRVAL3") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.CRVAL3");
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CTYPE3") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.CTYPE3");
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CUNIT3") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.CUNIT3");
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CROTA3") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.CROTA3");
       }
     }

   if (n == 2 && this->HeaderKeyValue.find("SlicerAstro.NAXIS2") != this->HeaderKeyValue.end())
     {
     int n2 = StringToInt((this->HeaderKeyValue.at("SlicerAstro.NAXIS2")).c_str());
     if (n2 == 1)
       {
       this->HeaderKeyValue["SlicerAstro.NAXIS"] = "1";
       n = 1;
       }
     }

   if (n == 1)
     {
     if (this->HeaderKeyValue.find("SlicerAstro.NAXIS2") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.NAXIS2");
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CDELT2") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.CDELT2");
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CRPIX2") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.CRPIX2");
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CRVAL2") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.CRVAL2");
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CTYPE2") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.CTYPE2");
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CUNIT2") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.CUNIT2");
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CROTA2") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue.erase("SlicerAstro.CROTA2");
       }
     }

   if (n < 1 || n > 4)
     {
       vtkErrorMacro("vtkFITSReader::AllocateHeader : \n"
                     "the data has wrong dimensionality. Please, check the fits header.");
     return false;
     }

   std::string temp = "SlicerAstro.NAXIS";

   if (n > 3)
     {
     vtkErrorMacro("vtkFITSReader::AllocateHeader : "
                   "SlicerAstro, currently, can't load datacube with NAXIS > 3.");
     return false;
     }

   for (ii = 1; ii <= n; ii++)
     {
     temp += IntToString(ii);

     if(this->HeaderKeyValue.find(temp.c_str()) == this->HeaderKeyValue.end())
       {
       vtkErrorMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the NAXIS" << ii <<
                     " keyword. It is not possible to load the datacube.");
       return false;
       }

     temp.erase(temp.size()-1);
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CRPIX1") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the CRPIX1 keyword.");
     this->HeaderKeyValue["SlicerAstro.CRPIX1"] = "0.0";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CRPIX2") == this->HeaderKeyValue.end() && n > 1)
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the CRPIX2 keyword.");
     this->HeaderKeyValue["SlicerAstro.CRPIX2"] = "0.0";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CRPIX3") == this->HeaderKeyValue.end() && n > 2)
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the CRPIX3 keyword.");
     this->HeaderKeyValue["SlicerAstro.CRPIX3"] = "0.0";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CRVAL1") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the CRVAL1 keyword.");
     this->HeaderKeyValue["SlicerAstro.CRVAL1"] = "UNDEFINED";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CRVAL2") == this->HeaderKeyValue.end() && n > 1)
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the CRVAL2 keyword.");
     this->HeaderKeyValue["SlicerAstro.CRVAL2"] = "UNDEFINED";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CRVAL3") == this->HeaderKeyValue.end() && n > 2)
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the CRVAL3 keyword.");
     this->HeaderKeyValue["SlicerAstro.CRVAL3"] = "UNDEFINED";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CTYPE1") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the CTYPE1 keyword.");
     this->HeaderKeyValue["SlicerAstro.CTYPE1"] = "NONE";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CTYPE2") == this->HeaderKeyValue.end() && n > 1)
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the CTYPE2 keyword.");
     this->HeaderKeyValue["SlicerAstro.CTYPE2"] = "NONE";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CTYPE3") == this->HeaderKeyValue.end() && n > 2)
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the CTYPE3 keyword.");
     this->HeaderKeyValue["SlicerAstro.CTYPE3"] = "NONE";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CUNIT1") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the CUNIT1 keyword. Assuming degree.");
     this->HeaderKeyValue["SlicerAstro.CUNIT1"] = "DEGREE";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CUNIT2") == this->HeaderKeyValue.end() && n > 1)
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the CUNIT2 keyword. Assuming degree.");
     this->HeaderKeyValue["SlicerAstro.CUNIT2"] = "DEGREE";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CUNIT3") == this->HeaderKeyValue.end() && n > 2)
     {
     std::string ctype3 = this->HeaderKeyValue.at("SlicerAstro.CTYPE3");
     if(!(ctype3.compare(0,4,"FREQ")))
       {
       vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                       "The fits header is missing the CUNIT3 keyword. Assuming Hz.");
       this->HeaderKeyValue["SlicerAstro.CUNIT3"] = "Hz";
       }
     else
       {
       vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                       " The fits header is missing the CUNIT3 keyword. Assuming km/s.");
       this->HeaderKeyValue["SlicerAstro.CUNIT3"] = "km/s";
       }
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CDELT1") == this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue["SlicerAstro.CDELT1"] = "1.0";
     }

   if (!strcmp(this->HeaderKeyValue["SlicerAstro.DSS"].c_str(), "1") &&
       !strcmp(this->HeaderKeyValue["SlicerAstro.CDELT1"].c_str(), "1.0"))
     {
     vtkWarningMacro("vtkFITSReader::AllocateWCS: "
                     "PPO, AMDX and AMDY matrices are not WCS fits standard. \n"
                     "Some opeartions (such as reprojection) will not be available. \n"
                     "Please provide data with PC matrix formalism "
                     "for full WCS support in SlicerAstro. \n"
                     "Proper dss images can be found at http://archive.eso.org/dss/dss");
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CDELT2") == this->HeaderKeyValue.end() && n > 1)
     {
     this->HeaderKeyValue["SlicerAstro.CDELT2"] = "1.0";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CDELT3") == this->HeaderKeyValue.end() && n > 2)
     {
     this->HeaderKeyValue["SlicerAstro.CDELT3"] = "1.0";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CROTA1") == this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue["SlicerAstro.CROTA1"] = "0.";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CROTA2") == this->HeaderKeyValue.end() && n > 1)
     {
     this->HeaderKeyValue["SlicerAstro.CROTA2"] = "0.";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CROTA3") == this->HeaderKeyValue.end() && n > 2)
     {
     this->HeaderKeyValue["SlicerAstro.CROTA3"] = "0.";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.BITPIX") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the BITPIX keyword. Using in default 32 (float).");
     this->HeaderKeyValue["SlicerAstro.BITPIX"] = "32";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.BTYPE") == this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue["SlicerAstro.BTYPE"] = "NONE";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.BUNIT") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the BUNIT keyword.");
     this->HeaderKeyValue["SlicerAstro.BUNIT"] = "UNDEFINED";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.BMAJ") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the BMAJ keyword.");
     this->HeaderKeyValue["SlicerAstro.BMAJ"] = "UNDEFINED";
     }

   if (!strcmp(this->HeaderKeyValue["SlicerAstro.BMAJ"].c_str(), "UNDEFINED"))
     {
     // 3DBAROLO
     if (this->HeaderKeyValue.find("SlicerAstro.BBMAJ") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.BMAJ"] = this->HeaderKeyValue["SlicerAstro.BBMAJ"];
       vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                        "Beam information recovered from alternative keywords (3DBAROLO) \n"
                        << " BMAJ = " << this->HeaderKeyValue["SlicerAstro.BMAJ"] << " DEGREE. \n"
                        << " It is recommended to check the value.");
       }
     // GIPSY
     else if (this->HeaderKeyValue.find("SlicerAstro.BMMAJ") != this->HeaderKeyValue.end())
       {
       double BMMAJ = StringToDouble(this->HeaderKeyValue["SlicerAstro.BMMAJ"].c_str());
       BMMAJ /= 3600.;
       this->HeaderKeyValue["SlicerAstro.BMAJ"] = DoubleToString(BMMAJ);
       vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                        "Beam information recovered from alternative keywords (GIPSY) \n"
                        << " BMAJ = " << this->HeaderKeyValue["SlicerAstro.BMAJ"] << " DEGREE. \n"
                        << " It is recommended to check the value.");
       }
     else
       {
       for (std::map<std::string,std::string>::iterator it = this->HeaderKeyValue.begin(); it != this->HeaderKeyValue.end(); ++it)
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
           this->HeaderKeyValue["SlicerAstro.BMAJ"] = DoubleToString(BMAJ);
           vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                            "Beam information recovered from alternative keywords (CASA) \n"
                            << " BMAJ = " << this->HeaderKeyValue["SlicerAstro.BMAJ"] << " DEGREE. \n"
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
           this->HeaderKeyValue["SlicerAstro.BMAJ"] = DoubleToString(BMAJ);
           vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                            "Beam information recovered from alternative keywords (MIRIAD) \n"
                            << " BMAJ = " << this->HeaderKeyValue["SlicerAstro.BMAJ"] << " DEGREE. \n"
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
           this->HeaderKeyValue["SlicerAstro.BMAJ"] = DoubleToString(BMAJ);
           vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                            "Beam information recovered from alternative keywords (AIPS) \n"
                            << " BMAJ = " << this->HeaderKeyValue["SlicerAstro.BMAJ"] << " DEGREE. \n"
                            << " It is recommended to check the value.");
           break;
           }
         }
       }
     }

   if (this->HeaderKeyValue.find("SlicerAstro.BMIN") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the BMIN keyword.");
     this->HeaderKeyValue["SlicerAstro.BMIN"] = "UNDEFINED";
     }

   if (!strcmp(this->HeaderKeyValue["SlicerAstro.BMIN"].c_str(), "UNDEFINED"))
     {
     // 3DBAROLO
     if (this->HeaderKeyValue.find("SlicerAstro.BBMIN") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.BMIN"] = this->HeaderKeyValue["SlicerAstro.BBMIN"];
       vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                        "Beam information recovered from alternative keywords (3DBAROLO) \n"
                        << " BMIN = " << this->HeaderKeyValue["SlicerAstro.BMIN"] << " DEGREE. \n"
                        << " It is recommended to check the value.");
       }
     // GIPSY
     else if (this->HeaderKeyValue.find("SlicerAstro.BMMIN") != this->HeaderKeyValue.end())
       {
       double BMMIN = StringToDouble(this->HeaderKeyValue["SlicerAstro.BMMIN"].c_str());
       BMMIN /= 3600.;
       this->HeaderKeyValue["SlicerAstro.BMIN"] = DoubleToString(BMMIN);
       vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                        "Beam information recovered from alternative keywords (GIPSY) \n"
                        << " BMIN = " << this->HeaderKeyValue["SlicerAstro.BMIN"] << " DEGREE. \n"
                        << " It is recommended to check the value.");
       }
     else
       {
       for (std::map<std::string,std::string>::iterator it = this->HeaderKeyValue.begin(); it != HeaderKeyValue.end(); ++it)
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
           this->HeaderKeyValue["SlicerAstro.BMIN"] = DoubleToString(BMIN);
           vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                            "Beam information recovered from alternative keywords (CASA) \n"
                            << " BMIN = " << this->HeaderKeyValue["SlicerAstro.BMIN"] << " DEGREE. \n"
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
           this->HeaderKeyValue["SlicerAstro.BMIN"] = DoubleToString(BMIN);
           vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                            "Beam information recovered from alternative keywords (MIRIAD) \n"
                            << " BMIN = " << this->HeaderKeyValue["SlicerAstro.BMIN"] << " DEGREE. \n"
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
           this->HeaderKeyValue["SlicerAstro.BMIN"] = DoubleToString(BMIN);
           vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                            "Beam information recovered from alternative keywords (AIPS) \n"
                            << " BMIN = " << this->HeaderKeyValue["SlicerAstro.BMIN"] << " DEGREE. \n"
                            << " It is recommended to check the value.");
           break;
           }
         }
       }
     }

   if (this->HeaderKeyValue.find("SlicerAstro.BPA") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the BPA keyword.");
     this->HeaderKeyValue["SlicerAstro.BPA"] = "UNDEFINED";
     }

   if (!strcmp(this->HeaderKeyValue["SlicerAstro.BPA"].c_str(), "UNDEFINED"))
     {
     double RadToDeg = 45. / atan(1.);
     // 3DBAROLO
     if (this->HeaderKeyValue.find("SlicerAstro.BBPA") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.BPA"] = this->HeaderKeyValue["SlicerAstro.BBPA"];
       vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                        "Beam information recovered from alternative keywords (3DBAROLO) \n"
                        << " BPA = " << this->HeaderKeyValue["SlicerAstro.BPA"] << " DEGREE. \n"
                        << " It is recommended to check the value.");
       }
     // GIPSY
     else if (this->HeaderKeyValue.find("SlicerAstro.BMPA") != this->HeaderKeyValue.end())
       {
       double BMPA = StringToDouble(this->HeaderKeyValue["SlicerAstro.BMPA"].c_str());
       BMPA /= 3600.;
       this->HeaderKeyValue["SlicerAstro.BPA"] = DoubleToString(BMPA);
       vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                        "Beam information recovered from alternative keywords (GIPSY) \n"
                        << " BPA = " << this->HeaderKeyValue["SlicerAstro.BPA"] << " DEGREE. \n"
                        << " It is recommended to check the value.");
       }
     else
       {
       for (std::map<std::string,std::string>::iterator it = this->HeaderKeyValue.begin(); it != this->HeaderKeyValue.end(); ++it)
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
           this->HeaderKeyValue["SlicerAstro.BPA"] = DoubleToString(BPA);
           vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                            "Beam information recovered from alternative keywords (CASA) \n"
                            << " BPA = " << this->HeaderKeyValue["SlicerAstro.BPA"] << " DEGREE. \n"
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
           this->HeaderKeyValue["SlicerAstro.BPA"] = DoubleToString(BPA);
           vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                            "Beam information recovered from alternative keywords (MIRIAD) \n"
                            << " BPA = " << this->HeaderKeyValue["SlicerAstro.BPA"] << " DEGREE. \n"
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
           this->HeaderKeyValue["SlicerAstro.BPA"] = DoubleToString(BPA);
           vtkWarningMacro( "vtkFITSReader::AllocateHeader : "
                            "Beam information recovered from alternative keywords (AIPS) \n"
                            << " BPA = " << this->HeaderKeyValue["SlicerAstro.BPA"] << " DEGREE. \n"
                            << " It is recommended to check the value.");
           break;
           }
         }
       }
     }

   if (this->HeaderKeyValue.find("SlicerAstro.BZERO") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the BZERO keyword. Assuming a value equal to zero.");
     this->HeaderKeyValue["SlicerAstro.BZERO"] = "0.";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.BSCALE") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the BSCALE keyword. Assuming a value equal to 1.");
     this->HeaderKeyValue["SlicerAstro.BSCALE"] = "1.";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.BLANK") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the BLANK keyword. Assuming a value equal to zero.");
     this->HeaderKeyValue["SlicerAstro.BLANK"] = "0.";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.DATAMODEL") == this->HeaderKeyValue.end())
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
     QRegExp pvDiagramName("(\\b|_)([Pp][Vv][Dd]iagram})(\\b|_)");
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
       this->HeaderKeyValue["SlicerAstro.DATAMODEL"] = "MASK";
       }
     else if (fileInfo.baseName().contains(modelName) ||
              fileInfo.baseName().contains(modelNamesShort))
       {
       this->HeaderKeyValue["SlicerAstro.DATAMODEL"] = "MODEL";
       }
     else if (fileInfo.baseName().contains(profileName))
       {
       this->HeaderKeyValue["SlicerAstro.DATAMODEL"] = "PROFILE";
       }
     else if (fileInfo.baseName().contains(pvDiagramName))
       {
       this->HeaderKeyValue["SlicerAstro.DATAMODEL"] = "PVDIAGRAM";
       }
     else if (fileInfo.baseName().contains(zeroMomentMapName) ||
              fileInfo.baseName().contains(zeroMomentMapNameShort))
       {
       this->HeaderKeyValue["SlicerAstro.DATAMODEL"] = "ZEROMOMENTMAP";
       }
     else if (fileInfo.baseName().contains(firstMomentMapName) ||
              fileInfo.baseName().contains(firstMomentMapNameShort))
       {
       this->HeaderKeyValue["SlicerAstro.DATAMODEL"] = "FIRSTMOMENTMAP";
       }
     else if (fileInfo.baseName().contains(secondMomentMapName) ||
              fileInfo.baseName().contains(secondMomentMapNameShort))
       {
       this->HeaderKeyValue["SlicerAstro.DATAMODEL"] = "SECONDMOMENTMAP";
       }
     else
       {
       this->HeaderKeyValue["SlicerAstro.DATAMODEL"] = "DATA";
       }
     }

   if (this->HeaderKeyValue.find("SlicerAstro.DATAMAX") == this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue["SlicerAstro.DATAMAX"] = "0.";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.DATAMIN") == this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue["SlicerAstro.DATAMIN"] = "0.";
     }

   this->HeaderKeyValue["SlicerAstro.DisplayThreshold"] = "0.";

   HeaderKeyValue["SlicerAstro.HistoMinSel"] = "0.";
   HeaderKeyValue["SlicerAstro.HistoMaxSel"] = "0.";

   if (this->HeaderKeyValue.find("SlicerAstro.DUNIT3") == this->HeaderKeyValue.end() && n > 2)
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the DUNIT3 keyword.");
     this->HeaderKeyValue["SlicerAstro.DUNIT3"] = "NONE";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.DRVAL3") == this->HeaderKeyValue.end() && n > 2)
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the DRVAL3 keyword.");
     this->HeaderKeyValue["SlicerAstro.DRVAL3"] = "0.";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.RESTFREQ") == this->HeaderKeyValue.end())
     {
     if (this->HeaderKeyValue.find("SlicerAstro.FREQ0") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.RESTFREQ"] = this->HeaderKeyValue.at("SlicerAstro.FREQ0");
       }
     else if (n > 2)
       {
       std::string dunit3 = this->HeaderKeyValue.at("SlicerAstro.DUNIT3");
       std::string cunit3 = this->HeaderKeyValue.at("SlicerAstro.CUNIT3");
       double drval3 = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.DRVAL3").c_str());
       double crval3 = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CRVAL3").c_str());
       if (!dunit3.compare("NONE") || fabs(drval3) < 1.E-06 ||
           !cunit3.compare("NONE") || fabs(crval3) < 1.E-06)
         {
         vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                         "The fits header is missing the RESTFREQ keyword. "
                         "Assuming HI data, i.e. RESTFREQ = 1.420405752E+09.");
         this->HeaderKeyValue["SlicerAstro.RESTFREQ"] = "1.420405752E+09";
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
         this->HeaderKeyValue["SlicerAstro.RESTFREQ"] = DoubleToString(freq0);
         }
       }
     else
       {
       vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                       "The fits header is missing the RESTFREQ keyword. "
                       "Assuming HI data, i.e. RESTFREQ = 1.420405752E+09.");
       this->HeaderKeyValue["SlicerAstro.RESTFREQ"] = "1.420405752E+09";
       }
     }

   if (this->HeaderKeyValue.find("SlicerAstro.DATE-OBS") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the DATE-OBS keyword.");
     this->HeaderKeyValue["SlicerAstro.DATE-OBS"] = "";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.EQUINOX") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the EQUINOX keyword.");
     if (this->HeaderKeyValue.find("SlicerAstro.EPOCH") != this->HeaderKeyValue.end())
       {
       vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                       "Found the EPOCH keyword. The value will be copied in the EQUINOX keyword");
       this->HeaderKeyValue["SlicerAstro.EQUINOX"] = this->HeaderKeyValue.at("SlicerAstro.EPOCH");
       }
     else
       {
       vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                       "Assuming J2000 for the EQUINOX keyword.");
       this->HeaderKeyValue["SlicerAstro.EQUINOX"] = "2000.";
       }
     }

   if (this->HeaderKeyValue.find("SlicerAstro.RADESYS") == this->HeaderKeyValue.end())
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "The fits header is missing the RADESYS keyword.");
     this->HeaderKeyValue["SlicerAstro.RADESYS"] = "";
     }

   if (this->HeaderKeyValue.find("SlicerAstro.CELLSCAL") == this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue["SlicerAstro.CELLSCAL"] = "";
     }
   else
     {
     vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                     "Found CELLSCAL keyword in fits header. SlicerAstro does not support it.");
     }

   if (this->HeaderKeyValue.find("SlicerAstro.TELESCOP") == this->HeaderKeyValue.end())
     {
     if (this->HeaderKeyValue.find("SlicerAstro.TELESC") != this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.TELESCOP"] = this->HeaderKeyValue.at("SlicerAstro.TELESC");
       }
     else
       {
       this->HeaderKeyValue["SlicerAstro.TELESCOP"] = "";
       }
     }

   if (this->HeaderKeyValue.find("SlicerAstro.OBJECT") == this->HeaderKeyValue.end())
     {
     this->HeaderKeyValue["SlicerAstro.OBJECT"] = "";
     }

   // Check pc, cd matrices and cdelt values
   if (n > 1)
     {
     bool pcMatrixFound = false;
     bool cdMatrixFound = false;
     bool cDeltNotValid = false;

     // pc matrix
     if (this->HeaderKeyValue.find("SlicerAstro.PC1_1") == this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.PC1_1"] = "0.";
       }
     if (this->HeaderKeyValue.find("SlicerAstro.PC1_2") == this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.PC1_2"] = "0.";
       }
     if (this->HeaderKeyValue.find("SlicerAstro.PC2_1") == this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.PC2_1"] = "0.";
       }
     if (this->HeaderKeyValue.find("SlicerAstro.PC2_2") == this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.PC2_2"] = "0.";
       }

     if (this->HeaderKeyValue.find("SlicerAstro.PC001001") == this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.PC001001"] = "0.";
       }
     if (this->HeaderKeyValue.find("SlicerAstro.PC001002") == this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.PC001002"] = "0.";
       }
     if (this->HeaderKeyValue.find("SlicerAstro.PC002001") == this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.PC002001"] = "0.";
       }
     if (this->HeaderKeyValue.find("SlicerAstro.PC002002") == this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.PC002002"] = "0.";
       }


     if (fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC1_1").c_str())) > 1.E-6 ||
         fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC1_2").c_str())) > 1.E-6 ||
         fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC2_1").c_str())) > 1.E-6 ||
         fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC2_2").c_str())) > 1.E-6 ||
         fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC001001").c_str())) > 1.E-6 ||
         fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC001002").c_str())) > 1.E-6 ||
         fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC002001").c_str())) > 1.E-6 ||
         fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC002002").c_str())) > 1.E-6)
       {
       pcMatrixFound = true;
       }

     if (n == 3)
       {
       if (this->HeaderKeyValue.find("SlicerAstro.PC1_3") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.PC1_3"] = "0.";
         }
       if (this->HeaderKeyValue.find("SlicerAstro.PC2_3") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.PC2_3"] = "0.";
         }
       if (this->HeaderKeyValue.find("SlicerAstro.PC3_1") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.PC3_1"] = "0.";
         }
       if (this->HeaderKeyValue.find("SlicerAstro.PC3_2") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.PC3_2"] = "0.";
         }
       if (this->HeaderKeyValue.find("SlicerAstro.PC3_3") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.PC3_3"] = "0.";
         }

       if (this->HeaderKeyValue.find("SlicerAstro.PC001003") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.PC001003"] = "0.";
         }
       if (this->HeaderKeyValue.find("SlicerAstro.PC002003") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.PC002003"] = "0.";
         }
       if (this->HeaderKeyValue.find("SlicerAstro.PC003001") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.PC003001"] = "0.";
         }
       if (this->HeaderKeyValue.find("SlicerAstro.PC003002") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.PC003002"] = "0.";
         }
       if (this->HeaderKeyValue.find("SlicerAstro.PC003003") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.PC003003"] = "0.";
         }

       if (fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC1_3").c_str())) > 1.E-6 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC2_3").c_str())) > 1.E-6 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC3_1").c_str())) > 1.E-6 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC3_2").c_str())) > 1.E-6 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC3_3").c_str())) > 1.E-6 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC001003").c_str())) > 1.E-6 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC002003").c_str())) > 1.E-6 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC003001").c_str())) > 1.E-6 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC003002").c_str())) > 1.E-6 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC003003").c_str())) > 1.E-6)
         {
         pcMatrixFound = true;
         }
       }

     // cdelti
     if (fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CDELT1").c_str())) < 1.E-9 ||
         fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CDELT1").c_str()) - 1.) < 1.E-9 ||
         fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CDELT2").c_str())) < 1.E-9 ||
         fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CDELT2").c_str()) - 1.) < 1.E-9 )
       {
       cDeltNotValid = true;
       }
     if (n > 2)
       {
       if (fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CDELT3").c_str())) < 1.E-9 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CDELT3").c_str()) - 1.) < 1.E-9)
         {
         cDeltNotValid = true;
         }
       }

     // cd matrix
     if (this->HeaderKeyValue.find("SlicerAstro.CD1_1") == this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.CD1_1"] = "0.";
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CD1_2") == this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.CD1_2"] = "0.";
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CD2_1") == this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.CD2_1"] = "0.";
       }
     if (this->HeaderKeyValue.find("SlicerAstro.CD2_2") == this->HeaderKeyValue.end())
       {
       this->HeaderKeyValue["SlicerAstro.CD2_2"] = "0.";
       }

     if (fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CD1_1").c_str())) > 1.E-6 ||
         fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CD1_2").c_str())) > 1.E-6 ||
         fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CD2_1").c_str())) > 1.E-6 ||
         fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CD2_2").c_str())) > 1.E-6)
       {
       cdMatrixFound = true;
       }

     if (n == 3)
       {
       if (this->HeaderKeyValue.find("SlicerAstro.CD1_3") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.CD1_3"] = "0.";
         }
       if (this->HeaderKeyValue.find("SlicerAstro.CD2_3") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.CD2_3"] = "0.";
         }
       if (this->HeaderKeyValue.find("SlicerAstro.CD3_1") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.CD3_1"] = "0.";
         }
       if (this->HeaderKeyValue.find("SlicerAstro.CD3_2") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.CD3_2"] = "0.";
         }
       if (this->HeaderKeyValue.find("SlicerAstro.CD3_3") == this->HeaderKeyValue.end())
         {
         this->HeaderKeyValue["SlicerAstro.CD3_3"] = "0.";
         }
       if (fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CD1_3").c_str())) > 1.E-6 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CD2_3").c_str())) > 1.E-6 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CD3_1").c_str())) > 1.E-6 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CD3_2").c_str())) > 1.E-6 ||
           fabs(StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CD3_3").c_str())) > 1.E-6)
         {
         cdMatrixFound = true;
         }
       }

     if (cdMatrixFound && !cDeltNotValid && !pcMatrixFound)
       {
       vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                     "cd matrix and cdelti found and pc matrix missing. This wcs keyword "
                     "combiantion is prohibited by FITS standards.");
       return false;
       }
     }

   if (this->ReadStatus) fits_report_error(stderr, this->ReadStatus); /* print any error message */

   return true;
}

//----------------------------------------------------------------------------
int vtkFITSReader::FixGipsyHeader()
{
  int n = StringToInt((this->HeaderKeyValue.at("SlicerAstro.NAXIS")).c_str());
  if (n != 3)
    {
    return 2;
    }

  std::vector<std::string> GipsyKeywords;
  GipsyKeywords.push_back("FREQ-OHEL");
  GipsyKeywords.push_back("FREQ-OLSR");
  GipsyKeywords.push_back("FREQ-RHEL");
  GipsyKeywords.push_back("FREQ-RLSR");

  if (!this->HeaderKeyValue.at("SlicerAstro.CTYPE3").compare(GipsyKeywords[0].c_str()) ||
      !this->HeaderKeyValue.at("SlicerAstro.CTYPE3").compare(GipsyKeywords[1].c_str()) ||
      !this->HeaderKeyValue.at("SlicerAstro.CTYPE3").compare(GipsyKeywords[2].c_str()) ||
      !this->HeaderKeyValue.at("SlicerAstro.CTYPE3").compare(GipsyKeywords[3].c_str()))
    {
    double c = 299792458.0;
    double vel = 0.;

    // Get Velocity reference info
    if(this->HeaderKeyValue.find("SlicerAstro.VELR") != this->HeaderKeyValue.end())
      {
      vel = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.VELR").c_str());
      }
    else
      {
      if (!this->HeaderKeyValue.at("SlicerAstro.DRVAL3").compare("0.") ||
          !this->HeaderKeyValue.at("SlicerAstro.DUNIT3").compare("NONE"))
        {
        vtkErrorMacro("vtkFITSReader::FixGipsyHeader: could not find DRVAL3 or DUNIT3.");
        return 0;
        }
      else
        {
        vel = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.DRVAL3").c_str());
        if (!this->HeaderKeyValue.at("SlicerAstro.DUNIT3").compare("km/s") ||
            !this->HeaderKeyValue.at("SlicerAstro.DUNIT3").compare("KM/S"))
          {
          vel *= 1000.;
          }
        }
      }

    if (fabs(vel) < 1.E-06)
      {
      vtkErrorMacro("vtkFITSReader::FixGipsyHeader: could not find velocity information.");
      return 0;
      }

    // Convert reference frequency to Hz
    double freq  = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CRVAL3").c_str());
    double dfreq = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CDELT3").c_str());
    std::string unit  = this->HeaderKeyValue.at("SlicerAstro.CUNIT3");
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
      vtkErrorMacro("vtkFITSReader::FixGipsyHeader : Freq unit not found.");
      return 0;
      }

    // Need rest frequency for conversion
    double freq0;
    if (!this->HeaderKeyValue.at("SlicerAstro.RESTFREQ").compare("0."))
      {
      vtkErrorMacro("vtkFITSReader::FixGipsyHeader: could not find RESTFREQ.");
      return 0;
      }
    else
      {
      freq0 = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.RESTFREQ").c_str());
      }

    // Calculate reference frequency in the barycentric system
    double freqB;
    if (!this->HeaderKeyValue.at("SlicerAstro.CTYPE3").compare(GipsyKeywords[0].c_str()) ||
        !this->HeaderKeyValue.at("SlicerAstro.CTYPE3").compare(GipsyKeywords[1].c_str()))
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

    this->HeaderKeyValue["SlicerAstro.CTYPE3"] = "FREQ";
    this->HeaderKeyValue["SlicerAstro.CUNIT3"] = "HZ";
    this->HeaderKeyValue["SlicerAstro.CRVAL3"] = DoubleToString(freqB);
    this->HeaderKeyValue["SlicerAstro.CDELT3"] = DoubleToString(dfreqB);

    vtkWarningMacro("vtkFITSReader::FixGipsyHeader: found Gipsy header keywords."
                    " CTYPE3, CUNIT3, CRVAL3 and CDELT3 has been automatically converted.");
    this->FixGipsyHeaderOn = true;
    }

  return 1;
}

//----------------------------------------------------------------------------
bool vtkFITSReader::AllocateWCS()
{
  char *header;
  int  i, nkeyrec, nreject, stat[NWCSFIX];

  // read header from fits file
  if ((this->WCSStatus = fits_hdr2str(this->fptr, 1, nullptr, 0, &header, &nkeyrec, &this->WCSStatus)))
    {
    fits_report_error(stderr, this->WCSStatus);
    }

  std::string stdHeader(header);
  size_t found;

  // fix gipsy keywords in wcs
  int n = StringToInt((this->HeaderKeyValue.at("SlicerAstro.NAXIS")).c_str());
  if (this->FixGipsyHeaderOn && n == 3)
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
        vtkErrorMacro("vtkFITSReader::AllocateWCS: Gipsy CTYPE3 keyword could not be replaced.");
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
        std::string replaceString = "'" + this->HeaderKeyValue.at("SlicerAstro.CUNIT3") + "'";
        blankString.replace(1, replaceString.size(), replaceString);
        stdHeader.replace(first + 1, blankString.size(), blankString);
        }
      else
        {
        vtkErrorMacro("vtkFITSReader::AllocateWCS: Gipsy CUNIT3 keyword could not be replaced.");
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
        std::string replaceString = this->HeaderKeyValue.at("SlicerAstro.CRVAL3");
        blankString.replace(1, replaceString.size(), replaceString);
        stdHeader.replace(first + 1, blankString.size(), blankString);
        }
      else
        {
        vtkErrorMacro("vtkFITSReader::AllocateWCS: Gipsy CRVAL3 keyword could not be replaced.");
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
        std::string replaceString = this->HeaderKeyValue.at("SlicerAstro.CDELT3");
        blankString.replace(1, replaceString.size(), replaceString);
        stdHeader.replace(first + 1, blankString.size(), blankString);
        }
      else
        {
        vtkErrorMacro("vtkFITSReader::AllocateWCS: Gipsy CDELT3 keyword could not be replaced.");
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
      std::string replaceString = this->HeaderKeyValue.at("SlicerAstro.RESTFREQ");
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
        std::string replaceString = this->HeaderKeyValue.at("SlicerAstro.RESTFREQ");
        blankString.replace(1, replaceString.size(), replaceString);
        stdHeader.replace(first + 1, blankString.size(), blankString);
        }
      }
    }

  // update wcs from fits header
  header = (char *)malloc(((int)(stdHeader.size())+1)*sizeof(char));
  std::strcpy(header, stdHeader.c_str());

  if ((this->WCSStatus = wcspih(header, nkeyrec, WCSHDR_all, 2, &nreject, &this->NWCS, &this->ReadWCS)))
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: wcspih ERROR "<<this->WCSStatus<<":\n"<<
                  "Message from "<<this->ReadWCS->err->function<<
                  "at line "<<this->ReadWCS->err->line_no<<" of file "<<this->ReadWCS->err->file<<
                  ": \n"<<this->ReadWCS->err->msg<<"\n");
    return false;
    }

  if (this->NWCS > 1)
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: the volume has more than one WCS, "
                  "SlicerAstro assume only one WCS per volume.");
    return false;
    }

  // run automatic wcs fix
  if ((this->WCSStatus = wcsfixi(7, 0, this->ReadWCS, stat, this->info)))
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: wcsfix error: "<<this->WCSStatus<<"\n");
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
    if (this->info[i].status < -1 || 0 < this->info[i].status)
      {
      vtkWarningMacro("wcsfix INFORMATIVE message from "<<this->info[i].function<<
                      "at line "<<this->info[i].line_no<<" of file "<<this->info[i].file<<
                      ": \n"<< this->info[i].msg<<"\n");
      }
    }

  // Reduce wcs dimensionality to the real one
  int readWcsAxis = this->ReadWCS->naxis;
  int nsub = 3;
  int axes[3];

  if (n == 3 && readWcsAxis >= 3)
    {
    nsub = 3;
    axes[0] = WCSSUB_LONGITUDE;
    axes[1] = WCSSUB_LATITUDE;
    axes[2] = WCSSUB_SPECTRAL;
    }
  else if (n == 2 && readWcsAxis >= 2)
    {
    nsub = 2;
    axes[0] = WCSSUB_LONGITUDE;
    axes[1] = WCSSUB_LATITUDE;
    axes[2] = -WCSSUB_SPECTRAL;
    }
  else if (n == 1 && readWcsAxis >= 1)
    {
    nsub = 1;
    axes[0] = WCSSUB_LONGITUDE;
    axes[1] = -WCSSUB_LATITUDE;
    axes[2] = -WCSSUB_SPECTRAL;
    }
  else
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                  "it is not possible to copy WCS from a volume with "
                  "naxis < than the naxis of the current volume. ");
    return false;
    }

  this->WCS->flag = -1;
  if ((this->WCSStatus = wcssub(1, this->ReadWCS, &nsub, axes, this->WCS)))
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                  "wcssub ERROR "<<this->WCSStatus<<":\n"<<
                  "Message from "<<this->WCS->err->function<<
                  "at line "<<this->WCS->err->line_no<<
                  " of file "<<this->WCS->err->file<<
                  ": \n"<<this->WCS->err->msg<<"\n");
    return false;
    }

  // set internally the wcs
  if ((this->WCSStatus = wcsset(this->WCS)))
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: wcsset ERROR "<<this->WCSStatus<<":\n"<<
                  "Message from "<<this->WCS->err->function<<
                  "at line "<<this->WCS->err->line_no<<" of file "<<this->WCS->err->file<<
                  ": \n"<<this->WCS->err->msg<<"\n");
    }

  if (n != this->WCS->naxis)
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                  "the dimensionality of the WCS and of the SlicerAstro attribute do not correspond.");
    return false;
    }

  if (this->WCS->naxis > 3)
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                  "the dimensionality of the WCS > 3 SlicerAstro can not visualize such data.");
    return false;
    }

  if (!strcmp(this->HeaderKeyValue["SlicerAstro.DSS"].c_str(), "1"))
    {
    if (this->WCS->naxis != 2)
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                    "the dimensionality of the WCS != 2 for dss images. Please check the header.");
      return false;
      }

    if (this->HeaderKeyValue.find("SlicerAstro.CDELT1") != this->HeaderKeyValue.end())
      {
      this->WCS->cdelt[0] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CDELT1").c_str());
      }
    else
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                    "DSS image CDELT1 not found. Provide DSS image with CDELT1 keyword.");
      return false;
      }

    if (this->HeaderKeyValue.find("SlicerAstro.CDELT2") != this->HeaderKeyValue.end())
      {
      this->WCS->cdelt[1] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CDELT2").c_str());
      }
    else
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                    "DSS image CDELT2 not found. Provide DSS image with CDELT2 keyword.");
      return false;
      }

    if (this->HeaderKeyValue.find("SlicerAstro.CRPIX1") != this->HeaderKeyValue.end())
      {
      this->WCS->crpix[0] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CRPIX1").c_str());
      }
    else
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                    "DSS image CRPIX1 not found. Provide DSS image with CRPIX1 keyword.");
      return false;
      }

    if (this->HeaderKeyValue.find("SlicerAstro.CRPIX2") != this->HeaderKeyValue.end())
      {
      this->WCS->crpix[1] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CRPIX2").c_str());
      }
    else
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                    "DSS image CRPIX2 not found. Provide DSS image with CRPIX2 keyword.");
      return false;
      }

    if (this->HeaderKeyValue.find("SlicerAstro.CRVAL1") != this->HeaderKeyValue.end())
      {
      this->WCS->crval[0] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CRVAL1").c_str());
      }
    else
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                    "DSS image CRVAL1 not found. Provide DSS image with CRVAL1 keyword.");
      return false;
      }

    if (this->HeaderKeyValue.find("SlicerAstro.CRVAL2") != this->HeaderKeyValue.end())
      {
      this->WCS->crval[1] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.CRVAL2").c_str());
      }
    else
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                    "DSS image CRVAL2 not found. Provide DSS image with CRVAL2 keyword.");
      return false;
      }


    this->WCS->crota[0] = 0.;
    this->WCS->crota[1] = 0.;

    this->WCS->cd[0] = 0.;
    this->WCS->cd[1] = 0.;
    this->WCS->cd[2] = 0.;
    this->WCS->cd[3] = 0.;

    if (this->HeaderKeyValue.find("SlicerAstro.PC1_1") != this->HeaderKeyValue.end())
      {
      this->WCS->pc[0] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC1_1").c_str());
      }
    if (fabs(this->WCS->pc[0]) < 1.E-6 && this->HeaderKeyValue.find("SlicerAstro.PC001001") != this->HeaderKeyValue.end())
      {
      this->WCS->pc[0] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC001001").c_str());
      }
    else
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                    "DSS image PC Matrix not found. Provide DSS image with PC matrix.");
      return false;
      }

    if (this->HeaderKeyValue.find("SlicerAstro.PC1_2") != this->HeaderKeyValue.end())
      {
      this->WCS->pc[1] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC1_2").c_str());
      }
    if (fabs(this->WCS->pc[1]) < 1.E-6 && this->HeaderKeyValue.find("SlicerAstro.PC001002") != this->HeaderKeyValue.end())
      {
      this->WCS->pc[1] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC001002").c_str());
      }
    else
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                    "DSS image PC Matrix not found. Provide DSS image with PC matrix.");
      return false;
      }

    if (this->HeaderKeyValue.find("SlicerAstro.PC2_1") != this->HeaderKeyValue.end())
      {
      this->WCS->pc[2] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC2_1").c_str());
      }
    if (fabs(this->WCS->pc[2]) < 1.E-6 && this->HeaderKeyValue.find("SlicerAstro.PC002001") != this->HeaderKeyValue.end())
      {
      this->WCS->pc[2] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC002001").c_str());
      }
    else
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                    "DSS image PC Matrix not found. Provide DSS image with PC matrix.");
      return false;
      }

    if (this->HeaderKeyValue.find("SlicerAstro.PC2_2") != this->HeaderKeyValue.end())
      {
      this->WCS->pc[3] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC2_2").c_str());
      }
    if (fabs(this->WCS->pc[3]) < 1.E-6 && this->HeaderKeyValue.find("SlicerAstro.PC002002") != this->HeaderKeyValue.end())
      {
      this->WCS->pc[3] = StringToDouble(this->HeaderKeyValue.at("SlicerAstro.PC002002").c_str());
      }
    else
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                    "DSS image PC Matrix not found. Provide DSS image with PC matrix.");
      return false;
      }
    }

  double CRVAL1 = this->WCS->crval[0];
  std::string CUNIT1 = this->HeaderKeyValue.at("SlicerAstro.CUNIT1");

  if (CUNIT1.find("DEGREE") != std::string::npos ||
      CUNIT1.find("degree") != std::string::npos ||
      CUNIT1.find("deg") != std::string::npos)
    {
    while (CRVAL1 < 0. ||  CRVAL1 > 360.)
      {
      if (CRVAL1 < 0.)
        {
        CRVAL1 += 360.;
        }
      else if (CRVAL1 > 360.)
        {
        CRVAL1 -= 360.;
        }
      }
    }
  else if (CUNIT1.find("RADIAN") != std::string::npos ||
           CUNIT1.find("radian") != std::string::npos ||
           CUNIT1.find("rad") != std::string::npos)
    {
    while (CRVAL1 < 0. ||  CRVAL1 > 2 * PI)
      {
      if (CRVAL1 < 0.)
        {
        CRVAL1 += 2 * PI;
        }
      else if (CRVAL1 > 2 * PI)
        {
        CRVAL1 -= 2 * PI;
        }
      }
    }

  this->WCS->crval[0] = CRVAL1;

  // Check pc, cd matrices and cdelt values
  bool cdMatrixFound = false;
  if (n > 1)
    {
    bool pcMatrixFound = false;
    if ((fabs(this->WCS->pc[0]) > 1.E-6 ||
         fabs(this->WCS->pc[1]) > 1.E-6 ||
         fabs(this->WCS->pc[2]) > 1.E-6 ||
         fabs(this->WCS->pc[3]) > 1.E-6))
      {
      pcMatrixFound = true;
      }
    if (n > 2)
      {
      if (fabs(this->WCS->pc[4]) > 1.E-6 ||
          fabs(this->WCS->pc[5]) > 1.E-6 ||
          fabs(this->WCS->pc[6]) > 1.E-6 ||
          fabs(this->WCS->pc[7]) > 1.E-6 ||
          fabs(this->WCS->pc[8]) > 1.E-6)
        {
        pcMatrixFound = true;
        }
      }

    bool cDeltNotValid = false;
    if (fabs(this->WCS->cdelt[0]) < 1.E-9 ||
        fabs(this->WCS->cdelt[0] - 1.) < 1.E-9 ||
        fabs(this->WCS->cdelt[1]) < 1.E-9 ||
        fabs(this->WCS->cdelt[1] - 1.) < 1.E-9 )
      {
      cDeltNotValid = true;
      }
    if (n > 2)
      {
      if (fabs(this->WCS->cdelt[2]) < 1.E-9 ||
          fabs(this->WCS->cdelt[2] - 1.) < 1.E-9)
        {
        cDeltNotValid = true;
        }
      }

    if ((fabs(this->WCS->cd[0]) > 1.E-6 ||
         fabs(this->WCS->cd[1]) > 1.E-6 ||
         fabs(this->WCS->cd[2]) > 1.E-6 ||
         fabs(this->WCS->cd[3]) > 1.E-6))
      {
      cdMatrixFound = true;
      }
    if (n > 2)
      {
      if (fabs(this->WCS->cd[4]) > 1.E-6 ||
          fabs(this->WCS->cd[5]) > 1.E-6 ||
          fabs(this->WCS->cd[6]) > 1.E-6 ||
          fabs(this->WCS->cd[7]) > 1.E-6 ||
          fabs(this->WCS->cd[8]) > 1.E-6)
        {
        cdMatrixFound = true;
        }
      }

    if (!pcMatrixFound && !cdMatrixFound && cDeltNotValid)
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                    "WCS didn't find either pc matrix, cdelti and cd matrix.");
      return false;
      }

    if (pcMatrixFound && !cdMatrixFound && cDeltNotValid)
      {
      vtkErrorMacro("vtkFITSReader::AllocateWCS: "
                    "WCS found pc matrix, but cdelti are invalid.");
      return false;
      }

    if ((!pcMatrixFound && cdMatrixFound) ||
        (pcMatrixFound && cdMatrixFound && cDeltNotValid))
      {
      double cd11 = 0., cd12 = 0., cd21 = 0., cd22 = 0.;
      if (n == 2)
        {
        cd11 = this->WCS->cd[0];
        cd12 = this->WCS->cd[1];
        cd21 = this->WCS->cd[2];
        cd22 = this->WCS->cd[3];
        }
      else if (n == 3)
        {
        cd11 = this->WCS->cd[0];
        cd12 = this->WCS->cd[1];
        cd21 = this->WCS->cd[3];
        cd22 = this->WCS->cd[4];
        }

      double CDELT1 = 0.;
      double CDELT2 = 0.;
      double CROTA2 = 0.;
      if (fabs(cd12) < 1.E-6 && fabs(cd21) < 1.E-6)
        {
        CROTA2 = 0.0;
        CDELT1 = cd11;
        CDELT2 = cd22;
        }
      else
        {
        CDELT1 = sqrt(cd11 * cd11 + cd21 * cd21);
        CDELT2 = sqrt(cd12 * cd12 + cd22 * cd22);
        double det = cd11 * cd22 - cd12 * cd21;
        if (fabs(det) < 1.E-6)
          {
          vtkWarningMacro("vtkFITSReader::AllocateHeader : "
                          "Determinant of CD matrix == 0. This means that the CD keyword may be wrong. "
                          "Please check that the CD matrix is not actually the PC matrix.");
          }
        double sign = 1.0;
        if (det < 0.0)
          {
          CDELT2 = -CDELT2;
          sign = -1.0;
          }
        double rot1_cd = atan2(-cd21, sign * cd11);
        double rot2_cd = atan2(sign * cd12, cd22);
        double rot_av = (rot1_cd + rot2_cd) * 0.5;
        const double rad2deg = 180. / PI;
        CROTA2 = (rot_av * rad2deg) + 180.;
        }

      this->HeaderKeyValue["SlicerAstro.CDELT1"] = DoubleToString(CDELT1);
      this->HeaderKeyValue["SlicerAstro.CDELT2"] = DoubleToString(CDELT2);
      this->HeaderKeyValue["SlicerAstro.CROTA2"] = DoubleToString(CROTA2);

      if (n == 3)
        {
        this->HeaderKeyValue["SlicerAstro.CDELT3"] = DoubleToString(this->WCS->cd[8]);
        }
      }

    if (pcMatrixFound && !cDeltNotValid)
      {
      double pc11 = 0., pc12 = 0., pc21 = 0., pc22 = 0.;
      if (n == 2)
        {
        pc11 = this->WCS->pc[0];
        pc12 = this->WCS->pc[1];
        pc21 = this->WCS->pc[2];
        pc22 = this->WCS->pc[3];
        }
      else if (n == 3)
        {
        pc11 = this->WCS->pc[0];
        pc12 = this->WCS->pc[1];
        pc21 = this->WCS->pc[3];
        pc22 = this->WCS->pc[4];
        }

      double CROTA2 = 0.;
      if (fabs(pc12) > 1.E-6 && fabs(pc21) > 1.E-6)
        {
        double det = pc11 * pc22 - pc12 * pc21;
        double sign = 1.0;
        if (det < 0.0)
          {
          sign = -1.0;
          }
        double rot1_cd = atan2(-pc21, sign * pc11);
        double rot2_cd = atan2(sign * pc12, pc22);
        double rot_av = (rot1_cd + rot2_cd) * 0.5;
        const double rad2deg = 180. / PI;

        CROTA2 = (rot_av * rad2deg) + 180.;
        }

      this->HeaderKeyValue["SlicerAstro.CROTA2"] = DoubleToString(CROTA2);
      }
    }

  // set internally the wcs
  if ((this->WCSStatus = wcsset(this->WCS)))
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: wcsset ERROR "<<this->WCSStatus<<":\n"<<
                  "Message from "<<this->WCS->err->function<<
                  "at line "<<this->WCS->err->line_no<<" of file "<<this->WCS->err->file<<
                  ": \n"<<this->WCS->err->msg<<"\n");
    }

  // project third axes always as a velocity
  if (this->WCS->naxis > 2)
    {
    std::string ctype2 = this->WCS->ctype[2];
    if (strncmp(this->WCS->ctype[2], "VOPT", 4))
      {
      int index = 2;
      char ctypeS[9];
      strcpy(ctypeS, "VOPT-???");

      if ((this->WCSStatus = wcssptr(this->WCS, &index, ctypeS)))
        {
        vtkErrorMacro("vtkFITSReader::AllocateWCS: wcssptr ERROR "<<this->WCSStatus<<":\n"<<
                      "Message from "<<this->WCS->err->function<<
                      "at line "<<this->WCS->err->line_no<<" of file "<<this->WCS->err->file<<
                      ": \n"<<this->WCS->err->msg<<"\n");
        }
      else
        {
        vtkWarningMacro("vtkFITSReader::AllocateWCS: 3rd WCS axes has been converted "
                        "from "<<ctype2<< " to "<<this->WCS->ctype[2]<<".");
        }

      if ((this->WCSStatus = wcsset(this->WCS)))
        {
        vtkErrorMacro("vtkFITSReader::AllocateWCS: wcsset ERROR "<<this->WCSStatus<<":\n"<<
                      "Message from "<<this->WCS->err->function<<
                      "at line "<<this->WCS->err->line_no<<" of file "<<this->WCS->err->file<<
                      ": \n"<<this->WCS->err->msg<<"\n");
        }
      }
    }

  if (this->WCSStatus != 0)
    {
    vtkErrorMacro("vtkFITSReader::AllocateWCS: WCSlib failed to create WCSstruct."<< "\n"<<
                  "World coordinates will not be displayed. "<< "\n");
    return false;
    }

  // copy everything also in the astroVolume attributes
  this->HeaderKeyValue["SlicerAstro.CRVAL1"] = DoubleToString(this->WCS->crval[0]);
  if (!cdMatrixFound)
    {
    this->HeaderKeyValue["SlicerAstro.CDELT1"] = DoubleToString(this->WCS->cdelt[0]);
    }
  this->HeaderKeyValue["SlicerAstro.CRPIX1"] = DoubleToString(this->WCS->crpix[0]);
  this->HeaderKeyValue["SlicerAstro.CTYPE1"] = this->WCS->ctype[0];
  this->HeaderKeyValue["SlicerAstro.CUNIT1"] = this->WCS->cunit[0];

  if (n > 1)
    {
    this->HeaderKeyValue["SlicerAstro.CRVAL2"] = DoubleToString(this->WCS->crval[1]);
    if (!cdMatrixFound)
      {
      this->HeaderKeyValue["SlicerAstro.CDELT2"] = DoubleToString(this->WCS->cdelt[1]);
      }
    this->HeaderKeyValue["SlicerAstro.CRPIX2"] = DoubleToString(this->WCS->crpix[1]);
    this->HeaderKeyValue["SlicerAstro.CTYPE2"] = this->WCS->ctype[1];
    this->HeaderKeyValue["SlicerAstro.CUNIT2"] = this->WCS->cunit[1];

    this->HeaderKeyValue["SlicerAstro.PC1_1"] = DoubleToString(this->WCS->pc[0]);
    this->HeaderKeyValue["SlicerAstro.PC1_2"] = DoubleToString(this->WCS->pc[1]);
    this->HeaderKeyValue["SlicerAstro.PC2_1"] = DoubleToString(this->WCS->pc[2]);
    this->HeaderKeyValue["SlicerAstro.PC2_2"] = DoubleToString(this->WCS->pc[3]);

    if (this->HeaderKeyValue.find("SlicerAstro.PC001001") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.PC001001");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.PC001002") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.PC001002");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.PC002001") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.PC002001");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.PC002002") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.PC002002");
      }

    if (this->HeaderKeyValue.find("SlicerAstro.CD1_1") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.CD1_1");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.CD1_2") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.CD1_2");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.CD2_1") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.CD2_1");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.CD2_2") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.CD2_2");
      }
    }

  if (n > 2)
    {
    this->HeaderKeyValue["SlicerAstro.CRVAL3"] = DoubleToString(this->WCS->crval[2]);
    if (!cdMatrixFound)
      {
      this->HeaderKeyValue["SlicerAstro.CDELT3"] = DoubleToString(this->WCS->cdelt[2]);
      }
    this->HeaderKeyValue["SlicerAstro.CRPIX3"] = DoubleToString(this->WCS->crpix[2]);
    this->HeaderKeyValue["SlicerAstro.CTYPE3"] = this->WCS->ctype[2];
    this->HeaderKeyValue["SlicerAstro.CUNIT3"] = this->WCS->cunit[2];

    this->HeaderKeyValue["SlicerAstro.PC1_3"] = DoubleToString(this->WCS->pc[2]);
    this->HeaderKeyValue["SlicerAstro.PC2_1"] = DoubleToString(this->WCS->pc[3]);
    this->HeaderKeyValue["SlicerAstro.PC2_2"] = DoubleToString(this->WCS->pc[4]);
    this->HeaderKeyValue["SlicerAstro.PC2_3"] = DoubleToString(this->WCS->pc[5]);
    this->HeaderKeyValue["SlicerAstro.PC3_1"] = DoubleToString(this->WCS->pc[6]);
    this->HeaderKeyValue["SlicerAstro.PC3_2"] = DoubleToString(this->WCS->pc[7]);
    this->HeaderKeyValue["SlicerAstro.PC3_3"] = DoubleToString(this->WCS->pc[8]);

    if (this->HeaderKeyValue.find("SlicerAstro.PC001003") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.PC001003");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.PC002001") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.PC002001");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.PC002002") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.PC002002");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.PC002003") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.PC002003");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.PC003001") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.PC003001");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.PC003002") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.PC003002");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.PC003003") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.PC003003");
      }

    if (this->HeaderKeyValue.find("SlicerAstro.CD1_3") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.CD1_3");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.CD2_1") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.CD2_1");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.CD2_2") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.CD2_2");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.CD2_3") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.CD2_3");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.CD3_1") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.CD3_1");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.CD3_2") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.CD3_2");
      }
    if (this->HeaderKeyValue.find("SlicerAstro.CD3_3") != this->HeaderKeyValue.end())
      {
      this->HeaderKeyValue.erase("SlicerAstro.CD3_3");
      }
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
    return nullptr;
    }

  this->ExecuteInformation();

  res->SetExtent(this->GetUpdateExtent());

  if (!this->AllocatePointData(res, outInfo))
    {
    vtkErrorMacro("vtkFITSReader::AllocateOutputData: AllocatePointData failed.");
    return nullptr;
    }

  return res;
}

//----------------------------------------------------------------------------
bool vtkFITSReader::AllocatePointData(vtkImageData *out, vtkInformation* outInfo) {

  vtkDataArray *pd = nullptr;
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

  if (data == nullptr)
    {
    vtkErrorMacro(<< "vtkFITSReader::ExecuteDataWithInformation: "
                     "data not allocated.");
    return;
    }

  if (this->GetFileName() == nullptr)
    {
    vtkErrorMacro(<< "vtkFITSReader::ExecuteDataWithInformation: "
                     "Either a FileName or FilePrefix must be specified.");
    return;
    }

  // Open the fits.  Yes, this means that the file is being opened
  // twice: once by ExecuteInformation, and once here
  if(fits_open_data(&this->fptr, this->GetFileName(), READONLY, &this->ReadStatus))
    {
    vtkErrorMacro("vtkFITSReader::ExecuteDataWithInformation: "
                  "ERROR IN CFITSIO! Error reading "<< this->GetFileName() << ":\n");
    fits_report_error(stderr, this->ReadStatus);
    return;
    }

  data->GetPointData()->GetScalars()->SetName("FITSImage");
  // Get data pointer
  void *ptr = nullptr;
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
  float nullptrval = NAN;
  int anynullptr;
  // load the data
  switch (this->DataType)
    {
    case VTK_DOUBLE:
      if(fits_read_img(this->fptr, TDOUBLE, 1, dim, &nullptrval, ptr, &anynullptr, &this->ReadStatus))
        {
        fits_report_error(stderr, this->ReadStatus);
        vtkErrorMacro(<< "vtkFITSReader::ExecuteDataWithInformation: data is nullptr.");
        return;
        }
      break;
    case VTK_FLOAT:
      if(fits_read_img(this->fptr, TFLOAT, 1, dim, &nullptrval, ptr, &anynullptr, &this->ReadStatus))
        {
        fits_report_error(stderr, this->ReadStatus);
        vtkErrorMacro(<< "vtkFITSReader::ExecuteDataWithInformation: data is nullptr.");
        return;
        }
      break;
  case VTK_SHORT:
    if(fits_read_img(this->fptr, TSHORT, 1, dim, &nullptrval, ptr, &anynullptr, &this->ReadStatus))
      {
      fits_report_error(stderr, this->ReadStatus);
      vtkErrorMacro(<< "vtkFITSReader::ExecuteDataWithInformation: data is nullptr.");
      return;
      }
    break;
    default:
      vtkErrorMacro("vtkFITSReader::ExecuteDataWithInformation: Could not load data");
      return;
    }

  if (fits_close_file(this->fptr, &this->ReadStatus))
    {
    vtkErrorMacro("vtkFITSReader::ExecuteDataWithInformation: ERROR IN CFITSIO! Error closing "
                  << this->GetFileName() << ":\n");
    fits_report_error(stderr, this->ReadStatus);
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
