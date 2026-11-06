#include <dspmodule.h>

const unsigned short dspmodule_requiredAPIversion = 3;

unsigned short dspmodule_startup(const DSPLoaderAPI *lapi, int argc, char * const argv[], const char **sysname, const char **dispname)
{
    *sysname = "dspopusplayer";
    *dispname = "Opus Player";
    return 0;
}

unsigned short dspmodule_process(const DSPLoaderAPI *lapi, unsigned long long position, unsigned long duration, unsigned long rate, unsigned long long nsectime)
{

    return 0;
}

