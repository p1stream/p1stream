#include "core_priv.h"

#include <mach/mach_time.h>
#include <mach/mach_error.h>

namespace p1stream {


static mach_timebase_info_data_t timebase;

int64_t system_time()
{
    return mach_absolute_time() * timebase.numer / timebase.denom;
}

void module_platform_init()
{
    kern_return_t k_ret = mach_timebase_info(&timebase);
    if (k_ret != 0)
        abort();
}


} // namespace p1stream
