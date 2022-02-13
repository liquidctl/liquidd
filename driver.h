#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define LIQUID_TYPE_DRIVER (liquid_driver_get_type())
G_DECLARE_DERIVABLE_TYPE(LiquidDriver, liquid_driver, LIQUID, DRIVER, GDBusObjectSkeleton)

struct _LiquidDriverClass
{
    GDBusObjectSkeletonClass parent_class;
};

GDBusObjectSkeleton *
liquid_driver_add_channel(LiquidDriver *driver, const gchar *name);

typedef void (*LiquidDriverForEachChannelCallback)(LiquidDriver *driver,
                                                   const gchar *name,
                                                   GDBusObjectSkeleton *channel,
                                                   gpointer user_data);

void
liquid_driver_for_each_channel(LiquidDriver *driver,
                               LiquidDriverForEachChannelCallback callback,
                               gpointer user_data);

void
liquid_driver_export(LiquidDriver *driver, GDBusObjectManagerServer *object_manager_server);

G_END_DECLS
