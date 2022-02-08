#include "hid_manager.h"

#include "hid_device_info.h"

struct _LiquidHidManager
{
    GObject parent;

    GUdevClient *udev_client;
    GHashTable *devices;
};

G_DEFINE_FINAL_TYPE(LiquidHidManager, liquid_hid_manager, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_UDEV_CLIENT,
    N_PROPERTIES
};

static GParamSpec *pspecs[N_PROPERTIES];

static void
liquid_hid_manager_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    LiquidHidManager *manager = LIQUID_HID_MANAGER(object);

    switch (property_id)
    {
    case PROP_UDEV_CLIENT:
        g_value_set_object(value, manager->udev_client);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
liquid_hid_manager_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    LiquidHidManager *manager = LIQUID_HID_MANAGER(object);

    switch (property_id)
    {
    case PROP_UDEV_CLIENT:
        g_set_object(&manager->udev_client, g_value_get_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
liquid_hid_manager_dispose(GObject *object)
{
    LiquidHidManager *manager = LIQUID_HID_MANAGER(object);

    g_hash_table_remove_all(manager->devices);
    g_clear_object(&manager->udev_client);

    G_OBJECT_CLASS(liquid_hid_manager_parent_class)->dispose(object);
}

static void
liquid_hid_manager_finalize(GObject *object)
{
    LiquidHidManager *manager = LIQUID_HID_MANAGER(object);

    g_clear_pointer(&manager->devices, g_hash_table_unref);

    G_OBJECT_CLASS(liquid_hid_manager_parent_class)->finalize(object);
}

static void
liquid_hid_manager_add_udev_device(LiquidHidManager *manager, GUdevDevice *udev_device)
{
    g_autoptr(LiquidHidDeviceInfo) info = liquid_hid_device_info_new_for_udev_device(udev_device);

    if (info == NULL)
    {
        return;
    }

    const gchar *key = liquid_hid_device_info_get_hidraw_path(info);

    g_return_if_fail(key != NULL);

    g_hash_table_insert(manager->devices,
                        g_strdup(key),
                        g_steal_pointer(&info));
}

static void
liquid_hid_manager_constructed(GObject *object)
{
    LiquidHidManager *manager = LIQUID_HID_MANAGER(object);

    g_autolist(GUdevDevice) devices = g_udev_client_query_by_subsystem(manager->udev_client, "hidraw");

    for (GList *i = devices; i; i = i->next)
    {
        liquid_hid_manager_add_udev_device(manager, G_UDEV_DEVICE(i->data));
    }

    G_OBJECT_CLASS(liquid_hid_manager_parent_class)->constructed(object);
}

static void
liquid_hid_manager_init(LiquidHidManager *info)
{
    info->devices = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
}

static void
liquid_hid_manager_class_init(LiquidHidManagerClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(class);

    gobject_class->dispose = liquid_hid_manager_dispose;
    gobject_class->finalize = liquid_hid_manager_finalize;
    gobject_class->get_property = liquid_hid_manager_get_property;
    gobject_class->set_property = liquid_hid_manager_set_property;
    gobject_class->constructed = liquid_hid_manager_constructed;

    pspecs[PROP_UDEV_CLIENT]
        = g_param_spec_object("udev-client", /* name */
                              "UDev client instance", /* nick */
                              "UDev client instance", /* blurb */
                              G_UDEV_TYPE_CLIENT, /* object_type */
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, pspecs);
}

LiquidHidManager *
liquid_hid_manager_new(GUdevClient *udev_client)
{
    return g_object_new(LIQUID_TYPE_HID_MANAGER,
                        "udev-client",
                        udev_client,
                        NULL);
}

void
liquid_hid_manager_for_each_device(LiquidHidManager *manager,
                                   LiquidHidManagerForEachDeviceCallback callback,
                                   gpointer user_data)
{
    g_return_if_fail(LIQUID_IS_HID_MANAGER(manager));

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, manager->devices);

    gchar *key;
    LiquidHidDeviceInfo *value;

    while (g_hash_table_iter_next(&iter, (gpointer)&key, (gpointer)&value))
    {
        callback(manager, value, user_data);
    }
}
