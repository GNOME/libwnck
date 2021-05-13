/*
 * Copyright (C) 2021 Alberts Muktupāvels
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WNCK_RESOURCE_USAGE_PRIVATE_H
#define WNCK_RESOURCE_USAGE_PRIVATE_H

#include "util.h"

G_BEGIN_DECLS

void _wnck_read_resource_usage_xid     (GdkDisplay        *gdisplay,
                                        gulong             xid,
                                        WnckResourceUsage *usage);

void _wnck_read_resource_usage_pid     (GdkDisplay        *gdisplay,
                                        gulong             pid,
                                        WnckResourceUsage *usage);

void _wnck_read_resources_shutdown_all (void);

G_END_DECLS

#endif
