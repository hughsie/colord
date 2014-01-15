.NOTPARALLEL:

generated =						\
	CIE-A.sp					\
	CIE-B.sp					\
	CIE-C.sp					\
	CIE-D50.sp					\
	CIE-D55.sp					\
	CIE-D65.sp					\
	CIE-D93.sp					\
	CIE-E.sp					\
	CIE-F10.sp					\
	CIE-F11.sp					\
	CIE-F12.sp					\
	CIE-F1.sp					\
	CIE-F2.sp					\
	CIE-F3.sp					\
	CIE-F4.sp					\
	CIE-F5.sp					\
	CIE-F6.sp					\
	CIE-F7.sp					\
	CIE-F8.sp					\
	CIE-F9.sp

illuminantdir = $(datadir)/colord/illuminant
illuminant_DATA =					\
	$(generated)

noinst_DATA =						\
	CIE-A.csv					\
	CIE-B.csv					\
	CIE-C.csv					\
	CIE-D50.csv					\
	CIE-D55.csv					\
	CIE-D65.csv					\
	CIE-D93.csv					\
	CIE-E.csv					\
	CIE-F10.csv					\
	CIE-F11.csv					\
	CIE-F12.csv					\
	CIE-F1.csv					\
	CIE-F2.csv					\
	CIE-F3.csv					\
	CIE-F4.csv					\
	CIE-F5.csv					\
	CIE-F6.csv					\
	CIE-F7.csv					\
	CIE-F8.csv					\
	CIE-F9.csv

CIE-A.sp: CIE-A.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-A.csv 100.0
CIE-B.sp: CIE-B.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-B.csv 100.0
CIE-C.sp: CIE-C.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-C.csv 100.0
CIE-D50.sp: CIE-D50.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-D50.csv 100.0
CIE-D55.sp: CIE-D55.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-D55.csv 100.0
CIE-D65.sp: CIE-D65.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-D65.csv 100.0
CIE-D93.sp: CIE-D93.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-D93.csv 100.0
CIE-E.sp: CIE-E.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-E.csv 100.0
CIE-F10.sp: CIE-F10.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-F10.csv 100.0
CIE-F11.sp: CIE-F11.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-F11.csv 100.0
CIE-F12.sp: CIE-F12.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-F12.csv 100.0
CIE-F1.sp: CIE-F1.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-F1.csv 100.0
CIE-F2.sp: CIE-F2.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-F2.csv 100.0
CIE-F3.sp: CIE-F3.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-F3.csv 100.0
CIE-F4.sp: CIE-F4.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-F4.csv 100.0
CIE-F5.sp: CIE-F5.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-F5.csv 100.0
CIE-F6.sp: CIE-F6.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-F6.csv 100.0
CIE-F7.sp: CIE-F7.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-F7.csv 100.0
CIE-F8.sp: CIE-F8.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-F8.csv 100.0
CIE-F9.sp: CIE-F9.csv
	$(AM_V_GEN)					\
	$(top_builddir)/client/cd-it8 create-sp $@	\
		$(srcdir)/CIE-F9.csv 100.0

CLEANFILES =						\
	$(generated)

EXTRA_DIST =						\
	$(noinst_DATA)

-include $(top_srcdir)/git.mk
