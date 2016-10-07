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
  and was supported through the European Research Consil grant nr. 291531.

==============================================================================*/

#include <map>
#include<cstdlib>

// vtkASTRO includes
#include <vtkFITSWriter.h>

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkPointData.h>
#include <vtkObjectFactory.h>
#include <vtkInformation.h>
#include <vtkVersion.h>

// STD includes
#include <sstream>

class AttributeMapType: public std::map<std::string, std::string> {};

vtkStandardNewMacro(vtkFITSWriter);

//----------------------------------------------------------------------------
vtkFITSWriter::vtkFITSWriter()
{
  this->FileName = NULL;
  this->UseCompression = 0;
  this->FileType = VTK_BINARY;
  this->WriteErrorOff();
  this->Attributes = new AttributeMapType;
  this->WriteStatus = 0;
}

//----------------------------------------------------------------------------
vtkFITSWriter::~vtkFITSWriter()
{
  if ( this->FileName )
    {
    delete [] this->FileName;
    }

  if (this->Attributes)
    {
    delete this->Attributes;
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


//----------------------------------------------------------------------------
vtkImageData* vtkFITSWriter::GetInput()
{
  return vtkImageData::SafeDownCast(this->Superclass::GetInput());
}

//----------------------------------------------------------------------------
vtkImageData* vtkFITSWriter::GetInput(int port)
{
  return vtkImageData::SafeDownCast(this->Superclass::GetInput(port));
}

//----------------------------------------------------------------------------
int vtkFITSWriter::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation *info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");
  return 1;

}


//----------------------------------------------------------------------------
// Writes all the data from the input.
void vtkFITSWriter::WriteData()
{

  this->WriteErrorOff();
  if (this->GetFileName() == NULL)
    {
    vtkErrorMacro("FileName has not been set. Cannot save file");
    this->WriteErrorOn();
    return;
    }

    // Fill in image information.

  vtkImageData *input = this->GetInput();
  vtkDataArray *array;
  array = static_cast<vtkDataArray *> (input->GetPointData()->GetScalars());
  int vtkType = array->GetDataType();
  void *buffer = array->GetVoidPointer(0);
  unsigned int naxes = input->GetDataDimension();
  long int naxe[naxes];
  int dim = 1;

  //allocate FITS struct
  remove(this->GetFileName());
  fits_create_file(&fptr, this->GetFileName(), &WriteStatus);

  if ( this->GetUseCompression()){
    //here could be in principle be implemented a switch for other values
    //GZIP_1, RICE_1, HCOMPRESS_1 or PLIO_1
    /*fits_set_compression_type(fptr, RICE_1, &WriteStatus);
    long int tile = 100;
    fits_set_tile_dim (fptr, 6, &tile, &WriteStatus);
    *//*not working*/
  }


  switch (vtkType){
    case  VTK_DOUBLE:
      fits_create_img(fptr, DOUBLE_IMG, naxes, naxe, &WriteStatus);
      break;
    case VTK_FLOAT:
      fits_create_img(fptr, FLOAT_IMG, naxes, naxe, &WriteStatus);
      break;
    case  VTK_SHORT:
      fits_create_img(fptr, SHORT_IMG, naxes, naxe, &WriteStatus);
      break;
    default:
      vtkErrorMacro("Could not write data type");
      return;
  }

  // write the header.

  //fits_write_key
  AttributeMapType::iterator ait;
  for (ait = this->Attributes->begin(); ait != this->Attributes->end(); ++ait)
    {
    std::size_t pos = ait->first.find("SlicerAstro.");
    if(pos == std::string::npos)
      {
      continue;
      }
    std::string tmp = ait->first.substr(pos+12);

    if((!tmp.compare(0,6,"SIMPLE")) || (!tmp.compare(0,6,"EXTEND"))
          || (!tmp.compare(0,7,"BLOCKED")))
      {
      continue;
      }
    std::string ts = ((ait->second).substr(0,1));
    if((!tmp.compare(0,6,"BITPIX")) || (!tmp.compare(0,5,"NAXIS"))
            || (!tmp.compare(0,5,"BLANK")))
      {
      int ti = StringToInt((ait->second).c_str());;
      fits_update_key(fptr, TINT, tmp.c_str(), &ti, "", &WriteStatus);
      }
    else if (!(std::string::npos != ts.find_first_of("-1234567890"))
               || (!tmp.compare(0,4,"DATE")) || (!tmp.compare(0, 8, "CELLSCAL"))
               || (!tmp.compare(0,8,"DATATYPE")))
      {
      fits_update_key(fptr, TSTRING, tmp.c_str(), (char *) (ait->second).c_str(), "", &WriteStatus);
      }
    else
      {
      double td;
      td = StringToDouble((ait->second).c_str());
      fits_update_key(fptr, TDOUBLE, tmp.c_str(), &td, "", &WriteStatus);
      }
    }

  // Write the FITS to file.
  for (unsigned int axii=0; axii < naxes; axii++)
    {
    naxe[axii] = StringToInt(this->GetAttribute(("SlicerAstro.NAXIS"+IntToString(axii+1))));
    dim *= naxe[axii];
    }

  int fileType = this->GetFileType();

  switch (fileType)
    {
    case VTK_BINARY:
      switch (vtkType)
        {
        case  VTK_DOUBLE:
          if(fits_write_img(fptr, TDOUBLE, 1, dim, buffer, &WriteStatus))
            {
            fits_report_error(stderr, WriteStatus);
            vtkErrorMacro("Write: Error writing "<< this->GetFileName() << "\n");
            this->WriteErrorOn();
            }
          break;
        case VTK_FLOAT:
          if(fits_write_img(fptr, TFLOAT, 1, dim, buffer, &WriteStatus))
            {
            fits_report_error(stderr, WriteStatus);
            vtkErrorMacro("Write: Error writing "<< this->GetFileName() << "\n");
            this->WriteErrorOn();
            }
          break;
        case VTK_SHORT:
          if(fits_write_img(fptr, TSHORT, 1, dim, buffer, &WriteStatus))
            {
            fits_report_error(stderr, WriteStatus);
            vtkErrorMacro("Write: Error writing "<< this->GetFileName() << "\n");
            this->WriteErrorOn();
            }
          break;
        }
      break;
    case VTK_ASCII:
     vtkErrorMacro("In 3-DSlicer FITS table are not supported");
    break;
  }

  // Free the FITS struct
  fits_close_file(fptr, &WriteStatus);
  fits_report_error(stderr, WriteStatus);

  return;
}

void vtkFITSWriter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

void vtkFITSWriter::SetAttribute(const std::string& name, const std::string& value)
{
  if (!this->Attributes)
    {
    return;
    }

  (*this->Attributes)[name] = value;
}

const char* vtkFITSWriter::GetAttribute(const std::string &key)
{
  std::map<std::string,std::string>::iterator i = Attributes->find(key);
  if (i != Attributes->end())
    {
    return (i->second.c_str());
    }
  else
    {
    return NULL;
    }
}

