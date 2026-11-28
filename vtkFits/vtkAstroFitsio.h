/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#ifndef __vtkAstroFitsio_h
#define __vtkAstroFitsio_h

// fitsio.h, to include instead of it.
//
// CFITSIO defines its data type codes as macros (e.g. "#define TBYTE 11"),
// while winnt.h declares the type TBYTE: windows.h included after fitsio.h
// does not compile ("typedef ... 11"). On Windows windows.h is included
// first, so that a later inclusion is skipped by its include guard.

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// CFITSIO includes
#include <fitsio.h>

#endif
