#include "P1GLContext.h"


#define P1_TYPE_CL_CONTEXT \
    (p1_cl_context_get_type())
#define P1_CL_CONTEXT_CAST(obj) \
    ((P1CLContext *)(obj))
#define P1_CL_CONTEXT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1_TYPE_CL_CONTEXT, P1CLContext))
#define P1_CL_CONTEXT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1_TYPE_CL_CONTEXT, P1CLContextClass))
#define P1_IS_CL_CONTEXT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1_TYPE_CL_CONTEXT))
#define P1_IS_CL_CONTEXT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1_TYPE_CL_CONTEXT))
#define P1_CL_CONTEXT_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1_TYPE_CL_CONTEXT, P1CLContextClass))

typedef struct _P1CLContext P1CLContext;
typedef struct _P1CLContextClass P1CLContextClass;

struct _P1CLContext
{
    GObject parent_instance;

    /*< private >*/
    cl_context context;
    GMutex lock;
};

struct _P1CLContextClass
{
    GObjectClass parent_class;
};

GType p1_cl_context_get_type();


#define p1_cl_context_lock(self) \
    g_mutex_lock(&P1_CL_CONTEXT_CAST(self)->lock);

#define p1_cl_context_unlock(self) \
    g_mutex_unlock(&P1_CL_CONTEXT_CAST(self)->lock);

#define p1_cl_context_get_raw(self) \
    (P1_CL_CONTEXT_CAST(self)->context)

P1CLContext *p1_cl_context_new();
P1CLContext *p1_cl_context_new_shared_with_gl(P1GLContext *other);
P1CLContext *p1_cl_context_new_existing(cl_context context);
