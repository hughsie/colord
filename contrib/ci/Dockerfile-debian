FROM debian:unstable

RUN apt-get update -qq
RUN apt-get install -yq --no-install-recommends \
	bash-completion \
	xsltproc \
	docbook-xsl-ns \
	gettext \
	gobject-introspection \
	gtk-doc-tools \
	gvfs-bin \
	libdbus-glib-1-dev \
	libgirepository1.0-dev \
	libglib2.0-dev \
	libgudev-1.0-dev \
	libgusb-dev \
	liblcms2-dev \
	libpolkit-gobject-1-dev \
	libsane-dev \
	libsoup2.4-dev \
	libsqlite3-dev \
	libsystemd-dev \
	libtool-bin \
	libudev-dev \
	meson \
	pkg-config \
	policykit-1 \
	shared-mime-info \
	systemd \
	udev \
	valac \
	valgrind
RUN mkdir /build
WORKDIR /build
