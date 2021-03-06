#ifndef p1stream_priv_mac_h
#define p1stream_priv_mac_h

#include "p1stream_priv.h"

namespace p1stream {


class video_mixer_mac : public video_mixer_base {
public:
    video_mixer_mac();

    virtual bool platform_init(Handle<Object> params) final;
    virtual void platform_destroy() final;
    virtual bool activate_gl() final;
};


// ----- Inline implementations -----

inline video_mixer_mac::video_mixer_mac()
{
    cgl_context_ = NULL;
    surface_ = NULL;
}


}  // namespace p1stream

#endif  // p1stream_priv_mac_h
