///  vtkFitsWin32Header - manage Windows system differences
///
/// The vtkFitsWin32Header captures some system differences between Unix
/// and Windows operating systems.


#ifndef __vtkFitsWin32Header_h
#define __vtkFitsWin32Header_h

#include <vtkFitsConfigure.h>

#if defined(WIN32) && !defined(VTKFits_STATIC)
#if defined(FITS_EXPORTS)
#define VTK_FITS_EXPORT __declspec( dllexport )
#else
#define VTK_FITS_EXPORT __declspec( dllimport )
#endif
#else
#define VTK_FITS_EXPORT
#endif

#endif
