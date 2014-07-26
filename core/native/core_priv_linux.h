#ifndef p1_core_priv_linux_h
#define p1_core_priv_linux_h

#include "core_priv.h"

namespace p1_core {

void module_platform_init();

class video_mixer_linux : public video_mixer_base {
public:
    virtual Handle<Value> platform_init(Handle<Object> params) final;
    virtual void platform_destroy() final;
    virtual bool activate_gl() final;
};


}  // namespace p1_core

#endif  // p1_core_priv_linux_h
