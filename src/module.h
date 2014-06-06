#ifndef p1_constants_h
#define p1_constants_h

#include <v8.h>

namespace p1stream {


using namespace v8;

extern Persistent<String> width_sym;
extern Persistent<String> height_sym;
extern Persistent<String> x264_preset_sym;
extern Persistent<String> x264_tuning_sym;
extern Persistent<String> x264_params_sym;
extern Persistent<String> x264_profile_sym;
extern Persistent<String> source_sym;
extern Persistent<String> x1_sym;
extern Persistent<String> y1_sym;
extern Persistent<String> x2_sym;
extern Persistent<String> y2_sym;
extern Persistent<String> u1_sym;
extern Persistent<String> v1_sym;
extern Persistent<String> u2_sym;
extern Persistent<String> v2_sym;
extern Persistent<String> data_sym;
extern Persistent<String> type_sym;
extern Persistent<String> offset_sym;
extern Persistent<String> size_sym;
extern Persistent<String> on_frame_sym;
extern Persistent<String> on_error_sym;


} // namespace p1stream

#endif  // p1_constants_h
