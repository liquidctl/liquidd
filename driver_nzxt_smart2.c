#include "driver_nzxt_smart2.h"

#include "dbus_interfaces.h"
#include "driver.h"
#include "gio/gdbusinterfaceskeleton.h"
#include "gio/gdbusobjectskeleton.h"

#define OUTPUT_REPORT_SIZE 64

#define FAN_CHANNELS 3
#define FAN_CHANNELS_MAX 8

#define INPUT_REPORT_ID_FAN_CONFIG 0x61
#define INPUT_REPORT_ID_FAN_STATUS 0x67

#define FAN_STATUS_REPORT_SPEED 0x02
#define FAN_STATUS_REPORT_VOLTAGE 0x04

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

struct _LiquidDriverNzxtSmart2
{
    LiquidDriverHid parent;

    LiquidDBusFanSpeedRPM *rpm[FAN_CHANNELS];
};

G_DEFINE_FINAL_TYPE(LiquidDriverNzxtSmart2, liquid_driver_nzxt_smart2, LIQUID_TYPE_DRIVER_HID)

static gboolean
liquid_driver_nzxt_smart2_input_report_fan_config(LiquidDriverHid *driver G_GNUC_UNUSED,
                                                  GBytes *bytes)
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
liquid_driver_nzxt_smart2_input_report_fan_status(LiquidDriverNzxtSmart2 *driver,
                                                  GBytes *bytes)
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

            liquid_dbus_fan_speed_rpm_set_fan_speed_rpm(driver->rpm[i],
                                                        GUINT16_FROM_LE(data->fan_speed.fan_rpm[i]));
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
liquid_driver_nzxt_smart2_input_report_unknown(LiquidDriverHid *driver G_GNUC_UNUSED,
                                               GBytes *bytes)
{
    gsize size = 0;
    const guint8 *data = g_bytes_get_data(bytes, &size);

    if (size > 0)
    {
        g_printerr("Unhandled input report %#x\n", *data);
    }

    return TRUE;
}

static gboolean
liquid_driver_nzxt_smart2_init_device(LiquidDriverNzxtSmart2 *driver, GError **error)
{
    g_return_val_if_fail(LIQUID_IS_DRIVER_NZXT_SMART2(driver), FALSE);

    LiquidHidDevice *hid_device = liquid_driver_hid_get_device(LIQUID_DRIVER_HID(driver));
    g_autoptr(GError) inner_error = NULL;

    if (!liquid_hid_device_output_report(hid_device,
                                         detect_fans_report,
                                         OUTPUT_REPORT_SIZE,
                                         &inner_error))
    {
        g_propagate_prefixed_error(error, inner_error, "Failed to send detect fans command: ");
        return FALSE;
    }

    if (!liquid_hid_device_output_report(hid_device,
                                         set_update_interval_report,
                                         OUTPUT_REPORT_SIZE,
                                         &inner_error))
    {
        g_propagate_prefixed_error(error, inner_error, "Failed to send update interval command: ");
        return FALSE;
    }

    return TRUE;
}

static gboolean
liquid_driver_nzxt_smart2_handle_init_device(LiquidDBusInitDeviceSkeleton *interface G_GNUC_UNUSED,
                                             GDBusMethodInvocation *invocation,
                                             LiquidDriverNzxtSmart2 *driver)
{
    g_autoptr(GError) error = NULL;

    if (liquid_driver_nzxt_smart2_init_device(driver, &error))
    {
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
    }

    return TRUE;
}

static void
liquid_driver_nzxt_smart2_dispose(GObject *object)
{
    LiquidDriverNzxtSmart2 *driver = LIQUID_DRIVER_NZXT_SMART2(object);

    for (int i = 0; i < FAN_CHANNELS; i++)
    {
        g_clear_object(&driver->rpm[i]);
    }

    G_OBJECT_CLASS(liquid_driver_nzxt_smart2_parent_class)->dispose(object);
}

static void
liquid_driver_nzxt_smart2_class_init(LiquidDriverNzxtSmart2Class *class)
{
    LiquidDriverHidClass *driver_hid_class = LIQUID_DRIVER_HID_CLASS(class);

    driver_hid_class->input_report = liquid_driver_nzxt_smart2_input_report_unknown;

    GObjectClass *gobject_class = G_OBJECT_CLASS(class);

    gobject_class->dispose = liquid_driver_nzxt_smart2_dispose;
}

static void
liquid_driver_nzxt_smart2_init(LiquidDriverNzxtSmart2 *driver)
{
    g_signal_connect(driver,
                     "input-report::" G_STRINGIFY(INPUT_REPORT_ID_FAN_CONFIG),
                     G_CALLBACK(liquid_driver_nzxt_smart2_input_report_fan_config),
                     NULL);

    g_signal_connect(driver,
                     "input-report::" G_STRINGIFY(INPUT_REPORT_ID_FAN_STATUS),
                     G_CALLBACK(liquid_driver_nzxt_smart2_input_report_fan_status),
                     NULL);

    g_autoptr(LiquidDBusInitDevice) init_interface = liquid_dbus_init_device_skeleton_new();

    g_signal_connect(init_interface,
                     "handle-init-device",
                     G_CALLBACK(liquid_driver_nzxt_smart2_handle_init_device),
                     driver);

    g_dbus_object_skeleton_add_interface(G_DBUS_OBJECT_SKELETON(driver),
                                         G_DBUS_INTERFACE_SKELETON(init_interface));

    for (int i = 0; i < FAN_CHANNELS; i++)
    {
        g_autofree gchar *channel_name = g_strdup_printf("fan%d", i);
        g_autoptr(GDBusObjectSkeleton) channel_dbus = liquid_driver_add_channel(LIQUID_DRIVER(driver), channel_name);
        g_autoptr(LiquidDBusFanSpeedRPM) rpm = liquid_dbus_fan_speed_rpm_skeleton_new();

        g_dbus_object_skeleton_add_interface(channel_dbus, G_DBUS_INTERFACE_SKELETON(rpm));
        driver->rpm[i] = g_steal_pointer(&rpm);
    }
}

gboolean
liquid_driver_nzxt_smart2_match(LiquidHidDeviceInfo *info)
{
    const unsigned int vendor_nzxt = 0x1e71;

    if (liquid_hid_device_info_get_vendor_id(info) != vendor_nzxt)
    {
        return FALSE;
    }

    const unsigned int supported_products[] = {
        0x2006,
        0x200d,
        0x2009,
        0x200e,
        0x2010,
    };

    for (unsigned i = 0; i < G_N_ELEMENTS(supported_products); i++)
    {
        if (supported_products[i] == liquid_hid_device_info_get_product_id(info))
        {
            return TRUE;
        }
    }

    return FALSE;
}

LiquidDriverNzxtSmart2 *
liquid_driver_nzxt_smart2_new(LiquidHidDevice *hid_device, LiquidHidDeviceInfo *info)
{
    return g_object_new(LIQUID_TYPE_DRIVER_NZXT_SMART2,
                        "hid-device",
                        hid_device,
                        "hid-device-info",
                        info,
                        NULL);
}
