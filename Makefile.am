AUTOMAKE_OPTIONS = 1.7

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

NULL =

SUBDIRS = 						\
	etc						\
	policy						\
	man						\
	lib						\
	rules						\
	po

# this needs libcolord
SUBDIRS += 						\
	client						\
	contrib						\
	doc						\
	examples

# this needs client/cd-create-profile
SUBDIRS += 						\
	data

# this needs data/profiles/*.icc
SUBDIRS += 						\
	src

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-$(VERSION)-`date +"%Y%m%d"`

DISTCHECK_CONFIGURE_FLAGS =				\
	--enable-introspection				\
	--enable-fd-fallback				\
	--enable-gtk					\
	--enable-sane					\
	--disable-print-profiles			\
	--disable-bash-completion			\
	--enable-gtk-doc				\
	--with-udevrulesdir=$$dc_install_base/$(udevrulesdir) \
	--with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

EXTRA_DIST =						\
	COPYING						\
	MAINTAINERS					\
	AUTHORS						\
	README						\
	NEWS						\
	autogen.sh					\
	config.h					\
	intltool-extract.in				\
	intltool-merge.in				\
	intltool-update.in

MAINTAINERCLEANFILES =					\
	$(srcdir)/INSTALL				\
	$(srcdir)/ABOUT-NLS				\
	$(srcdir)/aclocal.m4				\
	$(srcdir)/autoscan.log				\
	$(srcdir)/compile				\
	$(srcdir)/config.guess				\
	$(srcdir)/config.rpath				\
	$(srcdir)/config.h.in				\
	$(srcdir)/config.sub				\
	$(srcdir)/configure.scan			\
	$(srcdir)/depcomp				\
	$(srcdir)/install-sh				\
	$(srcdir)/ltmain.sh				\
	$(srcdir)/missing				\
	$(srcdir)/mkinstalldirs				\
	$(srcdir)/omf.make				\
	$(srcdir)/test-driver				\
	$(srcdir)/xmldocs.make				\
	$(srcdir)/gtk-doc.make				\
	$(srcdir)/ChangeLog				\
	$(srcdir)/po/Makefile.in.in~			\
	$(srcdir)/colord-*.tar.*			\
	`find "$(srcdir)" -type f -name Makefile.in -print`

GITIGNOREFILES =					\
	.tx

clang:
	$(MAKE) clean;					\
	rm -rf clang;					\
	scan-build --use-analyzer=/usr/bin/clang	\
		-o clang-report				\
		./autogen.sh				\
		--enable-introspection=no		\
		--disable-print-profiles;		\
	scan-build --use-analyzer=/usr/bin/clang	\
		-o clang-report make

coverity:
	$(MAKE);					\
	cd src; $(MAKE) clean; cd -;			\
	cd lib; $(MAKE) clean; cd -;			\
	cd examples; $(MAKE) clean; cd -;		\
	cd client; $(MAKE) clean; cd -;			\
	cd contrib/session-helper; $(MAKE) clean; cd -;	\
	cov-build --dir cov-int $(MAKE);		\
	tar czvf colord-coverity.tgz cov-int;		\
	export COVERITY_EMAIL=`cat ~/.coverity | grep EMAIL | cut -f2`; \
	export COVERITY_PASSWORD=`cat ~/.coverity | grep PASSWORD_colord | cut -f2`; \
	curl --form file=@colord-coverity.tgz		\
	     --form project=colord			\
	     --form password=${COVERITY_PASSWORD}	\
	     --form email=${COVERITY_EMAIL}		\
	     http://scan5.coverity.com/cgi-bin/upload.py

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

-include $(top_srcdir)/git.mk
