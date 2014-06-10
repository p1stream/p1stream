#ifndef p1_constants_h
#define p1_constants_h

#include "core_types.h"

namespace p1stream {


using namespace v8;

extern Persistent<String> buffer_size_sym;
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
extern Persistent<String> buf_sym;
extern Persistent<String> frames_sym;
extern Persistent<String> pts_sym;
extern Persistent<String> dts_sym;
extern Persistent<String> nals_sym;
extern Persistent<String> type_sym;
extern Persistent<String> priority_sym;
extern Persistent<String> offset_sym;
extern Persistent<String> size_sym;
extern Persistent<String> on_data_sym;
extern Persistent<String> on_error_sym;

extern fraction_t mach_timebase;


} // namespace p1stream

#endif  // p1_constants_h
