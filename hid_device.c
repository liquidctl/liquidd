#include "hid_device.h"

#include <fcntl.h>
#include <unistd.h>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

/* From Linux kernel's include/linux/hid.h */
#define HID_MIN_BUFFER_SIZE 64
#define HID_MAX_BUFFER_SIZE 16384

struct _LiquidHidDevice
{
    GObject parent;

    guint max_input_report_size;
    GInputStream *input_stream;
    GCancellable *read_cancellable;
    GOutputStream *output_stream;
};

G_DEFINE_FINAL_TYPE(LiquidHidDevice, liquid_hid_device, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_MAX_INPUT_REPORT_SIZE,
    PROP_INPUT_STREAM,
    PROP_OUTPUT_STREAM,
    N_PROPERTIES
};

static GParamSpec *pspecs[N_PROPERTIES];

enum
{
    SIGNAL_INPUT_REPORT,
    SIGNAL_ERROR,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static GQuark byte_quarks[G_MAXUINT8 + 1];

static void
liquid_hid_device_read_input_report(LiquidHidDevice *device);

static void
liquid_hid_device_input_report_ready(GObject *source_object, GAsyncResult *result, gpointer user_data)
{
    GInputStream *stream = G_INPUT_STREAM(source_object);
    GWeakRef *device_weak = user_data;

    g_autoptr(LiquidHidDevice) device = g_weak_ref_get(device_weak);
    g_weak_ref_clear(device_weak);
    g_free(device_weak);

    g_autoptr(GError) error = NULL;
    g_autoptr(GBytes) bytes = g_input_stream_read_bytes_finish(stream, result, &error);

    if (device == NULL)
    {
        return;
    }

    if (error)
    {
        g_signal_emit(device, signals[SIGNAL_ERROR], 0, error);
    }

    if (bytes)
    {
        const guint8 *data = g_bytes_get_data(bytes, NULL);
        GQuark detail = g_bytes_get_size(bytes) > 0 ? byte_quarks[*data] : 0;
        gboolean return_value = FALSE;
        g_signal_emit(device, signals[SIGNAL_INPUT_REPORT], detail, bytes, &return_value);
        liquid_hid_device_read_input_report(device);
    }
}

static void
liquid_hid_device_read_input_report(LiquidHidDevice *device)
{
    g_return_if_fail(device->input_stream != NULL);
    g_return_if_fail(device->read_cancellable != NULL);

    GWeakRef *device_weak = g_new(GWeakRef, 1);
    g_weak_ref_init(device_weak, device);

    g_input_stream_read_bytes_async(device->input_stream, /* stream */
                                    device->max_input_report_size, /* count */
                                    G_PRIORITY_DEFAULT, /* io_priority */
                                    device->read_cancellable, /* cancellable */
                                    liquid_hid_device_input_report_ready, /* callback */
                                    device_weak /* user_data */);
}

static void
liquid_hid_device_dispose(GObject *object)
{
    LiquidHidDevice *device = LIQUID_HID_DEVICE(object);

    g_cancellable_cancel(device->read_cancellable);
    g_clear_object(&device->output_stream);

    G_OBJECT_CLASS(liquid_hid_device_parent_class)->dispose(object);
}

static void
liquid_hid_device_finalize(GObject *object)
{
    LiquidHidDevice *device = LIQUID_HID_DEVICE(object);

    g_clear_object(&device->read_cancellable);
    g_clear_object(&device->input_stream);

    G_OBJECT_CLASS(liquid_hid_device_parent_class)->finalize(object);
}

static void
liquid_hid_device_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    LiquidHidDevice *device = LIQUID_HID_DEVICE(object);

