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

// Logic includes
#include "vtkSlicerAstroVolumeLogic.h"
#include "vtkSlicerAstroModelingLogic.h"
#include "vtkSlicerAstroConfigure.h"

//Bbarolo includes
#include "param.hh"
#include "cube.hh"
#include "stats.hh"
#include "moment.hh"
#include "ringmodel.hh"
#include "smooth3D.hh"
#include "galfit.hh"
#include "spacepar.hh"
#include "utils.hh"
#include "ellprof.hh"

// MRML includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroModelingParametersNode.h>

// VTK includes
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
#include <vtkVersion.h>

// STD includes
#include <cassert>
#include <iostream>

// Qt includes
#include <QtDebug>

#include <iostream>
#include <sys/time.h>

//----------------------------------------------------------------------------
class vtkSlicerAstroModelingLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkImageData> tempVolumeData;
};

//----------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = vtkSmartPointer<vtkSlicerAstroVolumeLogic>::New();
  this->tempVolumeData = vtkSmartPointer<vtkImageData>::New();
}

//---------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::vtkInternal::~vtkInternal()
{
}

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroModelingLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::vtkSlicerAstroModelingLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::~vtkSlicerAstroModelingLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerAstroModelingLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic;
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
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

}// end namespace

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerAstroModelingLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  vtkMRMLAstroModelingParametersNode* pNode = vtkMRMLAstroModelingParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

//----------------------------------------------------------------------------
int vtkSlicerAstroModelingLogic::Apply(vtkMRMLAstroModelingParametersNode* pnode)
{
  int success = 0;
  pnode->SetStatus(1);
/* now I have to modify this:
 * make it working without mask
 * give parameters hard coded
 * then give parameters from interface
 * then give mask from segmentationNode
 * look for the best layout for the output
  Param *par = new Param;

  if (!par->getopts(argc, argv)) return EXIT_FAILURE;
  if (par->getImageList()=="NONE") par->setImage(par->getImageFile());
  std::cout << *par;

  for (int im=0; im<par->getListSize(); im++) {

      if (par->getListSize()>1) {
          std::cout << setfill('_') << std::endl;
          std::cout << setw(70) << "" << std::endl << std::endl;
          std::string s = "Working on "+ par->getImage(im)+" ";
          std::cout << setfill(' ') << right << setw(70) << s;
          std::cout << std::endl << left;
          std::cout << std::endl << " File "<< im+1
                    << " of " << par->getListSize()<<std::endl<<std::endl;
      }

      par->setImageFile(par->getImage(im));
      std::string fname = par->getImage(im);
      int found = fname.find("[");
      if (found>=0) fname.erase(found, fname.size()-found);
      if (!fexists(fname)) {
          std::cout << "\nError reading " << par->getImage(im)
                    << " : the file doesn't exist!\n";
          if(par->getListSize()-im>1) std::cout << "Skipping to next file...\n";
          else {std::cout << "Exiting ...\n\n"; return EXIT_FAILURE;}
          continue;
      }

      Cube<float> *c = new Cube<float>;
      c->saveParam(*par);

      if (!c->readCube(par->getImageFile())) {
          std::cout << par->getImageFile() << " is not a readable FITS image!\n";
          if(par->getListSize()-im>1) std::cout << "Skipping to next file...\n";
          else std::cout << "Exiting ...\n\n";
          delete c;
          continue;
      }

      if (par->getCheckCh()) c->CheckChannels();

      if (par->getflagSmooth()) {
          Smooth3D<float> *sm = new Smooth3D<float>;
          sm->cubesmooth(c);
          sm->fitswrite();
          delete sm;
      }

      ///<<<<< Searching stuff
      if (par->getSearch()) {
          c->Search();
          c->plotDetections();
          std::ofstream detout((outfolder+"detections.txt").c_str());
          c->printDetections(detout);
      }

  if (par->getflagGalFit()) {
      Model::Galfit<float> *fit = new Model::Galfit<float>(c);
      fit->galfit();
      if (par->getTwoStage()) fit->SecondStage();
      if (par->getFlagDebug()) fit->writeModel("BOTH");
      else fit->writeModel(par->getNORM());


      delete fit;
  }
  */
  pnode->SetStatus(20);
  pnode->SetStatus(0);
  success = 1;
  return success;
}
