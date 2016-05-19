# SlicerAstro
Astronomy (HI) extension for 3-D Slicer

Upcoming HI (neutral Hydrogen) surveys will deliver large datasets, and automated processing using the full 3-D information (two positional dimensions and one spectral dimension) to find and characterize HI objects is imperative. In this context, visualization is an essential tool for enabling qualitative and quantitative human control on an automated source finding and analysis pipeline. Visual Analytics, the combination of automated data processing and human reasoning, creativity and intuition, supported by interactive visualization, enables flexible and fast interaction with the 3-D data, helping the astronomer to deal with the analysis of complex sources. 3-D visualization, coupled to modeling, provides additional capabilities helping the discovery and analysis of subtle structures in the 3-D domain.

<h3> Objective: general description</h3>
* 1) proper visualization of astronomical data cubes: using data astronomical data formats, such as FITS, and astronomical world coordinates system (WCS);
* 2) generation of Histogram, flux density profiles, moment maps and position-velocity diagrams linked with the 3-D view;
* 3) enabling interactive smoothing in all three dimensions and multiscale analysis, such as wavelet lifting;
* 4) interactive HI data modeling coupled to visualization;
* 5) introduction of the SAMP protocol to enable interoperability with Topcat, and other VO tools and catalogs.
</div>

<h3> Progress (1):  </h3>
* FITS reader and AstroVolume done;
* WCS included in AstroVolume.
* Added AstroLabelMapVolume (WCS compatible).
* Generalization of qSlicerUnits.
* Slicer dataProbe moduile overrided with AstroDataProbe one.
* First desing of the AstroVolume interface done.
* Added WCS axis in the 2-D views. 
* Customization of 2-D and 3-D Views.
</div>

<h3> Progress (3):  </h3>
* module created;
* interface designed;
* Logic methods implemented on CPU (OpenMP);
* Logic methods implemented on GPU (OpenGL);
</div>

<h3> General NOTE  </h3>
* at the moment the SlicerAstro is not aware of the CELLSCAL keyword
* at the moment datacubes NAXIS>3 not supported (NAXIS=4 and NAXIS4=1 -> NAXIS=3)
* at the moment SlicerAstro doesn't "align" label and foreground volume to the background. This means that, at the moment, the users should take care to load datacubes with same gridding and and same WCS.
</div>


