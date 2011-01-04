AUTOMAKE_OPTIONS = 1.7

ACLOCAL_AMFLAGS = -I m4

NULL =

SUBDIRS = 						\
	po						\
	policy						\
	data						\
	man						\
	client						\
	src

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-$(VERSION)-`date +"%Y%m%d"`

DISTCLEANFILES =					\
	colord-*.tar.gz					\
	intltool-extract				\
	intltool-merge					\
	intltool-update

EXTRA_DIST =						\
	COPYING						\
	MAINTAINERS					\
	AUTHORS						\
	INSTALL						\
	README						\
	NEWS						\
        autogen.sh					\
	config.h					\
        intltool-extract.in				\
        intltool-merge.in				\
        intltool-update.in

distclean-local:
	if test $(srdcir) = .; then :; else \
		rm -f ChangeLog; \
	fi

ChangeLog:
	@echo Creating $@
	@if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run git log e197d80c8937c622f21f569457d1bbd05746d951.. --stat -M -C --name-status  --date=short --no-color) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi

.PHONY: ChangeLog
