/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2012 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <sys/wait.h>
#include <fcntl.h>

#include <glib/gi18n.h>

#include "cd-spawn.h"

static void     cd_spawn_finalize	(GObject       *object);

#define CD_SPAWN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_SPAWN, CdSpawnPrivate))
#define CD_SPAWN_POLL_DELAY	50 /* ms */
#define CD_SPAWN_SIGKILL_DELAY	2500 /* ms */

struct CdSpawnPrivate
{
	pid_t			 child_pid;
	gint			 stdin_fd;
	gint			 stdout_fd;
	gint			 stderr_fd;
	guint			 poll_id;
	guint			 kill_id;
	gboolean		 finished;
	gboolean		 allow_sigkill;
	CdSpawnExitType		 exit;
	GString			*stdout_buf;
	GString			*stderr_buf;
};

enum {
	SIGNAL_EXIT,
	SIGNAL_STDOUT,
	SIGNAL_STDERR,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (CdSpawn, cd_spawn, G_TYPE_OBJECT)

/**
 * cd_spawn_read_fd_into_buffer:
 **/
static gboolean
cd_spawn_read_fd_into_buffer (gint fd, GString *string)
{
	gint bytes_read;
	gchar buffer[BUFSIZ];

	/* ITS4: ignore, we manually NULL terminate and GString cannot overflow */
	while ((bytes_read = read (fd, buffer, BUFSIZ-1)) > 0) {
		buffer[bytes_read] = '\0';
		g_string_append (string, buffer);
	}

	return TRUE;
}

/**
 * cd_spawn_emit_whole_lines:
 **/
static gboolean
cd_spawn_emit_whole_lines (CdSpawn *spawn, GString *string)
{
	guint i;
	guint size;
	gchar **lines;
	guint bytes_processed;

	/* if nothing then don't emit */
	if (string->len == 0)
		return FALSE;

	/* split into lines - the last line may be incomplete */
	lines = g_strsplit (string->str, "\n", 0);
	if (lines == NULL)
		return FALSE;

	/* find size */
	size = g_strv_length (lines);

	bytes_processed = 0;
	/* we only emit n-1 strings */
	for (i = 0; i < (size-1); i++) {
		g_signal_emit (spawn, signals [SIGNAL_STDOUT], 0, lines[i]);
		/* ITS4: ignore, g_strsplit always NULL terminates */
		bytes_processed += strlen (lines[i]) + 1;
	}

	/* remove the text we've processed */
	g_string_erase (string, 0, bytes_processed);

	g_strfreev (lines);
	return TRUE;
}

/**
 * cd_spawn_exit_type_enum_to_string:
 **/
static const gchar *
cd_spawn_exit_type_enum_to_string (CdSpawnExitType type)
{
	if (type == CD_SPAWN_EXIT_TYPE_SUCCESS)
		return "success";
	if (type == CD_SPAWN_EXIT_TYPE_FAILED)
		return "failed";
	if (type == CD_SPAWN_EXIT_TYPE_SIGQUIT)
		return "sigquit";
	if (type == CD_SPAWN_EXIT_TYPE_SIGKILL)
		return "sigkill";
	return "unknown";
}

/**
 * cd_spawn_check_child:
 **/
static gboolean
cd_spawn_check_child (CdSpawn *spawn)
{
	pid_t pid;
	int status;
	gint retval;
	static guint limit_printing = 0;

	/* this shouldn't happen */
	if (spawn->priv->finished) {
		g_warning ("finished twice!");
		return FALSE;
	}

	cd_spawn_read_fd_into_buffer (spawn->priv->stdout_fd, spawn->priv->stdout_buf);
	cd_spawn_read_fd_into_buffer (spawn->priv->stderr_fd, spawn->priv->stderr_buf);

	/* emit all lines on standard out in one callback, as it's all probably
	* related to the error that just happened */
	if (spawn->priv->stderr_buf->len != 0) {
		g_signal_emit (spawn, signals [SIGNAL_STDERR], 0, spawn->priv->stderr_buf->str);
		g_string_set_size (spawn->priv->stderr_buf, 0);
	}

	/* all usual output goes on standard out, only bad libraries bitch to stderr */
	cd_spawn_emit_whole_lines (spawn, spawn->priv->stdout_buf);

	/* Only print one in twenty times to avoid filling the screen */
	if (limit_printing++ % 20 == 0)
		g_debug ("polling child_pid=%ld (1/20)", (long)spawn->priv->child_pid);

	/* check if the child exited */
	pid = waitpid (spawn->priv->child_pid, &status, WNOHANG);
	if (pid == -1) {
		g_warning ("failed to get the child PID data for %ld", (long)spawn->priv->child_pid);
		return TRUE;
	}
	if (pid == 0) {
		/* process still exist, but has not changed state */
		return TRUE;
	}
	if (pid != spawn->priv->child_pid) {
		g_warning ("some other process id was returned: got %ld and wanted %ld",
			     (long)pid, (long)spawn->priv->child_pid);
		return TRUE;
	}

	/* disconnect the poll as there will be no more updates */
	if (spawn->priv->poll_id > 0) {
		g_source_remove (spawn->priv->poll_id);
		spawn->priv->poll_id = 0;
	}

	/* child exited, close resources */
	close (spawn->priv->stdin_fd);
	close (spawn->priv->stdout_fd);
	close (spawn->priv->stderr_fd);
	spawn->priv->stdin_fd = -1;
	spawn->priv->stdout_fd = -1;
	spawn->priv->stderr_fd = -1;
	spawn->priv->child_pid = -1;

	/* use this to detect SIGKILL and SIGQUIT */
	if (WIFSIGNALED (status)) {
		retval = WTERMSIG (status);
		if (retval == SIGQUIT) {
			g_debug ("the child process was terminated by SIGQUIT");
			spawn->priv->exit = CD_SPAWN_EXIT_TYPE_SIGQUIT;
		} else if (retval == SIGKILL) {
			g_debug ("the child process was terminated by SIGKILL");
			spawn->priv->exit = CD_SPAWN_EXIT_TYPE_SIGKILL;
		} else {
			g_warning ("the child process was terminated by signal %i", WTERMSIG (status));
			spawn->priv->exit = CD_SPAWN_EXIT_TYPE_SIGKILL;
		}
	} else {
		/* check we are dead and buried */
		if (!WIFEXITED (status)) {
			g_warning ("the process did not exit, but waitpid() returned!");
			return TRUE;
		}

		/* get the exit code */
		retval = WEXITSTATUS (status);
		if (retval == 0) {
			g_debug ("the child exited with success");
			if (spawn->priv->exit == CD_SPAWN_EXIT_TYPE_UNKNOWN)
				spawn->priv->exit = CD_SPAWN_EXIT_TYPE_SUCCESS;
		} else if (retval == 254) {
			g_debug ("backend was exited rather than finished");
			spawn->priv->exit = CD_SPAWN_EXIT_TYPE_FAILED;
		} else {
			g_warning ("the child exited with return code %i", retval);
			if (spawn->priv->exit == CD_SPAWN_EXIT_TYPE_UNKNOWN)
				spawn->priv->exit = CD_SPAWN_EXIT_TYPE_FAILED;
		}
	}

	/* officially done, although no signal yet */
	spawn->priv->finished = TRUE;

	/* if we are trying to kill this process, cancel the SIGKILL */
	if (spawn->priv->kill_id != 0) {
		g_source_remove (spawn->priv->kill_id);
		spawn->priv->kill_id = 0;
	}

	/* don't emit if we just closed an invalid dispatcher */
	g_debug ("emitting exit %s", cd_spawn_exit_type_enum_to_string (spawn->priv->exit));
	g_signal_emit (spawn, signals [SIGNAL_EXIT], 0, spawn->priv->exit);

	return FALSE;
}

/**
 * cd_spawn_sigkill_cb:
 **/
static gboolean
cd_spawn_sigkill_cb (CdSpawn *spawn)
{
	gint retval;

	/* check if process has already gone */
	if (spawn->priv->finished) {
		g_debug ("already finished, ignoring");
		return FALSE;
	}

	/* set this in case the script catches the signal and exits properly */
	spawn->priv->exit = CD_SPAWN_EXIT_TYPE_SIGKILL;

	g_debug ("sending SIGKILL %ld", (long)spawn->priv->child_pid);
	retval = kill (spawn->priv->child_pid, SIGKILL);
	if (retval == EINVAL) {
		g_warning ("The signum argument is an invalid or unsupported number");
		return FALSE;
	} else if (retval == EPERM) {
		g_warning ("You do not have the privilege to send a signal to the process");
		return FALSE;
	}

	/* never repeat */
	return FALSE;
}

/**
 * cd_spawn_is_running:
 *
 * Is this instance controlling a script?
 *
 **/
gboolean
cd_spawn_is_running (CdSpawn *spawn)
{
	return (spawn->priv->child_pid != -1);
}

/**
 * cd_spawn_kill:
 *
 * We send SIGQUIT and after a few ms SIGKILL (if allowed)
 *
 * IMPORTANT: This is not a syncronous operation, and client programs will need
 * to wait for the ::exit signal.
 **/
gboolean
cd_spawn_kill (CdSpawn *spawn)
{
	gint retval;

	g_return_val_if_fail (CD_IS_SPAWN (spawn), FALSE);
	g_return_val_if_fail (spawn->priv->kill_id == 0, FALSE);

	/* is there a process running? */
	if (spawn->priv->child_pid == -1) {
		g_warning ("no child pid to kill!");
		return FALSE;
	}

	/* check if process has already gone */
	if (spawn->priv->finished) {
		g_debug ("already finished, ignoring");
		return FALSE;
	}

	/* set this in case the script catches the signal and exits properly */
	spawn->priv->exit = CD_SPAWN_EXIT_TYPE_SIGQUIT;

	g_debug ("sending SIGQUIT %ld", (long)spawn->priv->child_pid);
	retval = kill (spawn->priv->child_pid, SIGQUIT);
	if (retval == EINVAL) {
		g_warning ("The signum argument is an invalid or unsupported number");
		return FALSE;
	} else if (retval == EPERM) {
		g_warning ("You do not have the privilege to send a signal to the process");
		return FALSE;
	}

	/* the program might not be able to handle SIGQUIT, give it a few seconds and then SIGKILL it */
	if (spawn->priv->allow_sigkill) {
		spawn->priv->kill_id = g_timeout_add (CD_SPAWN_SIGKILL_DELAY, (GSourceFunc) cd_spawn_sigkill_cb, spawn);
		g_source_set_name_by_id (spawn->priv->kill_id, "[CdSpawn] sigkill");
	}
	return TRUE;
}

/**
 * cd_spawn_send_stdin:
 *
 * Send new comands to a running (but idle) dispatcher script
 *
 **/
gboolean
cd_spawn_send_stdin (CdSpawn *spawn, const gchar *command)
{
	gint wrote;
	gint length;
	gchar *buffer = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail (CD_IS_SPAWN (spawn), FALSE);

	/* check if process has already gone */
	if (spawn->priv->finished) {
		g_debug ("already finished, ignoring");
		ret = FALSE;
		goto out;
	}

	/* is there a process running? */
	if (spawn->priv->child_pid == -1) {
		g_debug ("no child pid");
		ret = FALSE;
		goto out;
	}

	/* buffer always has to have trailing newline */
	g_debug ("sending '%s'", command);
	buffer = g_strdup_printf ("%s\n", command);

	/* ITS4: ignore, we generated this */
	length = strlen (buffer);

	/* write to the waiting process */
	wrote = write (spawn->priv->stdin_fd, buffer, length);
	if (wrote != length) {
		g_warning ("wrote %i/%i bytes on fd %i (%s)", wrote, length, spawn->priv->stdin_fd, strerror (errno));
		ret = FALSE;
	}
out:
	g_free (buffer);
	return ret;
}

/**
 * cd_spawn_argv:
 **/
gboolean
cd_spawn_argv (CdSpawn *spawn, gchar **argv, gchar **envp, GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	guint i;
	guint len;
	gint rc;

	g_return_val_if_fail (CD_IS_SPAWN (spawn), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	g_return_val_if_fail (argv != NULL, FALSE);

	len = g_strv_length (argv);
	for (i = 0; i < len; i++)
		g_debug ("argv[%i] '%s'", i, argv[i]);
	if (envp != NULL) {
		len = g_strv_length (envp);
		for (i = 0; i < len; i++)
			g_debug ("envp[%i] '%s'", i, envp[i]);
	}

	/* create spawned object for tracking */
	spawn->priv->finished = FALSE;
	g_debug ("creating new instance of %s", argv[0]);
	ret = g_spawn_async_with_pipes (NULL, argv, envp,
				 G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
				 NULL, NULL, &spawn->priv->child_pid,
				 &spawn->priv->stdin_fd,
				 &spawn->priv->stdout_fd,
				 &spawn->priv->stderr_fd,
				 &error_local);
	if (!ret) {
		g_set_error (error, 1, 0, "failed to spawn %s: %s", argv[0], error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* install an idle handler to check if the child returnd successfully. */
	rc = fcntl (spawn->priv->stdout_fd, F_SETFL, O_NONBLOCK);
	if (rc < 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "stdout fcntl failed");
		goto out;
	}
	rc = fcntl (spawn->priv->stderr_fd, F_SETFL, O_NONBLOCK);
	if (rc < 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "stderr fcntl failed");
		goto out;
	}

	/* sanity check */
	if (spawn->priv->poll_id != 0) {
		g_warning ("trying to set timeout when already set");
		g_source_remove (spawn->priv->poll_id);
	}

	/* poll quickly */
	spawn->priv->poll_id = g_timeout_add (CD_SPAWN_POLL_DELAY, (GSourceFunc) cd_spawn_check_child, spawn);
	g_source_set_name_by_id (spawn->priv->poll_id, "[CdSpawn] main poll");
out:
	return ret;
}

/**
 * cd_spawn_class_init:
 * @klass: The CdSpawnClass
 **/
static void
cd_spawn_class_init (CdSpawnClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = cd_spawn_finalize;

	signals [SIGNAL_EXIT] =
		g_signal_new ("exit",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      0, NULL, NULL, g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	signals [SIGNAL_STDOUT] =
		g_signal_new ("stdout",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      0, NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals [SIGNAL_STDERR] =
		g_signal_new ("stderr",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      0, NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	g_type_class_add_private (klass, sizeof (CdSpawnPrivate));
}

/**
 * cd_spawn_init:
 * @spawn: This class instance
 **/
static void
cd_spawn_init (CdSpawn *spawn)
{
	spawn->priv = CD_SPAWN_GET_PRIVATE (spawn);

	spawn->priv->child_pid = -1;
	spawn->priv->stdout_fd = -1;
	spawn->priv->stderr_fd = -1;
	spawn->priv->stdin_fd = -1;
	spawn->priv->allow_sigkill = TRUE;
	spawn->priv->exit = CD_SPAWN_EXIT_TYPE_UNKNOWN;

	spawn->priv->stdout_buf = g_string_new ("");
	spawn->priv->stderr_buf = g_string_new ("");
}

/**
 * cd_spawn_finalize:
 * @object: The object to finalize
 **/
static void
cd_spawn_finalize (GObject *object)
{
	CdSpawn *spawn;

	g_return_if_fail (object != NULL);
	g_return_if_fail (CD_IS_SPAWN (object));

	spawn = CD_SPAWN (object);

	g_return_if_fail (spawn->priv != NULL);

	/* disconnect the poll in case we were cancelled before completion */
	if (spawn->priv->poll_id != 0) {
		g_source_remove (spawn->priv->poll_id);
		spawn->priv->poll_id = 0;
	}

	/* disconnect the SIGKILL check */
	if (spawn->priv->kill_id != 0) {
		g_source_remove (spawn->priv->kill_id);
		spawn->priv->kill_id = 0;
	}

	/* still running? */
	if (spawn->priv->stdin_fd != -1) {
		g_debug ("killing as still running in finalize");
		cd_spawn_kill (spawn);
		/* just hope the script responded to SIGQUIT */
		if (spawn->priv->kill_id != 0)
			g_source_remove (spawn->priv->kill_id);
	}

	/* free the buffers */
	g_string_free (spawn->priv->stdout_buf, TRUE);
	g_string_free (spawn->priv->stderr_buf, TRUE);

	G_OBJECT_CLASS (cd_spawn_parent_class)->finalize (object);
}

/**
 * cd_spawn_new:
 *
 * Return value: a new CdSpawn object.
 **/
CdSpawn *
cd_spawn_new (void)
{
	CdSpawn *spawn;
	spawn = g_object_new (CD_TYPE_SPAWN, NULL);
	return CD_SPAWN (spawn);
}
