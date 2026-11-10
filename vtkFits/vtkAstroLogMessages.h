/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#ifndef __vtkAstroLogMessages_h
#define __vtkAstroLogMessages_h

// Logging helpers for messages that must reach the application log (header only).
//
// In Slicer the VTK output window is the error log: these helpers write one
// single-line message ("ClassName: message") to it, as information or as a
// warning. Unlike vtkWarningMacro they do not go through the observers of
// the object: Slicer observes storage nodes while reading and drops their
// warnings when the reading succeeds.

// VTK includes
#include <vtkObject.h>
#include <vtkOutputWindow.h>

// STD includes
#include <string>

enum class vtkAstroLogLevel
{
  Information,
  Warning
};

//----------------------------------------------------------------------------
/// Write a single-line message of \a object to the application log.
inline void vtkAstroLogMessage(vtkObject* object, vtkAstroLogLevel level, const std::string& message)
{
  if (!vtkObject::GetGlobalWarningDisplay())
    {
    return;
    }
  std::string text = object ? std::string(object->GetClassName()) + ": " : std::string();
  // Messages of the log are single-line: the Slicer error log drops what
  // follows the first line break.
  for (char c : message)
    {
    text += (c == '\n' || c == '\r') ? ' ' : c;
    }
  text += "\n";
  if (level == vtkAstroLogLevel::Warning)
    {
    vtkOutputWindowDisplayWarningText(text.c_str());
    }
  else
    {
    vtkOutputWindowDisplayText(text.c_str());
    }
}

#endif
