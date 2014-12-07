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

#ifndef __TMM_DECORATOR_H__
#define __TMM_DECORATOR_H__

#include <libtracker-miner/tracker-miner.h>

G_BEGIN_DECLS

#define TMM_TYPE_DECORATOR         (tmm_decorator_get_type())
#define TMM_DECORATOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TMM_TYPE_DECORATOR, TmmDecorator))
#define TMM_DECORATOR_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TMM_TYPE_DECORATOR, TmmDecoratorClass))
#define TMM_IS_DECORATOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TMM_TYPE_DECORATOR))
#define TMM_IS_DECORATOR_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),  TMM_TYPE_DECORATOR))
#define TMM_DECORATOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TMM_TYPE_DECORATOR, TmmDecoratorClass))

typedef struct _TmmDecorator TmmDecorator;
typedef struct _TmmDecoratorClass TmmDecoratorClass;

struct _TmmDecorator {
	TrackerDecoratorFS parent_instance;
};

struct _TmmDecoratorClass {
	TrackerDecoratorFSClass parent_class;
};

GType          tmm_decorator_get_type (void) G_GNUC_CONST;

TrackerMiner * tmm_decorator_new      (void);

G_END_DECLS

#endif /* __TMM_DECORATOR_H__ */
