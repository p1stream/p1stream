// Create a GstBuffer from an IOSurface plane.
GstBuffer *gst_buffer_new_iosurface(IOSurfaceRef buffer, GstMemoryFlags flags);

// Get the IOSurface backing a GstBuffer.
IOSurfaceRef gst_buffer_get_iosurface(GstBuffer *buffer);


// Static initialization to be called once.
void p1_iosurface_allocator_static_init();
