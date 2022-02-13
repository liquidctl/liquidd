#include "driver_hid.h"

static GQuark byte_quarks[G_MAXUINT8 + 1];

enum
{
    PROP_0,
    PROP_HID_DEVICE,
    N_PROPERTIES
};

static GParamSpec *pspecs[N_PROPERTIES];

enum
{
    SIGNAL_INPUT_REPORT,
    SIGNAL_DEVICE_ERROR,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

typedef struct
{
    LiquidHidDevice *hid_device;
} LiquidDriverHidPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(LiquidDriverHid, liquid_driver_hid, LIQUID_TYPE_DRIVER)

static void
liquid_driver_hid_emit_input_report(LiquidHidDevice *hid_device G_GNUC_UNUSED,
                                    GBytes *report,
                                    LiquidDriverHid *driver)
{
    const guint8 *data = g_bytes_get_data(report, NULL);
    GQuark detail = g_bytes_get_size(report) > 0 ? byte_quarks[*data] : 0;
    gboolean return_value = FALSE;

    g_signal_emit(driver, signals[SIGNAL_INPUT_REPORT], detail, report, &return_value);
}

static void
liquid_driver_hid_emit_device_error(LiquidHidDevice *hid_device G_GNUC_UNUSED,
                                    GError *error,
                                    LiquidDriverHid *driver)
{
    g_signal_emit(driver, signals[SIGNAL_DEVICE_ERROR], 0, error);
}

static void
liquid_driver_hid_dispose(GObject *object)
{
    LiquidDriverHid *driver = LIQUID_DRIVER_HID(object);
    LiquidDriverHidPrivate *priv = liquid_driver_hid_get_instance_private(driver);

    if (priv->hid_device)
    {
        g_signal_handlers_disconnect_by_func(priv->hid_device,
                                             liquid_driver_hid_emit_input_report,
                                             driver);

        g_signal_handlers_disconnect_by_func(priv->hid_device,
                                             liquid_driver_hid_emit_device_error,
                                             driver);

        g_clear_object(&priv->hid_device);
    }

    G_OBJECT_CLASS(liquid_driver_hid_parent_class)->dispose(object);
}

static void
liquid_driver_hid_get_property(GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
    LiquidDriverHid *driver = LIQUID_DRIVER_HID(object);
    LiquidDriverHidPrivate *priv = liquid_driver_hid_get_instance_private(driver);

    switch (property_id)
    {
    case PROP_HID_DEVICE:
        g_value_set_object(value, priv->hid_device);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
liquid_driver_hid_set_property(GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    LiquidDriverHid *driver = LIQUID_DRIVER_HID(object);
    LiquidDriverHidPrivate *priv = liquid_driver_hid_get_instance_private(driver);

    switch (property_id)
    {
    case PROP_HID_DEVICE:
        g_set_object(&priv->hid_device, g_value_get_object(value));

        g_signal_connect(priv->hid_device,
                         "input-report",
                         G_CALLBACK(liquid_driver_hid_emit_input_report),
                         driver);

        g_signal_connect(priv->hid_device,
                         "error",
                         G_CALLBACK(liquid_driver_hid_emit_device_error),
                         driver);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
liquid_driver_hid_class_init(LiquidDriverHidClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(class);

    gobject_class->dispose = liquid_driver_hid_dispose;
    gobject_class->get_property = liquid_driver_hid_get_property;
    gobject_class->set_property = liquid_driver_hid_set_property;

    pspecs[PROP_HID_DEVICE]
        = g_param_spec_object("hid-device", /* name */
                              "HID device", /* nick */
                              "HID device", /* blurb */
                              LIQUID_TYPE_HID_DEVICE, /* object_type */
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, pspecs);

    signals[SIGNAL_INPUT_REPORT]
        = g_signal_new("input-report", /* signal_name */
                       G_TYPE_FROM_CLASS(class), /* itype */
                       G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, /* signal_flags */
                       G_STRUCT_OFFSET(LiquidDriverHidClass, input_report), /* class_offset */
                       g_signal_accumulator_true_handled, /* accumulator */
                       NULL, /* accu_data */
                       NULL, /* c_marshaller */
                       G_TYPE_BOOLEAN, /* return_type */
                       1, /* n_params */
                       G_TYPE_BYTES);

    signals[SIGNAL_DEVICE_ERROR]
        = g_signal_new("device-error", /* signal_name */
                       G_TYPE_FROM_CLASS(class), /* itype */
                       G_SIGNAL_RUN_LAST, /* signal_flags */
                       G_STRUCT_OFFSET(LiquidDriverHidClass, device_error), /* class_offset */
                       NULL, /* accumulator */
                       NULL, /* accu_data */
                       NULL, /* c_marshaller */
                       G_TYPE_NONE, /* return_type */
                       1, /* n_params */
                       G_TYPE_ERROR);

    for (guint i = 0; i < G_N_ELEMENTS(byte_quarks); i++)
    {
        g_autofree gchar *s = g_strdup_printf("%#x", i);
        byte_quarks[i] = g_quark_from_string(s);
    }
}

static void
liquid_driver_hid_init(LiquidDriverHid *driver G_GNUC_UNUSED)
{
}

LiquidHidDevice *
liquid_driver_hid_get_device(LiquidDriverHid *driver)
{
    g_return_val_if_fail(LIQUID_IS_DRIVER_HID(driver), NULL);

    LiquidDriverHidPrivate *priv = liquid_driver_hid_get_instance_private(driver);

    return priv->hid_device;
}
