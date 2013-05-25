#import "P1Utils.h"
#import "P1TexturePool.h"

static GQuark gl_context_quark = NULL;
static GQuark context_quark = NULL;

void p1_utils_static_init()
{
    gl_context_quark = g_quark_from_static_string("gl_context");
    context_quark = g_quark_from_static_string("context");
}

GstQuery *gst_query_new_gl_context()
{
    GstStructure *structure = gst_structure_new_id(gl_context_quark,
        context_quark, G_TYPE_OBJECT, NULL, NULL);
    return gst_query_new_custom(GST_QUERY_GL_CONTEXT, structure);
}

P1GLContext *gst_query_get_gl_context(GstQuery *query)
{
    const GstStructure *structure = gst_query_get_structure(query);
    if (structure == NULL)
        return NULL;

    const GValue *val = gst_structure_id_get_value(structure, context_quark);
    if (val == NULL)
        return NULL;

    return g_value_get_object(val);
}

gboolean gst_query_set_gl_context(GstQuery *query, P1GLContext *context)
{
    GstStructure *structure = gst_query_writable_structure(query);
    if (structure == NULL)
        return NULL;

    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_OBJECT);
    g_value_set_object(&val, context);
    gst_structure_id_take_value(structure, context_quark, &val);

    return TRUE;
}

// Compile a shader from a resource.
GLuint p1_build_shader(GLuint type, NSString *resource, NSString *ext)
{
    NSString *path = [[NSBundle mainBundle] pathForResource:resource ofType:ext];
    NSData *data = [NSData dataWithContentsOfFile:path];
    if (!data) return 0;

    GLuint shader = glCreateShader(type);
    const char *source = [data bytes];
    const GLint length = (GLint)[data length];
    glShaderSource(shader, 1, &source, &length);
    glCompileShader(shader);

    GLint logSize = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
    if (logSize) {
        GLchar *log = malloc(logSize);
        if (log) {
            glGetShaderInfoLog(shader, logSize, NULL, log);
            NSLog(@"Shader compiler log for '%@':\n%s", resource, log);
            free(log);
        }
    }

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    g_assert(success == GL_TRUE);
    g_assert(glGetError() == GL_NO_ERROR);

    return shader;
}

// Compiler a program from vertex and fragment shader resources.
void p1_build_shader_program(GLuint program, NSString *resource)
{
    GLuint vertexShader = p1_build_shader(GL_VERTEX_SHADER, resource, @"vert.sl");
    GLuint fragmentShader = p1_build_shader(GL_FRAGMENT_SHADER, resource, @"frag.sl");

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint logSize = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);
    if (logSize) {
        GLchar *log = malloc(logSize);
        if (log) {
            glGetProgramInfoLog(program, logSize, NULL, log);
            NSLog(@"Shader linker log:\n%s", log);
            free(log);
        }
    }

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    g_assert(success == GL_TRUE);
    g_assert(glGetError() == GL_NO_ERROR);
}


// Strip off all metas.
static void gst_query_strip_metas(GstQuery *query)
{
    guint n_metas = gst_query_get_n_allocation_metas(query);
    while (n_metas--) {
        gst_query_remove_nth_allocation_meta(query, n_metas);
    }
}

// Strip of all allocators.
static void gst_query_strip_allocation_params(GstQuery *query)
{
}

// Decide allocation for texture elements.
gboolean p1_decide_texture_allocation(GstQuery *query)
{
    guint i, num;

    // Strip metas and allocation params. We don't use these.
    num = gst_query_get_n_allocation_metas(query);
    for (i = 0; i < num; i++) {
        gst_query_remove_nth_allocation_meta(query, i);
    }
    num = gst_query_get_n_allocation_params(query);
    for (i = 0; i < num; i++) {
        gst_query_set_nth_allocation_param(query, i, NULL, NULL);
    }

    // Keep only texture pools, and select the first in the list.
    GstBufferPool *pool = NULL;
    guint size, min, max;
    num = gst_query_get_n_allocation_pools(query);
    for (i = 0; i < num; i++) {
        gst_query_parse_nth_allocation_pool(query, i, &pool, &size, &min, &max);
        if (P1_IS_TEXTURE_POOL(pool))
            break;

        gst_object_unref(pool);
        pool = NULL;
    }

    // No texture pool, create our own.
    if (pool == NULL) {
        pool = GST_BUFFER_POOL_CAST(p1_texture_pool_new());
        g_return_val_if_fail(pool != NULL, FALSE);
        size = 1;
        min = max = 2;
    }

    // Extract caps, which we need to set on the pool config
    GstCaps *outcaps;
    gst_query_parse_allocation(query, &outcaps, NULL);

    // Build the pool config
    GstStructure *config = gst_buffer_pool_get_config(pool);
    gst_buffer_pool_config_set_params(config, outcaps, size, min, max);
    gst_buffer_pool_set_config(pool, config);

    // Fix the pool selection.
    if (num == 0)
        gst_query_add_allocation_pool(query, pool, size, min, max);
    else
        gst_query_set_nth_allocation_pool(query, 0, pool, size, min, max);
    gst_object_unref(pool);

    return TRUE;
}