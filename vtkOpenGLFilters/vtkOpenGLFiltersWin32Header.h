///  vtkFitsWin32Header - manage Windows system differences
///
/// The vtkFitsWin32Header captures some system differences between Unix
/// and Windows operating systems.


#ifndef __vtkOpenGLFiltersWin32Header_h
#define __vtkOpenGLFiltersWin32Header_h

#include <vtkOpenGLFiltersConfigure.h>

#if defined(WIN32) && !defined(vtkOpenGLFilters_STATIC)
#if defined(FITS_EXPORTS)
#define VTK_OPENGLFILTERS_EXPORT __declspec( dllexport )
#else
#define VTK_OPENGLFILTERS_EXPORT __declspec( dllimport )
#endif
#else
#define VTK_OPENGLFILTERS_EXPORT
#endif

#endif
