.NOTPARALLEL:

profilesdir = $(datadir)/color/icc/colord
generatedprofiles =					\
	AdobeRGB1998.icc				\
	AppleRGB.icc					\
	BestRGB.icc					\
	BetaRGB.icc					\
	BruceRGB.icc					\
	CIE-RGB.icc					\
	ColorMatchRGB.icc				\
	DonRGB4.icc					\
	ECI-RGBv2.icc					\
	EktaSpacePS5.icc				\
	NTSC-RGB.icc					\
	PAL-RGB.icc					\
	ProPhotoRGB.icc					\
	SMPTE-C-RGB.icc					\
	sRGB.icc					\
	WideGamutRGB.icc				\
	SwappedRedAndGreen.icc				\
	bluish.icc					\
	gamma5000.icc					\
	gamma5500.icc					\
	gamma6500.icc					\
	crayons.icc					\
	x11-colors.icc

if CD_BUILD_PRINT_PROFILES
generatedprofiles +=					\
	ISOnewspaper26.icc				\
	FOGRA27L_coated.icc				\
	FOGRA28L_webcoated.icc				\
	FOGRA29L_uncoated.icc				\
	FOGRA30L_uncoated_yellowish.icc			\
	FOGRA39L_coated.icc				\
	FOGRA40L_SC_paper.icc				\
	GRACoL_TR006_coated.icc				\
	SNAP_TR002_newsprint.icc			\
	SWOP_TR003_coated_3.icc				\
	SWOP_TR005_coated_5.icc
endif

profiles_DATA =						\
	$(generatedprofiles)

colprof_manufacturer = "Argyll Color Management System"
colprof_options = -nc -v -qh -bh
copyright = "This profile is free of known copyright restrictions."
license = "CC0"

AdobeRGB1998.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "Compatible with Adobe RGB (1998)" \
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=adobe-rgb" \
		2.2 D65					\
		0.6400 0.3300 0.297361			\
		0.2100 0.7100 0.627355			\
		0.1500 0.0600 0.075285
AppleRGB.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "Apple RGB"		\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=apple-rgb" \
		1.8 D65					\
		0.6250 0.3400 0.244634			\
		0.2800 0.5950 0.672034			\
		0.1550 0.0700 0.083332
BestRGB.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "Best RGB"		\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=best-rgb" \
		2.2 D50					\
		0.7347 0.2653 0.228457			\
		0.2150 0.7750 0.737352			\
		0.1300 0.0350 0.034191
BetaRGB.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "Beta RGB"		\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=beta-rgb" \
		2.2 D50					\
		0.6888 0.3112 0.303273			\
		0.1986 0.7551 0.663786			\
		0.1265 0.0352 0.032941
BruceRGB.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "Bruce RGB"		\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=bruce-rgb" \
		2.2 D65					\
		0.6400 0.3300 0.240995			\
		0.2800 0.6500 0.683554			\
		0.1500 0.0600 0.075452
CIE-RGB.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "CIE RGB"			\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=cie-rgb" \
		2.2 E					\
		0.7350 0.2650 0.176204			\
		0.2740 0.7170 0.812985			\
		0.1670 0.0090 0.010811
ColorMatchRGB.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "ColorMatch RGB"		\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=colormatch-rgb"	\
		1.8 D50					\
		0.6300 0.3400 0.274884			\
		0.2950 0.6050 0.658132			\
		0.1500 0.0750 0.066985
DonRGB4.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "Don RGB 4"		\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=don-rgb4" \
		2.2 D50					\
		0.6960 0.3000 0.278350			\
		0.2150 0.7650 0.687970			\
		0.1300 0.0350 0.033680
ECI-RGBv2.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "ECI RGB v2"		\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=eci-rgb-v2" \
		L* D50					\
		0.6700 0.3300 0.320250			\
		0.2100 0.7100 0.602071			\
		0.1400 0.0800 0.077679
EktaSpacePS5.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "Ekta Space PS5"		\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=ekta-space-ps5" \
		2.2 D50					\
		0.6950 0.3050 0.260629			\
		0.2600 0.7000 0.734946			\
		0.1100 0.0050 0.004425
NTSC-RGB.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "NTSC RGB"		\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=ntsc-rgb" \
		2.2 C					\
		0.6700 0.3300 0.298839			\
		0.2100 0.7100 0.586811			\
		0.1400 0.0800 0.114350
PAL-RGB.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "PAL/SECAM RGB"		\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=pal-rgb" \
		2.2 D65					\
		0.6400 0.3300 0.222021			\
		0.2900 0.6000 0.706645			\
		0.1500 0.0600 0.071334
ProPhotoRGB.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "ProPhoto RGB"		\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=prophoto-rgb" \
		1.8 D50					\
		0.7347 0.2653 0.288040			\
		0.1596 0.8404 0.711874			\
		0.0366 0.0001 0.000086
SMPTE-C-RGB.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "SMPTE-C RGB"		\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=smpte-c-rgb" \
		2.2 D65					\
		0.6300 0.3400 0.212395			\
		0.3100 0.5950 0.701049			\
		0.1550 0.0700 0.086556
sRGB.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "sRGB"			\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=srgb" \
		sRGB D65				\
		0.6400 0.3300 0.212656			\
		0.3000 0.6000 0.715158			\
		0.1500 0.0600 0.072186
