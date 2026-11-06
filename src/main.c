#include <dspmodule.h>

#include <stddef.h>
#include <getopt.h>
#include <opus/opusfile.h>

const unsigned short dspmodule_requiredAPIversion = 3;

static OggOpusFile *f;
static void *leftport, *rightport;

unsigned short dspmodule_startup(const DSPLoaderAPI *lapi, int argc, char * const argv[], const char **sysname, const char **dispname)
{
    const char *filepath = NULL;
    {
        int opt;
        while ((opt = getopt(argc, argv, "f:")) != -1)
        {
            switch (opt)
            {
                case 'f':
                    filepath = optarg;
                    break;
            }
        }
    }

    if (!filepath)
    { lapi->log(DSPLogLevel_Fatal, "pass path to Opus-encoded audio file through -f option"); return 1; }

    if (!(f = op_open_file(filepath, NULL)))
    { lapi->log(DSPLogLevel_Fatal, "unable to open file"); return 1; }

    const char *portname = "left";
    if (!(leftport = lapi->addport(portname, NULL, DSPPortDirection_Output, 0))) goto addportfail;
    portname = "right";
    if (!(rightport = lapi->addport(portname, NULL, DSPPortDirection_Output, 0))) goto addportfail;

    lapi->log(DSPLogLevel_Info, "filepath: %s", filepath);
    *sysname = "dspopusplayer";
    *dispname = "Opus Player";
    return 0;

    addportfail:
        lapi->log(DSPLogLevel_Fatal, "failed adding output port %s", portname);
        op_free(f);
    return 1;
}

unsigned short dspmodule_process(const DSPLoaderAPI *lapi, unsigned long long position, unsigned long duration, unsigned long rate, unsigned long long nsectime)
{
    if (rate != 48000)
    { lapi->log(DSPLogLevel_Error, "sample rate doesnt match 48000 (got %lu), shutting down...", rate); return 1; }

    float *left = lapi->getportbuffer(leftport, duration);
    float *right = lapi->getportbuffer(rightport, duration);

    float buff[(size_t)duration << 1];
    int c = op_read_float_stereo(f, buff, sizeof(buff));
    if (!c)
    { lapi->log(DSPLogLevel_Info, "reached end of file, exitting..."); return 1; }
    else if (c < 0)
    { lapi->log(DSPLogLevel_Error, "caused Opus decoder error while processing a cycle, shutting down..."); return 1; }

    for (unsigned long i = 0; i < duration; i++)
    {
        if (left) left[i] = i < c ? buff[(size_t)i << 1] : 0;
        if (right) right[i] = i < c ? buff[((size_t)i << 1) + 1] : 0;
    }

    return 0;
}

void dspmodule_cleanup(void)
{
    op_free(f);
}
