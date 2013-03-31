.NOTPARALLEL:

@INTLTOOL_XML_RULE@
noinst_DATA =					\
	AdobeRGB1998.xml			\
	AppleRGB.xml				\
	BestRGB.xml				\
	BetaRGB.xml				\
	Bluish.xml				\
	BruceRGB.xml				\
	CIE-RGB.xml				\
	ColorMatchRGB.xml			\
	Crayons.xml				\
	DonRGB4.xml				\
	ECI-RGBv1.xml				\
	ECI-RGBv2.xml				\
	EktaSpacePS5.xml			\
	Gamma5000K.xml				\
	Gamma5500K.xml				\
	Gamma6500K.xml				\
	NTSC-RGB.xml				\
	PAL-RGB.xml				\
	ProPhotoRGB.xml				\
	SMPTE-C-RGB.xml				\
	sRGB.xml				\
	SwappedRedAndGreen.xml			\
	WideGamutRGB.xml			\
	x11-colors.xml

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
	ECI-RGBv1.icc					\
	ECI-RGBv2.icc					\
	EktaSpacePS5.icc				\
	NTSC-RGB.icc					\
	PAL-RGB.icc					\
	ProPhotoRGB.icc					\
	SMPTE-C-RGB.icc					\
	sRGB.icc					\
	WideGamutRGB.icc				\
	SwappedRedAndGreen.icc				\
	Bluish.icc					\
	Gamma5000K.icc					\
	Gamma5500K.icc					\
	Gamma6500K.icc					\
	Crayons.icc					\
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

AdobeRGB1998.icc: AdobeRGB1998.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/AdobeRGB1998.xml
AppleRGB.icc: AppleRGB.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/AppleRGB.xml
BestRGB.icc: BestRGB.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/BestRGB.xml
BetaRGB.icc: BetaRGB.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/BetaRGB.xml
BruceRGB.icc: BruceRGB.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/BruceRGB.xml
CIE-RGB.icc: CIE-RGB.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/CIE-RGB.xml
ColorMatchRGB.icc: ColorMatchRGB.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/ColorMatchRGB.xml
DonRGB4.icc: DonRGB4.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/DonRGB4.xml
ECI-RGBv1.icc: ECI-RGBv1.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/ECI-RGBv1.xml
ECI-RGBv2.icc: ECI-RGBv2.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/ECI-RGBv2.xml
EktaSpacePS5.icc: EktaSpacePS5.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/EktaSpacePS5.xml
NTSC-RGB.icc: NTSC-RGB.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/NTSC-RGB.xml
PAL-RGB.icc: PAL-RGB.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/PAL-RGB.xml
ProPhotoRGB.icc: ProPhotoRGB.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/ProPhotoRGB.xml
SMPTE-C-RGB.icc: SMPTE-C-RGB.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/SMPTE-C-RGB.xml
sRGB.icc: sRGB.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/sRGB.xml
WideGamutRGB.icc: WideGamutRGB.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/WideGamutRGB.xml
SwappedRedAndGreen.icc: SwappedRedAndGreen.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/SwappedRedAndGreen.xml
x11-colors.icc: x11-colors.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/x11-colors.xml
Crayons.icc: Crayons.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/Crayons.xml
Bluish.icc: Bluish.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/Bluish.xml
Gamma6500K.icc: Gamma6500K.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/Gamma6500K.xml
Gamma5500K.icc: Gamma5500K.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/Gamma5500K.xml
Gamma5000K.icc: Gamma5000K.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/Gamma5000K.xml

if CD_BUILD_PRINT_PROFILES
colprof_manufacturer = "Argyll Color Management System"
colprof_options = -nc -v -qh -bh
copyright = "This profile is free of known copyright restrictions."
license = "CC0"

FOGRA27L_coated.icc: FOGRA27L.ti3
	$(AM_V_GEN)						\
	${COLPROF} -kp 0 .3 .94 .94 .48 -l300			\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "FOGRA27L Coated"				\
		-O $@ $(top_srcdir)/data/profiles/FOGRA27L;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space fogra27l $@
