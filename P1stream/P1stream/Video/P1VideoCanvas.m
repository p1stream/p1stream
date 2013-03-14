#import "P1VideoCanvas.h"
#import "P1DesktopVideoSource.h"

#include "RGBAtoYUV420.cl.h"


struct VertexDrawData
{
    GLfloat position[2];
    GLfloat texCoords[2];
};

struct SlotDrawData
{
    struct VertexDrawData vertices[4];
};


@implementation P1VideoSourceSlot

@synthesize source, context, textureName, drawArea;

- (id)initForSource:(id<P1VideoSource>)source_ withContext:(NSOpenGLContext *)context_ withDrawArea:(CGRect)drawArea_
{
    self = [super init];
    if (self) {
        source = source_;
        context = context_;
        drawArea = drawArea_;

        [context makeCurrentContext];
        glGenTextures(1, &textureName);
        glBindTexture(GL_TEXTURE_RECTANGLE, textureName);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    return self;
}

- (void)dealloc
{
    if (context && textureName) {
        [context makeCurrentContext];
        glDeleteTextures(1, &textureName);
    }
}

@end


@implementation P1VideoCanvas

@synthesize slots, delegate;

- (id)init
{
    self = [super init];
    if (self) {
        slots = [[NSMutableArray alloc] init];

        // The OpenGL context.
        NSOpenGLPixelFormatAttribute attributes[] = {
            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
			(NSOpenGLPixelFormatAttribute) 0
        };
        pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
        if (!pixelFormat) {
            NSLog(@"Failed to create pixel format for video canvas.");
            return nil;
        }
        context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];
        if (!context) {
            NSLog(@"Failed to create OpenGL context for video canvas.");
            return nil;
        }
        gcl_gl_set_sharegroup(CGLGetShareGroup([context CGLContextObj]));

        // Semaphore protecting buffers.
        semaphore = dispatch_semaphore_create(P1_VIDEO_CANVAS_NUM_BUFFERS);
        if (!semaphore) {
            NSLog(@"Failed to create video canvas buffer semaphore.");
            return nil;
        }

        [context makeCurrentContext];

        // Black background.
        glClearColor(0, 0, 0, 1);

        // VBO for all drawing area vertex coordinates.
        glGenBuffers(1, &vertexBufferName);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferName);

        // VAO for the above, with interleaved position and texture coordinates.
        const GLsizei stride = sizeof(struct VertexDrawData);
        const void *texCoordsOffset = (void *)offsetof(struct VertexDrawData, texCoords);
        glGenVertexArrays(1, &vertexArrayName);
        glBindVertexArray(vertexArrayName);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, 0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, texCoordsOffset);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        // FBO with just a single color attachment.
        glGenFramebuffers(1, &frameBufferName);
        glBindFramebuffer(GL_FRAMEBUFFER, frameBufferName);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        // Set up renderbuffers.
        GLuint renderBufferNames[P1_VIDEO_CANVAS_NUM_BUFFERS];
        glGenRenderbuffers(P1_VIDEO_CANVAS_NUM_BUFFERS, renderBufferNames);
        for (size_t i = 0; i < P1_VIDEO_CANVAS_NUM_BUFFERS; i++) {
            struct P1VideoCanvasBuffer *buffer = &buffers[i];
            buffer->renderBufferName = renderBufferNames[i];
        }

        // Dirt simple shader.
        shaderProgram = glCreateProgram();
        glBindAttribLocation(shaderProgram, 0, "a_Position");
        glBindAttribLocation(shaderProgram, 1, "a_TexCoords");
        glBindFragDataLocation(shaderProgram, 0, "o_FragColor");
        if (!buildShaderProgram(shaderProgram, @"simple")) {
            NSLog(@"Failed to build video canvas shader program.");
            return nil;
        }
        glUseProgram(shaderProgram);

        // Always use texture unit 0.
        GLint textureUniform = glGetUniformLocation(shaderProgram, "u_Texture");
        glUniform1i(textureUniform, 0);
        glActiveTexture(GL_TEXTURE0);
        
        if (checkAndLogGLError(@"video canvas init")) return nil;;
    }
    return self;
}

- (void)dealloc
{
    if (clockSource) {
        [clockSource setDelegate:nil];
    }

    // Ensure the sources are dealloc'd before the OpenGL context.
    if (slots) {
        [slots removeAllObjects];
    }

    for (size_t i = 0; i < P1_VIDEO_CANVAS_NUM_BUFFERS; i++) {
        struct P1VideoCanvasBuffer *buffer = &buffers[i];
        if (buffer->clInput) {
            gcl_release_image(buffer->clInput);
        }
        if (buffer->clOutput) {
            gcl_free(buffer->clOutput);
        }
    }
}

