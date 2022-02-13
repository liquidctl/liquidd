#include "driver.h"
#include "gio/gdbusobjectskeleton.h"

typedef struct
{
    GHashTable *channels;
} LiquidDriverPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(LiquidDriver, liquid_driver, G_TYPE_DBUS_OBJECT_SKELETON)

static void
liquid_driver_dispose(GObject *object)
{
    LiquidDriver *driver = LIQUID_DRIVER(object);
    LiquidDriverPrivate *priv = liquid_driver_get_instance_private(driver);

    g_hash_table_remove_all(priv->channels);

    G_OBJECT_CLASS(liquid_driver_parent_class)->dispose(object);
}

static void
liquid_driver_finalize(GObject *object)
{
    LiquidDriver *driver = LIQUID_DRIVER(object);
    LiquidDriverPrivate *priv = liquid_driver_get_instance_private(driver);

    g_hash_table_unref(priv->channels);

    G_OBJECT_CLASS(liquid_driver_parent_class)->finalize(object);
}

static void
liquid_driver_class_init(LiquidDriverClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(class);

    gobject_class->dispose = liquid_driver_dispose;
    gobject_class->finalize = liquid_driver_finalize;
}

static void
liquid_driver_init(LiquidDriver *driver)
{
    LiquidDriverPrivate *priv = liquid_driver_get_instance_private(driver);

    priv->channels = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
}

void
liquid_driver_for_each_channel(LiquidDriver *driver,
                               LiquidDriverForEachChannelCallback callback,
                               gpointer user_data)
{
    g_return_if_fail(LIQUID_IS_DRIVER(driver));

    LiquidDriverPrivate *priv = liquid_driver_get_instance_private(driver);

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, priv->channels);

    gchar *key;
    GDBusObjectSkeleton *value;

    while (g_hash_table_iter_next(&iter, (gpointer)&key, (gpointer)&value))
    {
        callback(driver, key, value, user_data);
    }
}

GDBusObjectSkeleton *
liquid_driver_add_channel(LiquidDriver *driver, const gchar *name)
{
    g_return_val_if_fail(LIQUID_IS_DRIVER(driver), NULL);

    LiquidDriverPrivate *priv = liquid_driver_get_instance_private(driver);
    GDBusObjectSkeleton *object_skeleton = g_object_new(G_TYPE_DBUS_OBJECT_SKELETON, NULL);

    g_hash_table_insert(priv->channels, g_strdup(name), g_object_ref(object_skeleton));

    return g_steal_pointer(&object_skeleton);
}

static void
liquid_driver_export_channel(LiquidDriver *driver,
                             const gchar *name,
                             GDBusObjectSkeleton *channel,
                             gpointer user_data)
{
    GDBusObjectManagerServer *object_manager_server = user_data;

    const gchar *base_path = g_dbus_object_get_object_path(G_DBUS_OBJECT(driver));
    g_autofree gchar *path = g_strdup_printf("%s/%s", base_path, name);

    g_dbus_object_skeleton_set_object_path(channel, path);
    g_dbus_object_manager_server_export(object_manager_server, channel);
}

void
liquid_driver_export(LiquidDriver *driver, GDBusObjectManagerServer *object_manager_server)
{
    g_return_if_fail(LIQUID_IS_DRIVER(driver));
    g_return_if_fail(G_IS_DBUS_OBJECT_MANAGER_SERVER(object_manager_server));

    const gchar *base_path =
        g_dbus_object_manager_get_object_path(G_DBUS_OBJECT_MANAGER(object_manager_server));

    g_autofree gchar *path = g_strdup_printf("%s/%s", base_path, G_OBJECT_TYPE_NAME(driver));

    g_dbus_object_skeleton_set_object_path(G_DBUS_OBJECT_SKELETON(driver), path);
    g_dbus_object_manager_server_export(object_manager_server, G_DBUS_OBJECT_SKELETON(driver));

    liquid_driver_for_each_channel(driver, liquid_driver_export_channel, object_manager_server);
}
