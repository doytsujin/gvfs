/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <glib.h>
#include <glib/gi18n.h>
#include "gvfsjobmove.h"
#include "gvfsdbus.h"

G_DEFINE_TYPE (GVfsJobMove, g_vfs_job_move, G_VFS_TYPE_JOB_PROGRESS)

static void         run          (GVfsJob        *job);
static gboolean     try          (GVfsJob        *job);
static void         create_reply (GVfsJob               *job,
                                  GVfsDBusMount         *object,
                                  GDBusMethodInvocation *invocation);

static void
g_vfs_job_move_finalize (GObject *object)
{
  GVfsJobMove *job;

  job = G_VFS_JOB_MOVE (object);
  
  g_free (job->source);
  g_free (job->destination);
  
  if (G_OBJECT_CLASS (g_vfs_job_move_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_vfs_job_move_parent_class)->finalize) (object);
}

static void
g_vfs_job_move_class_init (GVfsJobMoveClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GVfsJobClass *job_class = G_VFS_JOB_CLASS (klass);
  GVfsJobDBusClass *job_dbus_class = G_VFS_JOB_DBUS_CLASS (klass);
  
  gobject_class->finalize = g_vfs_job_move_finalize;
  job_class->run = run;
  job_class->try = try;
  job_dbus_class->create_reply = create_reply;
}

static void
g_vfs_job_move_init (GVfsJobMove *job)
{
}

gboolean
g_vfs_job_move_new_handle (GVfsDBusMount *object,
                           GDBusMethodInvocation *invocation,
                           const gchar *arg_path1_data,
                           const gchar *arg_path2_data,
                           guint arg_flags,
                           const gchar *arg_progress_obj_path,
                           GVfsBackend *backend)
{
  GVfsJobMove *job;
  GVfsJobProgress *progress_job;

  if (g_vfs_backend_invocation_first_handler (object, invocation, backend))
    return TRUE;
  
  job = g_object_new (G_VFS_TYPE_JOB_MOVE,
                      "object", object,
                      "invocation", invocation,
                      NULL);
  progress_job = G_VFS_JOB_PROGRESS (job);

  job->source = g_strdup (arg_path1_data);
  job->destination = g_strdup (arg_path2_data);
  job->backend = backend;
  job->flags = arg_flags;
  if (strcmp (arg_progress_obj_path, "/org/gtk/vfs/void") != 0)
    progress_job->callback_obj_path = g_strdup (arg_progress_obj_path);
  progress_job->send_progress = progress_job->callback_obj_path != NULL;

  g_vfs_job_source_new_job (G_VFS_JOB_SOURCE (backend), G_VFS_JOB (job));
  g_object_unref (job);

  return TRUE;
}

static void
run (GVfsJob *job)
{
  GVfsJobMove *op_job = G_VFS_JOB_MOVE (job);
  GVfsJobProgress *progress_job = G_VFS_JOB_PROGRESS (job);
  GVfsBackendClass *class = G_VFS_BACKEND_GET_CLASS (op_job->backend);

  if (class->move == NULL)
    {
      g_vfs_job_failed (job, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
			_("Operation not supported"));
      return;
    }
  
  g_vfs_job_progress_construct_proxy (job);
  
  class->move (op_job->backend,
	       op_job,
	       op_job->source,
	       op_job->destination,
	       op_job->flags,
               progress_job->send_progress ? g_vfs_job_progress_callback : NULL,
               progress_job->send_progress ? job : NULL);
}

static gboolean
try (GVfsJob *job)
{
  GVfsJobMove *op_job = G_VFS_JOB_MOVE (job);
  GVfsJobProgress *progress_job = G_VFS_JOB_PROGRESS (job);
  GVfsBackendClass *class = G_VFS_BACKEND_GET_CLASS (op_job->backend);
  gboolean res;

  if (class->try_move == NULL)
    return FALSE;
  
  g_vfs_job_progress_construct_proxy (job);
  
  res = class->try_move (op_job->backend,
			 op_job,
			 op_job->source,
			 op_job->destination,
			 op_job->flags,
		         progress_job->send_progress ? g_vfs_job_progress_callback : NULL,
		         progress_job->send_progress ? job : NULL);

  return res;
}

/* Might be called on an i/o thread */
static void
create_reply (GVfsJob *job,
              GVfsDBusMount *object,
              GDBusMethodInvocation *invocation)
{
  gvfs_dbus_mount_complete_move (object, invocation);
}
