#import "P1CLContext.h"


G_DEFINE_TYPE(P1CLContext, p1_cl_context, G_TYPE_OBJECT)
static GObjectClass *parent_class;

static void p1_cl_context_dispose(GObject *gobject);
static void p1_cl_context_finalize(GObject *gobject);


static void p1_cl_context_class_init(P1CLContextClass *klass)
{
    parent_class = g_type_class_ref(G_TYPE_OBJECT);

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose  = p1_cl_context_dispose;
    gobject_class->finalize = p1_cl_context_finalize;
}

static void p1_cl_context_init(P1CLContext *self)
{
    self->context = NULL;
    g_mutex_init(&self->lock);

    self->parent = NULL;
}

static void p1_cl_context_dispose(GObject *gobject)
{
    P1CLContext *self = P1_CL_CONTEXT(gobject);

    if (self->context) {
        clReleaseContext(self->context);
        self->context = NULL;
    }

    if (self->parent) {
        g_object_unref(self->parent);
        self->parent = NULL;
    }

    parent_class->dispose(gobject);
}

static void p1_cl_context_finalize(GObject *gobject)
{
    P1CLContext *self = P1_CL_CONTEXT(gobject);

    g_mutex_clear(&self->lock);
}

P1CLContext *p1_cl_context_new()
{
    cl_device_id device_id;
    cl_uint num_devices;
    cl_int err = clGetDeviceIDs(NULL, CL_DEVICE_TYPE_ALL, 1, &device_id, &num_devices);
    g_return_val_if_fail(err == CL_SUCCESS, NULL);

    // FIXME: log with gstreamer
    cl_context context = clCreateContext(NULL, num_devices, &device_id, clLogMessagesToStdoutAPPLE, NULL, NULL);
    g_return_val_if_fail(context != NULL, NULL);

    P1CLContext *obj = g_object_new(P1_TYPE_CL_CONTEXT, NULL);
    obj->context = context;
    return obj;
}

P1CLContext *p1_cl_context_new_shared_with_gl(P1GLContext *other)
{
    CGLContextObj raw_gl_context = p1_gl_context_get_raw(other);
    CGLShareGroupObj share_group = CGLGetShareGroup(raw_gl_context);
    cl_context_properties props[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties)share_group,
        0
    };

    cl_context context = clCreateContext(props, 0, NULL, clLogMessagesToStdoutAPPLE, NULL, NULL);
    g_return_val_if_fail(context != NULL, NULL);

    P1CLContext *obj = g_object_new(P1_TYPE_CL_CONTEXT, NULL);
    obj->context = context;
    obj->parent = g_object_ref(other);
    return obj;
}

P1CLContext *p1_cl_context_new_existing(cl_context context)
{
    P1CLContext *obj = g_object_new(P1_TYPE_CL_CONTEXT, NULL);
    obj->context = clRetainContext(context);
    return obj;
}
