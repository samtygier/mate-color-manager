/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#ifndef __MCM_DEVICE_VIRTUAL_H
#define __MCM_DEVICE_VIRTUAL_H

#include <glib-object.h>

#include "mcm-device.h"
#include "mcm-enum.h"

G_BEGIN_DECLS

#define MCM_TYPE_DEVICE_VIRTUAL		(mcm_device_virtual_get_type ())
#define MCM_DEVICE_VIRTUAL(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_DEVICE_VIRTUAL, McmDeviceVirtual))
#define MCM_IS_DEVICE_VIRTUAL(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_DEVICE_VIRTUAL))

typedef struct _McmDeviceVirtual	McmDeviceVirtual;
typedef struct _McmDeviceVirtualClass	McmDeviceVirtualClass;

struct _McmDeviceVirtual
{
	 McmDevice			 parent;
};

struct _McmDeviceVirtualClass
{
	McmDeviceClass			 parent_class;
};

GType		 mcm_device_virtual_get_type		  	(void);
McmDevice	*mcm_device_virtual_new				(void);
gboolean	 mcm_device_virtual_create_from_params		(McmDeviceVirtual	*device_virtual,
								 McmDeviceKind		 device_kind,
								 const gchar		*model,
								 const gchar		*manufacturer,
								 const gchar		*serial,
								 McmColorspace		 colorspace);

G_END_DECLS

#endif /* __MCM_DEVICE_VIRTUAL_H */

