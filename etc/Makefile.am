configdir = $(sysconfdir)
config_in_files = colord.conf.in
config_DATA = $(config_in_files:.conf.in=.conf)

$(config_DATA): $(config_in_files) Makefile
	@sed -e "s|\@enablevolumesearch\@|$(ENABLE_VOLUME_SEARCH)|" $< > $@

EXTRA_DIST =							\
	$(config_in_files)

CLEANFILES =							\
	colord.conf

-include $(top_srcdir)/git.mk
