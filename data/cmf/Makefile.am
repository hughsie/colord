.NOTPARALLEL:

generated =						\
	CIE1931-2deg-XYZ.cmf				\
	CIE1964-10deg-XYZ.cmf

cmfdir = $(datadir)/colord/cmf
cmf_DATA =						\
	$(generated)

noinst_DATA =						\
	CIE1931-2deg-XYZ.csv				\
	CIE1964-10deg-XYZ.csv

CIE1931-2deg-XYZ.cmf: CIE1931-2deg-XYZ.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-cmf $@	\
		$(srcdir)/CIE1931-2deg-XYZ.csv 1.0

CIE1964-10deg-XYZ.cmf: CIE1964-10deg-XYZ.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-cmf $@	\
		$(srcdir)/CIE1964-10deg-XYZ.csv 1.0

CLEANFILES =						\
	$(generated)

EXTRA_DIST =						\
	$(noinst_DATA)

-include $(top_srcdir)/git.mk
