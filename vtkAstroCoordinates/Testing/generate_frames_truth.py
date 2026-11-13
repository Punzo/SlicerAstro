"""Reference values (FRAMES_TRUTH in AstroFramesTest.py) of the celestial and spectral frame conversions, computed
with astropy (offline, not a runtime dependency). Output: Python literals for
AstroFramesTest.py.

Usage: python generate_frames_truth.py
"""
import math

import astropy
import erfa
import astropy.units as u
from astropy.coordinates import (FK4, FK5, ICRS, LSRD, LSRK, EarthLocation, FK4NoETerms, Galactic,
                                 SkyCoord, SpectralCoord, Supergalactic)
from astropy.time import Time
import warnings
warnings.simplefilter("ignore")

POSITIONS = [(10.68458, 41.26917), (359.9, -30.0), (150.0, 60.0), (266.405, -28.936), (45.0, 89.5)]

FRAMES = {
    "ICRS": lambda: ICRS(),
    "FK5": lambda: FK5(equinox="J2000"),
    "FK4": lambda: FK4(equinox="B1950"),
    "FK4_B1900": lambda: FK4(equinox="B1900"),
    "FK4NoETerms": lambda: FK4NoETerms(equinox="B1950"),
    "Galactic": lambda: Galactic(),
    "Supergalactic": lambda: Supergalactic(),
}

print("# astropy", astropy.__version__)
print("CELESTIAL = [")
for lon, lat in POSITIONS:
    source = SkyCoord(lon * u.deg, lat * u.deg, frame=FK5(equinox="J2000"))
    values = {}
    for name, frame in FRAMES.items():
        converted = source.transform_to(frame())
        values[name] = (round(float(converted.spherical.lon.deg), 10), round(float(converted.spherical.lat.deg), 10))
    # FK5 at J1975: precession IAU 1976 (FITS WCS paper II; astropy uses IAU 2006)
    date1, date2 = erfa.epj2jd(1975.0)
    vector = erfa.pmat76(date1, date2) @ erfa.s2c(math.radians(lon), math.radians(lat))
    ra1975, dec1975 = erfa.c2s(vector)
    values["FK5_J1975"] = (round(math.degrees(float(erfa.anp(ra1975))), 10), round(math.degrees(float(dec1975)), 10))
    print("    %r," % values)
print("]")

# FK4 precession (within FK4, with the E-terms of each equinox)
print("FK4_PRECESSION = [")
for lon, lat in POSITIONS:
    b1950 = SkyCoord(lon * u.deg, lat * u.deg, frame=FK4(equinox="B1950"))
    b1900 = b1950.transform_to(FK4(equinox="B1900"))
    print("    %r," % (((lon, lat), (round(float(b1900.ra.deg), 10), round(float(b1900.dec.deg), 10))),))
print("]")

# spectral frames: a 1.42 GHz signal received at the VLA
VLA = (-1601185.4, -5041977.5, 3554875.9)
TIME = "2021-06-15T12:00:00"
print("SPECTRAL_OBSERVATORY = %r" % (VLA,))
print("SPECTRAL_MJD = %r" % float(Time(TIME, scale="utc").mjd))
print("SPECTRAL = [")
location = EarthLocation.from_geocentric(*VLA, unit=u.m)
observer = location.get_itrs(obstime=Time(TIME, scale="utc"))
for ra, dec in [(10.68458, 41.26917), (150.0, 60.0), (266.405, -28.936)]:
    target = SkyCoord(ra * u.deg, dec * u.deg, frame="icrs")
    topocentric = SpectralCoord(1.42e9 * u.Hz, observer=observer, target=target)
    values = {"ra": ra, "dec": dec, "TOPOCENT": 1.42e9}
    for name, frame in (("GEOCENTR", "gcrs"), ("BARYCENT", "icrs"), ("LSRK", LSRK()), ("LSRD", LSRD())):
        values[name] = float(topocentric.with_observer_stationary_relative_to(frame).to_value(u.Hz))
    print("    %r," % values)
print("]")
