/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* The implementation that should be used for integer audio resampling witll
   be benchmarked at runtime */
#define AUDIORESAMPLE_FORMAT_AUTO 1

/* The float implementation should be used for integer audio resampling */
/* #undef AUDIORESAMPLE_FORMAT_FLOAT */

/* The int implementation should be used for integer audio resampling */
/* #undef AUDIORESAMPLE_FORMAT_INT */

/* defined if cdda headers are in a cdda/ directory */
/* #undef CDPARANOIA_HEADERS_IN_DIR */

/* Default audio sink */
#define DEFAULT_AUDIOSINK "autoaudiosink"

/* Default audio source */
#define DEFAULT_AUDIOSRC "osxaudiosrc"

/* Default video sink */
#define DEFAULT_VIDEOSINK "autovideosink"

/* Default video source */
#define DEFAULT_VIDEOSRC "v4l2src"

/* Default visualizer */
#define DEFAULT_VISUALIZER "goom"

/* Disable Orc */
/* #undef DISABLE_ORC */

/* gettext package name */
#define GETTEXT_PACKAGE "NULL"

/* The GIO library directory. */
#define GIO_LIBDIR "/usr/local/Cellar/glib/2.34.3/lib"

/* The GIO modules directory. */
#define GIO_MODULE_DIR "/usr/local/Cellar/glib/2.34.3/lib/gio/modules"

/* set to disable libxml2-dependent code in subparse */
#define GST_DISABLE_XML 1

/* plugin install helper script */
#define GST_INSTALL_PLUGINS_HELPER "DISABLED"

/* Define to enable ALSA (used by alsa). */
/* #undef HAVE_ALSA */

/* Define to enable CDParanoia (used by cdparanoia). */
/* #undef HAVE_CDPARANOIA */

/* Define to 1 if you have the <emmintrin.h> header file. */
#define HAVE_EMMINTRIN_H 1

/* Define to enable building of experimental plug-ins. */
/* #undef HAVE_EXPERIMENTAL */

/* Define to enable building of plug-ins with external deps. */
#define HAVE_EXTERNAL /**/

/* make use of iso-codes for ISO-639 */
/* #undef HAVE_ISO_CODES */

/* Define to enable integer vorbis plug-in (used by ivorbisdec). */
/* #undef HAVE_IVORBIS */

/* Define to 1 if you have the `asound' library (-lasound). */
/* #undef HAVE_LIBASOUND */

/* Define to enable libvisual visualization library (used by libvisual). */
/* #undef HAVE_LIBVISUAL */

/* Define to 1 if you have the `log2' function. */
#define HAVE_LOG2 1

/* Define if you have C99's lrint function. */
#define HAVE_LRINT 1

/* Define if you have C99's lrintf function. */
#define HAVE_LRINTF 1

/* Define to enable Xiph Ogg library (used by ogg). */
#define HAVE_OGG /**/

/* Use Orc */
#define HAVE_ORC 1

/* Define to enable Pango font rendering (used by pango). */
/* #undef HAVE_PANGO */

/* Define to enable Xiph Theora video codec (used by theora). */
#define HAVE_THEORA /**/

/* Define to enable Xiph Vorbis audio codec (used by vorbis). */
#define HAVE_VORBIS /**/

/* defined if vorbis_synthesis_restart is present */
#define HAVE_VORBIS_SYNTHESIS_RESTART 1

/* Define to enable X libraries and plugins (used by ximagesink). */
/* #undef HAVE_X */

/* Define to 1 if you have the <xmmintrin.h> header file. */
#define HAVE_XMMINTRIN_H 1

/* Define to enable X Shared Memory extension. */
/* #undef HAVE_XSHM */

/* Define to enable X11 XVideo extensions (used by xvimagesink). */
/* #undef HAVE_XVIDEO */

/* Define to enable zlib support for ID3 parsing in libgsttag. */
#define HAVE_ZLIB /**/

/* prefix */
/* #undef ISO_CODES_PREFIX */

/* */
/* #undef ISO_CODES_VERSION */

/* directory in which the detected libvisual's plugins are located */
/* #undef LIBVISUAL_PLUGINSBASEDIR */

/* gettext locale dir */
#define LOCALEDIR "DISABLED"

/* directory where plugins are located */
#define PLUGINDIR "DISABLED"

/* "Define if building for android" */
/* #undef USE_TREMOLO */

/* Define to 1 if the X Window System is missing or not being used. */
#define X_DISPLAY_MISSING 1
