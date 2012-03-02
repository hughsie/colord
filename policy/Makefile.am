org.freedesktop.color.policy.in: org.freedesktop.color.policy.in.in Makefile
	$(AM_V_GEN)if test "x$(daemon_user)" != "xroot"; then \
		sed -e "s|<!--@ANNOTATE_OWNER@-->|<annotate key=\"org.freedesktop.policykit.owner\">unix-user:$(daemon_user)</annotate>|g" $< > $@ ; \
	else \
		sed -e "/^\s*<!--@ANNOTATE_OWNER@-->\s*$$/d;s|<!--@ANNOTATE_OWNER@-->||g" $< > $@ ; \
	fi

@INTLTOOL_POLICY_RULE@
polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_in_files = org.freedesktop.color.policy.in
polkit_policy_DATA = $(polkit_policy_in_files:.policy.in=.policy)
#polkit_policy_DATA =					\
#	org.freedesktop.color.policy

EXTRA_DIST = org.freedesktop.color.policy.in.in
CLEANFILES =						\
	org.freedesktop.color.policy			\
	org.freedesktop.color.policy.in

-include $(top_srcdir)/git.mk
