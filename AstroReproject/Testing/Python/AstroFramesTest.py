import math

import slicer
import vtk
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)

#
# Reference values computed with astropy (FK5 precession at J1975: IAU 1976,
# as the FITS WCS paper II) by
# vtkAstroCoordinates/Testing/generate_frames_truth.py
#
# astropy 8.0.2
CELESTIAL = [
    {'ICRS': (10.6845684607, 41.2691685409), 'FK5': (10.68458, 41.26917), 'FK4': (10.0002564259, 40.9953438424), 'FK4_B1900': (9.3197057903, 40.720834351), 'FK4NoETerms': (10.0002588593, 40.995287575), 'Galactic': (121.174232434, -21.5728867156), 'Supergalactic': (336.193375367, 12.5522843102), 'FK5_J1975': (10.3420610165, 41.1323112519)},
    {'ICRS': (359.8999968278, -30.0000025374), 'FK5': (359.9, -30.0), 'FK4': (359.2581211273, -30.2784520325), 'FK4_B1900': (358.6149517245, -30.5568620253), 'FK4NoETerms': (359.2581436991, -30.2783983456), 'Galactic': (15.7214734845, -78.268810422), 'Supergalactic': (263.8630966199, 3.6837501734), 'FK5_J1975': (359.5793498469, -30.1391940457)},
    {'ICRS': (149.9999997414, 60.000004953), 'FK5': (150.0, 60.0), 'FK4': (149.1139436348, 60.2399584397), 'FK4_B1900': (148.219334083, 60.4778056943), 'FK4NoETerms': (149.1138156937, 60.2400236153), 'Galactic': (152.3300468025, 46.1441254153), 'Supergalactic': (47.7298525587, -5.6464840258), 'FK5_J1975': (149.5580700709, 60.1202775942)},
    {'ICRS': (266.4049920525, -28.9360053584), 'FK5': (266.405, -28.936), 'FK4': (265.6107443935, -28.9166126447), 'FK4_B1900': (264.8173956121, -28.8933802621), 'FK4NoETerms': (265.6108488871, -28.9166179659), 'Galactic': (0.0001488702, 8.70141e-05), 'Supergalactic': (185.786021463, 42.3104475196), 'FK5_J1975': (266.0079449749, -28.9267906391)},
    {'ICRS': (44.9993409234, 89.5000021213), 'FK5': (45.0, 89.5), 'FK4': (28.6946851993, 89.275198025), 'FK4_B1900': (20.2848461088, 89.0212079475), 'FK4NoETerms': (28.692418463, 89.2751076507), 'Galactic': (123.2296751827, 26.7045645757), 'Supergalactic': (26.00683343, 15.4491351551), 'FK5_J1975': (35.3697643883, 89.3933073795)},
]
FK4_PRECESSION = [
    ((10.68458, 41.26917), (10.0007600837, 40.9952961442)),
    ((359.9, -30.0), (359.2586448171, -30.2783899625)),
    ((150.0, 60.0), (149.1143245754, 60.2400166443)),
    ((266.405, -28.936), (265.6113535925, -28.9166194402)),
    ((45.0, 89.5), (28.6932029395, 89.2751154548)),
]
SPECTRAL_OBSERVATORY = (-1601185.4, -5041977.5, 3554875.9)
SPECTRAL_MJD = 59380.5
SPECTRAL = [
    {'ra': 10.68458, 'dec': 41.26917, 'TOPOCENT': 1420000000.0, 'GEOCENTR': 1419999221.344327, 'BARYCENT': 1419902890.8567622, 'LSRK': 1419882061.2926528, 'LSRD': 1419890378.9341106},
    {'ra': 150.0, 'dec': 60.0, 'TOPOCENT': 1420000000.0, 'GEOCENTR': 1419999904.2435687, 'BARYCENT': 1420068028.5277934, 'LSRK': 1420048102.744392, 'LSRD': 1420051989.6586463},
    {'ra': 266.405, 'dec': -28.936, 'TOPOCENT': 1420000000.0, 'GEOCENTR': 1420001499.0885897, 'BARYCENT': 1419994657.9520025, 'LSRK': 1419946010.920601, 'LSRD': 1419952029.056523},
]

