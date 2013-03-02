// Create a GstMemory from an IOSurface plane.
GstMemory *p1g_memory_iosurface_new(IOSurfaceRef buffer, size_t planeIndex);

// Create a GstBuffer from an IOSurface plane.
GstBuffer *gst_buffer_new_with_iosurface(IOSurfaceRef buffer, size_t planeIndex);

// Static initialization to be called once.
void p1g_iosurface_allocator_static_init();