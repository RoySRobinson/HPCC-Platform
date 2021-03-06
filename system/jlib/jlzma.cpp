/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */


// LZMA adapted for jlib
#include "platform.h"

#include "jlzma.hpp"

#include "Types.h"
#include "LzFind.h"
#include "LzHash.h"
#include "LzmaEnc.h"
#include "LzmaDec.h"

static void *SzAlloc(void *, size_t size)
{ 
    return checked_malloc(size,-1);
}
static void SzFree(void *, void *address)
{
    free(address);
}
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

class CLZMA
{
    CLzmaEncHandle enc;
    CLzmaEncProps props;
public:
    CLZMA()
    {
        enc = 0;
    }
    ~CLZMA()
    {
        if (enc)
            LzmaEnc_Destroy(enc, &g_Alloc, &g_Alloc);
    }

    size32_t compress(const void* input, size32_t inlength, void* output)
    { // returns (size32_t)-1 if cannot compress
        if (!enc) {
            enc = LzmaEnc_Create(&g_Alloc);
            if (enc == 0)
                throw MakeStringException(-1,"LzmaEnc_Create failed");
            LzmaEncProps_Init(&props);
            if (LzmaEnc_SetProps(enc, &props)!=SZ_OK) 
                throw MakeStringException(-1,"LzmaEnc_SetProps failed");
        }
        if (inlength+LZMA_PROPS_SIZE+sizeof(size32_t)<1024)
            return (size32_t)-1; // don't compress less than 1K
        SizeT propsize = LZMA_PROPS_SIZE;
        LzmaEnc_WriteProperties(enc, (byte *)output+sizeof(size32_t), &propsize);
        *(size32_t *)output = (size32_t)propsize;
        SizeT reslen = inlength-1-propsize-sizeof(size32_t);
        SRes res = LzmaEnc_MemEncode(enc, (byte *)output+propsize+sizeof(size32_t), &reslen, (const byte *)input, inlength, true, NULL, &g_Alloc, &g_Alloc);
        if (res==SZ_ERROR_OUTPUT_EOF) 
            return (size32_t)-1;
        if (res!=SZ_OK)
            throw MakeStringException(-1,"LzmaEnc_MemEncode failed(%d)",(int)res);
        return reslen+propsize+sizeof(size32_t);
    }

    size32_t expand(const void* input, size32_t inlength, void* output, size32_t maxout)
    {
        SizeT reslen = maxout;
        SizeT propsize = *(size32_t *)input;
        SizeT inlen = inlength -= sizeof(size32_t)+propsize;
        ELzmaStatus status;
        SRes res = LzmaDecode((byte *)output, &reslen, (const byte *)input+sizeof(size32_t)+propsize, &inlen, 
            (byte *)input+sizeof(size32_t), propsize, LZMA_FINISH_END, &status, &g_Alloc);
        if (res!=SZ_OK)
            throw MakeStringException(-1,"LzmaDecode failed(%d)",(int)res);
        return reslen;
    }

};

void LZMACompressToBuffer(MemoryBuffer & out, size32_t len, const void * src)
{
    CLZMA lzma;
    size32_t outbase = out.length();
    out.append(len);
    DelayedMarker<size32_t> cmpSzMarker(out);
    void *cmpData = out.reserve(len);
    size32_t sz = lzma.compress(src, len, cmpData);
    if (sz>len)
    {
        sz = len;
        memcpy(cmpData, src, len);
    }
    else 
        out.setLength(outbase+sizeof(size32_t)*2+sz);
    cmpSzMarker.write(sz);
}

void LZMADecompressToBuffer(MemoryBuffer & out, const void * src)
{
    size32_t *sz = (size32_t *)src;
    size32_t expsz = *(sz++);
    size32_t cmpsz = *(sz++);
    void *o = out.reserve(expsz);
    if (cmpsz!=expsz) {
        CLZMA lzma;
        size32_t written = lzma.expand(sz,cmpsz,o,expsz);
        if (written!=expsz)
            throw MakeStringException(0, "fastLZDecompressToBuffer - corrupt data(1) %d %d",written,expsz);
    }
    else
        memcpy(o,sz,expsz);
}

void LZMADecompressToBuffer(MemoryBuffer & out, MemoryBuffer & in)
{
    size32_t expsz;
    size32_t cmpsz;
    in.read(expsz).read(cmpsz);
    void *o = out.reserve(expsz);
    if (cmpsz!=expsz) {
        CLZMA lzma;
        size32_t written = lzma.expand(in.readDirect(cmpsz),cmpsz,o,expsz);
        if (written!=expsz)
            throw MakeStringException(0, "fastLZDecompressToBuffer - corrupt data(3) %d %d",written,expsz);
    }
    else
        memcpy(o,in.readDirect(cmpsz),expsz);
}

void LZMADecompressToAttr(MemoryAttr & out, const void * src)
{
    size32_t *sz = (size32_t *)src;
    size32_t expsz = *(sz++);
    size32_t cmpsz = *(sz++);
    void *o = out.allocate(expsz);
    if (cmpsz!=expsz) {
        CLZMA lzma;
        size32_t written = lzma.expand(sz,cmpsz,o,expsz);
        if (written!=expsz)
            throw MakeStringException(0, "fastLZDecompressToBuffer - corrupt data(2) %d %d",written,expsz);
    }
    else
        memcpy(o,sz,expsz);
}

void LZMALZDecompressToBuffer(MemoryAttr & out, MemoryBuffer & in)
{
    size32_t expsz;
    size32_t cmpsz;
    in.read(expsz).read(cmpsz);
    void *o = out.allocate(expsz);
    if (cmpsz!=expsz) {
        CLZMA lzma;
        size32_t written = lzma.expand(in.readDirect(cmpsz),cmpsz,o,expsz);
        if (written!=expsz)
            throw MakeStringException(0, "fastLZDecompressToBuffer - corrupt data(4) %d %d",written,expsz);
    }
    else
        memcpy(o,in.readDirect(cmpsz),expsz);
}





