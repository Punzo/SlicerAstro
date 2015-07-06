#include <string>
#include <cstdlib>

// VTK includes
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkObjectFactory.h>

// MRML includes
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>

// CropModuleMRML includes
#include <vtkMRMLAstroVolumeNode.h>

// STD includes
#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroVolumeNode);

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode::vtkMRMLAstroVolumeNode()
{
  WcsStatus = 0;
  wcs = new struct wcsprm;
  wcs->flag=-1;
  if((WcsStatus = wcsini(1,0,wcs))){
    vtkErrorMacro("wcsini ERROR "<<WcsStatus<<": "<<wcshdr_errmsg[WcsStatus]<<"\n");
  }
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode::~vtkMRMLAstroVolumeNode()
{
  if(wcs){
    if((WcsStatus = wcsfree(wcs))){
      vtkErrorMacro("wcsfree ERROR "<<WcsStatus<<": "<<wcshdr_errmsg[WcsStatus]<<"\n");
    }
    delete [] wcs;
    wcs = NULL;
  }
}


//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::ReadXMLAttributes(const char** atts)
{
  this->Superclass::ReadXMLAttributes(atts);

  wcs->flag=-1;
  if((WcsStatus = wcsini(1, std::atoi(this->GetAttribute("SlicerAstro.NAXIS")),wcs))){
    vtkErrorMacro("wcsfree ERROR "<<WcsStatus<<": "<<wcshdr_errmsg[WcsStatus]<<"\n");
  }

  const char* attName;
  const char* attValue;
  std::string pre = "SlicerAstro.WCS.";
  std::string und = "UNDEFINED";
  std::string temp;
  int i, j, k;

  while (*atts != NULL){
    attName = *(atts++);
    attValue = *(atts++);

    temp = pre + "naxis";
    if (!strcmp(attName, temp.c_str())){
      wcs->naxis = std::stoi(attValue);
      continue;
    }

    for (i = 0; i < wcs->naxis; i++) {
      temp = pre + "crpix" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        wcs->crpix[i] = std::stod(attValue);
        continue;
      }
    }

    k = 0;
    for (i = 0; i < wcs->naxis; i++) {
      for (j = 0; j < wcs->naxis; j++) {
          temp = pre + "pc" + std::to_string(k);
          if (!strcmp(attName, temp.c_str())){
            wcs->pc[k] = std::stod(attValue);
            continue;
          }
        k++;
      }
    }

    for (i = 0; i < wcs->naxis; i++) {
      temp = pre + "cdelt" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        wcs->cdelt[i] = std::stod(attValue);
        continue;
      }
    }

    for (i = 0; i < wcs->naxis; i++) {
      temp = pre + "crval" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        wcs->crval[i] = std::stod(attValue);
        continue;
      }
    }

    for (i = 0; i < wcs->naxis; i++) {
      temp = pre + "cunit" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        strcpy(wcs->cunit[i], attValue);
        continue;
      }
    }

    for (i = 0; i < wcs->naxis; i++) {
      temp = pre + "ctype" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        strcpy(wcs->ctype[i], attValue);
        continue;
      }
    }

    temp = pre + "lonpole";
    if (!strcmp(attName, temp.c_str())){
      wcs->lonpole = std::stod(attValue);
      continue;
    }

    temp = pre + "latpole";
    if (!strcmp(attName, temp.c_str())){
      wcs->latpole = std::stod(attValue);
      continue;
    }

    temp = pre + "restfrq";
    if (!strcmp(attName, temp.c_str())){
      wcs->restfrq = std::stod(attValue);
      continue;
    }

    temp = pre + "restwav";
    if (!strcmp(attName, temp.c_str())){
      wcs->restwav = std::stod(attValue);
      continue;
    }

    temp = pre + "npv";
    if (!strcmp(attName, temp.c_str())){
      wcs->npv = std::stoi(attValue);
      continue;
    }

    temp = pre + "npvmax";
    if (!strcmp(attName, temp.c_str())){
      wcs->npvmax = std::stoi(attValue);
      continue;
    }

    for (i = 0; i < wcs->npv; i++) {
      temp = pre + "pvi" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        (wcs->pv[i]).i = std::stoi(attValue);
        continue;
      }
      temp = pre + "pvvalue" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        wcs->pv[i].value = std::stod(attValue);
        continue;
      }
    }

    temp = pre + "nps";
    if (!strcmp(attName, temp.c_str())){
      wcs->nps = std::stoi(attValue);
      continue;
    }

    temp = pre + "npsmax";
    if (!strcmp(attName, temp.c_str())){
      wcs->npsmax = std::stoi(attValue);
      continue;
    }

    for (i = 0; i < wcs->npv; i++) {
      temp = pre + "psi" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        (wcs->ps[i]).i = std::stoi(attValue);
        continue;
      }
      temp = pre + "psvalue" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        strcpy(wcs->ps[i].value, attValue);
        continue;
      }
    }

    k = 0;
    for (i = 0; i < wcs->naxis; i++) {
      for (j = 0; j < wcs->naxis; j++) {
          temp = pre + "cd" + std::to_string(k);
          if (!strcmp(attName, temp.c_str())){
            wcs->cd[k] = std::stod(attValue);
            continue;
          }
        k++;
      }
    }

    for (i = 0; i < wcs->naxis; i++) {
      temp = pre + "crota" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        wcs->crota[i] = std::stod(attValue);
        continue;
      }
    }

    temp = pre + "altlin";
    if (!strcmp(attName, temp.c_str())){
      wcs->altlin = std::stoi(attValue);
      continue;
    }

    temp = pre + "velref";
    if (!strcmp(attName, temp.c_str())){
      wcs->velref = std::stoi(attValue);
      continue;
    }

    temp = pre + "alt";
    if (!strcmp(attName, temp.c_str())){
      strcpy(wcs->alt, attValue);
      continue;
    }

    temp = pre + "colnum";
    if (!strcmp(attName, temp.c_str())){
      wcs->colnum = std::stoi(attValue);
      continue;
    }

    for (i = 0; i < wcs->naxis; i++) {
      temp = pre + "colax" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        wcs->colax[i] = std::stoi(attValue);
        continue;
      }
    }

    temp = pre + "wcsname";
    if (!strcmp(attName, temp.c_str())){
      if(!strcmp(attValue, und.c_str())){
        *(wcs->wcsname) = '\0';
      }else{
        strcpy(wcs->wcsname, attValue);
      }
      continue;
    }

    for (i = 0; i < wcs->naxis; i++) {
      temp = pre + "cname" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        if(!strcmp(attValue, und.c_str())){
          *(wcs->cname[i]) = '\0';
        }else{
          strcpy(wcs->cname[i], attValue);
        }
        continue;
      }
    }

    for (i = 0; i < wcs->naxis; i++) {
      temp = pre + "crder" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        if(!strcmp(attValue, und.c_str())){
          wcs->crder[i] = 0.;
        }else{
          wcs->crder[i] = std::stod(attValue);
        }
        continue;
      }
    }

    for (i = 0; i < wcs->naxis; i++) {
      temp = pre + "csyer" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        if(!strcmp(attValue, und.c_str())){
          wcs->csyer[i] = 0.;
        }else{
          wcs->csyer[i] = std::stod(attValue);
        }
        continue;
      }
    }

    temp = pre + "radesys";
    if (!strcmp(attName, temp.c_str())){
      if(!strcmp(attValue, und.c_str())){
        *(wcs->radesys) = '\0';
      }else{
        strcpy(wcs->radesys, attValue);
      }
      continue;
    }

    temp = pre + "equinox";
    if (!strcmp(attName, temp.c_str())){
      if(!strcmp(attValue, und.c_str())){
        wcs->equinox = 0.;
      }else{
        wcs->equinox = std::stod(attValue);
      }
      continue;
    }

    temp = pre + "specsys";
    if (!strcmp(attName, temp.c_str())){
      if(!strcmp(attValue, und.c_str())){
        *(wcs->specsys) = '\0';
      }else{
        strcpy(wcs->specsys, attValue);
      }
      continue;
    }

    temp = pre + "ssysobs";
    if (!strcmp(attName, temp.c_str())){
      if(!strcmp(attValue, und.c_str())){
        *(wcs->ssysobs) = '\0';
      }else{
        strcpy(wcs->ssysobs, attValue);
      }
      continue;
    }

    temp = pre + "velosys";
    if (!strcmp(attName, temp.c_str())){
      if(!strcmp(attValue, und.c_str())){
        wcs->velosys = 0.;
      }else{
        wcs->velosys = std::stod(attValue);
      }
      continue;
    }

    temp = pre + "ssyssrc";
    if (!strcmp(attName, temp.c_str())){
      if(!strcmp(attValue, und.c_str())){
        *(wcs->ssyssrc) = '\0';
      }else{
        strcpy(wcs->ssyssrc, attValue);
      }
      continue;
    }

    temp = pre + "zsource";
    if (!strcmp(attName, temp.c_str())){
      if(!strcmp(attValue, und.c_str())){
        wcs->zsource = 0.;
      }else{
        wcs->zsource = std::stod(attValue);
      }
      continue;
    }

    for (i = 0; i < 3; i++) {
      temp = pre + "obsgeo" + std::to_string(i);
      if (!strcmp(attName, temp.c_str())){
        if(!strcmp(attValue, und.c_str())){
          wcs->obsgeo[i] = 0.;
        }else{
          wcs->obsgeo[i] = std::stod(attValue);
        }
        continue;
      }
    }

    temp = pre + "dateobs";
    if (!strcmp(attName, temp.c_str())){
      if(!strcmp(attValue, und.c_str())){
        *(wcs->dateobs) = '\0';
      }else{
        strcpy(wcs->dateobs, attValue);
      }
      continue;
    }

    temp = pre + "dateavg";
    if (!strcmp(attName, temp.c_str())){
      if(!strcmp(attValue, und.c_str())){
        *(wcs->dateavg) = '\0';
      }else{
        strcpy(wcs->dateavg, attValue);
      }
      continue;
    }

    temp = pre + "mjdobs";
    if (!strcmp(attName, temp.c_str())){
      if(!strcmp(attValue, und.c_str())){
        wcs->mjdobs = 0.;
      }else{
        wcs->mjdobs = std::stod(attValue);
      }
      continue;
    }

    temp = pre + "mjdavg";
    if (!strcmp(attName, temp.c_str())){
      if(!strcmp(attValue, und.c_str())){
        wcs->mjdavg = 0.;
      }else{
        wcs->mjdavg = std::stod(attValue);
      }
      continue;
    }





  }


  if ((WcsStatus = wcsset(wcs))) {
    vtkErrorMacro("wcsset ERROR "<<WcsStatus<<": "<<wcshdr_errmsg[WcsStatus]<<"\n");
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

  of << indent << pre << "flag=\"" << wcs->flag << "\"";
  of << indent << pre << "naxis=\"" << wcs->naxis << "\"";
  for (i = 0; i < wcs->naxis; i++) {
    of << indent << pre << "crpix"<<i<<"=\"" << wcs->crpix[i] << "\"";
  }

  // Linear transformation.
  k = 0;
  for (i = 0; i < wcs->naxis; i++) {
    for (j = 0; j < wcs->naxis; j++) {
      of << indent << pre << "pc"<<k<<"=\"" << wcs->pc[k] << "\"";
      k++;
    }
  }

  // Coordinate increment at reference point.
  for (i = 0; i < wcs->naxis; i++) {
    of << indent << pre << "cdelt"<<i<<"=\"" << wcs->cdelt[i] << "\"";
  }

  // Coordinate value at reference point.
  for (i = 0; i < wcs->naxis; i++) {
    of << indent << pre << "crval"<<i<<"=\"" << wcs->crval[i] << "\"";
  }

  // Coordinate units and type.
  for (i = 0; i < wcs->naxis; i++) {
    of << indent << pre << "cunit"<<i<<"=\"" << wcs->cunit[i] << "\"";
  }

  for (i = 0; i < wcs->naxis; i++) {
    of << indent << pre << "ctype"<<i<<"=\"" << wcs->ctype[i] << "\"";
  }

  // Celestial and spectral transformation parameters.
  of << indent << pre << "lonpole=\"" << wcs->lonpole << "\"";
  of << indent << pre << "latpole=\"" << wcs->latpole << "\"";
  of << indent << pre << "restfrq=\"" << wcs->restfrq << "\"";
  of << indent << pre << "restwav=\"" << wcs->restwav << "\"";
  of << indent << pre << "npv=\"" << wcs->npv << "\"";
  of << indent << pre << "npvmax=\"" << wcs->npvmax << "\"";

  for (i = 0; i < wcs->npv; i++) {
      of << indent << pre << "pvi"<<i<<"=\"" << (wcs->pv[i]).i << "\"";
      of << indent << pre << "pvvalue"<<i<<"=\"" << (wcs->pv[i]).value << "\"";
  }

  of << indent << pre << "nps=\"" << wcs->nps << "\"";
  of << indent << pre << "npsmax=\"" << wcs->npsmax << "\"";

  for (i = 0; i < wcs->nps; i++) {
      of << indent << pre << "psi"<<i<<"=\"" << (wcs->ps[i]).i << "\"";
      of << indent << pre << "psvalue"<<i<<"=\"" << (wcs->ps[i]).value << "\"";
  }

  // Alternate linear transformations.
  k = 0;
  if (wcs->cd) {
    for (i = 0; i < wcs->naxis; i++) {
      for (j = 0; j < wcs->naxis; j++) {
        of << indent << pre << "cd"<<k<<"=\"" << wcs->cd[k] << "\"";
        k++;
      }
    }
  }

  if (wcs->crota) {
    for (i = 0; i < wcs->naxis; i++) {
      of << indent << pre << "crota"<<i<<"=\"" << wcs->crota[i] << "\"";
    }
  }

  of << indent << pre << "altlin=\"" << wcs->altlin << "\"";
  of << indent << pre << "velref=\"" << wcs->velref << "\"";
  of << indent << pre << "alt=\"" << wcs->alt << "\"";
  of << indent << pre << "colnum=\"" << wcs->colnum << "\"";

  if (wcs->colax) {
    for (i = 0; i < wcs->naxis; i++) {
      of << indent << pre << "colax"<<i<<"=\"" << wcs->colax[i] << "\"";
    }
  }

  if (wcs->wcsname[0] == '\0') {
    of << indent << pre << "wcsname=\"" << und << "\"";
  } else {
    of << indent << pre << "wcsname=\"" << wcs->wcsname << "\"";
  }

  if (wcs->cname) {
    for (i = 0; i < wcs->naxis; i++) {
      if (wcs->cname[i][0] == '\0') {
        of << indent << pre << "cname"<<i<<"=\"" << und << "\"";
      } else {
        of << indent << pre << "cname"<<i<<"=\"" << wcs->cname[i] << "\"";
      }
    }
  }

  if (wcs->crder) {
    for (i = 0; i < wcs->naxis; i++) {
      if (undefined(wcs->crder[i])) {
        of << indent << pre << "crder"<<i<<"=\"" << und << "\"";
      } else{
        of << indent << pre << "crder"<<i<<"=\"" << wcs->crder[i] << "\"";
      }
    }
  }

  if (wcs->csyer) {
    for (i = 0; i < wcs->naxis; i++) {
      if (undefined(wcs->csyer[i])) {
        of << indent << pre << "csyer"<<i<<"=\"" << und << "\"";
      } else{
        of << indent << pre << "csyer"<<i<<"=\"" << wcs->csyer[i] << "\"";
      }
    }
  }

  if (wcs->radesys[0] == '\0') {
    of << indent << pre << "radesys=\"" << und << "\"";
  } else {
    of << indent << pre << "radesys=\"" << wcs->radesys << "\"";
  }

  if (undefined(wcs->equinox)) {
    of << indent << pre << "equinox=\"" << und << "\"";
  } else {
    of << indent << pre << "equinox=\"" << wcs->equinox << "\"";
  }

  if (wcs->specsys[0] == '\0') {
    of << indent << pre << "specsys=\"" << und << "\"";
  } else {
    of << indent << pre << "specsys=\"" << wcs->specsys << "\"";
  }

  if (wcs->ssysobs[0] == '\0') {
    of << indent << pre << "ssysobs=\"" << und << "\"";
  } else {
    of << indent << pre << "ssysobs=\"" << wcs->ssysobs << "\"";
  }

  if (undefined(wcs->velosys)) {
    of << indent << pre << "velosys=\"" << und << "\"";
  } else {
    of << indent << pre << "velosys=\"" << wcs->velosys << "\"";
  }

  if (wcs->ssyssrc[0] == '\0') {
    of << indent << pre << "ssyssrc=\"" << und << "\"";
  } else {
    of << indent << pre << "ssyssrc=\"" << wcs->ssyssrc << "\"";
  }

  if (undefined(wcs->zsource)) {
    of << indent << pre << "zsource=\"" << und << "\"";
  } else {
    of << indent << pre << "zsource=\"" << wcs->zsource << "\"";
  }

  for (i = 0; i < 3; i++) {
    if (undefined(wcs->obsgeo[i])) {
      of << indent << pre << "obsgeo"<<i<<"=\"" << und << "\"";
    } else {
      of << indent << pre << "obsgeo"<<i<<"=\"" << wcs->obsgeo[i] << "\"";
    }

  }

  if (wcs->dateobs[0] == '\0') {
    of << indent << pre << "dateobs=\"" << und << "\"";
  } else {
    of << indent << pre << "dateobs=\"" << wcs->dateobs << "\"";
  }

  if (wcs->dateavg[0] == '\0') {
    of << indent << pre << "dateavg=\"" << und << "\"";
  } else {
    of << indent << pre << "dateavg=\"" << wcs->dateavg << "\"";
  }

  if (undefined(wcs->mjdobs)) {
    of << indent << pre << "mjdobs=\"" << und << "\"";
  } else {
    of << indent << pre << "mjdobs=\"" << wcs->mjdobs << "\"";
  }

  if (undefined(wcs->mjdavg)) {
    of << indent << pre << "mjdavg=\"" << und << "\"";
  } else {
    of << indent << pre << "mjdavg=\"" << wcs->mjdavg << "\"";
  }
/*
 * tab seems not necessary.
  if (wcs->tab) {
    of << indent << pre << "ntab=\"" << wcs->ntab << "\"";
    for (j = 0; j < wcs->ntab; j++) {
      of << indent << pre << "tabflag=\"" << (wcs->tab+j)->flag << "\"";
      of << indent << pre << "tabM=\"" << (wcs->tab+j)->M << "\"";
      int N = (wcs->tab+j)->M;
      for (i = 0; i < (wcs->tab+j)->M; i++) {
        of << indent << pre << "tabK"<<i<<"=\"" << (wcs->tab+j)->K[i] << "\"";
        N *= (wcs->tab+j)->K[i];
      }
      for (i = 0; i < (wcs->tab+j)->M; i++) {
        of << indent << pre << "tabmap"<<i<<"=\"" << (wcs->tab+j)->map[i] << "\"";
      }
      for (i = 0; i < (wcs->tab+j)->M; i++) {
        of << indent << pre << "tabcrval"<<i<<"=\"" << (wcs->tab+j)->crval[i] << "\"";
      }
      for (i = 0; i < (wcs->tab+j)->M; i++) {
        if(((wcs->tab+j)->index[i])){
          for (k = 0; k < (wcs->tab+j)->K[i]; k++) {
            of << indent << pre << "tabindex"<<i<<k<<"=\"" << (wcs->tab+j)->index[i][k] << "\"";
          }
        }
      }

      for (i = 0; i < N; i++) {
        of << indent << pre << "tabcoord"<<i<<"=\"" << *((wcs->tab+j)->coord+i) << "\"";
      }
    }
  }*/

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

  this->wcs->flag=-1;
  if ((this->WcsStatus = wcscopy(1, astroVolumeNode->wcs, this->wcs))) {
    vtkErrorMacro("wcscopy ERROR "<<this->WcsStatus<<": "<<wcshdr_errmsg[this->WcsStatus]<<"\n");
  }

  if ((this->WcsStatus = wcsset(this->wcs))) {
    vtkErrorMacro("wcsset ERROR "<<this->WcsStatus<<": "<<wcshdr_errmsg[this->WcsStatus]<<"\n");
  }

}

