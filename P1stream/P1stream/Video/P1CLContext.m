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
}

static void p1_cl_context_dispose(GObject *gobject)
{
    P1CLContext *self = P1_CL_CONTEXT(gobject);

    if (self->context) {
        clReleaseContext(self->context);
        self->context = NULL;
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
    P1CLContext *obj = g_object_new(P1_TYPE_CL_CONTEXT, NULL);
    obj->context = NULL;
    return obj;
}
