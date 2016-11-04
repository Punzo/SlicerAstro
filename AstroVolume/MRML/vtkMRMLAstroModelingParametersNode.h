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

#ifndef __vtkMRMLAstroModelingParametersNode_h
#define __vtkMRMLAstroModelingParametersNode_h

#include "vtkMRML.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLNode.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkDoubleArray;

/// \ingroup Slicer_QtModules_AstroModeling
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroModelingParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroModelingParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroModelingParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual vtkMRMLNode* CreateNodeInstance();

  // Description:
  // Set node attributes
  virtual void ReadXMLAttributes( const char** atts);

  // Description:
  // Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent);

  // Description:
  // Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node);

  // Description:
  // Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() {return "AstroModelingParameters";};

  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  vtkSetStringMacro(OutputVolumeNodeID);
  vtkGetStringMacro(OutputVolumeNodeID);

  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

  vtkSetMacro(Status,int);
  vtkGetMacro(Status,int);

  virtual int* GetStatusPointer() {return &Status;};

  vtkSetMacro(FitSuccess,bool);
  vtkGetMacro(FitSuccess,bool);

protected:
  vtkMRMLAstroModelingParametersNode();
  ~vtkMRMLAstroModelingParametersNode();

  vtkMRMLAstroModelingParametersNode(const vtkMRMLAstroModelingParametersNode&);
  void operator=(const vtkMRMLAstroModelingParametersNode&);

  char *InputVolumeNodeID;
  char *OutputVolumeNodeID;
  int OutputSerial;

  int Status;

  bool FitSuccess;
};

#endif