- (CGSize)frameSize
{
    @synchronized(self) {
        return frameSize;
    }
}

- (void)setFrameSize:(CGSize)frameSize_
{
    // Claim all buffers.
    for (size_t i = 0; i < P1_VIDEO_CANVAS_NUM_BUFFERS; i++) {
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    }

    @synchronized(self) {
        for (size_t i = 0; i < P1_VIDEO_CANVAS_NUM_BUFFERS; i++) {
            struct P1VideoCanvasBuffer *buffer = &buffers[i];
            if (buffer->clInput) {
                gcl_release_image(buffer->clInput);
            }
            if (buffer->clOutput) {
                gcl_free(buffer->clOutput);
            }
        }

        [context makeCurrentContext];

        // Round to nearest multiple of 2.
        size_t halfWidth = frameSize_.width / 2;
        size_t halfHeight = frameSize_.height / 2;
        frameSize = CGSizeMake(halfWidth * 2, halfHeight * 2);
        glViewport(0, 0, frameSize.width, frameSize.height);

        // Calculate output buffer sizes.
        size_t pixelSize = frameSize.width * frameSize.height;
        outputSizeARGB = pixelSize * 4;
        outputSizeYUV = pixelSize * 1.5;
        
        // CL kernel range parameter. A work item is 2x2 pixel block.
        clRange = (cl_ndrange){
            .work_dim = 2,
            .global_work_offset = { 0, 0 },
            .global_work_size = { halfWidth, halfHeight, 0 }
        };

        // Create CL buffers.
        for (size_t i = 0; i < P1_VIDEO_CANVAS_NUM_BUFFERS; i++) {
            struct P1VideoCanvasBuffer *buffer = &buffers[i];

            glBindRenderbuffer(GL_RENDERBUFFER, buffer->renderBufferName);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, frameSize.width, frameSize.height);

            buffer->clInput = gcl_gl_create_image_from_renderbuffer(buffer->renderBufferName);
            buffer->clOutput = gcl_malloc(outputSizeYUV, NULL, CL_MEM_WRITE_ONLY);
        }

        checkAndLogGLError(@"video canvas frame alloc");
    }

    // Release all buffers.
    for (size_t i = 0; i < P1_VIDEO_CANVAS_NUM_BUFFERS; i++) {
        dispatch_semaphore_signal(semaphore);
    }
}

- (P1VideoSourceSlot *)addSource:(id<P1VideoSource>)source withDrawArea:(CGRect)drawArea
{
    @synchronized(self) {
        NSOpenGLContext *sharedContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat
                                                                    shareContext:context];
        if (!sharedContext) {
            NSLog(@"Failed to create shared OpenGL context for new video source.");
            return nil;
        }

        P1VideoSourceSlot *slot = [[P1VideoSourceSlot alloc]
                                   initForSource:source withContext:sharedContext withDrawArea:drawArea];
        if (!slot) {
            NSLog(@"Failed to create slot for new video source.");
            return nil;
        }
        [source setSlot:slot];
        [slots addObject:slot];

        [self updateSourceState];

        return slot;
    }
}

- (void)removeAllSources
{
    @synchronized(self) {
        for (P1VideoSourceSlot *slot in slots) {
            [slot.source setSlot:nil];
        }
        [slots removeAllObjects];

        [self updateSourceState];
    }
}

// Private. Sources were updated, refresh state.
- (void)updateSourceState
{
    // Determine a new clock source.
    // FIXME: More intelligent logic? User choice?
    id<P1VideoSource> newSource = nil;
    for (P1VideoSourceSlot *slot in slots) {
        id <P1VideoSource> source = slot.source;
        if ([source hasClock]) {
            newSource = source;
            break;
        }
    }
    // FIXME: dummy clock source if nil.

    // Swap clock source.
    if (newSource != clockSource) {
        if (clockSource) {
            [clockSource setDelegate:nil];
        }
        clockSource = newSource;
        if (clockSource) {
            [clockSource setDelegate:self];
        }
    }

    [context makeCurrentContext];

    // Update the VBO.
    size_t vboSize = [slots count] * sizeof(struct SlotDrawData);
    struct SlotDrawData *vboData = alloca(vboSize);
    size_t idx = 0;
    for (P1VideoSourceSlot *slot in slots) {
        const CGRect area = slot.drawArea;
        const CGPoint origin = area.origin;
        const CGSize size = area.size;
        vboData[idx++] = (struct SlotDrawData){
            .vertices = {
                {
                    .position = {
                        origin.x,
                        origin.y
                    },
                    .texCoords = { 0, 0 }
                },
                {
                    .position = {
                        origin.x + size.width,
                        origin.y
                    },
                    .texCoords = { 1, 0 }
                },
                {
                    .position = {
                        origin.x,
                        origin.y + size.height
                    },
                    .texCoords = { 0, 1 }
                },
                {
                    .position = {
                        origin.x + size.width,
                        origin.y + size.height
                    },
                    .texCoords = { 1, 1 }
                }
            }
        };
    };
    glBufferData(GL_ARRAY_BUFFER, vboSize, vboData, GL_STATIC_DRAW);
    checkAndLogGLError(@"video canvas source state init");
}

