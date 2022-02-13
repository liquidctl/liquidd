#include <signal.h>
#include <stdlib.h>

#include <gio/gio.h>
#include <glib-unix.h>

#include "driver.h"
#include "driver_nzxt_smart2.h"
#include "hid_device.h"
#include "hid_device_info.h"
#include "hid_manager.h"

#define HID_MAX_BUFFER_SIZE 16384

static gboolean
shutdown_signal(gpointer user_data)
{
    GMainLoop *loop = user_data;
    g_main_loop_quit(loop);
    return G_SOURCE_CONTINUE;
}

static void
probe_hid_device(LiquidHidManager *manager G_GNUC_UNUSED,
                 LiquidHidDeviceInfo *info,
                 gpointer user_data)
{
    GDBusObjectManagerServer *object_manager = user_data;

    const char *hidraw_path = liquid_hid_device_info_get_hidraw_path(info);

    g_printerr("hid device vendor=%0#6x product=%0#6x: %s\n",
               liquid_hid_device_info_get_vendor_id(info),
               liquid_hid_device_info_get_product_id(info),
               hidraw_path);

    if (!liquid_driver_nzxt_smart2_match(info))
    {
        return;
    }

    g_printerr("Device %s matched\n", hidraw_path);

    g_autoptr(GError) error = NULL;
    LiquidHidDevice *hid_device
        = liquid_hid_device_new_for_path(hidraw_path, HID_MAX_BUFFER_SIZE, &error);

    if (hid_device == NULL)
    {
        g_printerr("Can't open HID device %s: %s\n", hidraw_path, error->message);
        return;
    }

    g_autoptr(LiquidDriverNzxtSmart2) driver = liquid_driver_nzxt_smart2_new(hid_device);

    liquid_driver_export(LIQUID_DRIVER(driver), object_manager);
}

static void
dbus_name_acquired(GDBusConnection *connection G_GNUC_UNUSED, const gchar *name, gpointer user_data G_GNUC_UNUSED)
{
    g_printerr("D-Bus name acquired: %s\n", name);
}

static void
dbus_name_lost(GDBusConnection *connection G_GNUC_UNUSED, const gchar *name, gpointer user_data G_GNUC_UNUSED)
{
    g_printerr("D-Bus name lost: %s\n", name);
}

int
main(void)
{
    g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);

    g_autoptr(GError) error = NULL;
    g_autoptr(GDBusConnection) connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

    if (!connection)
    {
        g_printerr("Can't connect to D-Bus: %s\n", error->message);
        return EXIT_FAILURE;
    }

    g_autoptr(GDBusObjectManagerServer) object_manager = g_dbus_object_manager_server_new("/org/liquidctl/LiquidD");
    g_autoptr(GUdevClient) udev_client = g_udev_client_new(NULL);
    g_autoptr(LiquidHidManager) hid_manager = liquid_hid_manager_new(udev_client);

    liquid_hid_manager_for_each_device(hid_manager, probe_hid_device, object_manager);
    g_dbus_object_manager_server_set_connection(object_manager, connection);

    g_bus_own_name_on_connection(connection,
                                 "org.liquidctl.LiquidD", /* name */
                                 G_BUS_NAME_OWNER_FLAGS_NONE, /* flags */
                                 dbus_name_acquired, /* name_acquired_handler */
                                 dbus_name_lost, /* name_lost_handler */
                                 NULL, /* user_data */
                                 NULL /* user_data_free_func */);

    g_unix_signal_add(SIGINT, shutdown_signal, loop);
    g_unix_signal_add(SIGTERM, shutdown_signal, loop);

    g_main_loop_run(loop);

    return EXIT_SUCCESS;
}
