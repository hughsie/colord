test_files =						\
	LG-L225W-External.bin				\
	Lenovo-T61-Internal.bin				\
	calibration.ccmx				\
	corrupt-dict.icc				\
	crayons.icc					\
	empty.icc					\
	ibm-t61.icc					\
	measured.ti3					\
	normalised.ti3					\
	raw.ti3						\
	test.sp						\
	test.ccss					\
	example.cmf					\
	example.sp					\
	reference.ti3

if BUILDOPT_INSTALL_TESTS
testdatadir = $(libexecdir)/installed-tests/colord
testdata_DATA = $(test_files)
endif

EXTRA_DIST = $(test_files)

-include $(top_srcdir)/git.mk
