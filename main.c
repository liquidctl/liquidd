#include <signal.h>
#include <stdlib.h>

#include <glib-unix.h>
#include <glib.h>

#include "driver_nzxt_smart2.h"
#include "hid_device.h"
#include "hid_device_info.h"
#include "hid_manager.h"

#define HID_MAX_BUFFER_SIZE 16384

gboolean init_device = FALSE;

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
    GList *drivers = user_data;

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

    drivers = g_list_prepend(drivers, g_object_ref(driver));

    if (init_device && !liquid_driver_nzxt_smart2_init_device(driver, &error))
    {
        g_printerr("Failed to init device %s: %s\n", hidraw_path, error->message);
        return;
    }
}

int
main(int argc, char *argv[])
{
    const GOptionEntry entries[] = {
        {
            .long_name = "init",
            .arg = G_OPTION_ARG_NONE,
            .arg_data = &init_device,
            .description = "(Re)initialize the device",
        },
        G_OPTION_ENTRY_NULL,
    };

    g_autoptr(GError) error = NULL;
    g_autoptr(GOptionContext) context = g_option_context_new(NULL);
    g_option_context_add_main_entries(context, entries, NULL);

    if (!g_option_context_parse(context, &argc, &argv, &error))
    {
        g_printerr("%s\n", error->message);
        return EXIT_FAILURE;
    }

    g_autolist(GObject) drivers = NULL;
    g_autoptr(LiquidHidManager) hid_manager = liquid_hid_manager_new(g_udev_client_new(NULL));

    liquid_hid_manager_for_each_device(hid_manager, probe_hid_device, drivers);

    g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);

    g_unix_signal_add(SIGINT, shutdown_signal, loop);
    g_unix_signal_add(SIGTERM, shutdown_signal, loop);

    g_main_loop_run(loop);
    return EXIT_SUCCESS;
}
