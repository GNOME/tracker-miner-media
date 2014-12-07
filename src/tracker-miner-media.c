/*
 * Copyright (C) 2014 Carlos Garnacho  <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "tracker-miner-media.h"

#include <gdata/gdata.h>

#define TMM_DATA_SOURCE "tmm:urn:83443497-b4cf-4341-8ac8-74058828f6db"

typedef struct _TmmDecoratorPrivate TmmDecoratorPrivate;

struct _TmmDecoratorPrivate
{
	GDataFreebaseService *freebase_service;
	GCancellable *cancellable;
};

G_DEFINE_TYPE_WITH_PRIVATE (TmmDecorator, tmm_decorator, TRACKER_TYPE_DECORATOR_FS)

static void
tmm_decorator_finalize (GObject *object)
{
	TmmDecoratorPrivate *priv;

	priv = tmm_decorator_get_instance_private (TMM_DECORATOR (object));
	g_object_unref (priv->freebase_service);
}

static void
tmm_decorator_class_init (TmmDecoratorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tmm_decorator_finalize;
}

static void
tmm_decorator_init (TmmDecorator *decorator)
{
	TmmDecoratorPrivate *priv;

	priv = tmm_decorator_get_instance_private (decorator);
	priv->freebase_service = gdata_freebase_service_new (NULL, NULL);
	priv->cancellable = g_cancellable_new ();
}

TrackerMiner *
tmm_decorator_new (void)
{
	gchar *classes[] = {
		"nfo:Video",
		NULL
	};

	return g_object_new (TMM_TYPE_DECORATOR,
	                     "name", "Media",
	                     "data-source", TMM_DATA_SOURCE,
	                     "class-names", classes,
	                     NULL);
}
