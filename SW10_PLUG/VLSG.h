/**
 *
 *  Copyright (C) 2022 Roman Pauer
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy of
 *  this software and associated documentation files (the "Software"), to deal in
 *  the Software without restriction, including without limitation the rights to
 *  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is furnished to do
 *  so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 */

#pragma once

#include <stdint.h>
#include "IPlug_include_in_plug_hdr.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(_MSC_VER) && (defined(VLSG_BUILD_DLL) || defined(VLSG_IMPORT_DLL))
#define VLSG_IMPORT         __declspec(dllimport)
#define VLSG_EXPORT         __declspec(dllexport)
#define VLSG_CALLTYPE       __stdcall
#else
#define VLSG_IMPORT
#define VLSG_EXPORT
#define VLSG_CALLTYPE
#endif

#if defined(VLSG_BUILD_DLL)
#define VLSG_API            VLSG_EXPORT int VLSG_CALLTYPE
#define VLSG_API_(type)     VLSG_EXPORT type VLSG_CALLTYPE
#elif defined(VLSG_IMPORT_DLL)
#define VLSG_API            VLSG_IMPORT int VLSG_CALLTYPE
#define VLSG_API_(type)     VLSG_IMPORT type VLSG_CALLTYPE
#else
#define VLSG_API            int VLSG_CALLTYPE
#define VLSG_API_(type)     type VLSG_CALLTYPE
#endif

#define VLSG_PFN(name)          typedef int (VLSG_CALLTYPE* name)
#define VLSG_PFN_(type, name)   typedef type (VLSG_CALLTYPE* name)


#define VLSG_FALSE  0
#define VLSG_TRUE   1

typedef int VLSG_Bool;

enum ParameterType
{
    PARAMETER_OutputBuffer  = 1,
    PARAMETER_ROMAddress    = 2,
    PARAMETER_Frequency     = 3,
    PARAMETER_Polyphony     = 4,
    PARAMETER_Effect        = 5,
    PARAMETER_VelocityFunc = 6, // Experimental
};


VLSG_PFN_(uint32_t, VLSG_GETVERSION)(void);
VLSG_API_(uint32_t) VLSG_GetVersion(void);

VLSG_PFN_(const char*, VLSG_GETNAME)(void);
VLSG_API_(const char*) VLSG_GetName(void);

VLSG_PFN_(uint32_t, VLSG_GETTIME)(void);
VLSG_API_(uint32_t) VLSG_GetTime(void);

VLSG_PFN_(void, VLSG_SETFUNC_GETTIME)(VLSG_GETTIME get_time);
VLSG_API_(void) VLSG_SetFunc_GetTime(VLSG_GETTIME get_time);

VLSG_PFN_(VLSG_Bool, VLSG_SETPARAMETER)(uint32_t type, uintptr_t value);
VLSG_API_(VLSG_Bool) VLSG_SetParameter(uint32_t type, uintptr_t value);

VLSG_PFN_(VLSG_Bool, VLSG_SETWAVEBUFFER)(void* ptr);
VLSG_API_(VLSG_Bool) VLSG_SetWaveBuffer(void* ptr);

VLSG_PFN_(VLSG_Bool, VLSG_SETROMADDRESS)(const void* ptr);
VLSG_API_(VLSG_Bool) VLSG_SetRomAddress(const void* ptr);

VLSG_PFN_(VLSG_Bool, VLSG_SETFREQUENCY)(unsigned int frequency);
VLSG_API_(VLSG_Bool) VLSG_SetFrequency(unsigned int frequency);

VLSG_PFN_(VLSG_Bool, VLSG_SETPOLYPHONY)(unsigned int poly);
VLSG_API_(VLSG_Bool) VLSG_SetPolyphony(unsigned int poly);

VLSG_PFN_(VLSG_Bool, VLSG_SETEFFECT)(unsigned int effect);
VLSG_API_(VLSG_Bool) VLSG_SetEffect(unsigned int effect);

VLSG_PFN_(VLSG_Bool, VLSG_SETVELOCITYCURVE)(unsigned int curveIdx);
VLSG_API_(VLSG_Bool) VLSG_SetVelocityFunc(unsigned int curveIdx);

VLSG_PFN_(VLSG_Bool, VLSG_INIT)(void);
VLSG_API_(VLSG_Bool) VLSG_Init(void);

VLSG_PFN_(VLSG_Bool, VLSG_EXIT)(void);
VLSG_API_(VLSG_Bool) VLSG_Exit(void);

VLSG_PFN_(void, VLSG_WRITE)(const void* data, uint32_t len);
VLSG_API_(void) VLSG_Write(const void* data, uint32_t len);

VLSG_PFN_(int32_t, VLSG_BUFFER)(uint32_t output_buffer_counter);
VLSG_API_(int32_t) VLSG_Buffer(uint32_t output_buffer_counter);

// Invasive workaround
VLSG_API_(int32_t) VLSG_BufferVst(uint32_t output_buffer_counter, double** output, int nFrames, iplug::IMidiQueue& mMidiQueue, iplug::IMidiQueueBase<iplug::ISysEx>& mSysExQueue);

int32_t VLSG_PlaybackStart(void);
int32_t VLSG_PlaybackStop(void);
void VLSG_AddMidiData(uint8_t *ptr, uint32_t len);
int32_t VLSG_FillOutputBuffer(uint32_t output_buffer_counter);

VLSG_API_(void) ProcessMidiData(void);
VLSG_API_(void) ProcessMidiDataVst(iplug::IMidiMsg& msg);
VLSG_API_(void) ProcessPhase(void);

#ifdef __cplusplus
}
#endif

