#import "P1IOSurfaceBuffer.h"


#define P1_TYPE_IOSURFACE_ALLOCATOR \
    (p1_iosurface_allocator_get_type())
#define P1_IOSURFACE_ALLOCATOR(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), P1_TYPE_IOSURFACE_ALLOCATOR, P1IOSurfaceAllocator))
#define P1_IOSURFACE_ALLOCATOR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),  P1_TYPE_IOSURFACE_ALLOCATOR, P1IOSurfaceAllocatorClass))
#define P1_IS_IOSURFACE_ALLOCATOR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), P1_TYPE_IOSURFACE_ALLOCATOR))
#define P1_IS_IOSURFACE_ALLOCATOR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),  P1_TYPE_IOSURFACE_ALLOCATOR))
#define P1_IOSURFACE_ALLOCATOR_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),  P1_TYPE_IOSURFACE_ALLOCATOR, P1IOSurfaceAllocatorClass))

typedef struct _P1IOSurfaceAllocator P1IOSurfaceAllocator;
typedef struct _P1IOSurfaceAllocatorClass P1IOSurfaceAllocatorClass;

struct _P1IOSurfaceAllocator
{
    GstAllocator parent_instance;
};

struct _P1IOSurfaceAllocatorClass
{
    GstAllocatorClass parent_class;
};

GType p1_iosurface_allocator_get_type();

G_DEFINE_TYPE(P1IOSurfaceAllocator, p1_iosurface_allocator, GST_TYPE_ALLOCATOR);
static GstAllocator *allocator_singleton;

static GstMemory *p1_iosurface_allocator_alloc(GstAllocator *allocator, gsize size, GstAllocationParams *params);
static void p1_iosurface_allocator_free(GstAllocator *allocator, GstMemory *mem);
static gpointer p1_iosurface_allocator_mem_map(GstMemory *mem, gsize maxsize, GstMapFlags flags);
static void p1_iosurface_allocator_mem_unmap(GstMemory *mem);
static GstMemory *p1_iosurface_allocator_mem_copy(GstMemory *mem, gssize offset, gssize size);
static GstMemory *p1_iosurface_allocator_mem_share(GstMemory *mem, gssize offset, gssize size);
static gboolean p1_iosurface_allocator_mem_is_span(GstMemory *mem1, GstMemory *mem2, gsize *offset);


typedef struct _P1MemoryIOSurface P1MemoryIOSurface;

struct _P1MemoryIOSurface
{
    GstMemory mem;

    IOSurfaceRef buffer;

    uint32_t lock_flags;
};


static void p1_iosurface_allocator_class_init(P1IOSurfaceAllocatorClass *klass)
{
    GstAllocatorClass *allocator_klass = GST_ALLOCATOR_CLASS(klass);
    allocator_klass->free  = p1_iosurface_allocator_free;
    allocator_klass->alloc = p1_iosurface_allocator_alloc;
}

static void p1_iosurface_allocator_init(P1IOSurfaceAllocator *allocator)
{
    GstAllocator *alloc = GST_ALLOCATOR_CAST(allocator);
    alloc->mem_type    = "IOSurface";
    alloc->mem_map     = p1_iosurface_allocator_mem_map;
    alloc->mem_unmap   = p1_iosurface_allocator_mem_unmap;
    alloc->mem_copy    = p1_iosurface_allocator_mem_copy;
    alloc->mem_share   = p1_iosurface_allocator_mem_share;
    alloc->mem_is_span = p1_iosurface_allocator_mem_is_span;
}