WideGamutRGB.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "Wide Gamut RGB"		\
		--metadata "License=$(license),DATA_source=standard,STANDARD_space=wide-gamut-rgb" \
		2.2 D50					\
		0.7350 0.2650 0.258187			\
		0.1150 0.8260 0.724938			\
		0.1570 0.0180 0.016875

SwappedRedAndGreen.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-standard-space			\
		--description "Swapped Red & Green"	\
		--metadata "License=$(license),DATA_source=test" \
		sRGB D65				\
		0.3000 0.6000 0.715158			\
		0.6400 0.3300 0.212656			\
		0.1500 0.0600 0.072186

x11-colors.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-named-color srgb "X11" ""	\
		--metadata "License=$(license)"		\
		--description "X11 Colors"		\
		$(srcdir)/x11-colors.csv
crayons.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		create-named-color lab "Crayon" ""	\
		$(srcdir)/crayons.csv				\
		--metadata "License=$(license)"		\
		--description "Crayon Colors"
bluish.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		--description "Blue"			\
		--metadata "License=$(license),DATA_source=test" \
		create-x11-gamma			\
		0.7 0.8 1.0
gamma6500.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		--description "D65"			\
		--metadata "License=$(license),DATA_source=test" \
		create-temperature			\
		6504 1.0
gamma5500.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		--description "D55"			\
		--metadata "License=$(license),DATA_source=test" \
		create-temperature			\
		5503 1.0
gamma5000.icc:
	$(top_builddir)/client/cd-create-profile --output $@ \
		--description "D50"			\
		--metadata "License=$(license),DATA_source=test" \
		create-temperature			\
		5003 1.0

if CD_BUILD_PRINT_PROFILES
FOGRA27L_coated.icc: FOGRA27L.ti3
	${COLPROF} -kp 0 .3 .94 .94 .48 -l300		\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "FOGRA27L Coated"				\
		-O $@ $(top_srcdir)/data/profiles/FOGRA27L;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space fogra27l $@
FOGRA28L_webcoated.icc: FOGRA28L.ti3
	${COLPROF} -kp 0 .3 .94 .94 .48 -l300		\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "FOGRA28L Web Coated"			\
		-O $@ $(top_srcdir)/data/profiles/FOGRA28L;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space fogra28l $@
FOGRA29L_uncoated.icc: FOGRA29L.ti3
	${COLPROF} -kp 0 .3 .94 .94 .48 -l300		\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "FOGRA29L Uncoated"				\
		-O $@ $(top_srcdir)/data/profiles/FOGRA29L;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space fogra29l $@
FOGRA30L_uncoated_yellowish.icc: FOGRA30L.ti3
	${COLPROF} -kp 0 .3 1. 1. .48 -l340		\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "FOGRA30L Uncoated Yellowish"		\
		-O $@ $(top_srcdir)/data/profiles/FOGRA30L;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space fogra30l $@
FOGRA39L_coated.icc: FOGRA39L.ti3
	${COLPROF} -kp 0 .3 .94 .94 .48 -l300		\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "FOGRA39L Coated"				\
		-O $@ $(top_srcdir)/data/profiles/FOGRA39L;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space fogra39l $@
FOGRA40L_SC_paper.icc: FOGRA40L.ti3
	${COLPROF} -kp 0 .3 1. 1. .48 -l340		\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "FOGRA40L SC Paper"				\
		-O $@ $(top_srcdir)/data/profiles/FOGRA40L;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space fogra40l $@
GRACoL_TR006_coated.icc: TR006.ti3
	${COLPROF} -kp 0 .3 .94 .94 .48 -l300	\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "GRACoL TR006 Coated"			\
		-O $@ $(top_srcdir)/data/profiles/TR006;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space tr006 $@
SNAP_TR002_newsprint.icc: TR002.ti3
	${COLPROF} -kp 0 .4 1. 1. .1 -l320		\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "SNAP TR002 Newsprint"			\
		-O $@ $(top_srcdir)/data/profiles/TR002;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space tr002 $@
SWOP_TR003_coated_3.icc: TR003.ti3
	${COLPROF} -kp 0 .3 .94 .94 .4 -l300		\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "SWOP TR003 Coated"				\
		-O $@ $(top_srcdir)/data/profiles/TR003;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space tr003 $@
SWOP_TR005_coated_5.icc: TR005.ti3
	${COLPROF} -kp 0 .3 .94 .94 .48 -l300		\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "SWOP TR005 Coated"				\
		-O $@ $(top_srcdir)/data/profiles/TR005;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space tr005 $@

# Total Ink = 230%, Maximum Black = 95%
ISOnewspaper26.icc: IFRA26S.ti3
	${COLPROF} -kr -l230 -L100 -cpp -dpp			\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "IFRA26 Newsprint"				\
		-O $@ $(top_srcdir)/data/profiles/IFRA26S;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space ifra26 $@
endif

sourcedata =						\
	x11-colors.csv					\
	crayons.csv					\
	FOGRA27L.ti3					\
	FOGRA28L.ti3					\
	FOGRA29L.ti3					\
	FOGRA30L.ti3					\
	FOGRA39L.ti3					\
	FOGRA40L.ti3					\
	IFRA26S.ti3					\
	TR002.ti3					\
	TR003.ti3					\
	TR005.ti3					\
	TR006.ti3

CLEANFILES =						\
	$(generatedprofiles)

EXTRA_DIST =						\
	$(sourcedata)

-include $(top_srcdir)/git.mk
