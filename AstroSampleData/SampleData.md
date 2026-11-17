# SlicerAstro sample data (release SampleData-v2)

Sample data of SlicerAstro 2 (downloaded by the Astronomy category of the Sample Data module, checked with the SHA256 checksums of `SHA256SUMS`).

| File | Content |
|---|---|
| `WEIN069.fits` | HI data cube of the galaxy WEIN069 (Weinberger et al. 1995), a companion, a tidal tail and a faint filament between them: a sub-cube of the WSRT Zone of Avoidance Perseus-Pisces Filament survey (Westerbork Synthesis Radio Telescope; Ramatsoku et al. 2016, MNRAS, 460, 923), distributed with SlicerAstro since version 1. |
| `WEIN069_mask.fits` | Source mask of `WEIN069.fits` created by SoFiA 2.7.1 (32-bit source IDs; loaded by SlicerAstro as a segmentation). |
| `WEIN069_cat.txt`, `WEIN069_cat.xml` | SoFiA 2.7.1 source catalogue of `WEIN069.fits` (plain text and VOTable; loaded by SlicerAstro as a table and labelled source points). |
| `WEIN069_sofia.par` | SoFiA 2.7.1 parameters used for the mask and the catalogue: the defaults with mask output, mask dilation, and `filter.minSNR = 0` because `WEIN069.fits` has no beam information (BMAJ = BMIN = -1). |

Please cite the WSRT ZoA Perseus-Pisces Filament survey (Ramatsoku, M., Verheijen, M. A. W., Kraan-Korteweg, R. C., et al. 2016, MNRAS, 460, 923, arXiv:1605.02603) and SoFiA 2 (Westmeier et al. 2021, MNRAS 506, 3962) when using these data.
