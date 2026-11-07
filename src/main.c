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

#define SAMPLESCOUNT 960

unsigned short dspmodule_process(const DSPLoaderAPI *lapi, unsigned long long position, unsigned long duration, unsigned long rate, unsigned long long nsectime)
{
    static bool quitnextcycle = false;
    if (quitnextcycle) return 1;

    if (rate != 48000)
    { lapi->log(DSPLogLevel_Error, "sample rate doesnt match 48000 (got %lu), shutting down...", rate); return 1; }

    float *left = lapi->getportbuffer(leftport, duration);
    float *right = lapi->getportbuffer(rightport, duration);

    static float queuebuffleft[SAMPLESCOUNT], queuebuffright[SAMPLESCOUNT];
    static unsigned short queueidx = 0, queuelen = 0;
    unsigned long outbuffidx = 0;

    while (outbuffidx < duration)
    {
        if (queuelen)
        {   
            if (duration <= queuelen)
            {
                if (left) memcpy(left + outbuffidx, queuebuffleft + queueidx, duration);
                if (right) memcpy(right + outbuffidx, queuebuffright + queueidx, duration);
                queueidx += duration;
                queuelen -= duration;
                break;
            }
            if (left) memcpy(left + outbuffidx, queuebuffleft + queueidx, queuelen);
            if (right) memcpy(right + outbuffidx, queuebuffright + queueidx, queuelen);
            outbuffidx += queuelen;
            queueidx = 0;
            queuelen = 0;
        }

        static float freshbuff[SAMPLESCOUNT * 2];
        int c = op_read_float_stereo(f, freshbuff, SAMPLESCOUNT * 2);
        if (!c) { lapi->log(DSPLogLevel_Info, "reached end of file, exitting..."); goto errorquit; }
        else if (c < 0)
        { lapi->log(DSPLogLevel_Error, "caused Opus decoder error while processing a cycle, shutting down..."); goto errorquit; }

        size_t freshbuffidx = 0;
        for (; freshbuffidx < c * 2; freshbuffidx += 2)
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
                queueidx = 0;
            }
        }
    }

    for (unsigned long i = 0; i < duration; i++)
    {
        if (left) left[i] = adjf(left[i], ampmod) * volmod;
        if (right) right[i] = adjf(right[i], ampmod) * volmod;
    }

    return 0;

    errorquit:
        if (outbuffidx < duration)
        {
            if (left) memset(left + outbuffidx, 0, duration - outbuffidx);
            if (right) memset(right + outbuffidx, 0, duration - outbuffidx);
        }
    quitnextcycle = true;
    return 0;
}

void dspmodule_cleanup(void)
{
    op_free(f);
}
