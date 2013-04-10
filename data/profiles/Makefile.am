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
	FOGRA27L_coated.xml			\
	FOGRA28L_webcoated.xml			\
	FOGRA29L_uncoated.xml			\
	FOGRA30L_uncoated_yellowish.xml		\
	FOGRA39L_coated.xml			\
	FOGRA40L_SC_paper.xml			\
	FOGRA45L_lwc.xml			\
	FOGRA47L_uncoated.xml			\
	Gamma5000K.xml				\
	Gamma5500K.xml				\
	Gamma6500K.xml				\
	GRACoL_TR006_coated.xml			\
	IFRA26S_2004_newsprint.xml			\
	NTSC-RGB.xml				\
	PAL-RGB.xml				\
	ProPhotoRGB.xml				\
	SMPTE-C-RGB.xml				\
	SNAP_TR002_newsprint.xml		\
	sRGB.xml				\
	SwappedRedAndGreen.xml			\
	SWOP_TR003_coated_3.xml			\
	SWOP_TR005_coated_5.xml			\
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
	FOGRA27L_coated.icc				\
	FOGRA28L_webcoated.icc				\
	FOGRA29L_uncoated.icc				\
	FOGRA30L_uncoated_yellowish.icc			\
	FOGRA39L_coated.icc				\
	FOGRA40L_SC_paper.icc				\
	FOGRA45L_lwc.icc				\
	FOGRA47L_uncoated.icc				\
	GRACoL_TR006_coated.icc				\
	SNAP_TR002_newsprint.icc			\
	SWOP_TR003_coated_3.icc				\
	SWOP_TR005_coated_5.icc				\
	IFRA26S_2004_newsprint.icc
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
FOGRA27L_coated.icc: FOGRA27L_coated.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/FOGRA27L_coated.xml
FOGRA28L_webcoated.icc: FOGRA28L_webcoated.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/FOGRA28L_webcoated.xml
FOGRA29L_uncoated.icc: FOGRA29L_uncoated.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/FOGRA29L_uncoated.xml
FOGRA30L_uncoated_yellowish.icc: FOGRA30L_uncoated_yellowish.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/FOGRA30L_uncoated_yellowish.xml
FOGRA39L_coated.icc: FOGRA39L_coated.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/FOGRA39L_coated.xml
FOGRA40L_SC_paper.icc: FOGRA40L_SC_paper.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/FOGRA40L_SC_paper.xml
FOGRA45L_lwc.icc: FOGRA45L_lwc.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/FOGRA45L_lwc.xml
FOGRA47L_uncoated.icc: FOGRA47L_uncoated.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/FOGRA47L_uncoated.xml
GRACoL_TR006_coated.icc: GRACoL_TR006_coated.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/GRACoL_TR006_coated.xml
SNAP_TR002_newsprint.icc: SNAP_TR002_newsprint.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/SNAP_TR002_newsprint.xml
SWOP_TR003_coated_3.icc: SWOP_TR003_coated_3.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/SWOP_TR003_coated_3.xml
SWOP_TR005_coated_5.icc: SWOP_TR005_coated_5.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/SWOP_TR005_coated_5.xml
IFRA26S_2004_newsprint.icc: IFRA26S_2004_newsprint.xml
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-create-profile	\
		--output $@				\
		$(builddir)/IFRA26S_2004_newsprint.xml
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
	FOGRA27L_coated.xml.in				\
	FOGRA28L_webcoated.xml.in			\
	FOGRA29L_uncoated.xml.in			\
	FOGRA30L_uncoated_yellowish.xml.in		\
	FOGRA39L_coated.xml.in				\
	FOGRA40L_SC_paper.xml.in			\
	FOGRA45L_lwc.xml.in				\
	FOGRA47L_uncoated.xml.in			\
	Gamma5000K.xml.in				\
	Gamma5500K.xml.in				\
	Gamma6500K.xml.in				\
	GRACoL_TR006_coated.xml.in			\
	IFRA26S_2004_newsprint.xml.in			\
	NTSC-RGB.xml.in					\
	PAL-RGB.xml.in					\
	ProPhotoRGB.xml.in				\
	SMPTE-C-RGB.xml.in				\
	SNAP_TR002_newsprint.xml.in			\
	sRGB.xml.in					\
	SwappedRedAndGreen.xml.in			\
	SWOP_TR003_coated_3.xml.in			\
	SWOP_TR005_coated_5.xml.in			\
	WideGamutRGB.xml.in				\
	x11-colors.xml.in

CLEANFILES =						\
	$(generatedprofiles)

DISTCLEANFILES =					\
	$(noinst_DATA)

EXTRA_DIST =						\
	$(sourcedata)

-include $(top_srcdir)/git.mk
