// Smoke test of the third-party libraries of SlicerAstro: CFITSIO writes and
// reads a small HI cube, WCSLIB converts its coordinates (frequency to radio
// velocity), FFTW transforms a delta function, BBarolo reads the header.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <fitsio.h>
#include <fftw3.h>
#include <wcslib/wcslib.h>

#include <Arrays/header.hh>

namespace
{
int failures = 0;

void check(bool condition, const std::string& what)
{
  std::printf("%s: %s\n", condition ? "ok" : "FAILED", what.c_str());
  if (!condition)
    {
    failures++;
    }
}

const double restFrequency = 1.42040575177e9;
const double speedOfLight = 299792458.0;

bool writeCube(const char* path, std::vector<float>& data)
{
  std::remove(path);
  fitsfile* file = nullptr;
  int status = 0;
  long naxes[3] = { 8, 6, 4 };
  fits_create_file(&file, path, &status);
  fits_create_img(file, FLOAT_IMG, 3, naxes, &status);
  char ctype1[] = "RA---SIN", ctype2[] = "DEC--SIN", ctype3[] = "FREQ", bunit[] = "JY/BEAM";
  double crval1 = 180., cdelt1 = -0.001, crpix1 = 4.;
  double crval2 = 30., cdelt2 = 0.001, crpix2 = 3.;
  double crval3 = 1.42e9, cdelt3 = -2.e5, crpix3 = 1.;
  double bmaj = 0.004, bmin = 0.003, bpa = 0., restfrq = restFrequency;
  fits_write_key(file, TSTRING, "CTYPE1", ctype1, nullptr, &status);
  fits_write_key(file, TDOUBLE, "CRVAL1", &crval1, nullptr, &status);
  fits_write_key(file, TDOUBLE, "CDELT1", &cdelt1, nullptr, &status);
  fits_write_key(file, TDOUBLE, "CRPIX1", &crpix1, nullptr, &status);
  fits_write_key(file, TSTRING, "CTYPE2", ctype2, nullptr, &status);
  fits_write_key(file, TDOUBLE, "CRVAL2", &crval2, nullptr, &status);
  fits_write_key(file, TDOUBLE, "CDELT2", &cdelt2, nullptr, &status);
  fits_write_key(file, TDOUBLE, "CRPIX2", &crpix2, nullptr, &status);
  fits_write_key(file, TSTRING, "CTYPE3", ctype3, nullptr, &status);
  fits_write_key(file, TDOUBLE, "CRVAL3", &crval3, nullptr, &status);
  fits_write_key(file, TDOUBLE, "CDELT3", &cdelt3, nullptr, &status);
  fits_write_key(file, TDOUBLE, "CRPIX3", &crpix3, nullptr, &status);
  fits_write_key(file, TDOUBLE, "RESTFRQ", &restfrq, nullptr, &status);
  fits_write_key(file, TSTRING, "BUNIT", bunit, nullptr, &status);
  fits_write_key(file, TDOUBLE, "BMAJ", &bmaj, nullptr, &status);
  fits_write_key(file, TDOUBLE, "BMIN", &bmin, nullptr, &status);
  fits_write_key(file, TDOUBLE, "BPA", &bpa, nullptr, &status);
  data.resize(8 * 6 * 4);
  for (size_t i = 0; i < data.size(); ++i)
    {
    data[i] = static_cast<float>(i) * 0.5f;
    }
  fits_write_img(file, TFLOAT, 1, static_cast<LONGLONG>(data.size()), data.data(), &status);
  fits_close_file(file, &status);
  return status == 0;
}
}

int main(int argc, char* argv[])
{
  const char* path = argc > 1 ? argv[1] : "smoke.fits";
  float version = 0.f;
  std::printf("CFITSIO %.3f, WCSLIB %s, %s\n", fits_get_version(&version), wcslib_version(nullptr), fftw_version);

  // CFITSIO
  std::vector<float> data;
  check(writeCube(path, data), "CFITSIO writes a cube");
  fitsfile* file = nullptr;
  int status = 0;
  fits_open_data(&file, path, READONLY, &status);
  std::vector<float> read(data.size(), 0.f);
  float nullValue = NAN;
  int anyNull = 0;
  fits_read_img(file, TFLOAT, 1, static_cast<LONGLONG>(read.size()), &nullValue, read.data(), &anyNull, &status);
  check(status == 0 && read == data, "CFITSIO reads the cube");

  // WCSLIB: header -> wcsprm, frequency -> radio velocity
  char* header = nullptr;
  int nkeyrec = 0;
  fits_hdr2str(file, 1, nullptr, 0, &header, &nkeyrec, &status);
  fits_close_file(file, &status);
  int nreject = 0, nwcs = 0;
  struct wcsprm* wcs = nullptr;
  check(header && wcspih(header, nkeyrec, WCSHDR_all, 0, &nreject, &nwcs, &wcs) == 0 && nwcs > 0, "WCSLIB parses the header");
  fits_free_memory(header, &status);
  if (nwcs > 0)
    {
    int index = -1;
    char ctype[9] = "VRAD";
    check(wcsset(wcs) == 0 && wcssptr(wcs, &index, ctype) == 0 && wcsset(wcs) == 0, "WCSLIB converts FREQ to VRAD");
    double pixel[3] = { 4., 3., 2. }, imgcrd[3], phi, theta, world[3];
    int stat = 0;
    check(wcsp2s(wcs, 1, 3, pixel, imgcrd, &phi, &theta, world, &stat) == 0, "WCSLIB pixel to world");
    const double frequency = 1.42e9 - 2.e5;
    const double velocity = speedOfLight * (1. - frequency / restFrequency);
    check(std::fabs(world[0] - 180.) < 1.e-9 && std::fabs(world[1] - 30.) < 1.e-9 &&
          std::fabs(world[2] - velocity) < 1.e-3, "WCSLIB world coordinates");
    wcsvfree(&nwcs, &wcs);
    }

  // FFTW: the transform of a delta function is constant
  const int n = 16;
  std::vector<double> in(n, 0.);
  in[0] = 1.;
  fftw_complex* out = fftw_alloc_complex(n / 2 + 1);
  fftw_plan plan = fftw_plan_dft_r2c_1d(n, in.data(), out, FFTW_ESTIMATE);
  fftw_execute(plan);
  bool constant = true;
  for (int i = 0; i <= n / 2; ++i)
    {
    constant = constant && std::fabs(out[i][0] - 1.) < 1.e-12 && std::fabs(out[i][1]) < 1.e-12;
    }
  check(constant, "FFTW transform");
  fftw_destroy_plan(plan);
  fftw_free(out);

  // BBarolo reads the header
  try
    {
    Header bbHeader(path);
    check(bbHeader.DimAx(0) == 8 && bbHeader.DimAx(1) == 6 && bbHeader.DimAx(2) == 4, "BBarolo reads the header");
    }
  catch (const std::exception& error)
    {
    check(false, std::string("BBarolo reads the header: ") + error.what());
    }

  std::remove(path);
  std::printf("%d failure(s)\n", failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
