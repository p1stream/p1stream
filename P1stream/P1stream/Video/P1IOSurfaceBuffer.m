#import "P1IOSurfaceBuffer.h"


#define P1G_TYPE_IOSURFACE_ALLOCATOR \
    (p1g_iosurface_allocator_get_type())
#define P1G_IOSURFACE_ALLOCATOR(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), P1G_TYPE_IOSURFACE_ALLOCATOR, P1GIOSurfaceAllocator))
#define P1G_IOSURFACE_ALLOCATOR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),  P1G_TYPE_IOSURFACE_ALLOCATOR, P1GIOSurfaceAllocatorClass))
#define P1G_IS_IOSURFACE_ALLOCATOR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), P1G_TYPE_IOSURFACE_ALLOCATOR))
#define P1G_IS_IOSURFACE_ALLOCATOR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),  P1G_TYPE_IOSURFACE_ALLOCATOR))
#define P1G_IOSURFACE_ALLOCATOR_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),  P1G_TYPE_IOSURFACE_ALLOCATOR, P1GIOSurfaceAllocatorClass))

typedef struct _P1GIOSurfaceAllocator P1GIOSurfaceAllocator;
typedef struct _P1GIOSurfaceAllocatorClass P1GIOSurfaceAllocatorClass;
typedef struct _P1GMemoryIOSurface P1GMemoryIOSurface;

struct _P1GIOSurfaceAllocator
{
    GstAllocator parent_instance;
};

struct _P1GIOSurfaceAllocatorClass
{
    GstAllocatorClass parent_class;
};

GType p1g_iosurface_allocator_get_type();


G_DEFINE_TYPE(P1GIOSurfaceAllocator, p1g_iosurface_allocator, GST_TYPE_ALLOCATOR);
static GstAllocator *allocator_singleton;

struct _P1GMemoryIOSurface
{
    GstMemory mem;
    IOSurfaceRef buffer;
    size_t planeIndex;
};

GstMemory *p1g_memory_iosurface_new(IOSurfaceRef buffer, size_t planeIndex);
static void p1g_iosurface_allocator_free(GstAllocator *allocator, GstMemory *mem);
static gpointer p1g_iosurface_allocator_mem_map(GstMemory *mem, gsize maxsize, GstMapFlags flags);
static void p1g_iosurface_allocator_mem_unmap(GstMemory *mem);
static GstMemory * p1g_iosurface_allocator_mem_copy(GstMemory *mem, gssize offset, gssize size);


GstMemory *p1g_memory_iosurface_new(IOSurfaceRef buffer, size_t planeIndex)
{
    // FIXME: Custom flags, but need read/write and sharing support.
    const GstMemoryFlags flags = GST_MEMORY_FLAG_READONLY | GST_MEMORY_FLAG_NO_SHARE;

    size_t stride = IOSurfaceGetBytesPerRowOfPlane(buffer, planeIndex);
    size_t size = IOSurfaceGetHeightOfPlane(buffer, planeIndex) * stride;
    g_assert(size != 0);

    P1GMemoryIOSurface *memio = g_slice_alloc(sizeof(P1GMemoryIOSurface));
    gst_memory_init(GST_MEMORY_CAST(memio), flags, allocator_singleton, NULL, size, 0, 0, size);

    CFRetain(buffer);
    IOSurfaceIncrementUseCount(buffer);
    memio->buffer = buffer;
    memio->planeIndex = planeIndex;

    return (GstMemory *)memio;
}

static void p1g_iosurface_allocator_free(GstAllocator *allocator, GstMemory *mem)
{
    P1GMemoryIOSurface *memio = (P1GMemoryIOSurface *)mem;

    IOSurfaceDecrementUseCount(memio->buffer);
    CFRelease(memio->buffer);

    g_slice_free1(sizeof(P1GMemoryIOSurface), mem);
}

static gpointer p1g_iosurface_allocator_mem_map(GstMemory *mem, gsize maxsize, GstMapFlags flags)
{
    P1GMemoryIOSurface *memio = (P1GMemoryIOSurface *)mem;

    // FIXME: write support
    if (flags & GST_MAP_WRITE)
        return NULL;

    IOReturn ret = IOSurfaceLock(memio->buffer, kIOSurfaceLockReadOnly, NULL);
    if (ret != kIOReturnSuccess)
        return NULL;

    void *data = IOSurfaceGetBaseAddressOfPlane(memio->buffer, memio->planeIndex);
    if (data == NULL) {
        p1g_iosurface_allocator_mem_unmap(mem);
        return NULL;
    }

    return data;
}

static void p1g_iosurface_allocator_mem_unmap(GstMemory *mem)
{
    P1GMemoryIOSurface *memio = (P1GMemoryIOSurface *)mem;

    IOReturn ret = IOSurfaceUnlock(memio->buffer, kIOSurfaceLockReadOnly, NULL);
    g_assert(ret == kIOReturnSuccess);
}

static GstMemory * p1g_iosurface_allocator_mem_copy(GstMemory *mem, gssize offset, gssize size)
{
    GstMemory *copy;
    GstMapInfo sinfo, dinfo;

    if (!gst_memory_map(mem, &sinfo, GST_MAP_READ))
        return NULL;

    if (size == -1)
        size = sinfo.size > offset ? sinfo.size - offset : 0;

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


static void p1g_iosurface_allocator_class_init(P1GIOSurfaceAllocatorClass *klass)
{
    GstAllocatorClass *allocator_klass = GST_ALLOCATOR_CLASS(klass);
    allocator_klass->free = p1g_iosurface_allocator_free;
    // FIXME: alloc
}

static void p1g_iosurface_allocator_init(P1GIOSurfaceAllocator *allocator)
{
    GstAllocator *alloc = GST_ALLOCATOR_CAST(allocator);
    alloc->mem_type = "IOSurface";
    alloc->mem_map = p1g_iosurface_allocator_mem_map;
    alloc->mem_unmap = p1g_iosurface_allocator_mem_unmap;
    alloc->mem_copy = p1g_iosurface_allocator_mem_copy;
    // FIXME: mem_copy, mem_share, mem_is_span
}

GstBuffer *gst_buffer_new_with_iosurface(IOSurfaceRef buffer, size_t planeIndex)
{
    GstMemory *mem = p1g_memory_iosurface_new(buffer, planeIndex);
    GstBuffer *res = gst_buffer_new();
    gst_buffer_append_memory(res, mem);
    return res;
}

void p1g_iosurface_allocator_static_init()
{
    allocator_singleton = g_object_new(P1G_TYPE_IOSURFACE_ALLOCATOR, NULL);
    gst_allocator_register("IOSurface", gst_object_ref(allocator_singleton));
}
