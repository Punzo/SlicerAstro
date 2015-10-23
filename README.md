# SlicerAstro
Astronomy (HI) extension for 3-D Slicer

Upcoming HI (neutral Hydrogen) surveys will deliver large datasets, and automated processing using the full 3-D information (two positional dimensions and one spectral dimension) to find and characterize HI objects is imperative. In this context, visualization is an essential tool for enabling qualitative and quantitative human control on an automated source finding and analysis pipeline. Visual Analytics, the combination of automated data processing and human reasoning, creativity and intuition, supported by interactive visualization, enables flexible and fast interaction with the 3-D data, helping the astronomer to deal with the analysis of complex sources. 3-D visualization, coupled to modeling, provides additional capabilities helping the discovery and analysis of subtle structures in the 3-D domain.

<h3> Objective: general description</h3>
* 1) proper visualization of astronomical data cubes: using data astronomical data formats, such as FITS, and astronomical world coordinates system (WCS);
* 2) generation of Histogram, flux density profiles, moment maps and position-velocity diagrams linked with the 3-D view;
* 3) enabling interactive smoothing in all three dimensions and multiscale analysis, such as wavelet lifting;
* 4) interactive HI data modeling coupled to visualization;
* 5) interactive 3-D selection of HI sources;
* 6) introduction of the SAMP protocol to enable interoperability with Topcat, and other VO tools and catalogs.
</div>

<h3> Progress:  </h3>
* 1) FITS reader and AstroVolume done;
* 1) WCS included in AstroVolume.
* 1) Added AstroLabelMapVolume (WCS compatible).
* 1) Generalization of qSlicerUnits.
* 1) Slicer dataProbe moduile overrided with AstroDataProbe one.
* 1) first desing of the AstroVolume interface done.
</div>

<h3> To Do list before first alpha release  </h3>
* SliceViewNode factorization: coordinates and units on the slice widget and setting the names of the planes.
* DataProbeLib: ruler -> make axes like kvis.
</div>

<h3> NOTE  </h3>
* Editor and LabelStatistics for the moment are not fully compatible with child class of ScalarVolume and LabelVolume. However let's wait the release of the segmentation class and editor from SlicerRT.
* at the moment the Slicer annotaions (ROI, fiducials and ruler) display the RAS coordinates (for SlicerAstro are the display coordinates).
* label of 3-D view:  use W, S, N, etc. for the labels. (hint: ctkAxesWidgetPrivate -> AxesLabels)
* at the moment the SLicerAstro is not aware of the CELLSCALL keyword
* WCSlib a fully integrated in SlicerAstro, but for the moment celestial and spectral trasformation are not implmented in the interface.
* at the moment datacubes NAXIS>3 not supported (NAXIS=4 and NAXIS4=1 -> NAXIS=3)
* at the moment SlicerAstro doesn't "align" label and foreground volume to the background. This means that, at the moment, the users should take care to load datacubes with same gridding and and same WCS.
</div>


