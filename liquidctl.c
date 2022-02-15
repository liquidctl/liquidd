#include <stdlib.h>

#include <gio/gio.h>

#include "dbus_interfaces.h"

static gint
object_path_cmp(gconstpointer a, gconstpointer b)
{
    GDBusObject *obj_a = G_DBUS_OBJECT(a);
    GDBusObject *obj_b = G_DBUS_OBJECT(b);

    return g_strcmp0(g_dbus_object_get_object_path(obj_a),
                     g_dbus_object_get_object_path(obj_b));
}

int
main(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GDBusConnection) connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

    if (!connection)
    {
        g_printerr("Can't connect to D-Bus: %s\n", error->message);
        return EXIT_FAILURE;
    }

    g_autoptr(GDBusObjectManager) manager
        = liquid_dbus_object_manager_client_new_sync(connection, /* connection */
                                                     G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE, /* flags */
                                                     "org.liquidctl.LiquidD", /* name */
                                                     "/org/liquidctl/LiquidD", /* object_path */
                                                     NULL, /* cancellable */
                                                     &error /* error */);
    if (!manager)
    {
        g_printerr("Can't connect to LiquidD: %s\n", error->message);
        return EXIT_FAILURE;
    }

    g_autolist(GDBusObject) objects = g_dbus_object_manager_get_objects(manager);
    objects = g_list_sort(objects, object_path_cmp);

    for (GList *i = objects; i; i = i->next)
    {
        GDBusObject *object = G_DBUS_OBJECT(i->data);

        g_print("%s:\n", g_dbus_object_get_object_path(object));

        g_autolist(GDBusInterface) interfaces = g_dbus_object_get_interfaces(object);

        for (GList *j = interfaces; j; j = j->next)
        {
            GDBusProxy *proxy = G_DBUS_PROXY(j->data);
            g_auto(GStrv) properties = g_dbus_proxy_get_cached_property_names(proxy);

            if (!properties)
            {
                continue;
            }

            g_print("\t%s:\n", g_dbus_proxy_get_interface_name(proxy));

            for (GStrv property = properties; *property; property++)
            {
                g_autoptr(GVariant) value = g_dbus_proxy_get_cached_property(proxy, *property);
                g_autofree gchar *value_str = g_variant_print(value, TRUE);

                g_print("\t\t%s: %s\n", *property, value_str);
            }
        }
    }

    return EXIT_SUCCESS;
}