SPEED_OF_LIGHT = 299792458.0


class AstroFramesTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Frames Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = []
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Test of the celestial (ICRS, FK5, FK4, Galactic, Supergalactic) and spectral
(topocentric, geocentric, barycentric, LSRK, LSRD) reference frames of
vtkAstroFrames against astropy.
"""
        self.parent.hidden = True


class AstroFramesTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroFramesTestLogic(ScriptedLoadableModuleLogic):
    pass


def separationMilliarcsec(a, b):
    """Angular distance (mas) of two (longitude, latitude) positions in degrees."""
    la, ba, lb, bb = map(math.radians, (a[0], a[1], b[0], b[1]))
    x = math.cos(bb) * math.sin(lb - la)
    y = math.cos(ba) * math.sin(bb) - math.sin(ba) * math.cos(bb) * math.cos(lb - la)
    z = math.sin(ba) * math.sin(bb) + math.cos(ba) * math.cos(bb) * math.cos(lb - la)
    return math.degrees(math.atan2(math.hypot(x, y), z)) * 3.6e6


class AstroFramesTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        from vtkAstroCoordinatesPython import vtkAstroFrames
        self.frames = vtkAstroFrames

    def runTest(self):
        self.setUp()
        self.test_FramesFromHeaders()
        self.test_Celestial()
        self.test_FK4Precession()
        self.test_Spectral()

    def test_FramesFromHeaders(self):
        F = self.frames
        equinox = vtk.reference(0.)
        self.assertEqual(F.GetCelestialFrame("RA---SIN", "", 0., equinox), F.ICRS)
        self.assertEqual(F.GetCelestialFrame("RA---SIN", "", 2000., equinox), F.FK5)
        self.assertEqual(float(equinox), 2000.)
        self.assertEqual(F.GetCelestialFrame("RA---TAN", "", 1950., equinox), F.FK4)
        self.assertEqual(float(equinox), 1950.)
        self.assertEqual(F.GetCelestialFrame("RA---TAN", "FK5", 0., equinox), F.FK5)
        self.assertEqual(float(equinox), 2000.)
        self.assertEqual(F.GetCelestialFrame("RA---TAN", "FK4-NO-E", 0., equinox), F.FK4NoETerms)
        self.assertEqual(F.GetCelestialFrame("RA---TAN", "ICRS ", 2000., equinox), F.ICRS)
        self.assertEqual(F.GetCelestialFrame("GLON-CAR", "", 0., equinox), F.Galactic)
        self.assertEqual(F.GetCelestialFrame("SLON-CAR", "", 0., equinox), F.Supergalactic)
        self.assertEqual(F.GetCelestialFrame("ELON-CAR", "", 0., equinox), F.CelestialUnknown)
        for name, frame in (("TOPOCENT", F.Topocentric), ("GEOCENTR", F.Geocentric), ("BARYCENT", F.Barycentric),
                            ("HELIOCEN", F.Barycentric), ("LSRK", F.LSRK), ("lsrd", F.LSRD), ("GALACTOC", F.Galactocentric),
                            ("LOCALGRP", F.LocalGroup), ("CMBDIPOL", F.CMB), ("SOURCE", F.SpectralUnknown)):
            self.assertEqual(F.GetSpectralFrame(name), frame, name)

    def test_Celestial(self):
        """Every frame to every frame, against astropy: 0.1 mas (10 mas when
        FK4 is involved: ERFA fk45z/fk54z vs the astropy FK4 matrices)."""
        F = self.frames
        frames = {"ICRS": (F.ICRS, 0.), "FK5": (F.FK5, 2000.), "FK5_J1975": (F.FK5, 1975.), "FK4": (F.FK4, 1950.),
                  "FK4_B1900": (F.FK4, 1900.), "FK4NoETerms": (F.FK4NoETerms, 1950.), "Galactic": (F.Galactic, 0.),
                  "Supergalactic": (F.Supergalactic, 0.)}
        for row in CELESTIAL:
            for source, (fromFrame, fromEquinox) in frames.items():
                for target, (toFrame, toEquinox) in frames.items():
                    if source.startswith("FK4") and target.startswith("FK4"):
                        # different epochs of observation in the reference values: test_FK4Precession
                        continue
                    result = [0., 0.]
                    self.assertTrue(self.frames.ConvertCelestial(row[source][0], row[source][1], fromFrame, fromEquinox,
                                                                 toFrame, toEquinox, 0., result))
                    tolerance = 10. if "FK4" in source + target else 0.1
                    error = separationMilliarcsec(result, row[target])
                    self.assertLess(error, tolerance, f"{source} {row[source]} -> {target}: {error:.3f} mas")

    def test_FK4Precession(self):
        """FK4 B1950 -> B1900: Newcomb precession with the E-terms of each equinox."""
        for b1950, b1900 in FK4_PRECESSION:
            result = [0., 0.]
            self.assertTrue(self.frames.ConvertCelestial(b1950[0], b1950[1], self.frames.FK4, 1950.,
                                                         self.frames.FK4, 1900., 0., result))
            error = separationMilliarcsec(result, b1900)
            self.assertLess(error, 0.5, f"FK4 {b1950}: {error:.3f} mas")

    def test_Spectral(self):
        """Topocentric frequencies to the other frames: 0.1 m/s (velocities of
        the frames up to 25 km/s)."""
        F = self.frames
        for row in SPECTRAL:
            for name in ("GEOCENTR", "BARYCENT", "LSRK", "LSRD"):
                for fromName, toName in (("TOPOCENT", name), (name, "TOPOCENT")):
                    result = vtk.reference(0.)
                    self.assertTrue(F.ConvertFrequency(row[fromName], F.GetSpectralFrame(fromName), F.GetSpectralFrame(toName),
                                                       row["ra"], row["dec"], SPECTRAL_MJD, SPECTRAL_OBSERVATORY, result))
                    errorVelocity = (float(result) - row[toName]) / row[toName] * SPEED_OF_LIGHT
                    self.assertLess(abs(errorVelocity), 0.1, f"{fromName} -> {toName} at {row['ra']}: {errorVelocity:.3f} m/s")
        # galactocentric: LSRD, then 220 km/s towards (l, b) = (90, 0) deg
        for ra, dec in ((10.68458, 41.26917), (266.405, -28.936)):
            lsrd = vtk.reference(0.)
            galactocentric = vtk.reference(0.)
            self.assertTrue(F.ConvertFrequency(1.42e9, F.Barycentric, F.LSRD, ra, dec, 0., (0., 0., 0.), lsrd))
            self.assertTrue(F.ConvertFrequency(1.42e9, F.Barycentric, F.Galactocentric, ra, dec, 0., (0., 0., 0.), galactocentric))
            galactic = [0., 0.]
            F.ConvertCelestial(ra, dec, F.ICRS, 0., F.Galactic, 0., 0., galactic)
            l, b = map(math.radians, galactic)
            projected = 220.e3 * math.cos(b) * math.sin(l)  # (90, 0) . source direction
            beta = -projected / SPEED_OF_LIGHT
            expected = float(lsrd) * math.sqrt((1. + beta) / (1. - beta))
            self.assertAlmostEqual(float(galactocentric) / expected, 1., delta=1.e-9)
        # geocentric/topocentric frames need the time and the observatory
        result = vtk.reference(0.)
        self.assertFalse(F.ConvertFrequency(1.42e9, F.Topocentric, F.Barycentric, 10., 10., 0., (0., 0., 0.), result))
        self.delayDisplay("Test passed")