- (void)videoSourceClockTick
{
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    
    // No use if we don't have a delegate.
    id delegate_ = [self delegate];
    if (!delegate_) return;

    // Claim a buffer, or drop the frame if we have none.
    if (dispatch_semaphore_wait(semaphore, DISPATCH_TIME_NOW) != 0) return;
    struct P1VideoCanvasBuffer *buffer;
    @synchronized(self) {
        for (size_t i = 0; i < P1_VIDEO_CANVAS_NUM_BUFFERS; i++) {
            buffer = &buffers[i];
            if (buffer->inuse != TRUE) {
                buffer->inuse = TRUE;
                break;
            }
        }

        [context makeCurrentContext];

        // Render a frame.
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, buffer->renderBufferName);
        glClear(GL_COLOR_BUFFER_BIT);
        GLint idx = 0;
        for (P1VideoSourceSlot *slot in slots) {
            glBindTexture(GL_TEXTURE_RECTANGLE, slot.textureName);
            glDrawArrays(GL_TRIANGLE_STRIP, idx, 4);
            idx += 4;
        }
        glFinish();

        // Download RGBA and callback async.
        if ([delegate_ respondsToSelector:@selector(videoCanvasFrameARGB)]) {
            void *outputARGB = [delegate_ getVideoCanvasOutputBufferARGB:outputSizeARGB
                                                          withDimensions:frameSize];
            if (outputARGB) {
                // FIXME: perhaps we can read from clInput instead, and do this async?
                glReadPixels(0, 0, frameSize.width, frameSize.height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, outputARGB);
                dispatch_async(queue, ^{
                    [delegate_ videoCanvasFrameARGB];
                });
            }
        }

#ifdef DEBUG_EACH_GL_FRAME
        checkAndLogGLError(@"video canvas render");
#endif
    }

    // Async callback with the RGBA buffer.
    dispatch_async(queue, ^{
        // Async convert to YUV420 and callback.
        void *outputYUV = NULL;
        if ([delegate_ respondsToSelector:@selector(videoCanvasFrameYUV)]) {
            outputYUV = [delegate_ getVideoCanvasOutputBufferYUV:outputSizeYUV
                                                  withDimensions:frameSize];
            if (outputYUV) {
                RGBAtoYUV420_kernel(&clRange, buffer->clInput, buffer->clOutput);
                gcl_memcpy(outputYUV, buffer->clOutput, outputSizeYUV);
                [delegate_ videoCanvasFrameYUV];
            }
        }

        // Release the buffer.
        @synchronized(self) {
            buffer->inuse = FALSE;
        }
        dispatch_semaphore_signal(semaphore);
    });
}

- (NSDictionary *)serialize
{
    @synchronized(self) {
        return @{
            @"slots" : [slots mapObjectsWithBlock:^NSDictionary *(P1VideoSourceSlot *slot, NSUInteger idx) {
                id<P1VideoSource> source = slot.source;

                NSString *name;
                Class sourceClass = [source class];
                if (sourceClass == [P1DesktopVideoSource class])
                    name = @"desktop";

                return @{
                    @"type" : name,
                    @"drawArea" : CFBridgingRelease(CGRectCreateDictionaryRepresentation(slot.drawArea)),
                    @"params" : [source serialize]
                };
            }]
        };
    }
}

- (void)deserialize:(NSDictionary *)dict
{
    @synchronized(self) {
        [self removeAllSources];
        for (NSDictionary *slotDict in [dict objectForKey:@"slots"]) {
            Class sourceClass;
            NSString *name = [slotDict objectForKey:@"type"];
            if ([name isEqualToString:@"desktop"])
                sourceClass = [P1DesktopVideoSource class];
            else
                continue;

            id<P1VideoSource> source = [[sourceClass alloc] init];
            [source deserialize:[slotDict objectForKey:@"params"]];

            CGRect drawArea;
            NSDictionary *drawAreaDict = [slotDict objectForKey:@"drawArea"];
            CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)drawAreaDict, &drawArea);

            [self addSource:source withDrawArea:drawArea];
        }
    }
}

@end
