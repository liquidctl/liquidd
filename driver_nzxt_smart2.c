#include "driver_nzxt_smart2.h"

#include "hid_device.h"
#include "hid_device_info.h"

#define OUTPUT_REPORT_SIZE 64

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
    GObject parent;

    LiquidHidDevice *hid_device;
};

G_DEFINE_FINAL_TYPE(LiquidDriverNzxtSmart2, liquid_driver_nzxt_smart2, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_HID_DEVICE,
    N_PROPERTIES
};

static GParamSpec *pspecs[N_PROPERTIES];

static gboolean
liquid_driver_nzxt_smart2_input_report_fan_config(LiquidHidDevice *hid_device G_GNUC_UNUSED,
                                                  GBytes *bytes,
                                                  LiquidDriverNzxtSmart2 *driver G_GNUC_UNUSED)
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
liquid_driver_nzxt_smart2_input_report_fan_status(LiquidHidDevice *device G_GNUC_UNUSED,
                                                  GBytes *bytes,
                                                  LiquidDriverNzxtSmart2 *driver G_GNUC_UNUSED)
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
liquid_driver_nzxt_smart2_input_report_unknown(LiquidHidDevice *device G_GNUC_UNUSED,
                                               GBytes *bytes,
                                               LiquidDriverNzxtSmart2 *driver G_GNUC_UNUSED)
{
    gsize size = 0;
    const guint8 *data = g_bytes_get_data(bytes, &size);

    if (size > 0)
    {
        g_printerr("Unhandled input report %#x\n", *data);
    }

    return TRUE;
}

static void
liquid_driver_nzxt_smart2_dispose(GObject *object)
{
    LiquidDriverNzxtSmart2 *device = LIQUID_DRIVER_NZXT_SMART2(object);

    if (device->hid_device)
    {
        g_signal_handlers_disconnect_by_func(device->hid_device,
                                             liquid_driver_nzxt_smart2_input_report_fan_config,
                                             device);
        g_signal_handlers_disconnect_by_func(device->hid_device,
                                             liquid_driver_nzxt_smart2_input_report_fan_status,
                                             device);
        g_signal_handlers_disconnect_by_func(device->hid_device,
                                             liquid_driver_nzxt_smart2_input_report_unknown,
                                             device);
    }

    g_clear_object(&device->hid_device);

    G_OBJECT_CLASS(liquid_driver_nzxt_smart2_parent_class)->dispose(object);
}

static void
liquid_driver_nzxt_smart2_get_property(GObject *object,
                                       guint property_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
    LiquidDriverNzxtSmart2 *device = LIQUID_DRIVER_NZXT_SMART2(object);

    switch (property_id)
    {
    case PROP_HID_DEVICE:
        g_value_set_object(value, device->hid_device);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
liquid_driver_nzxt_smart2_set_property(GObject *object,
                                       guint property_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
    LiquidDriverNzxtSmart2 *device = LIQUID_DRIVER_NZXT_SMART2(object);

    switch (property_id)
    {
    case PROP_HID_DEVICE:
        g_set_object(&device->hid_device, g_value_get_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
liquid_driver_nzxt_smart2_constructed(GObject *object)
{
    LiquidDriverNzxtSmart2 *device = LIQUID_DRIVER_NZXT_SMART2(object);

    g_autofree gchar *fan_config_signal
        = g_strdup_printf("input-report::%#x", INPUT_REPORT_ID_FAN_CONFIG);
    g_autofree gchar *fan_status_signal
        = g_strdup_printf("input-report::%#x", INPUT_REPORT_ID_FAN_STATUS);

    g_signal_connect(device->hid_device,
                     fan_config_signal,
                     G_CALLBACK(liquid_driver_nzxt_smart2_input_report_fan_config),
                     device);
    g_signal_connect(device->hid_device,
                     fan_status_signal,
                     G_CALLBACK(liquid_driver_nzxt_smart2_input_report_fan_status),
                     device);
    g_signal_connect(device->hid_device,
                     "input-report",
                     G_CALLBACK(liquid_driver_nzxt_smart2_input_report_unknown),
                     device);

    G_OBJECT_CLASS(liquid_driver_nzxt_smart2_parent_class)->constructed(object);
}

static void
liquid_driver_nzxt_smart2_class_init(LiquidDriverNzxtSmart2Class *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(class);

    gobject_class->dispose = liquid_driver_nzxt_smart2_dispose;
    gobject_class->get_property = liquid_driver_nzxt_smart2_get_property;
    gobject_class->set_property = liquid_driver_nzxt_smart2_set_property;
    gobject_class->constructed = liquid_driver_nzxt_smart2_constructed;

    pspecs[PROP_HID_DEVICE]
        = g_param_spec_object("hid-device", /* name */
                              "HID device", /* nick */
                              "HID device", /* blurb */
                              LIQUID_TYPE_HID_DEVICE, /* object_type */
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, pspecs);
}

static void
liquid_driver_nzxt_smart2_init(LiquidDriverNzxtSmart2 *device G_GNUC_UNUSED)
{
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
liquid_driver_nzxt_smart2_new(LiquidHidDevice *hid_device)
{
    return g_object_new(LIQUID_TYPE_DRIVER_NZXT_SMART2,
                        "hid-device",
                        hid_device,
                        NULL);
}

gboolean
liquid_driver_nzxt_smart2_init_device(LiquidDriverNzxtSmart2 *driver, GError **error)
{
    g_return_val_if_fail(LIQUID_IS_DRIVER_NZXT_SMART2(driver), FALSE);

    g_autoptr(GError) inner_error = NULL;

    if (!liquid_hid_device_output_report(driver->hid_device,
                                         detect_fans_report,
                                         OUTPUT_REPORT_SIZE,
                                         &inner_error))
    {
        g_propagate_prefixed_error(error, inner_error, "Failed to send detect fans command: ");
        return FALSE;
    }

    if (!liquid_hid_device_output_report(driver->hid_device,
                                         set_update_interval_report,
                                         OUTPUT_REPORT_SIZE,
                                         &inner_error))
    {
        g_propagate_prefixed_error(error, inner_error, "Failed to send update interval command: ");
        return FALSE;
    }

    return TRUE;
}
