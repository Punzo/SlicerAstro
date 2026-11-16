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

==============================================================================*/

#ifndef __vtkMRMLAstroTerminology_h
#define __vtkMRMLAstroTerminology_h

// STD includes
#include <string>
#include <vector>

/// \ingroup SlicerAstro_QtModules_AstroVolume
/// \brief Astronomical terminology of the segments (masks, sources, contours).
///
/// The terminology context is defined in
/// AstroVolume/Resources/Terminologies/SegmentationCategoryType-SlicerAstro.json
/// (loaded into the Terminologies module by the AstroVolume module); the
/// entries below are the ones SlicerAstro assigns itself, serialized as
/// vtkSlicerTerminologiesModuleLogic::SerializeTerminologyEntry does
/// (context~category~type~type modifier~region context~region~region modifier,
/// each code as scheme^value^meaning). AstroTerminologyTest checks that they
/// are in the context.
namespace vtkMRMLAstroTerminology
{
/// Name of the terminology context
inline std::string GetContextName()
{
  return "Segmentation category and type - SlicerAstro";
}

/// Coding scheme designator of the codes (private DICOM scheme: "99" prefix)
inline std::string GetCodingSchemeDesignator()
{
  return "99SLICERASTRO";
}

/// Serialized terminology entry of a category and type of the context
inline std::string GetEntry(const std::string& categoryCodeValue, const std::string& categoryCodeMeaning,
                            const std::string& typeCodeValue, const std::string& typeCodeMeaning)
{
  const std::string scheme = GetCodingSchemeDesignator();
  return GetContextName() + "~" +
         scheme + "^" + categoryCodeValue + "^" + categoryCodeMeaning + "~" +
         scheme + "^" + typeCodeValue + "^" + typeCodeMeaning + "~" +
         "^^~" + // no type modifier
         "~" +   // no region context
         "^^~" + // no region
         "^^";   // no region modifier
}

/// Detected source (e.g. the labels of a SoFiA mask)
inline std::string GetSourceEntry()
{
  return GetEntry("SA-C1", "Source detection", "SA-T101", "Source");
}

/// Mask (e.g. drawn by the user, or generated from the smoothed data)
inline std::string GetMaskEntry()
{
  return GetEntry("SA-C5", "Analysis region", "SA-T501", "Mask");
}

/// Contour of the data at a level
inline std::string GetContourEntry()
{
  return GetEntry("SA-C5", "Analysis region", "SA-T505", "Contour");
}

/// Model of the data (e.g. 3DBarolo's model and its mask)
inline std::string GetModelEntry()
{
  return GetEntry("SA-C5", "Analysis region", "SA-T506", "Model");
}

/// Splits a string at a separator (empty fields kept)
inline std::vector<std::string> Split(const std::string& text, char separator)
{
  std::vector<std::string> fields;
  std::string::size_type start = 0;
  std::string::size_type end = text.find(separator);
  while (end != std::string::npos)
    {
    fields.push_back(text.substr(start, end - start));
    start = end + 1;
    end = text.find(separator, start);
    }
  fields.push_back(text.substr(start));
  return fields;
}

/// Compact form of an entry of the context for a FITS header (at most 68
/// characters): "category code^category^type code^type". Empty for an entry
/// of another context (e.g. chosen by the user in a medical terminology).
inline std::string GetCompactEntry(const std::string& entry)
{
  const std::vector<std::string> parts = Split(entry, '~');
  if (parts.size() < 3 || parts[0] != GetContextName())
    {
    return std::string();
    }
  const std::vector<std::string> category = Split(parts[1], '^');
  const std::vector<std::string> type = Split(parts[2], '^');
  if (category.size() != 3 || type.size() != 3 ||
      category[0] != GetCodingSchemeDesignator() || type[0] != GetCodingSchemeDesignator())
    {
    return std::string();
    }
  return category[1] + "^" + category[2] + "^" + type[1] + "^" + type[2];
}

/// Entry of a compact form (GetCompactEntry); empty if it is not valid
inline std::string GetEntryFromCompact(const std::string& compactEntry)
{
  const std::vector<std::string> fields = Split(compactEntry, '^');
  if (fields.size() != 4 || fields[0].empty() || fields[2].empty())
    {
    return std::string();
    }
  return GetEntry(fields[0], fields[1], fields[2], fields[3]);
}
}

#endif
