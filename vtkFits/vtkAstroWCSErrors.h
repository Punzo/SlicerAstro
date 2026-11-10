/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#ifndef __vtkAstroWCSErrors_h
#define __vtkAstroWCSErrors_h

// Error reporting helpers for WCSLIB (header only).
//
// The messages are meant for vtkErrorMacro/vtkWarningMacro, so that they are
// shown in the Slicer error log (not printed to the console).

// WCSLIB includes
#include <wcslib.h>

// STD includes
#include <sstream>
#include <string>

//----------------------------------------------------------------------------
/// Short description of a wcsprm status code (wcs_errmsg_enum in wcs.h).
/// The wcs_errmsg array of WCSLIB is not used because data symbols are not
/// exported by the WCSLIB DLL on Windows.
inline const char* vtkAstroWCSStatusDescription(int status)
{
  switch (status)
    {
    case 0: return "success";
    case 1: return "null wcsprm pointer passed";
    case 2: return "memory allocation failed";
    case 3: return "linear transformation matrix is singular";
    case 4: return "inconsistent or unrecognized coordinate axis type";
    case 5: return "invalid parameter value";
    case 6: return "unrecognized coordinate transformation parameter";
    case 7: return "ill-conditioned coordinate transformation parameter";
    case 8: return "one or more of the pixel coordinates were invalid";
    case 9: return "one or more of the world coordinates were invalid";
    case 10: return "invalid world coordinate";
    case 11: return "no solution found in the specified interval";
    case 12: return "invalid subimage specification";
    case 13: return "non-separable subimage coordinate system";
    case 14: return "wcsprm struct is unset, use wcsset()";
    default: return "unknown error";
    }
}

//----------------------------------------------------------------------------
/// True for the statuses of wcsp2s/wcss2p meaning that the coordinates are
/// outside the domain of the projection (e.g. a corner of a slice view beyond
/// the sky): this is not an error of the WCS.
inline bool vtkAstroWCSIsInvalidCoordinate(int status)
{
  return status == WCSERR_BAD_PIX || status == WCSERR_BAD_WORLD;
}

//----------------------------------------------------------------------------
/// One-line description of a failed WCSLIB call, with the detailed message
/// of WCSLIB when available, e.g.
/// "WCSLIB wcsset() failed (status 4: inconsistent or unrecognized coordinate
/// axis type): Unrecognized celestial type ... [cel.c:123 celset()]".
inline std::string vtkAstroWCSErrorMessage(const char* function, int status, const struct wcsprm* wcs)
{
  std::ostringstream os;
  os << "WCSLIB " << (function ? function : "") << "() failed (status " << status << ": "
     << vtkAstroWCSStatusDescription(status) << ")";
  const struct wcserr* err = wcs ? wcs->err : nullptr;
  if (err && err->msg && err->msg[0] != '\0')
    {
    os << ": " << err->msg;
    if (err->file || err->function)
      {
      os << " [" << (err->file ? err->file : "") << ":" << err->line_no
         << " " << (err->function ? err->function : "") << "()]";
      }
    }
  return os.str();
}

#endif