static GstMemory *p1_iosurface_allocator_alloc(GstAllocator *allocator, gsize size, GstAllocationParams *params)
{
    CFNumberRef cfSize = CFNumberCreate(NULL, kCFNumberLongType, &size);
    const void *keys[1] = { kIOSurfaceAllocSize };
    const void *values[1] = { cfSize };
    CFDictionaryRef cfProps = CFDictionaryCreate(NULL, keys, values, 1,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    IOSurfaceRef buffer = IOSurfaceCreate(cfProps);
    CFRelease(cfProps);
    CFRelease(cfSize);
    if (!buffer)
        return NULL;

    P1MemoryIOSurface *memio = g_slice_alloc(sizeof(P1MemoryIOSurface));
    gst_memory_init(GST_MEMORY_CAST(memio), params->flags, allocator_singleton, NULL, size, 0, 0, size);

    memio->buffer = buffer;

    return GST_MEMORY_CAST(memio);
}

static void p1_iosurface_allocator_free(GstAllocator *allocator, GstMemory *mem)
{
    P1MemoryIOSurface *memio = (P1MemoryIOSurface *)mem;

    CFRelease(memio->buffer);

    g_slice_free1(sizeof(P1MemoryIOSurface), mem);
}

static gpointer p1_iosurface_allocator_mem_map(GstMemory *mem, gsize maxsize, GstMapFlags flags)
{
    P1MemoryIOSurface *memio = (P1MemoryIOSurface *)mem;

    uint32_t lock_flags;
    if (flags & GST_MAP_WRITE)
        lock_flags = 0;
    else
        lock_flags = kIOSurfaceLockReadOnly;

    IOReturn ret = IOSurfaceLock(memio->buffer, lock_flags, NULL);
    if (ret != kIOReturnSuccess)
        return NULL;

    memio->lock_flags = lock_flags;

    void *data = IOSurfaceGetBaseAddress(memio->buffer);
    if (data == NULL) {
        p1_iosurface_allocator_mem_unmap(mem);
        return NULL;
    }

    return data;
}

static void p1_iosurface_allocator_mem_unmap(GstMemory *mem)
{
    P1MemoryIOSurface *memio = (P1MemoryIOSurface *)mem;

    IOReturn ret = IOSurfaceUnlock(memio->buffer, memio->lock_flags, NULL);
    g_assert(ret == kIOReturnSuccess);
}

static GstMemory * p1_iosurface_allocator_mem_copy(GstMemory *mem, gssize offset, gssize size)
{
    GstMemory *copy;
    GstMapInfo sinfo, dinfo;

    if (!gst_memory_map(mem, &sinfo, GST_MAP_READ))
        return NULL;

    if (size == -1)
        size = sinfo.size > offset ? sinfo.size - offset : 0;

    // Use the default allocator, don't create another IOSurface.
    copy = gst_allocator_alloc(NULL, size, NULL);
    if (gst_memory_map(copy, &dinfo, GST_MAP_WRITE)) {
        memcpy(dinfo.data, sinfo.data + offset, size);
        gst_memory_unmap(copy, &dinfo);
    }
    else {
        gst_allocator_free(copy->allocator, copy);
        copy = NULL;
    }

    gst_memory_unmap(mem, &sinfo);
    
    return copy;
}

static GstMemory *p1_iosurface_allocator_mem_share(GstMemory *mem, gssize offset, gssize size)
{
    P1MemoryIOSurface *memio = (P1MemoryIOSurface *)mem;

    GstMemory *parent = mem->parent;
    if (parent == NULL)
        parent = mem;

    const GstMemoryFlags flags = GST_MINI_OBJECT_FLAGS(parent) | GST_MINI_OBJECT_FLAG_LOCK_READONLY;
    if (size == -1)
        size = mem->size - offset;
    offset += mem->offset;

    P1MemoryIOSurface *sub = g_slice_alloc(sizeof(P1MemoryIOSurface));
    gst_memory_init(GST_MEMORY_CAST(sub), flags, allocator_singleton, parent, mem->maxsize, 0, offset, size);

    CFRetain(memio->buffer);
    sub->buffer = memio->buffer;

    return GST_MEMORY_CAST(sub);
}

static gboolean p1_iosurface_allocator_mem_is_span(GstMemory *a, GstMemory *b, gsize *offset)
{
    if (offset)
        *offset = a->offset;

    if (a->parent == b->parent)
        return a->offset + a->size == b->offset;
    else
        return FALSE;
}


GstBuffer *gst_buffer_new_iosurface(IOSurfaceRef buffer, GstMemoryFlags flags)
{
    size_t size = IOSurfaceGetAllocSize(buffer);
    g_assert(size != 0);

    P1MemoryIOSurface *memio = g_slice_alloc(sizeof(P1MemoryIOSurface));
    gst_memory_init(GST_MEMORY_CAST(memio), flags, allocator_singleton, NULL, size, 0, 0, size);

    CFRetain(buffer);
    memio->buffer = buffer;

    GstBuffer *res = gst_buffer_new();
    gst_buffer_append_memory(res, GST_MEMORY_CAST(memio));
    return res;
}

IOSurfaceRef gst_buffer_get_iosurface(GstBuffer *buffer)
{
    if (gst_buffer_n_memory(buffer) == 1) {
        GstMemory *mem = gst_buffer_get_memory(buffer, 0);
        if (mem->allocator == allocator_singleton)
            return ((P1MemoryIOSurface *)mem)->buffer;
    }
    return NULL;
}


void p1_iosurface_allocator_static_init()
{
    allocator_singleton = g_object_new(P1_TYPE_IOSURFACE_ALLOCATOR, NULL);
    gst_allocator_register("IOSurface", gst_object_ref(allocator_singleton));
}
