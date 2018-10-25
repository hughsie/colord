FROM fedora:28

RUN dnf -y update
RUN dnf -y install \
	argyllcms \
	bash-completion \
	color-filesystem \
	dbus-devel \
	libxslt \
	docbook5-style-xsl \
	gettext \
	glib2-devel \
	gobject-introspection-devel \
	gtk-doc \
	lcms2-devel \
	libgudev1-devel \
	libgusb-devel \
	libtool \
	meson \
	polkit-devel \
	redhat-rpm-config \
	sane-backends-devel \
	shared-mime-info \
	sqlite-devel \
	systemd-devel \
	systemd-udev \
	vala-tools

RUN mkdir /build
WORKDIR /build
