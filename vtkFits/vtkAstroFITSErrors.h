/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#ifndef __vtkAstroFITSErrors_h
#define __vtkAstroFITSErrors_h

// Error reporting helper for CFITSIO (header only).
//
// fits_report_error(stderr, status) prints to the console: this helper
// returns the same information (status text and the CFITSIO error message
// stack) for vtkErrorMacro, so that it is shown in the Slicer error log.

// CFITSIO includes
#include <fitsio.h>

// STD includes
#include <sstream>
#include <string>

//----------------------------------------------------------------------------
/// Description of a CFITSIO status, followed by the messages of the CFITSIO
/// error stack (the stack is emptied).
inline std::string vtkAstroFITSErrorMessage(int status)
{
  std::ostringstream os;
  char statusText[FLEN_STATUS] = {0};
  fits_get_errstatus(status, statusText);
  os << "CFITSIO error " << status << ": " << statusText;
  // CFITSIO splits long texts (e.g. file names) into several messages of
  // at most FLEN_ERRMSG - 1 characters: these pieces are joined again.
  char message[FLEN_ERRMSG] = {0};
  bool continuation = false;
  while (fits_read_errmsg(message))
    {
    std::string text(message);
    os << (continuation ? "" : " | ") << text;
    continuation = text.size() >= FLEN_ERRMSG - 1;
    }
  return os.str();
}

#endif
