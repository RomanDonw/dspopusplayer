/*
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/

#include <dspmodule.h>

#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <opus/opusfile.h>

#include "err.h"

const unsigned short dspmodule_requiredAPIversion = 3;

static OggOpusFile *f;
static void *leftport, *rightport;
static float ampmod = 0, volmod = 0.1;
static int repeat = 0;

enum sourcetype
{
    STYPE_NONE = 0,
    STYPE_FILE = 1,
    STYPE_URL = 2
} typedef sourcetype;
static sourcetype stype = STYPE_NONE;

unsigned short dspmodule_startup(const DSPLoaderAPI *lapi, int argc, char * const argv[], const char **sysname, const char **dispname)
{
    const char *path = NULL;
    {
        int opt;
        while ((opt = getopt(argc, argv, "f:a:v:r:u:")) != -1)
        {
            switch (opt)
            {
                case 'f':
                    if (stype == STYPE_URL)
                    { lapi->log(DSPLogLevel_Fatal, "already selected source type: URL"); return 1; }
                    stype = STYPE_FILE;
                    path = optarg;
                    break;

                case 'a':
                    if (sscanf(optarg, "%f", &ampmod) < 1) goto parsefltoptfail;
                    break;

                case 'v':
                    if (sscanf(optarg, "%f", &volmod) < 1) goto parsefltoptfail;
                    break;

                case 'r':
                    if (sscanf(optarg, "%d", &repeat) < 1)
                    { lapi->log(DSPLogLevel_Fatal, "failed to parse option -r"); return 1; }
                    break;

                case 'u':
                    if (stype == STYPE_FILE)
                    { lapi->log(DSPLogLevel_Fatal, "already selected source type: file"); return 1; }
                    stype = STYPE_URL;
                    path = optarg;
                    break;

                parsefltoptfail:
                    lapi->log(DSPLogLevel_Fatal, "failed parsing option -%c (required float)", (char)opt);
                return 1;
            }
        }
    }

    switch (stype)
    {
        int err;

        case STYPE_FILE:
            if (!(f = op_open_file(path, &err)))
            { lapi->log(DSPLogLevel_Fatal, "unable to open file \"%s\": %s", path, stropuserror(err)); return 1; }
            break;

        case STYPE_URL:
            if (!(f = op_open_url(path, &err, NULL)))
            { lapi->log(DSPLogLevel_Fatal, "unable to open URL \"%s\": %s", path, stropuserror(err)); return 1; }
            break;

        default:
            lapi->log(DSPLogLevel_Fatal, "specify path to Opus-encoded audio file or stream URL through -f or -u option respectively");
            return 1;
    }

    const char *portname = "left";
    if (!(leftport = lapi->addport(portname, NULL, DSPPortDirection_Output, 0))) goto addportfail;
    portname = "right";
    if (!(rightport = lapi->addport(portname, NULL, DSPPortDirection_Output, 0))) goto addportfail;

    lapi->log(DSPLogLevel_Info, "path: %s\nampmod: %f\nvolmod: %f", path, ampmod, volmod);
    if (op_seekable(f))
    {
        if (repeat < 0) lapi->log(DSPLogLevel_Info, "repeat: forever");
        else lapi->log(DSPLogLevel_Info, "repeat: %u times", (unsigned int)repeat);
    }
    else if (repeat)
    { lapi->log(DSPLogLevel_Warning, "repeating is not allowed for endless streams"); repeat = 0; }
    
    *sysname = "dspopusplayer";
    *dispname = "Opus Player";
    return 0;

    addportfail:
        lapi->log(DSPLogLevel_Fatal, "failed adding output port %s", portname);
        op_free(f);
    return 1;
}

static inline void modchs(float *left, float *right, unsigned long duration)
{
    for (unsigned long i = 0; i < duration; i++)
    {
        if (left) left[i] = adjf(left[i], ampmod) * volmod;
        if (right) right[i] = adjf(right[i], ampmod) * volmod;
    }
}

#define OPFRAMESIZE 960

unsigned short dspmodule_process(const DSPLoaderAPI *lapi, unsigned long long position, unsigned long duration, unsigned long rate, unsigned long long nsectime)
{
    static bool quitnextcycle = false;
    if (quitnextcycle) return 1;

    if (rate != 48000)
    { lapi->log(DSPLogLevel_Error, "graph sample rate doesnt match 48000 (got %lu), shutting down...", rate); return 1; }

    float *left = lapi->getportbuffer(leftport, duration);
    float *right = lapi->getportbuffer(rightport, duration);

    static float queuebuffleft[OPFRAMESIZE], queuebuffright[OPFRAMESIZE];
    static unsigned long queueidx = 0, queuelen = 0;
    static unsigned long repeatcounter = 0;
    unsigned long outbuffidx = 0;

    if (queuelen)
    {
        if (duration <= queuelen)
        {
            if (left) memcpy(left, queuebuffleft + queueidx, duration * sizeof(float));
            if (right) memcpy(right, queuebuffright + queueidx, duration * sizeof(float));
            queuelen -= duration;
            if (queuelen) queueidx += duration;
            else queueidx = 0;
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
        static float freshbuff[OPFRAMESIZE * 2];
        int ret = op_read_float_stereo(f, freshbuff, OPFRAMESIZE * 2);
        if (!ret)
        {
            if (repeat < 0 || repeatcounter++ < repeat)
            {
                int err = op_pcm_seek(f, 0);
                if (err)
                {
                    lapi->log(DSPLogLevel_Fatal, "failed seeking to start of the file/stream: %s",
                        stropuserror(err));
                    goto errorquit;
                }
                continue;
            }
            lapi->log(DSPLogLevel_Info, "reached end of file/stream, exitting...");
            goto errorquit;
        }
        else if (ret == OP_HOLE) goto handlestreamhole;
        else if (ret < 0)
        {
            lapi->log(DSPLogLevel_Error, "caused Opus decoder error while processing a cycle, shutting down...\nerror code: %s",
                stropuserror(ret));
            goto errorquit;
        }

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
        quitnextcycle = true;
    handlestreamhole:
        if (outbuffidx < duration)
        {
            if (left) memset(left + outbuffidx, 0, (duration - outbuffidx) * sizeof(float));
            if (right) memset(right + outbuffidx, 0, (duration - outbuffidx) * sizeof(float));
        }
    modchs(left, right, duration);
    return 0;
}

void dspmodule_cleanup(void)
{
    op_free(f);
}
