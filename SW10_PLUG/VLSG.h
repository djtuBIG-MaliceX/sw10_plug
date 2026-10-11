/**
 *
 *  Copyright (C) 2022 Roman Pauer
 *  Copyright (C) 2024 James Alan Nguyen
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

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <climits>
#include <cmath>
#include "IPlug_include_in_plug_hdr.h"

#ifdef _MSC_VER
#ifndef __cplusplus
#define inline __inline
#endif
#include <intrin.h>
#endif

#define MIDI_CHANNELS 16
#define DRUM_CHANNEL 9
#define MAX_VOICES 256  // hehehe

// Base reverb tap positions (sample counts) of the comb/delay network, anchored to the
// plugin's historical 44.1 kHz clock and scaled to the host rate so pre-delay / spacing
// stay constant at any sample rate. The long tail of Reverb 3/4 comes from ring size +
// recirc damp (ReverbTailScheme), not from wider taps.
enum ReverbDelayIdx
{
  RVD_500 = 0, RVD_501,
  RVD_826, RVD_827,
  RVD_1038, RVD_1039,
  RVD_1176, RVD_1177, RVD_1178, RVD_1179,
  RVD_1339,
  RVD_3177, RVD_3178, RVD_3179, RVD_3180,
  RVD_3335,
  RVD_5118,
  RVD_NUM
};

// Reverb tail scheme (per mode; see VLSG_SetEffect). Tap spacing is ALWAYS anchored
// to the plugin's historical 44.1 kHz clock (classic pre-delay everywhere); what this
// selects is the ring size + recirculation damp behind those taps:
//  - CLASSIC:     original ring sizing + original 96/97 damp — the plugin's
//                 long-standing short reverb. Reverb 1/2.
//  - LONG_RING4:  same taps through a 4x larger ring with raised 176/177 damp —
//                 sustained "hall" tail, no echo holes. Reverb 3/4 ("Long").
enum ReverbTailScheme
{
  REVERB_TAIL_CLASSIC = 0,
  REVERB_TAIL_LONG_RING4 = 1
};


typedef struct
{
  uint16_t program_change;
  int16_t modulation;
  int16_t channel_pressure;
  int16_t expression;
  int16_t volume;
  int16_t pitch_bend;
  int16_t pan;    // 0 is middle even though MIDI val=64
  uint16_t chflags;
  int16_t pitch_bend_sense;
  int16_t fine_tune;
  int16_t coarse_tune;
  uint8_t parameter_number_MSB;
  uint8_t parameter_number_LSB;
  uint8_t data_entry_MSB;
  uint8_t data_entry_LSB;
} Channel_Data;

typedef struct
{
  uint32_t wv_fpos;
  uint32_t wv_end;
  uint32_t wv_start;
  int32_t field_0C[4];
  uint32_t wv_un3_hi;
  uint32_t wv_pos;
  uint32_t v_freq;
  int32_t field_28;
  int32_t field_2C;
  int32_t field_30;
  int32_t field_34;
  int32_t field_38;
  int32_t note_number;
  int16_t note_velocity;
  int16_t channel_num_2;
  int16_t base_freq;
  uint16_t vflags;
  int16_t field_48;
  int16_t field_4A;
  int16_t field_4C;
  uint16_t v_vol;
  int16_t field_50;
  int16_t field_52;
  int16_t field_54;
  int16_t detune;   // = pgm.detune
  int16_t pgm_f0E;  // = pgm.field_0E
  int16_t pgm_f10;  // = pgm.field_10
  uint16_t index;   // = pgm.index
  uint16_t pgm_f14; // = pgm.field_14
  int16_t wv_un3_lo;
  int16_t v_velocity;
  int16_t vol;
  int16_t wv_un1_lo;
  int16_t wv_un1_hi;
  int16_t v_panpot;
} Voice_Data;

typedef struct
{
  uint16_t field_00;
  uint16_t field_02;
  int16_t  detune;
  int16_t  field_06;
  int16_t  field_08;
  int16_t  panpot;
  int16_t  field_0C;
  int16_t  field_0E;
  int16_t  field_10;
  uint16_t index;
  uint16_t field_14;
  int16_t  field_16;
  int16_t  field_18;
  int16_t  field_1A;
} Program_Data;


enum ParameterType
{
    PARAMETER_OutputBuffer  = 1,
    PARAMETER_ROMAddress    = 2,
    PARAMETER_Frequency     = 3,
    PARAMETER_Polyphony     = 4,
    PARAMETER_Effect        = 5,
    PARAMETER_VelocityFunc  = 6, // Experimental
};


enum Voice_Flags
{
  VFLAG_Mask07 = 0x07,
  VFLAG_NotMask07 = 0xF8,

  VFLAG_Mask38 = 0x38,
  VFLAG_NotMask38 = 0xC7,

  VFLAG_Value40 = 0x40,
  VFLAG_Value80 = 0x80,
  VFLAG_MaskC0 = 0xC0,
};

enum Channel_Flags
{
  CHFLAG_Sostenuto = 0x2000,
  CHFLAG_Soft = 0x4000,
  CHFLAG_Sustain = 0x8000,
};

inline void WRITE_LE_UINT16(uint8_t* ptr, uint16_t value)
{
  ptr[0] = value & 0xff;
  ptr[1] = (value >> 8) & 0xff;
}

inline void WRITE_LE_UINT32(uint8_t* ptr, uint32_t value)
{
  ptr[0] = value & 0xff;
  ptr[1] = (value >> 8) & 0xff;
  ptr[2] = (value >> 16) & 0xff;
  ptr[3] = (value >> 24) & 0xff;
}

inline uint16_t READ_LE_UINT16(const uint8_t* ptr)
{
  return ptr[0] | (ptr[1] << 8);
}

// UI plotting: read-only row (128 entries, each 0..127) of the engine's real velocity
// response for curve index 0-11 (same row voice_data path uses: [curveIdx + 1]).
const int32_t* GetVLSGVelocityCurve(int curveIdx);

// UI helper: PC number the engine accepts on the drum channel for kit index 0-7
// (GS/GM2 kit numbers minus 1; -1 = invalid index). See drum_kits[] in VLSG.cpp.
int GetVLSGDrumKitPC(int idx);

class VLSG
{
public:
  constexpr uint32_t VLSG_GetVersion(void) const;
  constexpr const char* VLSG_GetName(void) const;
  uint32_t VLSG_GetTime(void);
  void VLSG_SetFunc_GetTime(uint32_t (*get_time)());
  bool VLSG_SetParameter(uint32_t type, uintptr_t value);
  bool VLSG_SetWaveBuffer(void* ptr);
  bool VLSG_SetRomAddress(const void* ptr);
  bool VLSG_SetFrequency(unsigned int frequency);
  bool VLSG_SetPolyphony(unsigned int poly);
  bool VLSG_SetEffect(unsigned int effect);
  bool VLSG_SetVelocityFunc(unsigned int curveIdx);
  bool VLSG_PlaybackStart(void);
  bool VLSG_PlaybackStop(void);
  void VLSG_Write(const void* data, uint32_t len);
  int32_t VLSG_Buffer(uint32_t output_buffer_counter);
  void VLSG_AddMidiData(uint8_t* ptr, uint32_t len);

  // Invasive workarounds
  int32_t VLSG_BufferVst(uint32_t output_buffer_counter, double** output, int nFrames, iplug::IMidiQueue& mMidiQueue, iplug::IMidiQueueBase<iplug::ISysEx>& mSysExQueue);
  void ProcessMidiData(void);
  inline void ProcessMidiDataVst(iplug::IMidiMsg& msg);   // TODO adapt for pluggable queue
  inline void ProcessSysExDataVst(iplug::ISysEx& msg);    // TODO adapt for pluggable queue
  void ProcessPhase(void);

  // Read-only channel state for UI mirroring (program, volume, pan, expression, ...)
  const Channel_Data* GetChannelData(int channel) const { return &(channel_data[channel & (MIDI_CHANNELS - 1)]); }
  // Frames rendered per VLSG_Buffer() call (legacy "Original Driver" render mode)
  int GetLegacyChunkSamples(void) const { return 4 * output_size_para; }
  // Legacy ring buffer capacity in frames / bytes
  uint32_t GetOutputBufferSamples(void) const { return output_buffer_size_samples; }
  uint32_t GetOutputBufferBytes(void) const { return output_buffer_size_bytes; }

  ~VLSG();

private:
  uint32_t dword_C0000000;
  uint32_t dword_C0000004;
  uint32_t dword_C0000008;
  int32_t output_size_para;
  // Envelope/phase scheduler: ProcessPhase() runs every phase_interval samples (fractional,
  // exact) so envelope & LFO rates are identical at any host sample rate. Reference rate is
  // the original driver's 11025 Hz / 64-sample quantum = 172.2685... steps per second.
  double phase_interval = 64.0;
  double phase_acc = -1.0;   // < 0 means "fire on first sample of the first block"
  uint32_t system_time_2;
  uint8_t event_data[256];
  uint32_t recent_voice_index;
  Program_Data* program_data_ptr;
  Channel_Data* channel_data_ptr;
  uint32_t event_type;
  int32_t event_length = 0;
  int32_t* reverb_data_buffer = nullptr; // dynamically sized (scales with sample rate)
  uint32_t reverb_data_size = 0;
  uint32_t reverb_data_mask = 0x7FFF;
  int reverb_delay[RVD_NUM];             // taps (44.1k-anchored) scaled to output_frequency
  uint32_t reverb_data_index;
  int32_t is_reverb_enabled;
  uint32_t reverb_shift;
  volatile uint32_t midi_data_read_index;
  uint8_t midi_data_buffer[65536];
  volatile uint32_t midi_data_write_index;
  uint32_t processing_phase;
  uint32_t rom_offset;
  Program_Data program_data[MIDI_CHANNELS * 2];
  Channel_Data channel_data[MIDI_CHANNELS];
  Voice_Data voice_data[MAX_VOICES];
  uint32_t velocity_func;
  int32_t current_polyphony;
  const uint8_t* romsxgm_ptr;
  uint32_t output_frequency;
  int32_t maximum_polyphony_new_value;
  uint32_t system_time_1;
  int32_t maximum_polyphony;
  uint8_t* output_data_ptr;
  uint32_t output_buffer_size_samples;
  uint32_t output_buffer_size_bytes;
  uint32_t effect_param_value;
  int32_t* reverb_data_ptr = nullptr;
  ReverbTailScheme reverb_tail = REVERB_TAIL_CLASSIC; // ring+damp scheme (see enum)
  int32_t reverb_fb_a = 96;            // recirc-damp numerator /256 (loop A)
  int32_t reverb_fb_b = 97;            // recirc-damp numerator /256 (loop B)
  uint32_t(*get_time_func)();

  bool InitializeVelocityFunc(void);
  constexpr bool EMPTY_DeinitializeVelocityFunc(void);
  bool InitializeVariables(void);
  constexpr bool EMPTY_DeinitializeVariables(void);
  inline void CountActiveVoices(void);
  void SetMaximumVoices(int maximum_voices);
  Voice_Data* FindAvailableVoice(int32_t channel_num_2, int32_t note_number);
  Voice_Data* FindVoice(int32_t channel_num_2, int32_t note_number);
  void NoteOff(void);
  void NoteOn(int32_t ch);
  void ControlChange(void);
  void SystemExclusive(void);
  bool InitializeReverbBuffer(void);
  void ComputeReverbDelays(void);      // re-derives taps from the 44.1k anchor + rate
  bool DeinitializeReverbBuffer(void);
  void EnableReverb(void);
  void DisableReverb(void);
  void SetReverbShift(uint32_t shift);
  void DefragmentVoices(void);
  void GenerateOutputData(uint8_t* output_ptr, uint32_t offset1, uint32_t offset2);
  inline void GenerateOutputDataVst(double** output_ptr, uint32_t offset1, uint32_t offset2); // invasive workaround
  bool InitializeMidiDataBuffer(void);
  bool EMPTY_DeinitializeMidiDataBuffer(void);
  void AddByteToMidiDataBuffer(uint8_t value);
  uint8_t GetValueFromMidiDataBuffer(void);
  bool InitializePhase(void);
  bool EMPTY_DeinitializePhase(void);
  void voice_set_freq(Voice_Data* voice_data_ptr, int32_t pitch);
  int32_t voice_get_index(Voice_Data* voice_data_ptr, int32_t pitch);
  void ProgramChange(Program_Data* program_data_ptr, uint32_t program_number);
  void ReduceActiveVoices(int32_t maximum_voices);
  void VoiceSoundOff(Voice_Data* voice_data_ptr);
  void VoiceNoteOff(Voice_Data* voice_data_ptr);
  void AllChannelNotesOff(int32_t channel_num);
  void AllChannelSoundsOff(int32_t channel_num);
  void AllVoicesSoundsOff(void);
  void ControllerSettingsOn(int32_t channel_num);
  void ControllerSettingsOff(int32_t channel_num);
  void StartPlayingVoice(Voice_Data* voice_data_ptr, Channel_Data* channel_data_ptr, Program_Data* program_data_ptr);
  uint8_t* parseMidiMsg(iplug::IMidiMsg& msg);
  void voice_set_panpot(Voice_Data* voice_data_ptr);
  void voice_set_flags(Voice_Data* voice_data_ptr);
  void voice_set_flags2(Voice_Data* voice_data_ptr);
  void voice_set_amp(Voice_Data* voice_data_ptr);
  inline int32_t sub_C0036FB0(int16_t value3);
  void sub_C0036FE0(void);
  void sub_C0037140(void);
  bool InitializeStructures(void);
  bool EMPTY_DeinitializeStructures(void);
  void ResetAllControllers(Channel_Data* channel_data_ptr);
  void ResetChannel(Channel_Data* channel_data_ptr);
  uint32_t rom_change_bank(uint32_t bank, int32_t index);
  uint16_t rom_read_word(void);
  int16_t rom_read_word_at(uint32_t offset);
};
