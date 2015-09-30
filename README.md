# SlicerAstro
Astronomy (HI) extension for 3-D Slicer

Upcoming HI (neutral Hydrogen) surveys will deliver large datasets, and automated processing using the full 3-D information (two positional dimensions and one spectral dimension) to find and characterize HI objects is imperative. In this context, visualization is an essential tool for enabling qualitative and quantitative human control on an automated source finding and analysis pipeline. Visual Analytics, the combination of automated data processing and human reasoning, creativity and intuition, supported by interactive visualization, enables flexible and fast interaction with the 3-D data, helping the astronomer to deal with the analysis of complex sources. 3-D visualization, coupled to modeling, provides additional capabilities helping the discovery and analysis of subtle structures in the 3-D domain.

<h3> Objective: general description</h3>
* 1) proper visualization of astronomical data cubes: using data astronomical data formats, such as FITS, and astronomical world coordinates system (WCS);
* 2) generation of flux density profiles, moment maps and position-velocity diagrams linked with the 3-D view;
* 3) enabling interactive smoothing in all three dimensions and multiscale analysis, such as wavelet lifting;
* 4) interactive HI data modeling coupled to visualization;
* 5) interactive 3-D selection of HI sources;
* 6) introduction of the SAMP protocol to enable interoperability with Topcat, and other VO tools and catalogs.
</div>

<h3> Progress:  </h3>
* 1) FITS reader and AstroVolume done; WCS included in AstroVolume. 
* 1) Added AstroLabelMapVolume (WCS compatible).
* 1) Generalization of qSlicerUnits and the Slicer dataProbe modules done.
* 1) Working on SliceViewNode factorization.
</div>

<h3> To Do list before first alpha release  </h3>
* SliceViewNode factorization: coordinates and units on the slice widget and setting the names of the planes.
* factorization of 3-D view:  use W, S, N, etc. for the labels. (hint: ctkAxesWidgetPrivate -> AxesLabels)
* DataProbeLib: ruler -> make axes like kvis.
* design AstroVolume interface.
* enable clestial transformation and enable spectral transformation FREQ -> VRAD, etc.
* make GetReferenceSpace aware of CELLSCALL keyword.
</div>