    switch (property_id)
    {
    case PROP_MAX_INPUT_REPORT_SIZE:
        g_value_set_uint(value, device->max_input_report_size);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
liquid_hid_device_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    LiquidHidDevice *device = LIQUID_HID_DEVICE(object);

    switch (property_id)
    {
    case PROP_MAX_INPUT_REPORT_SIZE:
        device->max_input_report_size = g_value_get_uint(value);
        break;

    case PROP_INPUT_STREAM:
        g_set_object(&device->input_stream, g_value_get_object(value));
        break;

    case PROP_OUTPUT_STREAM:
        g_set_object(&device->output_stream, g_value_get_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
liquid_hid_device_constructed(GObject *object)
{
    LiquidHidDevice *device = LIQUID_HID_DEVICE(object);

    liquid_hid_device_read_input_report(device);

    G_OBJECT_CLASS(liquid_hid_device_parent_class)->constructed(object);
}

static void
liquid_hid_device_class_init(LiquidHidDeviceClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(class);

    gobject_class->dispose = liquid_hid_device_dispose;
    gobject_class->finalize = liquid_hid_device_finalize;
    gobject_class->get_property = liquid_hid_device_get_property;
    gobject_class->set_property = liquid_hid_device_set_property;
    gobject_class->constructed = liquid_hid_device_constructed;

    pspecs[PROP_MAX_INPUT_REPORT_SIZE]
        = g_param_spec_uint("max-input-report-size", /* name */
                            "Maximum input report size", /* nick */
                            "Maximum HID input report size, in bytes", /* blurb */
                            HID_MIN_BUFFER_SIZE, /* minimum */
                            HID_MAX_BUFFER_SIZE, /* maximum */
                            HID_MAX_BUFFER_SIZE, /* default_value */
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    pspecs[PROP_INPUT_STREAM]
        = g_param_spec_object("input-stream", /* name */
                              "Input stream", /* nick */
                              "HID device input stream", /* blurb */
                              G_TYPE_INPUT_STREAM, /* object_type */
                              G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    pspecs[PROP_OUTPUT_STREAM]
        = g_param_spec_object("output-stream", /* name */
                              "Output stream", /* nick */
                              "HID device output stream", /* blurb */
                              G_TYPE_OUTPUT_STREAM, /* object_type */
                              G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, pspecs);

    signals[SIGNAL_INPUT_REPORT]
        = g_signal_new("input-report", /* signal_name */
                       G_TYPE_FROM_CLASS(class), /* itype */
                       G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, /* signal_flags */
                       0, /* class_offset */
                       g_signal_accumulator_true_handled, /* accumulator */
                       NULL, /* accu_data */
                       NULL, /* c_marshaller */
                       G_TYPE_BOOLEAN, /* return_type */
                       1, /* n_params */
                       G_TYPE_BYTES);

    signals[SIGNAL_ERROR]
        = g_signal_new("error", /* signal_name */
                       G_TYPE_FROM_CLASS(class), /* itype */
                       G_SIGNAL_RUN_LAST, /* signal_flags */
                       0, /* class_offset */
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
liquid_hid_device_init(LiquidHidDevice *device)
{
    device->read_cancellable = g_cancellable_new();
}

LiquidHidDevice *
liquid_hid_device_new_for_fd(int fd, guint max_input_report_size)
{
    GInputStream *input_stream = g_unix_input_stream_new(fd, TRUE);

    /* TODO: output stream shouldn't poll - writability poll is broken for hidraw on older kernels */
    GOutputStream *output_stream = g_unix_output_stream_new(fd, FALSE);

    return g_object_new(LIQUID_TYPE_HID_DEVICE,
                        "input-stream",
                        input_stream,
                        "output-stream",
                        output_stream,
                        "max-input-report-size",
                        max_input_report_size,
                        NULL);
}

LiquidHidDevice *
liquid_hid_device_new_for_path(const char *path, guint max_input_report_size, GError **error)
{
    int fd = open(path, O_RDWR);

    if (fd == -1)
    {
        int errsv = errno;
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errsv), "open: %s", g_strerror(errsv));
        return NULL;
    }

    return liquid_hid_device_new_for_fd(fd, max_input_report_size);
}

gboolean
liquid_hid_device_output_report(LiquidHidDevice *device, const void *buffer, gsize count, GError **error)
{
    return g_output_stream_write(device->output_stream, buffer, count, NULL, error) >= 0;
}
