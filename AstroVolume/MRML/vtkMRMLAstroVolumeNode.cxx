#include <string>
#include <cstdlib>

// VTK includes
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkObjectFactory.h>
#include <vtkImageData.h>

// MRML includes
#include "vtkMRMLVolumeNode.h"
#include "vtkMRMLAstroVolumeNode.h"
#include "vtkMRMLAstroVolumeDisplayNode.h"
#include "vtkMRMLAstroVolumeStorageNode.h"

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroVolumeNode);

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode::vtkMRMLAstroVolumeNode()
{
  this->WCSStatus = 0;
  this->WCS = new struct wcsprm;
  this->WCS->flag=-1;
  if((this->WCSStatus = wcsini(1,0,this->WCS)))
    {
    vtkErrorMacro("wcsini ERROR "<<this->WCSStatus<<": "<<wcshdr_errmsg[this->WCSStatus]<<"\n");
    }
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode::~vtkMRMLAstroVolumeNode()
{
  if(this->WCS)
    {
    if((this->WCSStatus = wcsfree(this->WCS)))
      {
      vtkErrorMacro("wcsfree ERROR "<<this->WCSStatus<<": "<<wcshdr_errmsg[this->WCSStatus]<<"\n");
      }
    delete [] this->WCS;
    this->WCS = NULL;
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
}// end namespace


//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::ReadXMLAttributes(const char** atts)
{
  this->Superclass::ReadXMLAttributes(atts);

  this->WCS->flag=-1;
  if((this->WCSStatus = wcsini(1, StringToInt(this->GetAttribute("SlicerAstro.NAXIS")), this->WCS)))
    {
    vtkErrorMacro("wcsfree ERROR "<<this->WCSStatus<<": "<<wcshdr_errmsg[this->WCSStatus]<<"\n");
    }

  const char* attName;
  const char* attValue;
  std::string pre = "SlicerAstro.WCS.";
  std::string und = "UNDEFINED";
  std::string temp;
  int i, j, k;

  while (*atts != NULL)
    {
    attName = *(atts++);
    attValue = *(atts++);

    temp = pre + "naxis";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->naxis = StringToInt(attValue);
      continue;
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "crpix" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        this->WCS->crpix[i] = StringToDouble(attValue);
        continue;
        }
      }

    k = 0;
    for (i = 0; i < this->WCS->naxis; i++)
      {
      for (j = 0; j < this->WCS->naxis; j++)
        {
        temp = pre + "pc" + IntToString(k);
        if (!strcmp(attName, temp.c_str()))
          {
          this->WCS->pc[k] = StringToDouble(attValue);
          continue;
          }
        k++;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "cdelt" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        this->WCS->cdelt[i] = StringToDouble(attValue);
        continue;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "crval" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        this->WCS->crval[i] = StringToDouble(attValue);
        continue;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "cunit" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        strcpy(this->WCS->cunit[i], attValue);
        continue;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "ctype" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        strcpy(this->WCS->ctype[i], attValue);
        continue;
        }
      }

    temp = pre + "lonpole";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->lonpole = StringToDouble(attValue);
      continue;
      }

    temp = pre + "latpole";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->latpole = StringToDouble(attValue);
      continue;
      }

    temp = pre + "restfrq";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->restfrq = StringToDouble(attValue);
      continue;
      }

    temp = pre + "restwav";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->restwav = StringToDouble(attValue);
      continue;
      }

    temp = pre + "npv";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->npv = StringToInt(attValue);
      continue;
      }

    temp = pre + "npvmax";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->npvmax = StringToInt(attValue);
      continue;
      }

    for (i = 0; i < this->WCS->npv; i++)
      {
      temp = pre + "pvi" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
         (this->WCS->pv[i]).i = StringToInt(attValue);
        continue;
        }
      temp = pre + "pvvalue" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        this->WCS->pv[i].value = StringToDouble(attValue);
        continue;
        }
      }

    temp = pre + "nps";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->nps = StringToInt(attValue);
      continue;
      }

    temp = pre + "npsmax";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->npsmax = StringToInt(attValue);
      continue;
      }

    for (i = 0; i < this->WCS->npv; i++)
      {
      temp = pre + "psi" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        (this->WCS->ps[i]).i = StringToInt(attValue);
        continue;
        }
      temp = pre + "psvalue" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        strcpy (this->WCS->ps[i].value, attValue);
        continue;
        }
      }

    k = 0;
    for (i = 0; i < this->WCS->naxis; i++)
      {
      for (j = 0; j < this->WCS->naxis; j++)
        {
        temp = pre + "cd" + IntToString(k);
        if (!strcmp(attName, temp.c_str()))
          {
          this->WCS->cd[k] = StringToDouble(attValue);
          continue;
          }
        k++;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "crota" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        this->WCS->crota[i] = StringToDouble(attValue);
        continue;
        }
      }

    temp = pre + "altlin";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->altlin = StringToInt(attValue);
      continue;
      }

    temp = pre + "velref";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->velref = StringToInt(attValue);
      continue;
      }

    temp = pre + "alt";
    if (!strcmp(attName, temp.c_str()))
      {
      strcpy(this->WCS->alt, attValue);
      continue;
      }

    temp = pre + "colnum";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->colnum = StringToInt(attValue);
      continue;
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "colax" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))\
        {
        this->WCS->colax[i] = StringToInt(attValue);
        continue;
        }
      }

    temp = pre + "wcsname";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->wcsname) = '\0';
        }
      else
        {
        strcpy(this->WCS->wcsname, attValue);
        }
      continue;
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "cname" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        if(!strcmp(attValue, und.c_str()))
          {
          *(this->WCS->cname[i]) = '\0';
          }
        else
          {
          strcpy(this->WCS->cname[i], attValue);
          }
        continue;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "crder" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        if (!strcmp(attValue, und.c_str()))
          {
          this->WCS->crder[i] = 0.;
          }
        else
          {
          this->WCS->crder[i] = StringToDouble(attValue);
          }
        continue;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "csyer" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        if (!strcmp(attValue, und.c_str()))
          {
          this->WCS->csyer[i] = 0.;
          }
        else
          {
          this->WCS->csyer[i] = StringToDouble(attValue);
          }
        continue;
        }
      }

    temp = pre + "radesys";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->radesys) = '\0';
        }
      else
        {
        strcpy(this->WCS->radesys, attValue);
        }
      continue;
      }

    temp = pre + "equinox";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        this->WCS->equinox = 0.;
        }
      else
        {
        this->WCS->equinox = StringToDouble(attValue);
        }
      continue;
      }

    temp = pre + "specsys";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->specsys) = '\0';
        }
      else
        {
        strcpy(this->WCS->specsys, attValue);
        }
      continue;
      }

    temp = pre + "ssysobs";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->ssysobs) = '\0';
        }
      else
        {
        strcpy(this->WCS->ssysobs, attValue);
        }
      continue;
      }

    temp = pre + "velosys";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        this->WCS->velosys = 0.;
        }
      else
        {
        this->WCS->velosys = StringToDouble(attValue);
        }
      continue;
      }

    temp = pre + "ssyssrc";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->ssyssrc) = '\0';
        }
      else
        {
        strcpy(this->WCS->ssyssrc, attValue);
        }
      continue;
      }

    temp = pre + "zsource";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        this->WCS->zsource = 0.;
        }
      else
        {
        this->WCS->zsource = StringToDouble(attValue);
        }
      continue;
      }

    for (i = 0; i < 3; i++)
      {
      temp = pre + "obsgeo" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        if (!strcmp(attValue, und.c_str()))
          {
          this->WCS->obsgeo[i] = 0.;
          }
        else
          {
          this->WCS->obsgeo[i] = StringToDouble(attValue);
          }
        continue;
        }
      }

    temp = pre + "dateobs";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->dateobs) = '\0';
        }
      else
        {
        strcpy(this->WCS->dateobs, attValue);
        }
      continue;
      }

    temp = pre + "dateavg";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->dateavg) = '\0';
        }
      else
        {
        strcpy(this->WCS->dateavg, attValue);
        }
      continue;
      }

    temp = pre + "mjdobs";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        this->WCS->mjdobs = 0.;
        }
      else
        {
        this->WCS->mjdobs = StringToDouble(attValue);
        }
      continue;
      }

    temp = pre + "mjdavg";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        this->WCS->mjdavg = 0.;
        }
      else
        {
        this->WCS->mjdavg = StringToDouble(attValue);
        }
      continue;
      }
  }

  if ((this->WCSStatus = wcsset(this->WCS)))
    {
    vtkErrorMacro("wcsset ERROR "<<WCSStatus<<": "<<wcshdr_errmsg[WCSStatus]<<"\n");
    }

  this->WriteXML(std::cout,0);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::WriteXML(ostream& of, int nIndent)
{
  this->Superclass::WriteXML(of, nIndent);
  vtkIndent indent(nIndent);

  int i,j,k;

  std::string pre = " SlicerAstro.WCS.";
  std::string und = "UNDEFINED";

  of << indent << pre << "flag=\"" << this->WCS->flag << "\"";
  of << indent << pre << "naxis=\"" << this->WCS->naxis << "\"";
  for (i = 0; i < this->WCS->naxis; i++)
    {
    of << indent << pre << "crpix"<<i<<"=\"" << this->WCS->crpix[i] << "\"";
    }

  // Linear transformation.
  k = 0;
  for (i = 0; i < this->WCS->naxis; i++)
    {
    for (j = 0; j < this->WCS->naxis; j++)
      {
      of << indent << pre << "pc"<<k<<"=\"" << this->WCS->pc[k] << "\"";
      k++;
      }
    }

  // Coordinate increment at reference point.
  for (i = 0; i < this->WCS->naxis; i++)
    {
    of << indent << pre << "cdelt"<<i<<"=\"" << this->WCS->cdelt[i] << "\"";
    }

  // Coordinate value at reference point.
  for (i = 0; i < this->WCS->naxis; i++)
    {
    of << indent << pre << "crval"<<i<<"=\"" << this->WCS->crval[i] << "\"";
    }

  // Coordinate units and type.
  for (i = 0; i < this->WCS->naxis; i++)
    {
    of << indent << pre << "cunit"<<i<<"=\"" << this->WCS->cunit[i] << "\"";
    }

  for (i = 0; i < this->WCS->naxis; i++)
    {
    of << indent << pre << "ctype"<<i<<"=\"" << this->WCS->ctype[i] << "\"";
    }

  // Celestial and spectral transformation parameters.
  of << indent << pre << "lonpole=\"" << this->WCS->lonpole << "\"";
  of << indent << pre << "latpole=\"" << this->WCS->latpole << "\"";
  of << indent << pre << "restfrq=\"" << this->WCS->restfrq << "\"";
  of << indent << pre << "restwav=\"" << this->WCS->restwav << "\"";
  of << indent << pre << "npv=\"" << this->WCS->npv << "\"";
  of << indent << pre << "npvmax=\"" << this->WCS->npvmax << "\"";

  for (i = 0; i < this->WCS->npv; i++)
    {
    of << indent << pre << "pvi"<<i<<"=\"" <<  (this->WCS->pv[i]).i << "\"";
    of << indent << pre << "pvvalue"<<i<<"=\"" <<  (this->WCS->pv[i]).value << "\"";
    }

  of << indent << pre << "nps=\"" << this->WCS->nps << "\"";
  of << indent << pre << "npsmax=\"" << this->WCS->npsmax << "\"";

  for (i = 0; i < this->WCS->nps; i++)
    {
    of << indent << pre << "psi"<<i<<"=\"" <<  (this->WCS->ps[i]).i << "\"";
    of << indent << pre << "psvalue"<<i<<"=\"" <<  (this->WCS->ps[i]).value << "\"";
    }

  // Alternate linear transformations.
  k = 0;
  if (this->WCS->cd)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      for (j = 0; j < this->WCS->naxis; j++)
        {
        of << indent << pre << "cd"<<k<<"=\"" << this->WCS->cd[k] << "\"";
        k++;
        }
      }
    }

  if (this->WCS->crota)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      of << indent << pre << "crota"<<i<<"=\"" << this->WCS->crota[i] << "\"";
      }
    }

  of << indent << pre << "altlin=\"" << this->WCS->altlin << "\"";
  of << indent << pre << "velref=\"" << this->WCS->velref << "\"";
  of << indent << pre << "alt=\"" << this->WCS->alt << "\"";
  of << indent << pre << "colnum=\"" << this->WCS->colnum << "\"";

  if  (this->WCS->colax)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      of << indent << pre << "colax"<<i<<"=\"" << this->WCS->colax[i] << "\"";
      }
    }

  if (this->WCS->wcsname[0] == '\0')
    {
    of << indent << pre << "wcsname=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "wcsname=\"" << this->WCS->wcsname << "\"";
    }

  if (this->WCS->cname)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      if  (this->WCS->cname[i][0] == '\0')
        {
        of << indent << pre << "cname"<<i<<"=\"" << und << "\"";
        }
      else
        {
        of << indent << pre << "cname"<<i<<"=\"" << this->WCS->cname[i] << "\"";
        }
      }
    }

  if (this->WCS->crder)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      if (undefined (this->WCS->crder[i]))
        {
        of << indent << pre << "crder"<<i<<"=\"" << und << "\"";
        }
      else
        {
        of << indent << pre << "crder"<<i<<"=\"" << this->WCS->crder[i] << "\"";
        }
      }
    }

  if (this->WCS->csyer)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      if (undefined (this->WCS->csyer[i]))
        {
        of << indent << pre << "csyer"<<i<<"=\"" << und << "\"";
        }
      else
        {
        of << indent << pre << "csyer"<<i<<"=\"" << this->WCS->csyer[i] << "\"";
        }
      }
    }

  if (this->WCS->radesys[0] == '\0')
    {
    of << indent << pre << "radesys=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "radesys=\"" << this->WCS->radesys << "\"";
    }

  if (undefined (this->WCS->equinox))
    {
    of << indent << pre << "equinox=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "equinox=\"" << this->WCS->equinox << "\"";
    }

  if (this->WCS->specsys[0] == '\0')
    {
    of << indent << pre << "specsys=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "specsys=\"" << this->WCS->specsys << "\"";
    }

  if (this->WCS->ssysobs[0] == '\0')
    {
    of << indent << pre << "ssysobs=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "ssysobs=\"" << this->WCS->ssysobs << "\"";
    }

  if (undefined (this->WCS->velosys))
    {
    of << indent << pre << "velosys=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "velosys=\"" << this->WCS->velosys << "\"";
    }

  if (this->WCS->ssyssrc[0] == '\0')
    {
    of << indent << pre << "ssyssrc=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "ssyssrc=\"" << this->WCS->ssyssrc << "\"";
    }

  if (undefined (this->WCS->zsource))
    {
    of << indent << pre << "zsource=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "zsource=\"" << this->WCS->zsource << "\"";
    }

  for (i = 0; i < 3; i++)
    {
    if (undefined (this->WCS->obsgeo[i]))
      {
      of << indent << pre << "obsgeo"<<i<<"=\"" << und << "\"";
      }
    else
      {
      of << indent << pre << "obsgeo"<<i<<"=\"" << this->WCS->obsgeo[i] << "\"";
      }
    }

  if (this->WCS->dateobs[0] == '\0')
    {
    of << indent << pre << "dateobs=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "dateobs=\"" << this->WCS->dateobs << "\"";
    }

  if (this->WCS->dateavg[0] == '\0')
    {
    of << indent << pre << "dateavg=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "dateavg=\"" << this->WCS->dateavg << "\"";
    }

  if (undefined (this->WCS->mjdobs))
    {
    of << indent << pre << "mjdobs=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "mjdobs=\"" << this->WCS->mjdobs << "\"";
    }

  if (undefined (this->WCS->mjdavg))
    {
    of << indent << pre << "mjdavg=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "mjdavg=\"" << this->WCS->mjdavg << "\"";
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::Copy(vtkMRMLNode *anode)
{
  vtkMRMLAstroVolumeNode *astroVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(anode);
  if (!astroVolumeNode)
    {
    return;
    }

  this->Superclass::Copy(anode);

  this->WCS->flag=-1;
  if ((this->WCSStatus = wcscopy(1, astroVolumeNode->WCS, this->WCS)))
    {
    vtkErrorMacro("wcscopy ERROR "<<this->WCSStatus<<": "<<wcshdr_errmsg[this->WCSStatus]<<"\n");
    this->SetWCSStatus(astroVolumeNode->GetWCSStatus());
    }

  if ((this->WCSStatus = wcsset(this->WCS)))
    {
    vtkErrorMacro("wcsset ERROR "<<this->WCSStatus<<": "<<wcshdr_errmsg[this->WCSStatus]<<"\n");
    this->SetWCSStatus(astroVolumeNode->GetWCSStatus());
   }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::SetWCSStruct(struct wcsprm* wcstemp)
{

  if(wcstemp)
    {
    this->WCS->flag=-1;
    if ((this->WCSStatus = wcscopy(1, wcstemp, this->WCS)))
      {
      vtkErrorMacro("wcscopy ERROR "<<this->WCSStatus<<": "<<wcshdr_errmsg[this->WCSStatus]<<"\n");
      }
    if ((this->WCSStatus = wcsset (this->WCS)))
      {
      vtkErrorMacro("wcsset ERROR "<<this->WCSStatus<<": "<<wcshdr_errmsg[this->WCSStatus]<<"\n");
      }
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

    int i,j,k;

    std::string pre=" SlicerAstro.WCS.";
    std::string und="UNDEFINED";

    os << indent << pre << "flag:   " << this->WCS->flag <<std::endl;
    os << indent << pre << "naxis:   " << this->WCS->naxis <<std::endl;
    for (i = 0; i < this->WCS->naxis; i++)
      {
      os << indent << pre << "crpix"<<i<<":   " << this->WCS->crpix[i] <<std::endl;
      }

    // Linear transformation.
    k = 0;
    for (i = 0; i < this->WCS->naxis; i++)
      {
      for (j = 0; j < this->WCS->naxis; j++)
        {
        os << indent << pre << "pc"<<k<<":   " << this->WCS->pc[k] <<std::endl;
        k++;
        }
      }

    // Coordinate increment at reference point.
    for (i = 0; i < this->WCS->naxis; i++)
      {
      os << indent << pre << "cdelt"<<i<<":   " << this->WCS->cdelt[i] <<std::endl;
      }

    // Coordinate value at reference point.
    for (i = 0; i < this->WCS->naxis; i++)
      {
      os << indent << pre << "crval"<<i<<":   " << this->WCS->crval[i] <<std::endl;
      }

    // Coordinate units and type.
    for (i = 0; i < this->WCS->naxis; i++)
      {
      os << indent << pre << "cunit"<<i<<":   " << this->WCS->cunit[i] <<std::endl;
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      os << indent << pre << "ctype"<<i<<":   " << this->WCS->ctype[i] <<std::endl;
      }

    // Celestial and spectral transformation parameters.
    os << indent << pre << "lonpole:   " << this->WCS->lonpole <<std::endl;
    os << indent << pre << "latpole:   " << this->WCS->latpole <<std::endl;
    os << indent << pre << "restfrq:   " << this->WCS->restfrq <<std::endl;
    os << indent << pre << "restwav:   " << this->WCS->restwav <<std::endl;
    os << indent << pre << "npv:   " << this->WCS->npv <<std::endl;
    os << indent << pre << "npvmax:   " << this->WCS->npvmax <<std::endl;

    for (i = 0; i < this->WCS->npv; i++)
      {
      os << indent << pre << "pvi"<<i<<":   " << (this->WCS->pv[i]).i <<std::endl;
      os << indent << pre << "pvvalue"<<i<<":   " << (this->WCS->pv[i]).value <<std::endl;
      }

    os << indent << pre << "nps:   " << this->WCS->nps <<std::endl;
    os << indent << pre << "npsmax:   " << this->WCS->npsmax <<std::endl;

    for (i = 0; i < this->WCS->nps; i++)
      {
      os << indent << pre << "pvi"<<i<<":   " << (this->WCS->ps[i]).i <<std::endl;
      os << indent << pre << "pvvalue"<<i<<":   " << (this->WCS->ps[i]).value <<std::endl;
      }

    // Alternate linear transformations.
    k = 0;
    if (this->WCS->cd)
      {
      for (i = 0; i < this->WCS->naxis; i++)
        {
        for (j = 0; j < this->WCS->naxis; j++)
          {
          os << indent << pre << "cd"<<k<<":   " << this->WCS->cd[k] <<std::endl;
          k++;
          }
        }
      }

    if (this->WCS->crota)
      {
      for (i = 0; i < this->WCS->naxis; i++)
        {
        os << indent << pre << "crota"<<i<<":   " << this->WCS->crota[i] <<std::endl;
        }
      }

    os << indent << pre << "altlin:   " << this->WCS->altlin <<std::endl;
    os << indent << pre << "velref:   " << this->WCS->velref <<std::endl;
    os << indent << pre << "alt:   " << this->WCS->alt <<std::endl;
    os << indent << pre << "colnum:   " << this->WCS->colnum <<std::endl;

    if (this->WCS->colax)
      {
      for (i = 0; i < this->WCS->naxis; i++)
        {
        os << indent << pre << "colax"<<i<<":   " << this->WCS->colax[i] <<std::endl;
        }
      }

    if (this->WCS->wcsname[0] == '\0')
      {
      os << indent << pre << "wcsname:   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "wcsname:   " << this->WCS->wcsname <<std::endl;
      }

    if (this->WCS->cname)
      {
      for (i = 0; i < this->WCS->naxis; i++)
        {
        if (this->WCS->cname[i][0] == '\0')
          {
          os << indent << pre << "cname"<<i<<":   " << und <<std::endl;
          }
        else
          {
          os << indent << pre << "cname"<<i<<":   " << this->WCS->cname[i] <<std::endl;
          }
        }
      }

    if (this->WCS->crder)
      {
      for (i = 0; i < this->WCS->naxis; i++)
        {
        if (undefined(this->WCS->crder[i]))
          {
          os << indent << pre << "crder"<<i<<":   " << und <<std::endl;
          }
        else
          {
          os << indent << pre << "crder"<<i<<":   " << this->WCS->crder[i] <<std::endl;
          }
        }
      }

    if (this->WCS->csyer)
      {
      for (i = 0; i < this->WCS->naxis; i++)
        {
        if (undefined(this->WCS->csyer[i]))
          {
          os << indent << pre << "csyer"<<i<<":   " << und <<std::endl;
          }
        else
          {
          os << indent << pre << "csyer"<<i<<":   " << this->WCS->csyer[i] <<std::endl;
          }
        }
      }

    if (this->WCS->radesys[0] == '\0')
      {
      os << indent << pre << "radesys:   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "radesys:   " << this->WCS->radesys <<std::endl;
      }

    if (undefined(this->WCS->equinox))
      {
      os << indent << pre << "equinox:   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "equinox:   " << this->WCS->equinox <<std::endl;
      }

    if (this->WCS->specsys[0] == '\0')
      {
      os << indent << pre << "specsys:   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "specsys:   " << this->WCS->specsys <<std::endl;
      }

    if (this->WCS->ssysobs[0] == '\0')
      {
      os << indent << pre << "ssysobs:   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "ssysobs:   " << this->WCS->ssysobs <<std::endl;
      }

    if (undefined(this->WCS->velosys))
      {
      os << indent << pre << "velosys:   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "velosys:   " << this->WCS->velosys <<std::endl;
      }

    if (this->WCS->ssyssrc[0] == '\0')
      {
      os << indent << pre << "ssyssrc:   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "ssyssrc:   " << this->WCS->ssyssrc <<std::endl;
      }

    if (undefined(this->WCS->zsource))
      {
      os << indent << pre << "zsource:   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "zsource:   " << this->WCS->zsource <<std::endl;
      }

    for (i = 0; i < 3; i++)
      {
      if (undefined(this->WCS->obsgeo[i]))
        {
        os << indent << pre << "obsgeo"<<i<<":   " << und <<std::endl;
        }
      else
        {
        os << indent << pre << "obsgeo"<<i<<":   " << this->WCS->obsgeo[i] <<std::endl;
        }
      }

    if (this->WCS->dateobs[0] == '\0')
      {
      os << indent << pre << "dateobs:   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "dateobs:   " << this->WCS->dateobs <<std::endl;
      }

    if (this->WCS->dateavg[0] == '\0')
      {
      os << indent << pre << "dateavg:   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "dateavg:   " << this->WCS->dateavg <<std::endl;
      }

    if (undefined(this->WCS->mjdobs))
      {
      os << indent << pre << "mjdobs:   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "mjdobs:   " << this->WCS->mjdobs <<std::endl;
      }

    if (undefined(this->WCS->mjdavg))
      {
      os << indent << pre << "mjdavg:   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "mjdavg:   " << this->WCS->mjdavg <<std::endl;
      }
}

//---------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode* vtkMRMLAstroVolumeNode::GetAstroVolumeDisplayNode()
{
  return vtkMRMLAstroVolumeDisplayNode::SafeDownCast(this->GetDisplayNode());
}

void vtkMRMLAstroVolumeNode::GetReferenceSpace(const double ijk[3], const char *Space, double SpaceCoordinates[3])
{
  if (Space != NULL)
    {
    if (!strcmp(Space, "WCS"))
      {
      double phi[1], imgcrd[3], theta[1];
      int stati[1];

      if ((this->WCSStatus = wcsp2s(this->WCS, 1, 3, ijk, imgcrd, phi, theta, SpaceCoordinates, stati)))
        {
        vtkErrorMacro("wcsp2s ERROR "<<this->WCSStatus<<": "<<wcshdr_errmsg[this->WCSStatus]<<"\n");
        }
      }
    }
}

//---------------------------------------------------------------------------
vtkMRMLStorageNode* vtkMRMLAstroVolumeNode::CreateDefaultStorageNode()
{
  return vtkMRMLAstroVolumeStorageNode::New();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::CreateDefaultDisplayNodes()
{
  vtkMRMLAstroVolumeDisplayNode *displayNode = 
    vtkMRMLAstroVolumeDisplayNode::SafeDownCast(this->GetDisplayNode());
  if(displayNode == NULL)
  {
    displayNode = vtkMRMLAstroVolumeDisplayNode::New();
    if(this->GetScene())
    {
      displayNode->SetScene(this->GetScene());
      this->GetScene()->AddNode(displayNode);
      displayNode->SetDefaultColorMap();
      displayNode->Delete();

      this->SetAndObserveDisplayNodeID(displayNode->GetID());
      std::cout << "Display node set and observed" << std::endl;
    }
  }
}
