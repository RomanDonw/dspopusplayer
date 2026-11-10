/*
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/

#include "err.h"

#include <opus/opusfile.h>

const char *stropuserror(int error)
{
    switch (error)
    {
        case OP_FALSE:
            return "request did not succeed";

        case OP_EOF:
            return "end of file";

        case OP_HOLE:
            return "page lost";

        case OP_EREAD:
            return "general read error";

        case OP_EFAULT:
            return "fault";

        case OP_EIMPL:
            return "not implemented";

        case OP_EINVAL:
            return "invalid arguments";

        case OP_ENOTFORMAT:
            return "invalid format";

        case OP_EBADHEADER:
            return "bad header";

        case OP_EVERSION:
            return "unrecognized version";

        case OP_ENOTAUDIO:
            return "not audio";

        case OP_EBADPACKET:
            return "bad packet";

        case OP_EBADLINK:
            return "bad link";

        case OP_ENOSEEK:
            return "unable to seek";

        case OP_EBADTIMESTAMP:
            return "bad timestamp";

        default:
            return NULL;
    }
}