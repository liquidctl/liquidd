#include <signal.h>
#include <stdlib.h>

#include <glib-unix.h>
#include <glib.h>

#include "hid_device.h"

#define HID_MAX_BUFFER_SIZE 16384

#define FAN_CHANNELS 3
#define FAN_CHANNELS_MAX 8

enum
{
    INPUT_REPORT_ID_FAN_CONFIG = 0x61,
    INPUT_REPORT_ID_FAN_STATUS = 0x67,
};

enum
{
    FAN_STATUS_REPORT_SPEED = 0x02,
    FAN_STATUS_REPORT_VOLTAGE = 0x04,
};

struct unknown_static_data
{
    guint8 unknown1[14]; // NOLINT(readability-magic-numbers)
} __attribute__((packed));

struct fan_config_report
{
    guint8 report_id; // == INPUT_REPORT_ID_FAN_CONFIG
    guint8 magic; // == 0x03
    struct unknown_static_data unknown_data;
    guint8 fan_type[FAN_CHANNELS_MAX];
} __attribute__((packed));

struct fan_status_report
{
    guint8 report_id; // == INPUT_REPORT_ID_FAN_STATUS
    guint8 type;
    struct unknown_static_data unknown_data;
    guint8 fan_type[FAN_CHANNELS_MAX];

    union
    {
        /* type == FAN_STATUS_REPORT_SPEED */
        struct
        {
            guint16 fan_rpm[FAN_CHANNELS_MAX];
            guint8 duty_percent[FAN_CHANNELS_MAX];
            guint8 duty_percent_dup[FAN_CHANNELS_MAX];
            guint8 noise_db;
        } __attribute__((packed)) fan_speed;
        /* type == FAN_STATUS_REPORT_VOLTAGE */
        struct
        {
            guint16 fan_in[FAN_CHANNELS_MAX];
            guint16 fan_current[FAN_CHANNELS_MAX];
        } __attribute__((packed)) fan_voltage;
    } __attribute__((packed));
} __attribute__((packed));

#define OUTPUT_REPORT_SIZE 64

enum
{
    OUTPUT_REPORT_ID_INIT_COMMAND = 0x60,
    OUTPUT_REPORT_ID_SET_FAN_SPEED = 0x62,
};

enum
{
    INIT_COMMAND_SET_UPDATE_INTERVAL = 0x02,
    INIT_COMMAND_DETECT_FANS = 0x03,
};

static const guint8 detect_fans_report[OUTPUT_REPORT_SIZE] = {
    OUTPUT_REPORT_ID_INIT_COMMAND,
    INIT_COMMAND_DETECT_FANS,
};

static const guint8 set_update_interval_report[OUTPUT_REPORT_SIZE] = {
    OUTPUT_REPORT_ID_INIT_COMMAND,
    INIT_COMMAND_SET_UPDATE_INTERVAL,
    0x01,
    0xe8,
    0x03,
    0x01,
    0xe8,
    0x03,
};

static gboolean
shutdown_signal(gpointer user_data)
{
    GMainLoop *loop = user_data;
    g_main_loop_quit(loop);
    return G_SOURCE_CONTINUE;
}

static gboolean
input_report_fan_config(LiquidHidDevice *device G_GNUC_UNUSED, GBytes *bytes)
{
    gsize size = 0;
    const struct fan_config_report *data = g_bytes_get_data(bytes, &size);

    if (size < sizeof(struct fan_config_report))
    {
        g_printerr("Fan config report too short: %" G_GSIZE_FORMAT "\n", size);
        return TRUE;
    }

    if (data->magic != 0x03)
    {
        g_printerr("Fan config report: invalid magic = %#x\n", data->magic);
        return TRUE;
    }

    for (int i = 0; i < FAN_CHANNELS; i++)
    {
        g_print("Fan %d type: %d\n", i + 1, data->fan_type[i]);
    }

    return TRUE;
}