void vtkMRMLAstroVolumeNode::SetWcsStruct(struct wcsprm* wcstemp)
{

  if(wcstemp){
    wcs->flag=-1;
    if ((WcsStatus = wcscopy(1, wcstemp, wcs))) {
      vtkErrorMacro("wcscopy ERROR "<<WcsStatus<<": "<<wcshdr_errmsg[WcsStatus]<<"\n");
    }

    if ((WcsStatus = wcsset(wcs))) {
      vtkErrorMacro("wcsset ERROR "<<WcsStatus<<": "<<wcshdr_errmsg[WcsStatus]<<"\n");
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

    os << indent << pre << "flag:   " << wcs->flag <<std::endl;
    os << indent << pre << "naxis:   " << wcs->naxis <<std::endl;
    for (i = 0; i < wcs->naxis; i++) {
      os << indent << pre << "crpix"<<i<<":   " << wcs->crpix[i] <<std::endl;
    }

    // Linear transformation.
    k = 0;
    for (i = 0; i < wcs->naxis; i++) {
      for (j = 0; j < wcs->naxis; j++) {
        os << indent << pre << "pc"<<k<<":   " << wcs->pc[k] <<std::endl;
        k++;
      }
    }

    // Coordinate increment at reference point.
    for (i = 0; i < wcs->naxis; i++) {
      os << indent << pre << "cdelt"<<i<<":   " << wcs->cdelt[i] <<std::endl;
    }

    // Coordinate value at reference point.
    for (i = 0; i < wcs->naxis; i++) {
      os << indent << pre << "crval"<<i<<":   " << wcs->crval[i] <<std::endl;
    }

    // Coordinate units and type.
    for (i = 0; i < wcs->naxis; i++) {
      os << indent << pre << "cunit"<<i<<":   " << wcs->cunit[i] <<std::endl;
    }

    for (i = 0; i < wcs->naxis; i++) {
      os << indent << pre << "ctype"<<i<<":   " << wcs->ctype[i] <<std::endl;
    }

    // Celestial and spectral transformation parameters.
    os << indent << pre << "lonpole:   " << wcs->lonpole <<std::endl;
    os << indent << pre << "latpole:   " << wcs->latpole <<std::endl;
    os << indent << pre << "restfrq:   " << wcs->restfrq <<std::endl;
    os << indent << pre << "restwav:   " << wcs->restwav <<std::endl;
    os << indent << pre << "npv:   " << wcs->npv <<std::endl;
    os << indent << pre << "npvmax:   " << wcs->npvmax <<std::endl;

    for (i = 0; i < wcs->npv; i++) {
        os << indent << pre << "pvi"<<i<<":   " << (wcs->pv[i]).i <<std::endl;
        os << indent << pre << "pvvalue"<<i<<":   " << (wcs->pv[i]).value <<std::endl;
    }

    os << indent << pre << "nps:   " << wcs->nps <<std::endl;
    os << indent << pre << "npsmax:   " << wcs->npsmax <<std::endl;

    for (i = 0; i < wcs->nps; i++) {
        os << indent << pre << "pvi"<<i<<":   " << (wcs->ps[i]).i <<std::endl;
        os << indent << pre << "pvvalue"<<i<<":   " << (wcs->ps[i]).value <<std::endl;
    }

    // Alternate linear transformations.
    k = 0;
    if (wcs->cd) {
      for (i = 0; i < wcs->naxis; i++) {
        for (j = 0; j < wcs->naxis; j++) {
          os << indent << pre << "cd"<<k<<":   " << wcs->cd[k] <<std::endl;
          k++;
        }
      }
    }

    if (wcs->crota) {
      for (i = 0; i < wcs->naxis; i++) {
        os << indent << pre << "crota"<<i<<":   " << wcs->crota[i] <<std::endl;
      }
    }

    os << indent << pre << "altlin:   " << wcs->altlin <<std::endl;
    os << indent << pre << "velref:   " << wcs->velref <<std::endl;
    os << indent << pre << "alt:   " << wcs->alt <<std::endl;
    os << indent << pre << "colnum:   " << wcs->colnum <<std::endl;

    if (wcs->colax) {
      for (i = 0; i < wcs->naxis; i++) {
        os << indent << pre << "colax"<<i<<":   " << wcs->colax[i] <<std::endl;
      }
    }

    if (wcs->wcsname[0] == '\0') {
      os << indent << pre << "wcsname:   " << und <<std::endl;
    } else {
      os << indent << pre << "wcsname:   " << wcs->wcsname <<std::endl;
    }

    if (wcs->cname) {
      for (i = 0; i < wcs->naxis; i++) {
        if (wcs->cname[i][0] == '\0') {
          os << indent << pre << "cname"<<i<<":   " << und <<std::endl;
        } else {
          os << indent << pre << "cname"<<i<<":   " << wcs->cname[i] <<std::endl;
        }
      }
    }

    if (wcs->crder) {
      for (i = 0; i < wcs->naxis; i++) {
        if (undefined(wcs->crder[i])) {
          os << indent << pre << "crder"<<i<<":   " << und <<std::endl;
        } else{
          os << indent << pre << "crder"<<i<<":   " << wcs->crder[i] <<std::endl;
        }
      }
    }

    if (wcs->csyer) {
      for (i = 0; i < wcs->naxis; i++) {
        if (undefined(wcs->csyer[i])) {
          os << indent << pre << "csyer"<<i<<":   " << und <<std::endl;
        } else{
          os << indent << pre << "csyer"<<i<<":   " << wcs->csyer[i] <<std::endl;
        }
      }
    }

    if (wcs->radesys[0] == '\0') {
      os << indent << pre << "radesys:   " << und <<std::endl;
    } else {
      os << indent << pre << "radesys:   " << wcs->radesys <<std::endl;
    }

    if (undefined(wcs->equinox)) {
      os << indent << pre << "equinox:   " << und <<std::endl;
    } else {
      os << indent << pre << "equinox:   " << wcs->equinox <<std::endl;
    }

    if (wcs->specsys[0] == '\0') {
      os << indent << pre << "specsys:   " << und <<std::endl;
    } else {
      os << indent << pre << "specsys:   " << wcs->specsys <<std::endl;
    }

    if (wcs->ssysobs[0] == '\0') {
      os << indent << pre << "ssysobs:   " << und <<std::endl;
    } else {
      os << indent << pre << "ssysobs:   " << wcs->ssysobs <<std::endl;
    }

    if (undefined(wcs->velosys)) {
      os << indent << pre << "velosys:   " << und <<std::endl;
    } else {
      os << indent << pre << "velosys:   " << wcs->velosys <<std::endl;
    }

    if (wcs->ssyssrc[0] == '\0') {
      os << indent << pre << "ssyssrc:   " << und <<std::endl;
    } else {
      os << indent << pre << "ssyssrc:   " << wcs->ssyssrc <<std::endl;
    }

    if (undefined(wcs->zsource)) {
      os << indent << pre << "zsource:   " << und <<std::endl;
    } else {
      os << indent << pre << "zsource:   " << wcs->zsource <<std::endl;
    }

    for (i = 0; i < 3; i++) {
      if (undefined(wcs->obsgeo[i])) {
        os << indent << pre << "obsgeo"<<i<<":   " << und <<std::endl;
      } else {
        os << indent << pre << "obsgeo"<<i<<":   " << wcs->obsgeo[i] <<std::endl;
      }

    }

    if (wcs->dateobs[0] == '\0') {
      os << indent << pre << "dateobs:   " << und <<std::endl;
    } else {
      os << indent << pre << "dateobs:   " << wcs->dateobs <<std::endl;
    }

    if (wcs->dateavg[0] == '\0') {
      os << indent << pre << "dateavg:   " << und <<std::endl;
    } else {
      os << indent << pre << "dateavg:   " << wcs->dateavg <<std::endl;
    }

    if (undefined(wcs->mjdobs)) {
      os << indent << pre << "mjdobs:   " << und <<std::endl;
    } else {
      os << indent << pre << "mjdobs:   " << wcs->mjdobs <<std::endl;
    }

    if (undefined(wcs->mjdavg)) {
      os << indent << pre << "mjdavg:   " << und <<std::endl;
    } else {
      os << indent << pre << "mjdavg:   " << wcs->mjdavg <<std::endl;
    }

    if (wcs->tab) {
      os << indent << pre << "ntab:   " << wcs->ntab <<std::endl;
      for (j = 0; j < wcs->ntab; j++) {
        os << indent << pre << "tabflag:   " << (wcs->tab+j)->flag <<std::endl;
        os << indent << pre << "tabM:   " << (wcs->tab+j)->M <<std::endl;
        int N = (wcs->tab+j)->M;
        for (i = 0; i < (wcs->tab+j)->M; i++) {
          os << indent << pre << "tabK"<<i<<":   " << (wcs->tab+j)->K[i] <<std::endl;
          N *= (wcs->tab+j)->K[i];
        }
        for (i = 0; i < (wcs->tab+j)->M; i++) {
          os << indent << pre << "tabmap"<<i<<":   " << (wcs->tab+j)->map[i] <<std::endl;
        }
        for (i = 0; i < (wcs->tab+j)->M; i++) {
          os << indent << pre << "tabcrval"<<i<<":   " << (wcs->tab+j)->crval[i] <<std::endl;
        }
        for (i = 0; i < (wcs->tab+j)->M; i++) {
          if(((wcs->tab+j)->index[i])){
            for (k = 0; k < (wcs->tab+j)->K[i]; k++) {
              os << indent << pre << "tabindex"<<i<<k<<":   " << (wcs->tab+j)->index[i][k] <<std::endl;
            }
          }
        }

        for (i = 0; i < N; i++) {
          os << indent << pre << "tabcoord"<<i<<":   " << *((wcs->tab+j)->coord+i) <<std::endl;
        }
      }
    }
}


vtkMRMLAstroVolumeDisplayNode* vtkMRMLAstroVolumeNode::GetAstroVolumeDisplayNode()
{
  return vtkMRMLAstroVolumeDisplayNode::SafeDownCast(this->GetDisplayNode());
}

vtkMRMLStorageNode* vtkMRMLAstroVolumeNode::CreateDefaultStorageNode()
{
  return vtkMRMLAstroVolumeStorageNode::New();
}

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
