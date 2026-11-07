#include <dspmodule.h>

#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <opus/opusfile.h>

const unsigned short dspmodule_requiredAPIversion = 3;

static OggOpusFile *f;
static void *leftport, *rightport;
static float ampmod = 0, volmod = 0.1;

unsigned short dspmodule_startup(const DSPLoaderAPI *lapi, int argc, char * const argv[], const char **sysname, const char **dispname)
{
    const char *filepath = NULL;
    {
        int opt;
        while ((opt = getopt(argc, argv, "f:a:v:")) != -1)
        {
            switch (opt)
            {
                case 'f':
                    filepath = optarg;
                    break;

                case 'a':
                    if (sscanf(optarg, "%f", &ampmod) < 1) goto parsefltoptfail;
                    break;

                case 'v':
                    if (sscanf(optarg, "%f", &volmod) < 1) goto parsefltoptfail;
                    break;

                parsefltoptfail:
                    lapi->log(DSPLogLevel_Fatal, "failed parsing option -%c (required float)", (char)opt);
                return 1;
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

    lapi->log(DSPLogLevel_Info, "filepath: %s\nampmod: %f\nvolmod: %f", filepath, ampmod, volmod);
    *sysname = "dspopusplayer";
    *dispname = "Opus Player";
    return 0;

    addportfail:
        lapi->log(DSPLogLevel_Fatal, "failed adding output port %s", portname);
        op_free(f);
    return 1;
}

#define OPSAMPCOUNT 960

static inline void modchs(float *left, float *right, unsigned long duration)
{
    for (unsigned long i = 0; i < duration; i++)
    {
        if (left) left[i] = adjf(left[i], ampmod) * volmod;
        if (right) right[i] = adjf(right[i], ampmod) * volmod;
    }
}

unsigned short dspmodule_process(const DSPLoaderAPI *lapi, unsigned long long position, unsigned long duration, unsigned long rate, unsigned long long nsectime)
{
    static bool quitnextcycle = false;
    if (quitnextcycle) return 1;

    if (rate != 48000)
    { lapi->log(DSPLogLevel_Error, "graph sample rate doesnt match 48000 (got %lu), shutting down...", rate); return 1; }

    float *left = lapi->getportbuffer(leftport, duration);
    float *right = lapi->getportbuffer(rightport, duration);

    static float queuebuffleft[OPSAMPCOUNT], queuebuffright[OPSAMPCOUNT];
    static unsigned long queueidx = 0, queuelen = 0;
    unsigned long outbuffidx = 0;

    if (queuelen)
    {
        if (duration <= queuelen)
        {
            if (left) memcpy(left, queuebuffleft + queueidx, duration * sizeof(float));
            if (right) memcpy(right, queuebuffright + queueidx, duration * sizeof(float));
            queueidx += duration;
            queuelen -= duration;
            goto queueflushed;
        }
        
        if (left) memcpy(left, queuebuffleft + queueidx, queuelen * sizeof(float));
        if (right) memcpy(right, queuebuffright + queueidx, queuelen * sizeof(float));
        outbuffidx += queuelen;
        queuelen = 0;
        queueidx = 0;
    }

    while (outbuffidx < duration)
    {
        static float freshbuff[OPSAMPCOUNT * 2];
        int ret = op_read_float_stereo(f, freshbuff, OPSAMPCOUNT * 2);
        if (!ret) { lapi->log(DSPLogLevel_Info, "reached end of file, exitting..."); goto errorquit; }
        else if (ret < 0)
        { lapi->log(DSPLogLevel_Error, "caused Opus decoder error while processing a cycle, shutting down..."); goto errorquit; }

        for (unsigned long freshbuffidx = 0; freshbuffidx < (unsigned long)(ret << 1); freshbuffidx += 2)
        {
            float l = freshbuff[freshbuffidx];
            float r = freshbuff[freshbuffidx + 1];
            
            if (outbuffidx < duration)
            {
                if (left) left[outbuffidx] = l;
                if (right) right[outbuffidx] = r;
                outbuffidx++;
            }
            else
            {
                queuebuffleft[queuelen] = l;
                queuebuffright[queuelen] = r;
                queuelen++;
            }
        }
    }
    queueflushed:

    modchs(left, right, duration);
    return 0;

    errorquit:
        if (outbuffidx < duration)
        {
            if (left) memset(left + outbuffidx, 0, (duration - outbuffidx) * sizeof(float));
            if (right) memset(right + outbuffidx, 0, (duration - outbuffidx) * sizeof(float));
        }
    modchs(left, right, duration);
    quitnextcycle = true;
    return 0;
}

void dspmodule_cleanup(void)
{
    op_free(f);
}