FOGRA28L_webcoated.icc: FOGRA28L.ti3
	$(AM_V_GEN)						\
	${COLPROF} -kp 0 .3 .94 .94 .48 -l300			\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "FOGRA28L Web Coated"			\
		-O $@ $(top_srcdir)/data/profiles/FOGRA28L;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space fogra28l $@
FOGRA29L_uncoated.icc: FOGRA29L.ti3
	$(AM_V_GEN)						\
	${COLPROF} -kp 0 .3 .94 .94 .48 -l300			\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "FOGRA29L Uncoated"				\
		-O $@ $(top_srcdir)/data/profiles/FOGRA29L;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space fogra29l $@
FOGRA30L_uncoated_yellowish.icc: FOGRA30L.ti3
	$(AM_V_GEN)						\
	${COLPROF} -kp 0 .3 1. 1. .48 -l340			\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "FOGRA30L Uncoated Yellowish"		\
		-O $@ $(top_srcdir)/data/profiles/FOGRA30L;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space fogra30l $@
FOGRA39L_coated.icc: FOGRA39L.ti3
	$(AM_V_GEN)						\
	${COLPROF} -kp 0 .3 .94 .94 .48 -l300			\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "FOGRA39L Coated"				\
		-O $@ $(top_srcdir)/data/profiles/FOGRA39L;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space fogra39l $@
FOGRA40L_SC_paper.icc: FOGRA40L.ti3
	$(AM_V_GEN)						\
	${COLPROF} -kp 0 .3 1. 1. .48 -l340			\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "FOGRA40L SC Paper"				\
		-O $@ $(top_srcdir)/data/profiles/FOGRA40L;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space fogra40l $@
GRACoL_TR006_coated.icc: TR006.ti3
	$(AM_V_GEN)						\
	${COLPROF} -kp 0 .3 .94 .94 .48 -l300			\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "GRACoL TR006 Coated"			\
		-O $@ $(top_srcdir)/data/profiles/TR006;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space tr006 $@
SNAP_TR002_newsprint.icc: TR002.ti3
	$(AM_V_GEN)						\
	${COLPROF} -kp 0 .4 1. 1. .1 -l320			\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "SNAP TR002 Newsprint"			\
		-O $@ $(top_srcdir)/data/profiles/TR002;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space tr002 $@
SWOP_TR003_coated_3.icc: TR003.ti3
	$(AM_V_GEN)						\
	${COLPROF} -kp 0 .3 .94 .94 .4 -l300			\
		$(colprof_options)				\
		-A $(colprof_manufacturer)			\
		-C $(copyright)					\
		-D "SWOP TR003 Coated"				\
		-O $@ $(top_srcdir)/data/profiles/TR003;
	$(top_builddir)/client/cd-fix-profile md-add License $(license) $@;
	$(top_builddir)/client/cd-fix-profile md-add DATA_source standard $@;
	$(top_builddir)/client/cd-fix-profile md-add STANDARD_space tr003 $@
SWOP_TR005_coated_5.icc: TR005.ti3
	$(AM_V_GEN)						\
	${COLPROF} -kp 0 .3 .94 .94 .48 -l300			\
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
	$(AM_V_GEN)						\
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
	AdobeRGB1998.xml.in				\
	AppleRGB.xml.in					\
	BestRGB.xml.in					\
	BetaRGB.xml.in					\
	Bluish.xml.in					\
	BruceRGB.xml.in					\
	CIE-RGB.xml.in					\
	ColorMatchRGB.xml.in				\
	Crayons.xml.in					\
	DonRGB4.xml.in					\
	ECI-RGBv1.xml.in				\
	ECI-RGBv2.xml.in				\
	EktaSpacePS5.xml.in				\
	Gamma5000K.xml.in				\
	Gamma5500K.xml.in				\
	Gamma6500K.xml.in				\
	NTSC-RGB.xml.in					\
	PAL-RGB.xml.in					\
	ProPhotoRGB.xml.in				\
	SMPTE-C-RGB.xml.in				\
	sRGB.xml.in					\
	SwappedRedAndGreen.xml.in			\
	WideGamutRGB.xml.in				\
	x11-colors.xml.in				\
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

DISTCLEANFILES =					\
	$(noinst_DATA)

EXTRA_DIST =						\
	$(sourcedata)

-include $(top_srcdir)/git.mk
