// Create a GstBuffer from an IOSurface plane.
GstBuffer *p1g_buffer_new_with_iosurface(IOSurfaceRef buffer, GstMemoryFlags flags);

// Get the IOSurface backing a GstBuffer.
IOSurfaceRef p1g_buffer_get_iosurface(GstBuffer *buffer);


// Static initialization to be called once.
void p1g_iosurface_allocator_static_init();