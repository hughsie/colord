test_files =						\
	calibration.ccmx				\
	crayons.icc					\
	ibm-t61.icc					\
	measured.ti3					\
	normalised.ti3					\
	raw.ti3						\
	reference.ti3

if BUILDOPT_INSTALL_TESTS
testdatadir = $(libexecdir)/installed-tests/colord
testdata_DATA = $(test_files)
endif

EXTRA_DIST = $(test_files)

-include $(top_srcdir)/git.mk