static gboolean
input_report_fan_status(LiquidHidDevice *device G_GNUC_UNUSED, GBytes *bytes)
{
    gsize size = 0;
    const struct fan_status_report *data = g_bytes_get_data(bytes, &size);

    if (size < sizeof(struct fan_status_report))
    {
        g_printerr("Fan status report too short: %" G_GSIZE_FORMAT "\n", size);
        return TRUE;
    }

    switch (data->type)
    {
    case FAN_STATUS_REPORT_SPEED:
        for (int i = 0; i < FAN_CHANNELS; i++)
        {
            g_print("Fan %d type: %d speed: %d RPM PWM: %d%%\n",
                    i + 1,
                    data->fan_type[i],
                    GUINT16_FROM_LE(data->fan_speed.fan_rpm[i]),
                    data->fan_speed.duty_percent[i]);
        }
        break;
    case FAN_STATUS_REPORT_VOLTAGE:
        for (int i = 0; i < FAN_CHANNELS; i++)
        {
            g_print("Fan %d type: %d voltage: %d mV current: %d mA\n",
                    i + 1,
                    data->fan_type[i],
                    GUINT16_FROM_LE(data->fan_voltage.fan_in[i]),
                    GUINT16_FROM_LE(data->fan_voltage.fan_current[i]));
        }
        break;
    default:
        g_printerr("Unknown fan status report type %#x\n", data->type);
        break;
    }

    return TRUE;
}

static gboolean
input_report_unknown(LiquidHidDevice *device G_GNUC_UNUSED, GBytes *bytes)
{
    gsize size = 0;
    const guint8 *data = g_bytes_get_data(bytes, &size);

    if (size > 0)
    {
        g_printerr("Unhandled input report %#x\n", *data);
    }

    return TRUE;
}

int
main(int argc, char *argv[])
{
    g_auto(GStrv) device_files = NULL;
    gboolean init_device = FALSE;

    const GOptionEntry entries[] = {
        {
            .long_name = G_OPTION_REMAINING,
            .arg = G_OPTION_ARG_FILENAME_ARRAY,
            .arg_data = &device_files,
            .description = "Device file name",
            .arg_description = "/dev/hidraw*",
        },
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

    if (!device_files || !device_files[0])
    {
        g_printerr("Device file is required");
        return EXIT_FAILURE;
    }

    g_autofree gchar *fan_config_signal = g_strdup_printf("input-report::%#x", INPUT_REPORT_ID_FAN_CONFIG);
    g_autofree gchar *fan_status_signal = g_strdup_printf("input-report::%#x", INPUT_REPORT_ID_FAN_STATUS);

    g_autolist(LiquidHidDevice) devices = NULL;

    for (GStrv device_file = device_files; *device_file; device_file++)
    {
        LiquidHidDevice *device = liquid_hid_device_new_for_path(*device_file, HID_MAX_BUFFER_SIZE, &error);

        if (device == NULL)
        {
            g_printerr("Can't open device %s: %s\n", *device_file, error->message);
            return EXIT_FAILURE;
        }

        devices = g_list_prepend(devices, device);

        g_signal_connect(device, fan_config_signal, G_CALLBACK(input_report_fan_config), NULL);
        g_signal_connect(device, fan_status_signal, G_CALLBACK(input_report_fan_status), NULL);
        g_signal_connect(device, "input-report", G_CALLBACK(input_report_unknown), NULL);

        if (init_device)
        {
            if (liquid_hid_device_output_report(device, detect_fans_report, OUTPUT_REPORT_SIZE, &error)
                != OUTPUT_REPORT_SIZE)
            {
                g_printerr("Failed to send detect fans command to %s: %s\n", *device_file, error->message);
                return EXIT_FAILURE;
            }

            if (liquid_hid_device_output_report(device, set_update_interval_report, OUTPUT_REPORT_SIZE, &error)
                != OUTPUT_REPORT_SIZE)
            {
                g_printerr("Failed to send update interval command to %s: %s\n", *device_file, error->message);
                return EXIT_FAILURE;
            }
        }
    }

    g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);

    g_unix_signal_add(SIGINT, shutdown_signal, loop);
    g_unix_signal_add(SIGTERM, shutdown_signal, loop);

    g_main_loop_run(loop);
    return EXIT_SUCCESS;
}
