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

#include <stddef.h>
#include <string.h>
#include <limits.h>
#include "VLSG.h"


#ifdef _MSC_VER
#define inline __inline
#include <intrin.h>
#endif


#define MIDI_CHANNELS 16
#define DRUM_CHANNEL 9
#define MAX_VOICES 128  // hehehhe


typedef struct
{
    uint16_t program_change;
    int16_t modulation;
    int16_t channel_pressure;
    int16_t expression;
    int16_t volume;
    int16_t pitch_bend;
    int16_t pan;
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


enum Voice_Flags
{
    VFLAG_Mask07    = 0x07,
    VFLAG_NotMask07 = 0xF8,

    VFLAG_Mask38    = 0x38,
    VFLAG_NotMask38 = 0xC7,

    VFLAG_Value40   = 0x40,
    VFLAG_Value80   = 0x80,
    VFLAG_MaskC0    = 0xC0,
};

enum Channel_Flags
{
    CHFLAG_Sostenuto  = 0x2000,
    CHFLAG_Soft       = 0x4000,
    CHFLAG_Sustain    = 0x8000,
};


static const char VLSG_Name[] = "CASIO SW-10";
static VLSG_GETTIME get_time_func;

static uint32_t dword_C0000000;
static uint32_t dword_C0000004;
static uint32_t dword_C0000008;
static int32_t output_size_para;
static uint32_t system_time_2;
static uint8_t event_data[256];
static uint32_t recent_voice_index;
static Program_Data *program_data_ptr;
static Channel_Data *channel_data_ptr;
static uint32_t event_type;
static int32_t event_length = 0;
static int32_t reverb_data_buffer[32768];
static uint32_t reverb_data_index;
static int32_t is_reverb_enabled;
static uint32_t reverb_shift;
static volatile uint32_t midi_data_read_index;
static uint8_t midi_data_buffer[65536];
static volatile uint32_t midi_data_write_index;
static uint32_t processing_phase;
static uint32_t rom_offset;
static Program_Data program_data[MIDI_CHANNELS * 2];
static Channel_Data channel_data[MIDI_CHANNELS];
static Voice_Data voice_data[MAX_VOICES];
static uint32_t velocity_func;
static int32_t current_polyphony;
static const uint8_t *romsxgm_ptr;
static uint32_t output_frequency;
static int32_t maximum_polyphony_new_value;
static uint32_t system_time_1;
static int32_t maximum_polyphony;
static uint8_t *output_data_ptr;
static uint32_t output_buffer_size_samples;
static uint32_t output_buffer_size_bytes;
static uint32_t effect_param_value;

static const uint32_t dword_C0032188[112+104+40] =
{
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     1,     1,     1,     1,
        1,     1,     1,     1,     1,     1,     1,     1,
        2,     2,     2,     2,     2,     2,     2,     2,
        3,     3,     3,     3,     4,     4,     4,     4,
        5,     5,     5,     5,     6,     6,     7,     7,
        8,     8,     8,     9,    10,    10,    11,    11,
       12,    13,    14,    15,    16,    16,    17,    19,
// dword_C0032348[104] // dword_C0032348 = &dword_C0032188[112]
       20,    21,    22,    23,    25,    26,    28,    30,
       32,    33,    35,    38,    40,    42,    45,    47,
       50,    53,    57,    60,    64,    67,    71,    76,
       80,    85,    90,    95,   101,   107,   114,   120,
      128,   135,   143,   152,   161,   170,   181,   191,
      203,   215,   228,   241,   256,   271,   287,   304,
      322,   341,   362,   383,   406,   430,   456,   483,
      512,   542,   574,   608,   645,   683,   724,   767,
      812,   861,   912,   966,  1024,  1084,  1149,  1217,
     1290,  1366,  1448,  1534,  1625,  1722,  1824,  1933,
     2048,  2169,  2298,  2435,  2580,  2733,  2896,  3068,
     3250,  3444,  3649,  3866,  4096,  4339,  4597,  4870,
     5160,  5467,  5792,  6137,  6501,  6888,  7298,  7732,
// dword_C00324E8[40] // dword_C00324E8 = &dword_C0032188[216]
     8192,  8679,  9195,  9741, 10321, 10935, 11585, 12274,
    13003, 13777, 14596, 15464, 16384, 17358, 18390, 19483,
    20642, 21870, 23170, 24548, 26007, 27554, 29192, 30928,
    32768, 34716, 36780, 38967, 41285, 43740, 46340, 49096,
    52015, 55108, 58385, 61857, 65536, 69432, 73561, 77935
};
static const uint32_t dword_C0032588[256] =
{
    32768, 32775, 32782, 32790, 32797, 32804, 32812, 32819,
    32827, 32834, 32842, 32849, 32856, 32864, 32871, 32879,
    32886, 32893, 32901, 32908, 32916, 32923, 32931, 32938,
    32945, 32953, 32960, 32968, 32975, 32983, 32990, 32998,
    33005, 33012, 33020, 33027, 33035, 33042, 33050, 33057,
    33065, 33072, 33080, 33087, 33094, 33102, 33109, 33117,
    33124, 33132, 33139, 33147, 33154, 33162, 33169, 33177,
    33184, 33192, 33199, 33207, 33214, 33222, 33229, 33237,
    33244, 33252, 33259, 33267, 33274, 33282, 33289, 33297,
    33304, 33312, 33319, 33327, 33334, 33342, 33349, 33357,
    33364, 33372, 33379, 33387, 33394, 33402, 33410, 33417,
    33425, 33432, 33440, 33447, 33455, 33462, 33470, 33477,
    33485, 33493, 33500, 33508, 33515, 33523, 33530, 33538,
    33546, 33553, 33561, 33568, 33576, 33583, 33591, 33599,
    33606, 33614, 33621, 33629, 33636, 33644, 33652, 33659,
    33667, 33674, 33682, 33690, 33697, 33705, 33712, 33720,
    33728, 33735, 33743, 33751, 33758, 33766, 33773, 33781,
    33789, 33796, 33804, 33811, 33819, 33827, 33834, 33842,
    33850, 33857, 33865, 33873, 33880, 33888, 33896, 33903,
    33911, 33918, 33926, 33934, 33941, 33949, 33957, 33964,
    33972, 33980, 33987, 33995, 34003, 34010, 34018, 34026,
    34033, 34041, 34049, 34057, 34064, 34072, 34080, 34087,
    34095, 34103, 34110, 34118, 34126, 34133, 34141, 34149,
    34157, 34164, 34172, 34180, 34187, 34195, 34203, 34211,
    34218, 34226, 34234, 34241, 34249, 34257, 34265, 34272,
    34280, 34288, 34296, 34303, 34311, 34319, 34327, 34334,
    34342, 34350, 34358, 34365, 34373, 34381, 34389, 34396,
    34404, 34412, 34420, 34427, 34435, 34443, 34451, 34458,
    34466, 34474, 34482, 34490, 34497, 34505, 34513, 34521,
    34528, 34536, 34544, 34552, 34560, 34567, 34575, 34583,
    34591, 34599, 34606, 34614, 34622, 34630, 34638, 34646,
    34653, 34661, 34669, 34677, 34685, 34692, 34700, 34708
};

#define DRUM_EXC_ORCHESTRA  38

static const int32_t drum_exc_map[38+34+1] =
{
    42, 44, 42, 46, 44, 42, 44, 46,
    46, 42, 46, 44, 71, 72, 72, 71,
    73, 74, 74, 73, 78, 79, 79, 78,
    80, 81, 81, 80, 29, 30, 30, 29,
    86, 87, 87, 86,
    255, 255,
// dword_C0032A20[34] // dword_C0032A20 = &drum_exc_map[DRUM_EXC_ORCHESTRA]
    27, 28, 27, 29, 28, 27, 28, 29,
    29, 27, 29, 28, 71, 72, 72, 71,
    73, 74, 74, 73, 78, 79, 79, 78,
    80, 81, 81, 80, 86, 87, 87, 86,
    255, 255,
// zero terminator
    0
};
static const int32_t velocity_curves[12][128] =
{
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   4,   5,   6,   7,   8,   9,
         11,  13,  14,  16,  18,  20,  22,  24,  26,  28,  30,  32,  34,  36,  39,  41,
         43,  45,  47,  49,  51,  52,  54,  55,  57,  59,  60,  61,  63,  64,  66,  67,
         68,  69,  70,  72,  73,  74,  76,  77,  78,  79,  81,  82,  83,  84,  85,  86,
         87,  87,  88,  89,  90,  91,  91,  92,  93,  93,  94,  95,  95,  96,  97,  97,
         98,  99, 100, 100, 101, 102, 102, 103, 104, 104, 105, 106, 106, 107, 108, 108,
        109, 110, 111, 111, 112, 113, 113, 114, 115, 115, 116, 117, 117, 118, 119, 119,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   4,   5,   6,   7,   8,   9,
         11,  13,  14,  16,  18,  20,  22,  24,  26,  28,  30,  32,  34,  36,  39,  41,
         43,  45,  47,  49,  51,  52,  54,  55,  57,  59,  60,  61,  63,  64,  66,  67,
         68,  69,  70,  72,  73,  74,  76,  77,  78,  79,  81,  82,  83,  84,  85,  86,
         87,  87,  88,  89,  90,  91,  91,  92,  93,  93,  94,  95,  95,  96,  97,  97,
         98,  99, 100, 100, 101, 102, 102, 103, 104, 104, 105, 106, 106, 107, 108, 108,
        109, 110, 111, 111, 112, 113, 113, 114, 115, 115, 116, 117, 117, 118, 119, 119,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   4,   5,   6,   7,   8,   9,
         11,  12,  13,  15,  17,  19,  21,  23,  25,  27,  29,  31,  33,  35,  37,  39,
         41,  43,  45,  47,  49,  50,  52,  53,  55,  57,  58,  59,  60,  61,  63,  64,
         65,  66,  67,  69,  70,  71,  73,  74,  75,  76,  78,  79,  80,  81,  82,  83,
         83,  84,  85,  86,  87,  88,  88,  89,  90,  90,  91,  92,  92,  93,  94,  94,
         95,  96,  97,  97,  98,  99,  99, 101, 102, 102, 103, 104, 104, 105, 106, 106,
        107, 108, 109, 110, 111, 112, 112, 113, 114, 114, 115, 116, 117, 118, 119, 119,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   4,   5,   6,   7,   7,   8,
         10,  12,  13,  15,  17,  18,  20,  22,  24,  26,  28,  29,  31,  33,  36,  38,
         40,  41,  43,  45,  47,  48,  50,  51,  52,  54,  55,  56,  58,  59,  61,  62,
         62,  63,  64,  66,  67,  68,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
         80,  80,  81,  82,  83,  84,  84,  85,  86,  87,  88,  89,  89,  90,  91,  91,
         92,  93,  94,  95,  96,  97,  97,  98,  99,  99, 101, 102, 102, 103, 104, 104,
        106, 107, 108, 108, 109, 110, 111, 112, 113, 113, 115, 116, 116, 117, 118, 119,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   4,   5,   5,   6,   7,   8,
         10,  11,  12,  14,  16,  18,  19,  21,  23,  25,  26,  28,  30,  32,  34,  36,
         38,  40,  41,  43,  45,  46,  47,  48,  50,  52,  53,  54,  55,  56,  58,  59,
         60,  61,  61,  63,  64,  65,  67,  68,  69,  69,  71,  72,  73,  74,  75,  76,
         76,  77,  78,  79,  80,  81,  81,  82,  83,  83,  84,  86,  86,  87,  88,  88,
         89,  91,  92,  92,  93,  94,  94,  96,  97,  97,  98, 100, 100, 101, 102, 103,
        104, 105, 106, 107, 108, 109, 110, 111, 112, 112, 114, 115, 116, 117, 118, 119,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   4,   4,   5,   6,   7,   8,
          9,  11,  12,  13,  15,  17,  18,  20,  22,  23,  25,  27,  28,  30,  33,  34,
         36,  38,  39,  41,  43,  44,  45,  46,  48,  49,  50,  51,  53,  54,  55,  56,
         57,  58,  59,  60,  61,  62,  64,  65,  65,  66,  68,  69,  70,  70,  71,  72,
         73,  73,  74,  75,  76,  77,  78,  79,  80,  80,  81,  82,  83,  84,  85,  85,
         87,  88,  89,  89,  90,  92,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101,
        102, 103, 105, 105, 107, 108, 108, 110, 111, 112, 113, 115, 115, 116, 118, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   3,   4,   5,   6,   7,   7,
          9,  10,  11,  13,  14,  16,  18,  19,  21,  22,  24,  26,  27,  29,  31,  33,
         34,  36,  37,  39,  41,  41,  43,  44,  45,  47,  48,  49,  50,  51,  53,  53,
         54,  55,  56,  57,  58,  59,  61,  61,  62,  63,  65,  65,  66,  67,  68,  69,
         69,  70,  71,  72,  73,  74,  74,  76,  77,  77,  78,  79,  80,  81,  82,  82,
         84,  85,  86,  87,  88,  89,  89,  91,  92,  93,  94,  95,  96,  97,  98,  99,
        100, 102, 103, 104, 105, 107, 107, 109, 110, 111, 112, 114, 115, 116, 118, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   3,   4,   5,   6,   6,   7,
          8,  10,  11,  12,  14,  15,  17,  18,  20,  21,  23,  24,  26,  27,  30,  31,
         33,  34,  36,  37,  39,  39,  41,  42,  43,  45,  45,  46,  48,  48,  50,  51,
         51,  52,  53,  54,  55,  56,  58,  58,  59,  60,  61,  62,  63,  64,  64,  65,
         66,  66,  67,  68,  70,  71,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,
         81,  82,  83,  84,  85,  86,  87,  88,  90,  90,  92,  93,  94,  95,  97,  97,
         99, 100, 102, 102, 104, 105, 106, 108, 109, 110, 112, 113, 114, 116, 117, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   2,   3,   4,   5,   5,   6,   7,
          8,   9,  10,  11,  13,  14,  16,  17,  19,  20,  21,  23,  24,  26,  28,  29,
         31,  32,  34,  35,  37,  37,  39,  39,  41,  42,  43,  44,  45,  46,  47,  48,
         49,  49,  50,  52,  52,  53,  54,  55,  56,  57,  58,  59,  59,  60,  61,  62,
         62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,
         78,  79,  81,  81,  83,  84,  84,  86,  87,  88,  89,  91,  92,  93,  95,  95,
         97,  98, 100, 101, 102, 104, 105, 107, 108, 109, 111, 113, 114, 115, 117, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   2,   2,   3,   4,   4,   5,   6,   6,
          7,   9,   9,  11,  12,  14,  15,  16,  18,  19,  20,  22,  23,  24,  26,  28,
         29,  30,  32,  33,  34,  35,  36,  37,  39,  40,  41,  41,  43,  43,  45,  45,
         46,  47,  47,  49,  49,  50,  51,  52,  53,  53,  55,  55,  56,  57,  57,  58,
         59,  59,  60,  62,  63,  64,  64,  66,  67,  67,  69,  70,  70,  72,  73,  74,
         75,  76,  78,  78,  80,  81,  82,  83,  85,  86,  87,  89,  89,  91,  93,  93,
         95,  97,  99,  99, 101, 103, 104, 106, 107, 108, 110, 112, 113, 115, 117, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   2,   2,   3,   4,   4,   5,   5,   6,
          7,   8,   9,  10,  11,  13,  14,  15,  17,  18,  19,  20,  22,  23,  25,  26,
         27,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,
         43,  44,  44,  46,  46,  47,  48,  49,  50,  50,  51,  52,  53,  53,  54,  55,
         55,  56,  57,  58,  59,  61,  61,  62,  64,  64,  65,  67,  67,  69,  70,  71,
         72,  74,  75,  76,  77,  79,  79,  81,  83,  83,  85,  87,  87,  89,  91,  92,
         93,  95,  97,  98, 100, 102, 103, 104, 106, 107, 109, 111, 113, 115, 117, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   4,   5,   5,   6,
          7,   8,   8,  10,  11,  12,  13,  14,  15,  17,  18,  19,  20,  21,  23,  24,
         26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  36,  37,  38,  39,  40,
         40,  41,  42,  43,  43,  44,  45,  46,  46,  47,  48,  49,  49,  50,  51,  51,
         52,  52,  53,  55,  56,  57,  58,  59,  60,  61,  62,  64,  64,  66,  67,  68,
         69,  71,  72,  73,  75,  76,  77,  79,  80,  81,  83,  84,  85,  87,  89,  90,
         92,  94,  95,  96,  98, 100, 101, 103, 105, 107, 109, 111, 112, 114, 116, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    }
};
static const uint8_t drum_kits[8] = { 0, 8, 16, 24, 25, 32, 40, 48 };
static const uint8_t drum_kit_numbers[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
static int32_t *reverb_data_ptr;
static const int32_t dword_C00342C0[4] = { 0, 1, 2, -1 };
static const uint16_t word_C00342D0[17] = { 0, 250, 561, 949, 1430, 2030, 2776, 3704, 4858, 6295, 8083, 10307, 13075, 16519, 20803, 26135, 32768 };


VLSG_API_(uint32_t) VLSG_GetVersion(void)
{
    return 0x103;
}

VLSG_API_(const char*) VLSG_GetName(void)
{
    return VLSG_Name;
}

VLSG_API_(uint32_t) VLSG_GetTime(void)
{
  if (get_time_func != NULL)
    return get_time_func();
  return 0;
}

VLSG_API_(void) VLSG_SetFunc_GetTime(VLSG_GETTIME get_time)
{
    get_time_func = get_time;
}

// Private functions
static int32_t InitializeVelocityFunc(void);
static int32_t EMPTY_DeinitializeVelocityFunc(void);
static int32_t InitializeVariables(void);
static int32_t EMPTY_DeinitializeVariables(void);
static void CountActiveVoices(void);
static void SetMaximumVoices(int maximum_voices);
//static void ProcessMidiData(void);
static Voice_Data *FindAvailableVoice(int32_t channel_num_2, int32_t note_number);
static Voice_Data *FindVoice(int32_t channel_num_2, int32_t note_number);
static void NoteOff(void);
static void NoteOn(int32_t ch);
static void ControlChange(void);
static void SystemExclusive(void);
static int32_t InitializeReverbBuffer(void);
static int32_t DeinitializeReverbBuffer(void);
static void EnableReverb(void);
static void DisableReverb(void);
static void SetReverbShift(uint32_t shift);
static void DefragmentVoices(void);
static void GenerateOutputData(uint8_t *output_ptr, uint32_t offset1, uint32_t offset2);
static void GenerateOutputDataVst(double** output_ptr, uint32_t offset1, uint32_t offset2); // invasive workaround
static int32_t InitializeMidiDataBuffer(void);
static int32_t EMPTY_DeinitializeMidiDataBuffer(void);
static void AddByteToMidiDataBuffer(uint8_t value);
static uint8_t GetValueFromMidiDataBuffer(void);
static int32_t InitializePhase(void);
static int32_t EMPTY_DeinitializePhase(void);
static void voice_set_panpot(Voice_Data *voice_data_ptr);
static void voice_set_flags(Voice_Data *voice_data_ptr);
static void voice_set_flags2(Voice_Data *voice_data_ptr);
static void voice_set_amp(Voice_Data *voice_data_ptr);
//static void ProcessPhase(void);
static int32_t sub_C0036FB0(int16_t value3);
static void sub_C0036FE0(void);
static void sub_C0037140(void);
static int32_t InitializeStructures(void);
static int32_t EMPTY_DeinitializeStructures(void);
static void ResetAllControllers(Channel_Data *channel_data_ptr);
static void ResetChannel(Channel_Data *channel_data_ptr);
static uint32_t rom_change_bank(uint32_t bank, int32_t index);
static uint16_t rom_read_word(void);
static int16_t rom_read_word_at(uint32_t offset);


static inline uint16_t READ_LE_UINT16(const uint8_t *ptr)
{
    return ptr[0] | (ptr[1] << 8);
}


VLSG_API_(VLSG_Bool) VLSG_SetParameter(uint32_t type, uintptr_t value)
{
    switch (type)
    {
        case PARAMETER_OutputBuffer:
            return VLSG_SetWaveBuffer((void*)value);

        case PARAMETER_ROMAddress:
            return VLSG_SetRomAddress((const void*)value);

        case PARAMETER_Frequency:
            if (value == 0) {
                return VLSG_SetFrequency(11025);
            }
            if (value == 1) {
              return VLSG_SetFrequency(22050);
            }
            if (value == 2) {
                return VLSG_SetFrequency(44100);
            }
            if (value == 3) { // Experimental
              return VLSG_SetFrequency(16538);
            }
            if (value == 4) { // Experimental
              return VLSG_SetFrequency(48000);
            }
            return VLSG_SetFrequency(44100);

        case PARAMETER_Polyphony:
            if (value == 0x11) {
                return VLSG_SetPolyphony(32);
            }
            else if (value == 0x12) {
                return VLSG_SetPolyphony(48);
            }
            else if (value == 0x13) {
                return VLSG_SetPolyphony(64);
            }
            else if (value == 0x14) { // Experimental
              return VLSG_SetPolyphony(128);
            }
            return VLSG_SetPolyphony(24);

        case PARAMETER_Effect:
            if (value == 0x20) {
                return VLSG_SetEffect(0);
            }
            if (value == 0x22) {
                return VLSG_SetEffect(2);
            }
            return VLSG_SetEffect(1);

        case PARAMETER_VelocityFunc:  // Sysex 0x40 but yeah don't care
            return VLSG_SetVelocityFunc(value & 0xF);

        default:
            return VLSG_FALSE;
    }
}

VLSG_API_(VLSG_Bool) VLSG_SetWaveBuffer(void* ptr)
{
    output_data_ptr = (uint8_t*)ptr;
    return VLSG_TRUE;
}

VLSG_API_(VLSG_Bool) VLSG_SetRomAddress(const void* ptr)
{
    romsxgm_ptr = (const uint8_t*)ptr;
    return VLSG_TRUE;
}

VLSG_API_(VLSG_Bool) VLSG_SetFrequency(unsigned int frequency)
{
    uint32_t buffer_size;

    if (frequency == 11025) {
        output_frequency = 11025;
        output_size_para = 64;
        buffer_size = 4096;
    } else if (frequency == 44100) {
        output_frequency = 44100;
        output_size_para = 256;
        buffer_size = 16384;
    } else if (frequency == 22050) {
        output_frequency = 22050;
        output_size_para = 128;
        buffer_size = 8192;
    } else if (frequency == 16538) {
        output_frequency = 16538;
        output_size_para = 96;
        buffer_size = 8000;
    } else if (frequency == 48000) {
        output_frequency = 48000;
        output_size_para = 384;
        buffer_size = 16384;
    } else {
        return VLSG_FALSE;
    }

    output_buffer_size_samples = buffer_size;
    output_buffer_size_bytes = 4 * buffer_size;
    InitializeReverbBuffer();
    return VLSG_TRUE;
}

VLSG_API_(VLSG_Bool) VLSG_SetPolyphony(unsigned int poly)
{
    int32_t polyphony;

    if (poly == 24 || poly == 32 || poly == 48 || poly == 64 || poly == 128) {
        polyphony = (int32_t)poly;
    } else {
        return VLSG_FALSE;
    }

    maximum_polyphony = polyphony;
    maximum_polyphony_new_value = polyphony;
    return VLSG_TRUE;
}

VLSG_API_(VLSG_Bool) VLSG_SetEffect(unsigned int effect)
{
    if (effect > 2) {
        return VLSG_FALSE;
    }
    effect_param_value = 0x20 + effect;
    DisableReverb();
    if (effect == 0) {
        DisableReverb(); // calls twice
        return VLSG_TRUE;
    }
    if (effect_param_value == 0x22)
    {
        SetReverbShift(0);
        EnableReverb();
        return VLSG_TRUE;
    }
    SetReverbShift(1);
    EnableReverb();
    return VLSG_TRUE;
}

VLSG_API_(VLSG_Bool) VLSG_SetVelocityFunc(unsigned int curveIdx)
{
    if (curveIdx < 0 || curveIdx >= 11)
      curveIdx = 6;
    
    velocity_func = curveIdx;
    return VLSG_TRUE;
}

VLSG_API_(VLSG_Bool) VLSG_Init(void)
{
    current_polyphony = 0;
    dword_C0000000 = 0;

    if (InitializeVelocityFunc())
    {
        return VLSG_FALSE;
    }

    if (InitializeVariables())
    {
        EMPTY_DeinitializeVelocityFunc();
        return VLSG_FALSE;
    }

    if (InitializeReverbBuffer())
    {
        EMPTY_DeinitializeVariables();
        EMPTY_DeinitializeVelocityFunc();
        return VLSG_FALSE;
    }

    if (InitializePhase())
    {
        DeinitializeReverbBuffer();
        EMPTY_DeinitializeVariables();
        EMPTY_DeinitializeVelocityFunc();
        return VLSG_FALSE;
    }

    if (InitializeMidiDataBuffer())
    {
        EMPTY_DeinitializePhase();
        DeinitializeReverbBuffer();
        EMPTY_DeinitializeVariables();
        EMPTY_DeinitializeVelocityFunc();
        return VLSG_FALSE;
    }

    if (InitializeStructures())
    {
        EMPTY_DeinitializeMidiDataBuffer();
        EMPTY_DeinitializePhase();
        DeinitializeReverbBuffer();
        EMPTY_DeinitializeVariables();
        EMPTY_DeinitializeVelocityFunc();
        return VLSG_FALSE;
    }

    dword_C0000004 = 2972;
    return VLSG_TRUE;
}

VLSG_API_(VLSG_Bool) VLSG_Exit(void)
{
    current_polyphony = 0;

    EMPTY_DeinitializeStructures();
    EMPTY_DeinitializeMidiDataBuffer();
    EMPTY_DeinitializePhase();
    DeinitializeReverbBuffer();
    EMPTY_DeinitializeVariables();
    return EMPTY_DeinitializeVelocityFunc();
}

VLSG_API_(void) VLSG_Write(const void* data, uint32_t len)
{
    const uint8_t* ptr = (const uint8_t*)(data);
    for (; len != 0; len--)
    {
        AddByteToMidiDataBuffer(*ptr++);
    }
}

VLSG_API_(int32_t) VLSG_Buffer(uint32_t output_buffer_counter)
{
    uint32_t time1, value1, time2, time3, offset1;
    int counter;
    uint8_t *output_ptr;
    uint32_t time4;

    time1 = VLSG_GetTime();

    if ((output_buffer_counter == 0) || (time1 - system_time_1 > 200))
    {
        value1 = 0;
        time2 = time1;
        system_time_1 = time1;
        system_time_2 = time1;
    }
    else
    {
        value1 = dword_C0000000;
        time2 = dword_C0000008;
    }

    dword_C0000000 = value1;
    dword_C0000008 = time2;

    if (value1 >= 512)
    {
        dword_C0000000 = 0;
        time2 += dword_C0000004;
        dword_C0000008 = time2;
        time3 = (7 * dword_C0000004 - system_time_2) + time1;

        if (time1 < time2)
        {
            time3 -= (time2 - time1) >> 4;
        }
        else if (time1 > time2)
        {
            time3 += (time1 - time2) >> 4;
        }

        system_time_2 = time1;
        dword_C0000004 = (time3 >> 3) + ((time3 & 4) >> 2);
    }

    offset1 = 0;

    output_ptr = &output_data_ptr[((output_buffer_counter & 0x0F) * output_size_para) << 4];
    
    for (counter = 4; counter != 0; counter--)
    {
        //ProcessMidiData();
        ProcessPhase();
        GenerateOutputData(output_ptr, offset1, offset1 + output_size_para);
        offset1 += output_size_para;
        dword_C0000000++;
        //system_time_1 = (((uint32_t)(dword_C0000000 * dword_C0000004)) >> 9) + dword_C0000008;
    }

    time4 = VLSG_GetTime();
    CountActiveVoices();
    time4 -= time1;
    maximum_polyphony = maximum_polyphony_new_value;

    if (time4 > 300)
    {
        SetMaximumVoices(2);
        return current_polyphony;
    }

    if (time4 >= 16)
    {
        if (time4 >= 20)
        {
            SetMaximumVoices((3 * current_polyphony) >> 2);
            return current_polyphony;
        }

        SetMaximumVoices((7 * current_polyphony) >> 3);
        return current_polyphony;
    }

    return current_polyphony;
}

// Seriously CBF that hardcoded buffer BS so writing the output directly on demand.
VLSG_API_(int32_t) VLSG_BufferVst(uint32_t output_buffer_counter, double** output, int nFrames, iplug::IMidiQueue& mMidiQueue, iplug::IMidiQueueBase<iplug::ISysEx>& mSysExQueue)
{
  /*if (output_buffer_counter == 0) {
    system_time_1 = VLSG_GetTime();
  }*/
  
  int32_t offset1 = 0;
  static int phaseAcc = INT_MIN;

  int quant = (nFrames > output_size_para) ? output_size_para : 1;
  //int quant = output_size_para / 4;
  //int quant = nFrames;
  int frames_left = nFrames;
  //if (quant < 16) quant = 16;
  for (; frames_left > 0; frames_left -= quant)
  {
    // If uneven quants, get the correct range
    if (frames_left < quant)
      quant = frames_left;

    //ProcessMidiData();  // in case anything missed
    while (!mSysExQueue.Empty()) {
      auto msg = mSysExQueue.Peek();
      if (msg.mOffset > offset1) break; // assume chronological order

      ProcessSysExDataVst(msg);
      mSysExQueue.Remove();
    }

    while (!mMidiQueue.Empty()) {
      auto msg = mMidiQueue.Peek();
      if (msg.mOffset > offset1) break; // assume chronological order

      ProcessMidiDataVst(msg);
      mMidiQueue.Remove();
    }

    // Do not progress envelope phase until after output_size_para frames (as per original hardcoded BS)
    if (phaseAcc == INT_MIN || phaseAcc >= output_size_para) {
      ProcessPhase();
      phaseAcc = (INT_MIN) ? 0 : (phaseAcc - output_size_para);
    } else {
      phaseAcc += quant;
    }
    GenerateOutputDataVst(output, offset1, offset1 + quant);
    offset1 += quant;

    //dword_C0000000++;
    //system_time_1 = (((uint32_t)(dword_C0000000 * dword_C0000004)) >> 9) + dword_C0000008;
    //0system_time_1 = VLSG_GetTime();
  }
  CountActiveVoices();
  maximum_polyphony = maximum_polyphony_new_value;
  return current_polyphony;
}


int32_t VLSG_PlaybackStart(void)
{
  return (int32_t)VLSG_Init();
}

int32_t VLSG_PlaybackStop(void)
{
  return (int32_t)VLSG_Exit();
}

void VLSG_AddMidiData(uint8_t *ptr, uint32_t len)
{
  VLSG_Write(ptr, len);
}

int32_t VLSG_FillOutputBuffer(uint32_t output_buffer_counter)
{
  return VLSG_Buffer(output_buffer_counter);
}

static int32_t InitializeVelocityFunc(void)
{
    velocity_func = 6;
    return 0;
}

static int32_t EMPTY_DeinitializeVelocityFunc(void)
{
    return 0;
}

static void voice_set_freq(Voice_Data *voice_data_ptr, int32_t pitch)
{
    Channel_Data *channel_ptr;
    int32_t value1;
    uint32_t value2;

    channel_ptr = &(channel_data[voice_data_ptr->channel_num_2 >> 1]);
    value1 = (((int32_t)(channel_ptr->pitch_bend * channel_ptr->pitch_bend_sense)) >> 13) + pitch + channel_ptr->fine_tune + 2180;
    value2 = dword_C0032188[216 + (value1 >> 8)] * dword_C0032588[value1 & 0xFF];

    voice_data_ptr->v_freq = value2;
    switch (output_frequency)
    {
        case 11025:
            voice_data_ptr->v_freq = value2 >> 17;
            break;
        case 22050:
            voice_data_ptr->v_freq = value2 >> 18;
            break;
        case 44100:
            voice_data_ptr->v_freq = value2 >> 19;
            break;
        case 16538:
            voice_data_ptr->v_freq = (value2 / 3) >> 16;
            break;
        default:
            voice_data_ptr->v_freq = (uint32_t)((value2 >> 17) * 11025) / output_frequency;
            break;
    }
}

static int32_t voice_get_index(Voice_Data *voice_data_ptr, int32_t pitch)
{
    uint32_t offset1;
    int32_t channel_num_2;
    int32_t note_number;

    offset1 = rom_change_bank(3, pitch);
    channel_num_2 = (int16_t)(voice_data_ptr->channel_num_2 & ~1);
    note_number = voice_data_ptr->note_number;

    if (channel_num_2 != (2 * DRUM_CHANNEL))
    {
        note_number += channel_data[channel_num_2 >> 1].coarse_tune;
        note_number += (voice_data_ptr->detune + 128) >> 8;

        if (note_number < 12)
        {
            note_number += 12 * ((23 - note_number) / 12);
        }

        if (note_number > 108)
        {
            note_number -= 12 * ((note_number - 97) / 12);
        }
    }

    return rom_read_word_at(offset1 + 2 * note_number);
}

static void ProgramChange(Program_Data *program_data_ptr, uint32_t program_number)
{
    int16_t *data;
    int counter, index;

    if (program_data_ptr == &(program_data[DRUM_CHANNEL * 2]))
    {
        program_number = (program_number & 7) + 128;
    }

    rom_change_bank(1, rom_read_word_at(rom_change_bank(19, 0) + 2 * program_number));

    for (counter = 2; counter != 0; counter--)
    {
        data = (int16_t*)program_data_ptr;
        for (index = 0; index < 14; index++)
        {
            data[index] = (int16_t)rom_read_word();
        }

        program_data_ptr->field_06 >>= 8;
        program_data_ptr->field_08 >>= 8;
        program_data_ptr->panpot   >>= 8;
        program_data_ptr->field_0C >>= 8;
        program_data_ptr->field_0E >>= 8;
        program_data_ptr->field_10 >>= 8;
        program_data_ptr->field_16 >>= 8;
        program_data_ptr->field_18 >>= 8;
        program_data_ptr->field_1A >>= 8;

        program_data_ptr++;
    }
}

static void VoiceSoundOff(Voice_Data *voice_data_ptr)
{
    voice_data_ptr->field_50 = 0x7FFF;
    voice_data_ptr->vflags &= ~VFLAG_Value40;
    voice_data_ptr->vflags |= VFLAG_Value80;
    voice_data_ptr->vflags &= VFLAG_MaskC0;
    voice_set_flags2(voice_data_ptr);
    voice_set_flags(voice_data_ptr);
}

static void VoiceNoteOff(Voice_Data *voice_data_ptr)
{
    voice_data_ptr->vflags |= VFLAG_Value80;

    if ((voice_data_ptr->vflags & VFLAG_Value40) == 0)
    {
        voice_data_ptr->vflags &= VFLAG_MaskC0;
        voice_set_flags2(voice_data_ptr);
        voice_set_flags(voice_data_ptr);
    }
}

static void AllChannelNotesOff(int32_t channel_num)
{
    int index;

    for (index = 0; index < MAX_VOICES; index++)
    {
        if ((voice_data[index].channel_num_2 >> 1) == channel_num)
        {
            VoiceNoteOff(&(voice_data[index]));
        }
    }
}

static void AllChannelSoundsOff(int32_t channel_num)
{
    int index;

    for (index = 0; index < MAX_VOICES; index++)
    {
        if ((voice_data[index].channel_num_2 >> 1) == channel_num)
        {
            VoiceSoundOff(&(voice_data[index]));
        }
    }
}

static void ControllerSettingsOn(int32_t channel_num)
{
    int index;

    for (index = 0; index < maximum_polyphony; index++)
    {
        if ((voice_data[index].channel_num_2 >> 1) == channel_num)
        {
            if (voice_data[index].note_number != 255)
            {
                if ((voice_data[index].vflags & VFLAG_Value80) == 0)
                {
                    voice_data[index].vflags |= VFLAG_Value40;
                }
            }
        }
    }
}

static void ControllerSettingsOff(int32_t channel_num)
{
    int index;

    for (index = 0; index < maximum_polyphony; index++)
    {
        if ((voice_data[index].channel_num_2 >> 1) == channel_num)
        {
            if (voice_data[index].note_number != 255)
            {
                voice_data[index].vflags &= ~VFLAG_Value40;
                if ((voice_data[index].vflags & VFLAG_Value80) != 0)
                {
                    voice_data[index].vflags &= VFLAG_MaskC0;

                    voice_set_flags2(&(voice_data[index]));
                    voice_set_flags(&(voice_data[index]));
                }
            }
        }
    }
}

static void StartPlayingVoice(Voice_Data *voice_data_ptr, Channel_Data *channel_data_ptr, Program_Data *program_data_ptr)
{
    uint16_t value0;
    uint32_t value1;
    int32_t value2;
    int32_t value3;
    int32_t channel_num_2;
    int32_t value4;
    int32_t value5;
    int32_t value6;
    int index;
    const int32_t *drum_exc_pair;
    uint32_t value7;
    int32_t value8;

    voice_data_ptr->detune = program_data_ptr->detune;
    voice_data_ptr->pgm_f0E = program_data_ptr->field_0E;
    voice_data_ptr->pgm_f10 = program_data_ptr->field_10;
    voice_data_ptr->index = program_data_ptr->index;
    voice_data_ptr->pgm_f14 = program_data_ptr->field_14;

    value1 = (uint16_t)rom_read_word_at(rom_change_bank(2, (program_data_ptr->field_02 & 0xFFF) + voice_get_index(voice_data_ptr, program_data_ptr->field_00 >> 8)));
    value2 = 0;
    value0 = rom_read_word();
    value1 |= (value0 & 0xFF) << 16;
    voice_data_ptr->wv_fpos = value1 << 10;

    value1 = value0 >> 8;
    value0 = rom_read_word();
    value1 |= value0 << 8;
    voice_data_ptr->wv_end = value1 & 0x3FFFFF;

    rom_read_word();
    value1 = rom_read_word();

    value0 = rom_read_word();
    value1 |= (value0 & 0xFF) << 16;
    voice_data_ptr->wv_un1_hi = value0 >> 8;
    voice_data_ptr->wv_un1_lo = value0 & 0xFF;
    voice_data_ptr->wv_start = value1 & 0x3FFFFF;

    voice_data_ptr->base_freq = rom_read_word();
    value0 = rom_read_word();

    voice_data_ptr->wv_un3_lo = value0 & 0xFF;
    voice_data_ptr->field_0C[3] = 0;
    voice_data_ptr->field_0C[2] = 0;
    voice_data_ptr->wv_pos = ((voice_data_ptr->wv_fpos & ~0x400u) >> 10) - 2;
    voice_data_ptr->wv_un3_hi = value0 >> 8;

    value3 = program_data_ptr->field_02 & 0x7000;
    if ( value3 != 0x7000 )
    {
        value2 = voice_data_ptr->note_number;
        channel_num_2 = (int16_t)(voice_data_ptr->channel_num_2 & ~1);
        if (channel_num_2 != (2 * DRUM_CHANNEL))
        {
            value2 += channel_data[channel_num_2 >> 1].coarse_tune;
            value2 += (voice_data_ptr->detune + 128) >> 8;

            if (value2 < 12)
            {
                value2 += 12 * ((23 - value2) / 12);
            }

            if (value2 > 108)
            {
                value2 -= 12 * ((value2 - 97) / 12);
            }
        }

        value2 = (value2 - voice_data_ptr->wv_un1_hi) << 8;

        for (; value3 != 0; value3 -= 0x1000)
        {
            value2 >>= 1;
        }
    }

    value2 += voice_data_ptr->base_freq;
    value2 += (int8_t)voice_data_ptr->detune;
    voice_data_ptr->base_freq = value2;
    voice_set_freq(voice_data_ptr, value2);
    voice_set_amp(voice_data_ptr);

    value4 = program_data_ptr->field_18;
    value5 = velocity_curves[velocity_func + 1][voice_data_ptr->note_velocity];

    if (value4 >= 0)
    {
        value5 = 127 - value5;
    }
    else
    {
        value4 = -value4;
    }

    value6 = (127 - (((int32_t)(value4 * value5)) >> 7)) + program_data_ptr->field_1A;

    if ((channel_data_ptr->chflags & CHFLAG_Soft) != 0)
    {
        value6 >>= 1;
    }

    if (value6 > 127)
    {
        voice_data_ptr->v_velocity = 127;
    }
    else
    {
        if (value6 <= 0)
        {
            value6 = 0;
        }
        voice_data_ptr->v_velocity = value6;
    }

    voice_data_ptr->field_4C = 0;
    voice_data_ptr->field_2C = 0;
    voice_data_ptr->field_52 = 0;
    voice_data_ptr->vflags = 0;
    voice_data_ptr->v_vol = 0;
    voice_set_flags(voice_data_ptr);
    voice_set_flags2(voice_data_ptr);

    if ((channel_data_ptr->chflags & CHFLAG_Sostenuto) != 0)
    {
        for (index = 0; index < maximum_polyphony; index++)
        {
            if (voice_data[index].note_number == 255) continue;
            if (voice_data_ptr->note_number != voice_data[index].note_number) continue;
            if (voice_data[index].channel_num_2 != voice_data_ptr->channel_num_2) continue;
            if ((voice_data[index].vflags & VFLAG_Value80) == 0) continue;
            if ((voice_data[index].vflags & VFLAG_Value40) == 0) continue;

            voice_data_ptr->vflags |= VFLAG_Value40;
            break;
        }
    }

    if ((channel_data_ptr->chflags & CHFLAG_Sustain) != 0)
    {
        voice_data_ptr->vflags |= VFLAG_Value40;
    }

    if ((voice_data_ptr->channel_num_2 & ~1) == (2 * DRUM_CHANNEL))
    {
        voice_data_ptr->v_panpot = rom_read_word_at(rom_change_bank(18, 0) + 4 * voice_data_ptr->note_number);
        voice_set_panpot(voice_data_ptr);

        drum_exc_pair = &(drum_exc_map[DRUM_EXC_ORCHESTRA]);
        // unless the orchestra drum is set
        if (channel_data[DRUM_CHANNEL].program_change != 135)
        {
            // look for hi-hat etc.
            drum_exc_pair = &(drum_exc_map[0]);
        }

        for (; drum_exc_pair[0] != 0; drum_exc_pair += 2)
        {
            if (drum_exc_pair[0] != voice_data_ptr->note_number) continue;

            for (index = 0; index < maximum_polyphony; index++)
            {
                if (voice_data[index].note_number == drum_exc_pair[1])
                {
                    if ((voice_data[index].channel_num_2 & ~1) == (2 * DRUM_CHANNEL))
                    {
                        voice_data[index].note_number = 255;
                    }
                }
            }
        }
    }
    else
    {
        value7 = rom_change_bank(17, 0);
        value8 = channel_data_ptr->pan + program_data_ptr->panpot;

        if (value8 > 127)
        {
            value8 = 127;
        }
        else if (value8 <= -127)
        {
            value8 = -127;
        }

        voice_data_ptr->v_panpot = rom_read_word_at(value7 + 2 * value8 + 256);
        voice_set_panpot(voice_data_ptr);
    }
}

static void AllVoicesSoundsOff(void)
{
    int index;

    for (index = 0; index < MAX_VOICES; index++)
    {
        if (voice_data[index].note_number != 255)
        {
            VoiceSoundOff(&(voice_data[index]));
        }
    }
}

static int32_t InitializeVariables(void)
{
    recent_voice_index = 0;
    event_length = 0;
    event_type = 0;
    return 0;
}

static int32_t EMPTY_DeinitializeVariables(void)
{
    return 0;
}

static void CountActiveVoices(void)
{
    int active_voices, index;

    active_voices = 0;
    for (index = 0; index < maximum_polyphony; index++)
    {
        if (voice_data[index].note_number != 255)
        {
            active_voices++;
        }
    }
    current_polyphony = active_voices;
}

static void ReduceActiveVoices(int32_t maximum_voices)
{
    int index1, index2, index3;
    int active_voices;

    if (maximum_voices >= maximum_polyphony) return;

    if (maximum_voices > 0)
    {
        index2 = recent_voice_index + 1;
        if (index2 >= maximum_polyphony)
        {
            index2 = 0;
        }

        active_voices = 0;
        for (index1 = 0; index1 < maximum_polyphony; index1++)
        {
            if (voice_data[index1].note_number != 255)
            {
                active_voices++;
            }
        }

        index3 = index2;
        do
        {
            if (voice_data[index3].note_number != 255)
            {
                if (voice_data[index3].vflags & VFLAG_Value80)
                {
                    voice_data[index3].note_number = 255;
                    active_voices--;

                    if (active_voices <= maximum_voices)
                    {
                        current_polyphony = active_voices;
                        return;
                    }
                }
            }

            index3++;
            if (index3 >= maximum_polyphony)
            {
                index3 = 0;
            }
        } while (index3 != recent_voice_index);

        while (1)
        {
            if (voice_data[index2].note_number != 255)
            {
                voice_data[index2].note_number = 255;
                active_voices--;

                if (active_voices <= maximum_voices)
                {
                    break;
                }
            }

            index2++;
            if (index2 >= maximum_polyphony)
            {
                index2 = 0;
            }
            if (index2 == recent_voice_index)
            {
                return;
            }
        }

        current_polyphony = active_voices;
    }
    else
    {
        for (index1 = 0; index1 < maximum_polyphony; index1++)
        {
            voice_data[index1].note_number = 255;
        }
        current_polyphony = 0;
    }
}

static void SetMaximumVoices(int maximum_voices)
{
    int index;

    ReduceActiveVoices(maximum_voices);
    DefragmentVoices();
    maximum_polyphony = maximum_voices;

    for (index = maximum_voices; index < MAX_VOICES; index++)
    {
        voice_data[index].note_number = 255;
    }

    CountActiveVoices();

    recent_voice_index = 0;
}

VLSG_API_(void) ProcessMidiData(void)
{
    uint8_t midi_value;

    while (1)
    {
        midi_value = GetValueFromMidiDataBuffer();
        if (midi_value == 0xFF) break; // Stop processing

        if (midi_value > 0xF7) continue; // Drop MIDI data, continue to next.

        if (midi_value == 0xF7)
        {
            if (event_data[0] != 0xF0) continue;
        }
        else if ((midi_value & 0x80) != 0)
        {
            event_length = 0;
            event_type = midi_value & 0xF0;
            event_data[0] = midi_value;
            channel_data_ptr = &(channel_data[midi_value & 0x0F]);
            program_data_ptr = &(program_data[(midi_value & 0x0F) * 2]);

            continue;
        }
        else
        {
            event_length++;
            if (event_length >= 256) continue;

            event_data[event_length] = midi_value;

            if (event_data[0] == 0xF0) continue;

            if ((event_type != 0xC0) && (event_type != 0xD0) && (event_length != 2)) continue;
        }

        switch (event_type)
        {
            case 0x80: // Note Off
                NoteOff();
                break;

            case 0x90: // Note On
                if (event_data[2] != 0)
                {
                    NoteOn(0);

                    if (program_data_ptr->field_02 & 0x8000)
                    {
                        NoteOn(1);
                    }
                }
                else
                {
                    NoteOff();
                }
                break;

            case 0xB0: // Controller
                ControlChange();
                break;

            case 0xC0: // Program Change
                if ((event_data[0] & 0x0F) == DRUM_CHANNEL)
                {
                    int drum_kit_index;

                    for (drum_kit_index = 0; drum_kit_index < 8; drum_kit_index++)
                    {
                        if (drum_kits[drum_kit_index] == event_data[1]) break;
                    }
                    if (drum_kit_index >= 8) break;

                    channel_data_ptr->program_change = drum_kit_numbers[drum_kit_index];
                    ProgramChange(program_data_ptr, drum_kit_numbers[drum_kit_index]);
                }
                else
                {
                    channel_data_ptr->program_change = event_data[1];
                    ProgramChange(program_data_ptr, event_data[1]);
                }
                break;

            case 0xD0: // Channel Pressure
                channel_data_ptr->channel_pressure = event_data[1];
                break;

            case 0xE0: // Pitch Bend
                channel_data_ptr->pitch_bend = event_data[1] + ((event_data[2] - 64) << 7);
                break;

            case 0xF0: // SysEx
                SystemExclusive();
                break;

            default:
                break;
        }

        event_length = 0;
    }

    system_time_1 = VLSG_GetTime();
}

static uint8_t* parseMidiMsg(iplug::IMidiMsg& msg)
{
  iplug::IMidiMsg::EStatusMsg status = msg.StatusMsg();

  // TODO port to msg.mData1/2 calls
  static uint8_t data[12];
  int length = 0;
  auto sampOffset = msg.mOffset; // TODO how to use this?
  uint8_t chan = msg.Channel() & 0xF;
  uint8_t key = msg.NoteNumber() & 0x7F;
  uint8_t vel = msg.Velocity() & 0x7F;
  uint8_t prog = msg.Program() & 0x7F;

  switch (status)
  {
  case iplug::IMidiMsg::kNoteOn:
    data[0] = 0x90 | chan;
    data[1] = key;
    data[2] = vel;
    length = 3;

    //lsgWrite(data, length, sampOffset);

    //printf("Note ON, channel:%d note:%d velocity:%d\n", chan, key, vel);
    
    break;

  case iplug::IMidiMsg::kNoteOff:
    data[0] = 0x80 | chan;
    data[1] = key;
    data[2] = vel;
    length = 3;

    //lsgWrite(data, length);

    //printf("Note OFF, channel:%d note:%d velocity:%d\n", chan, key, vel);
    
    break;

  case iplug::IMidiMsg::kControlChange: {
    uint8_t cc = msg.ControlChangeIdx();
    uint8_t ccVal = (uint8_t)(msg.ControlChange(msg.ControlChangeIdx()) * 127) & 0x7F; // assuming 7-bit
    data[0] = 0xB0 | chan;
    data[1] = cc & 0x7f;
    data[2] = ccVal & 0x7f;

    length = 3;

    //lsgWrite(data, length);

    //printf("Controller, channel:%d param:%d value:%d\n", chan, key, vel);
    
    break;
  }

  case iplug::IMidiMsg::kProgramChange:
    data[0] = 0xC0 | chan;
    data[1] = prog;
    length = 2;

    //lsgWrite(data, length);

    //printf("Program change: channel:%d value:%d\n", chan, prog);
    
    break;

  case iplug::IMidiMsg::kChannelAftertouch: {
    uint8_t aft = msg.ChannelAfterTouch() & 0x7F;
    data[0] = 0xD0 | chan;
    data[1] = aft;
    length = 2;

    //lsgWrite(data, length);

    //printf("Channel pressure: channel:%d value:%d\n", chan, aft);
    
    break;
  }

  case iplug::IMidiMsg::kPitchWheel: {
    auto pbVal = msg.PitchWheel();
    int pbValInt = (int)(pbVal * 8192) + 8192;

    if (pbValInt > 16384) {
      pbValInt = 16383;
    }
    else if (pbValInt < 0) {
      pbValInt = 0;
    }

    data[0] = 0xE0 | chan;
    data[1] = (pbValInt) & 0x7f;
    data[2] = (pbValInt >> 7) & 0x7f;
    length = 3;

    //lsgWrite(data, length);

    //printf("Pitch bend: channel:%d value:%d\n", chan, pbVal);
    
    break;
  }

  default:
    //msg.PrintMsg();
    break;
  }

  // FF unused bytes
  for (int i = length; i < 12; ++i) {
    data[i] = 0xFF;
  }

  return data;
}

VLSG_API_(void) ProcessSysExDataVst(iplug::ISysEx& msg)
{
  uint8_t* sysex_value_ptr = (uint8_t*)msg.mData;
  uint32_t count = 0;

  while (count < msg.mSize)
  {
    uint8_t syx_value = *sysex_value_ptr;
    if (count < msg.mSize) {
      ++sysex_value_ptr;
    }
    ++count;
    

    if (syx_value == 0xFF) break; // Stop processing

    if (syx_value > 0xF7) continue; // Drop MIDI data, continue to next.

    if (syx_value == 0xF7)
    {
      if (event_data[0] != 0xF0) continue;
    }
    else if ((syx_value & 0x80) != 0)
    {
      event_length = 0;
      event_type = syx_value & 0xF0;
      event_data[0] = syx_value;
      channel_data_ptr = &(channel_data[syx_value & 0x0F]);
      program_data_ptr = &(program_data[(syx_value & 0x0F) * 2]);

      continue;
    }
    else
    {
      event_length++;
      if (event_length >= 256) continue;

      event_data[event_length] = syx_value;

      if (event_data[0] == 0xF0) continue;

      if ((event_type != 0xC0) && (event_type != 0xD0) && (event_length != 2)) continue;
    }

    switch (event_type)
    {
    case 0xF0: // SysEx
      SystemExclusive();
      break;

    default:
      break;
    }

    event_length = 0;
  }

  system_time_1 = VLSG_GetTime();
}

VLSG_API_(void) ProcessMidiDataVst(iplug::IMidiMsg& msg)
{
  uint8_t *midi_value_ptr = parseMidiMsg(msg);

  while (true)
  {
    uint8_t midi_value = *midi_value_ptr;
    ++midi_value_ptr;

    if (midi_value == 0xFF) break; // Stop processing

    if (midi_value > 0xF7) continue; // Drop MIDI data, continue to next.

    if (midi_value == 0xF7)
    {
      if (event_data[0] != 0xF0) continue;
    }
    else if ((midi_value & 0x80) != 0)
    {
      event_length = 0;
      event_type = midi_value & 0xF0;
      event_data[0] = midi_value;
      channel_data_ptr = &(channel_data[midi_value & 0x0F]);
      program_data_ptr = &(program_data[(midi_value & 0x0F) * 2]);

      continue;
    }
    else
    {
      event_length++;
      if (event_length >= 32) continue;

      event_data[event_length] = midi_value;

      if (event_data[0] == 0xF0) continue;

      if ((event_type != 0xC0) && (event_type != 0xD0) && (event_length != 2)) continue;
    }

    switch (event_type)
    {
    case 0x80: // Note Off
      NoteOff();
      break;

    case 0x90: // Note On
      if (event_data[2] != 0)
      {
        NoteOn(0);

        if (program_data_ptr->field_02 & 0x8000)
        {
          NoteOn(1);
        }
      }
      else
      {
        NoteOff();
      }
      break;

    case 0xB0: // Controller
      ControlChange();
      break;

    case 0xC0: // Program Change
      if ((event_data[0] & 0x0F) == DRUM_CHANNEL)
      {
        int drum_kit_index;

        for (drum_kit_index = 0; drum_kit_index < 8; drum_kit_index++)
        {
          if (drum_kits[drum_kit_index] == event_data[1]) break;
        }
        if (drum_kit_index >= 8) break;

        channel_data_ptr->program_change = drum_kit_numbers[drum_kit_index];
        ProgramChange(program_data_ptr, drum_kit_numbers[drum_kit_index]);
      }
      else
      {
        channel_data_ptr->program_change = event_data[1];
        ProgramChange(program_data_ptr, event_data[1]);
      }
      break;

    case 0xD0: // Channel Pressure
      channel_data_ptr->channel_pressure = event_data[1];
      break;

    case 0xE0: // Pitch Bend
      channel_data_ptr->pitch_bend = event_data[1] + ((event_data[2] - 64) << 7);
      break;

    default:
      break;
    }

    event_length = 0;
  }

  system_time_1 = VLSG_GetTime();
}

static Voice_Data *FindAvailableVoice(int32_t channel_num_2, int32_t note_number)
{
    int index1, index2, index3, index4;

    index1 = recent_voice_index + 1;
    if (index1 >= maximum_polyphony)
    {
        index1 = 0;
    }

    for (index2 = 0; index2 < maximum_polyphony; index2++)
    {
        if (voice_data[index2].note_number == 255)
        {
            recent_voice_index = index2;
            return &(voice_data[index2]);
        }
    }

    index3 = index1;
    do
    {
        if ((voice_data[index3].vflags & VFLAG_Value80) != 0)
        {
            recent_voice_index = index3;
            return &(voice_data[index3]);
        }

        index3++;
        if (index3 >= maximum_polyphony)
        {
            index3 = 0;
        }
    } while (index3 != index1);

    index4 = index1;
    do
    {
        if ((voice_data[index4].channel_num_2 & ~1) == (2 * DRUM_CHANNEL))
        {
            recent_voice_index = index4;
            return &(voice_data[index4]);
        }

        index4++;
        if (index4 >= maximum_polyphony)
        {
            index4 = 0;
        }
    } while (index4 != index1);

    recent_voice_index = index1;
    return &(voice_data[index1]);
}

static Voice_Data *FindVoice(int32_t channel_num_2, int32_t note_number)
{
    int index;

    for (index = 0; index < maximum_polyphony; index++)
    {
        if (voice_data[index].note_number != 255)
        {
            if (voice_data[index].channel_num_2 == channel_num_2)
            {
                if (voice_data[index].note_number == note_number)
                {
                    if ((voice_data[index].vflags & VFLAG_Value80) == 0)
                    {
                        return &(voice_data[index]);
                    }
                }
            }
        }

    }

    return NULL;
}

static void NoteOff(void)
{
    Voice_Data *voice;

    if ((event_data[0] & 0x0F) == DRUM_CHANNEL)
    {
        if (channel_data_ptr->program_change != 7) return; // drum kit 49 (Orchestra Kit ?)
        if (event_data[1] != 88) return; // Applause ?
    }

    voice = FindVoice(2 * (event_data[0] & 0x0F), event_data[1]);
    if (voice != NULL)
    {
        VoiceNoteOff(voice);
    }

    voice = FindVoice(2 * (event_data[0] & 0x0F) + 1, event_data[1]);
    if (voice != NULL)
    {
        VoiceNoteOff(voice);
    }
}

static void NoteOn(int32_t part)
{
    Voice_Data *voice;

    voice = FindAvailableVoice(part + 2 * (event_data[0] & 0x0F), event_data[1]);
    if (voice->note_number != 255)
    {
        VoiceSoundOff(voice);
    }

    voice->channel_num_2 = part + 2 * (event_data[0] & 0x0F);
    voice->note_number = event_data[1];
    voice->note_velocity = event_data[2];
    StartPlayingVoice(voice, channel_data_ptr, &program_data_ptr[part]);
}

static void ControlChange(void)
{
    switch (event_data[1])
    {
        case 0x01: // Modulation
            channel_data_ptr->modulation = event_data[2];
            break;
        case 0x06: // Data Entry (MSB)
            channel_data_ptr->data_entry_MSB = event_data[2];
            if (channel_data_ptr->parameter_number_MSB == 0)
            {
                if (channel_data_ptr->parameter_number_LSB == 0) // Pitch bend range
                {
                    // F YOUR GM2 PITCH LIMITS
                    /*if (channel_data_ptr->data_entry_MSB <= 24)
                    {*/
                        channel_data_ptr->pitch_bend_sense = 2 * ((channel_data_ptr->data_entry_MSB << 7) + channel_data_ptr->data_entry_LSB);
                    //}
                }
                else if (channel_data_ptr->parameter_number_LSB == 1) // Fine tuning
                {
                    channel_data_ptr->fine_tune = ((channel_data_ptr->data_entry_LSB & 0x60) >> 5) + 4 * channel_data_ptr->data_entry_MSB - 256;
                }
                else if (channel_data_ptr->parameter_number_LSB == 2) // Coarse tuning
                {
                    if (channel_data_ptr->data_entry_MSB >= 40 && channel_data_ptr->data_entry_MSB <= 88)
                    {
                        channel_data_ptr->coarse_tune = channel_data_ptr->data_entry_MSB - 64;
                    }
                }
            }
            break;
        case 0x07: // Main Volume
            channel_data_ptr->volume = event_data[2];
            break;
        case 0x0A: // Pan
            channel_data_ptr->pan = (2 * event_data[2]) - 128;
            break;
        case 0x0B: // Expression Controller
            channel_data_ptr->expression = event_data[2];
            break;
        case 0x26: // Data Entry (LSB)
            channel_data_ptr->data_entry_LSB = event_data[2];
            if (channel_data_ptr->parameter_number_MSB == 0)
            {
                if (channel_data_ptr->parameter_number_LSB == 0) // Pitch bend range
                {
                    /*if (channel_data_ptr->data_entry_MSB <= 24)*/
                    {
                        channel_data_ptr->pitch_bend_sense = 2 * ((channel_data_ptr->data_entry_MSB << 7) + channel_data_ptr->data_entry_LSB);
                    }
                }
                else if (channel_data_ptr->parameter_number_LSB == 1) // Fine tuning
                {
                    channel_data_ptr->fine_tune = ((channel_data_ptr->data_entry_LSB & 0x60) >> 5) + 4 * channel_data_ptr->data_entry_MSB - 256;
                }
                else if (channel_data_ptr->parameter_number_LSB == 2) // Coarse tuning
                {
                    if (channel_data_ptr->data_entry_MSB >= 40 && channel_data_ptr->data_entry_MSB <= 88)
                    {
                        channel_data_ptr->coarse_tune = channel_data_ptr->data_entry_MSB - 64;
                    }
                }
            }
            break;
        case 0x40: // Damper pedal (sustain)
            if (event_data[2] <= 63)
            {
                channel_data_ptr->chflags &= ~CHFLAG_Sustain;
                ControllerSettingsOff(event_data[0] & 0x0F);
            }
            else
            {
                channel_data_ptr->chflags |= CHFLAG_Sustain;
                ControllerSettingsOn(event_data[0] & 0x0F);
            }
            break;
        case 0x42: // Sostenuto
            if (event_data[2] <= 63)
            {
                channel_data_ptr->chflags &= ~CHFLAG_Sostenuto;
                ControllerSettingsOff(event_data[0] & 0x0F);
            }
            else
            {
                channel_data_ptr->chflags |= CHFLAG_Sostenuto;
                ControllerSettingsOn(event_data[0] & 0x0F);
            }
          break;
        case 0x43: // Soft Pedal
            if (event_data[2] <= 63)
            {
                channel_data_ptr->chflags &= ~CHFLAG_Soft;
            }
            else
            {
                channel_data_ptr->chflags |= CHFLAG_Soft;
            }
            break;
        case 0x62: // Non-Registered Parameter Number (LSB)
            channel_data_ptr->parameter_number_LSB = event_data[2];
            break;
        case 0x63: // Non-Registered Parameter Number (MSB)
            channel_data_ptr->parameter_number_MSB = event_data[2];
            break;
        case 0x64: // Registered Parameter Number (LSB)
            channel_data_ptr->parameter_number_LSB = event_data[2];
            break;
        case 0x65: // Registered Parameter Number (MSB)
            channel_data_ptr->parameter_number_MSB = event_data[2];
            break;
        case 0x78: // All sounds off
            AllChannelSoundsOff(event_data[0] & 0x0F);
            break;
        case 0x79: // Reset all controllers
            ResetAllControllers(channel_data_ptr);
            ControllerSettingsOff(event_data[0] & 0x0F);
            break;
        case 0x7B: // All notes off
            AllChannelNotesOff(event_data[0] & 0x0F);
            break;
        default:
            break;
    }
}

static void SystemExclusive(void)
{
    int index;

    // GM reset / GS reset
    if ((event_data[0] == 0xF0 && event_data[1] == 0x7E && event_data[2] == 0x7F && event_data[3] == 0x09 && event_data[4] == 0x01) ||
        (event_data[0] == 0xF0 && event_data[1] == 0x41 && event_data[2] == 0x10 && event_data[3] == 0x42 && event_data[4] == 0x12 && event_data[5] == 0x40 && event_data[6] == 0x00 && event_data[7] == 0x7F && event_data[8] == 0x00 && event_data[9] == 0x41)
       )
    {
        AllVoicesSoundsOff();

        for (index = 0; index < MIDI_CHANNELS; index++)
        {
            ResetChannel(&(channel_data[index]));
        }

        for (index = 0; index < MIDI_CHANNELS; index++)
        {
            ProgramChange(&(program_data[index * 2]), 0);
        }

        return;
    }

    // change polyphony
    if (event_data[0] == 0xF0 && event_data[1] == 0x44 && event_data[2] == 0x0E && event_data[3] == 0x03)
    {
        switch (event_data[4])
        {
            case 0x10:
                SetMaximumVoices(24);
                maximum_polyphony_new_value = 24;
                return;

            case 0x11:
                SetMaximumVoices(32);
                maximum_polyphony_new_value = 32;
                return;

            case 0x12:
                SetMaximumVoices(48);
                maximum_polyphony_new_value = 48;
                return;

            case 0x13:
                maximum_polyphony = 64;
                maximum_polyphony_new_value = 64;
                return;

            case 0x14:
                maximum_polyphony = 128;
                maximum_polyphony_new_value = 128;
                return;

            default:
                break;
        }
    }

    // change reverb
    if (event_data[0] == 0xF0 && event_data[1] == 0x44 && event_data[2] == 0x0E && event_data[3] == 0x03)
    {
        switch (event_data[4])
        {
            case 0x20:
                DisableReverb();
                return;

            case 0x21:
                EnableReverb();
                SetReverbShift(1);
                return;

            case 0x22:
                EnableReverb();
                SetReverbShift(0);
                return;

            default:
                break;
        }
    }

    // change velocity curve
    if (event_data[0] == 0xF0 && event_data[1] == 0x44 && event_data[2] == 0x0E && event_data[3] == 0x03)
    {
        switch (event_data[4])
        {
            case 0x40:
                velocity_func = 0;
                return;
            case 0x41:
                velocity_func = 1;
                return;
            case 0x42:
                velocity_func = 2;
                return;
            case 0x43:
                velocity_func = 3;
                return;
            case 0x44:
                velocity_func = 4;
                return;
            case 0x45:
                velocity_func = 5;
                return;
            case 0x46:
                velocity_func = 6;
                return;
            case 0x47:
                velocity_func = 7;
                return;
            case 0x48:
                velocity_func = 8;
                return;
            case 0x49:
                velocity_func = 9;
                return;
            case 0x4A:
                velocity_func = 10;
                return;
            case 0x4B: // Experimental
                velocity_func = 11;
                return;
            default:
                break;
        }
    }
}

static int32_t InitializeReverbBuffer(void)
{
    reverb_data_ptr = reverb_data_buffer;
    reverb_data_index = 0;
    return 0;
}

static int32_t DeinitializeReverbBuffer(void)
{
    reverb_data_ptr = NULL;
    return 0;
}

static void EnableReverb(void)
{
    is_reverb_enabled = 1;
}

static void DisableReverb(void)
{
    is_reverb_enabled = 0;
#ifdef _MSC_VER
    __stosd((unsigned long*)reverb_data_buffer, 0, sizeof(reverb_data_buffer) / 4);
#else
    memset(reverb_data_buffer, 0, sizeof(reverb_data_buffer));
#endif
}

static void SetReverbShift(uint32_t shift)
{
    reverb_shift = shift;
}

static void DefragmentVoices(void)
{
    int index1, index2;

    index2 = 0;
    for (index1 = 0; index1 < maximum_polyphony; index1++)
    {
        if (voice_data[index1].note_number != 255) continue;

        if (index2 < index1)
        {
            index2 = index1;
        }
        while (voice_data[index2].note_number == 255)
        {
            index2++;
            if (index2 >= maximum_polyphony) return;
        }

        voice_data[index1] = voice_data[index2];
        voice_data[index2].note_number = 255;
    }
}

static void GenerateOutputDataVst(double **output_ptr, uint32_t offset1, uint32_t offset2)
{
  int index1, max_active_index;
  unsigned int index2;
  int64_t left;
  int64_t right;
  uint32_t value1;
  uint32_t value2;
  uint32_t value3;
  const uint8_t* rom_ptr;
  int32_t value4;
  int32_t value5;
  int32_t value6;
  int32_t value7;
  int32_t reverb_value1;
  int32_t reverb_value2;
  int32_t reverb_value3;
  int32_t reverb_value4;

  static int callCount = 0;

  if (callCount == 0) {
    DefragmentVoices();
  }
  ++callCount;
  if (callCount >= output_size_para) {
    callCount = 0;
  }

  max_active_index = -1;
  for (index1 = 0; index1 < maximum_polyphony; index1++)
  {
    if (voice_data[index1].note_number != 255)
    {
      max_active_index = index1;
    }
  }

  for (index2 = offset1; index2 < offset2; index2++)
  {
    left = 0;
    right = 0;
    for (index1 = 0; index1 <= max_active_index; index1++)
    {
      value1 = voice_data[index1].wv_end;
      value2 = voice_data[index1].wv_fpos >> 10;
      if (value2 >= value1)
      {
        if (value1 == voice_data[index1].wv_start)
        {
          voice_data[index1].note_number = 255;
          voice_data[index1].field_28 = 0;
          continue;
        }

        value3 = (value2 + (voice_data[index1].wv_start & 1) - value1) & ~1;
        if (value3 >= 10)
        {
          voice_data[index1].wv_fpos += (8 - value3) << 10;
          value3 = 8;
        }

        rom_ptr = &(romsxgm_ptr[voice_data[index1].wv_end]);
        value4 = ((int32_t)(READ_LE_UINT16(&(rom_ptr[value3])) << 17)) >> 17;
        voice_data[index1].wv_un3_hi = (((int32_t)READ_LE_UINT16(&(rom_ptr[10]))) >> (value3 + (value3 >> 1))) & 7;

        voice_data[index1].field_0C[1] = value4;
        voice_data[index1].field_0C[0] = value4 - ((((int32_t)(READ_LE_UINT16(&(romsxgm_ptr[voice_data[index1].wv_start & ~1])) << 16)) >> 25) << voice_data[index1].wv_un3_hi);

        voice_data[index1].wv_fpos += (voice_data[index1].wv_start - voice_data[index1].wv_end) << 10;
        value2 = voice_data[index1].wv_fpos >> 10;
        voice_data[index1].wv_pos = (value2 & ~1) + 2;
        value5 = READ_LE_UINT16(&(romsxgm_ptr[voice_data[index1].wv_pos]));
        voice_data[index1].wv_un3_hi += dword_C00342C0[value5 & 3];
        voice_data[index1].field_0C[2] = voice_data[index1].field_0C[1] + ((((int32_t)(value5 << 23)) >> 25) << voice_data[index1].wv_un3_hi);
        voice_data[index1].field_0C[3] = voice_data[index1].field_0C[2] + ((((int32_t)(value5 << 16)) >> 25) << voice_data[index1].wv_un3_hi);
      }
      else
      {
        while (voice_data[index1].wv_pos <= (value2 & ~1))
        {
          voice_data[index1].wv_pos += 2;
          if (voice_data[index1].wv_end <= voice_data[index1].wv_pos)
          {
            voice_data[index1].field_0C[0] = voice_data[index1].field_0C[2];
            voice_data[index1].field_0C[1] = voice_data[index1].field_0C[3];

            if ((voice_data[index1].wv_start & 1) != 0)
            {
              rom_ptr = &(romsxgm_ptr[voice_data[index1].wv_end]);
              value4 = ((int32_t)(READ_LE_UINT16(rom_ptr) << 17)) >> 17;
              voice_data[index1].wv_un3_hi = rom_ptr[10] & 7;

              voice_data[index1].field_0C[2] = value4;
            }
            else
            {
              rom_ptr = &(romsxgm_ptr[voice_data[index1].wv_end]);
              value4 = ((int32_t)(READ_LE_UINT16(rom_ptr) << 17)) >> 17;
              voice_data[index1].wv_un3_hi = rom_ptr[10] & 7;

              voice_data[index1].field_0C[3] = value4;
              voice_data[index1].field_0C[2] = value4 - ((((int32_t)(READ_LE_UINT16(&(romsxgm_ptr[voice_data[index1].wv_start & ~1])) << 16)) >> 25) << voice_data[index1].wv_un3_hi);
            }
          }
          else
          {
            value5 = READ_LE_UINT16(&(romsxgm_ptr[voice_data[index1].wv_pos]));
            voice_data[index1].field_0C[0] = voice_data[index1].field_0C[2];
            voice_data[index1].field_0C[1] = voice_data[index1].field_0C[3];
            voice_data[index1].wv_un3_hi += dword_C00342C0[value5 & 3];
            voice_data[index1].field_0C[2] = voice_data[index1].field_0C[1] + ((((int32_t)(value5 << 23)) >> 25) << voice_data[index1].wv_un3_hi);
            voice_data[index1].field_0C[3] = voice_data[index1].field_0C[2] + ((((int32_t)(value5 << 16)) >> 25) << voice_data[index1].wv_un3_hi);
          }
        }
      }

      value7 = voice_data[index1].field_0C[value2 & 1];
      value7 += ((int32_t)((voice_data[index1].field_0C[(value2 & 1) + 1] - value7) * (voice_data[index1].wv_fpos & 0x3FF))) >> 10;
      value6 = ((int32_t)(15 * voice_data[index1].field_2C + voice_data[index1].field_38)) >> 4;
      value7 = ((int32_t)(value7 * value6)) >> 12;

      voice_data[index1].field_2C = value6;
      voice_data[index1].wv_fpos += voice_data[index1].v_freq;
      left += value7 >> voice_data[index1].field_30;
      right += value7 >> voice_data[index1].field_34;
    }

    if (is_reverb_enabled == 1)
    {
      reverb_value1 = (left + right) >> 3;

      reverb_value2 = reverb_data_ptr[reverb_data_index & 0x7FFF];
      reverb_data_ptr[(reverb_data_index + 500) & 0x7FFF] = reverb_value1 - (reverb_value2 >> 1);
      reverb_value1 = (reverb_value1 >> 1) + reverb_value2;

      reverb_value2 = reverb_data_ptr[(reverb_data_index + 501) & 0x7FFF];
      reverb_data_ptr[(reverb_data_index + 826) & 0x7FFF] = reverb_value1 - (reverb_value2 >> 1);
      reverb_value1 = (reverb_value1 >> 1) + reverb_value2;

      reverb_value2 = reverb_data_ptr[(reverb_data_index + 827) & 0x7FFF];
      reverb_data_ptr[(reverb_data_index + 1038) & 0x7FFF] = reverb_value1 - (reverb_value2 >> 1);
      reverb_value1 = (reverb_value1 >> 1) + reverb_value2;

      reverb_value2 = reverb_data_ptr[(reverb_data_index + 1039) & 0x7FFF];
      reverb_data_ptr[(reverb_data_index + 1176) & 0x7FFF] = reverb_value1 - (reverb_value2 >> 1);
      reverb_value1 = (reverb_value1 >> 1) + reverb_value2;

      reverb_value3 = reverb_value1 >> 1;

      reverb_value4 = reverb_data_ptr[(reverb_data_index + 1177) & 0x7FFF] - ((96 * reverb_data_ptr[(reverb_data_index + 1179) & 0x7FFF]) >> 8);
      reverb_data_ptr[(reverb_data_index + 1178) & 0x7FFF] = reverb_value4 >> 3;
      reverb_data_ptr[(reverb_data_index + 3177) & 0x7FFF] = reverb_value4 + reverb_value3;

      reverb_value4 = reverb_data_ptr[(reverb_data_index + 3178) & 0x7FFF] - ((97 * reverb_data_ptr[(reverb_data_index + 3180) & 0x7FFF]) >> 8);
      reverb_data_ptr[(reverb_data_index + 3179) & 0x7FFF] = reverb_value4 >> 3;
      reverb_data_ptr[(reverb_data_index + 5118) & 0x7FFF] = reverb_value4 + reverb_value3;

      left += (reverb_data_ptr[(reverb_data_index + 1179) & 0x7FFF] + reverb_data_ptr[(reverb_data_index + 3335) & 0x7FFF]) >> reverb_shift;
      right += (reverb_data_ptr[(reverb_data_index + 1339) & 0x7FFF] + reverb_data_ptr[(reverb_data_index + 3180) & 0x7FFF]) >> reverb_shift;

      reverb_data_index = (reverb_data_index + 1) & 0x7FFF;
    }
    
    // Floating point supports going beyond clipping so F IT.
    //if (left > 32767)
    //{
    //  left = 32767;
    //}
    //else if (left <= -32767)
    //{
    //  left = -32767;
    //}

    //if (right > 32767)
    //{
    //  right = 32767;
    //}
    //else if (right <= -32767)
    //{
    //  right = -32767;
    //}

    (output_ptr)[0][index2] = ((double)left) / 32768.0;
    (output_ptr)[1][index2] = ((double)right) / 32768.0;
  }
}

static void GenerateOutputData(uint8_t *output_ptr, uint32_t offset1, uint32_t offset2)
{
    int index1, max_active_index;
    unsigned int index2;
    int32_t left;
    int32_t right;
    uint32_t value1;
    uint32_t value2;
    uint32_t value3;
    const uint8_t *rom_ptr;
    int32_t value4;
    int32_t value5;
    int32_t value6;
    int32_t value7;
    int32_t reverb_value1;
    int32_t reverb_value2;
    int32_t reverb_value3;
    int32_t reverb_value4;

    DefragmentVoices();

    max_active_index = -1;
    for (index1 = 0; index1 < maximum_polyphony; index1++)
    {
        if (voice_data[index1].note_number != 255)
        {
            max_active_index = index1;
        }
    }

    for (index2 = offset1; index2 < offset2; index2++)
    {
        left = 0;
        right = 0;
        for (index1 = 0; index1 <= max_active_index; index1++)
        {
            value1 = voice_data[index1].wv_end;
            value2 = voice_data[index1].wv_fpos >> 10;
            if (value2 >= value1)
            {
                if (value1 == voice_data[index1].wv_start)
                {
                    voice_data[index1].note_number = 255;
                    voice_data[index1].field_28 = 0;
                    continue;
                }

                value3 = (value2 + (voice_data[index1].wv_start & 1) - value1) & ~1;
                if (value3 >= 10)
                {
                    voice_data[index1].wv_fpos += (8 - value3) << 10;
                    value3 = 8;
                }

                rom_ptr = &(romsxgm_ptr[voice_data[index1].wv_end]);
                value4 = ((int32_t)(READ_LE_UINT16(&(rom_ptr[value3])) << 17)) >> 17;
                voice_data[index1].wv_un3_hi = (((int32_t)READ_LE_UINT16(&(rom_ptr[10]))) >> (value3 + (value3 >> 1))) & 7;

                voice_data[index1].field_0C[1] = value4;
                voice_data[index1].field_0C[0] = value4 - ((((int32_t)(READ_LE_UINT16(&(romsxgm_ptr[voice_data[index1].wv_start & ~1])) << 16)) >> 25) << voice_data[index1].wv_un3_hi);

                voice_data[index1].wv_fpos += (voice_data[index1].wv_start - voice_data[index1].wv_end) << 10;
                value2 = voice_data[index1].wv_fpos >> 10;
                voice_data[index1].wv_pos = (value2 & ~1) + 2;
                value5 = READ_LE_UINT16(&(romsxgm_ptr[voice_data[index1].wv_pos]));
                voice_data[index1].wv_un3_hi += dword_C00342C0[value5 & 3];
                voice_data[index1].field_0C[2] = voice_data[index1].field_0C[1] + ((((int32_t)(value5 << 23)) >> 25) << voice_data[index1].wv_un3_hi);
                voice_data[index1].field_0C[3] = voice_data[index1].field_0C[2] + ((((int32_t)(value5 << 16)) >> 25) << voice_data[index1].wv_un3_hi);
            }
            else
            {
                while (voice_data[index1].wv_pos <= (value2 & ~1))
                {
                    voice_data[index1].wv_pos += 2;
                    if (voice_data[index1].wv_end <= voice_data[index1].wv_pos)
                    {
                        voice_data[index1].field_0C[0] = voice_data[index1].field_0C[2];
                        voice_data[index1].field_0C[1] = voice_data[index1].field_0C[3];

                        if ((voice_data[index1].wv_start & 1) != 0)
                        {
                            rom_ptr = &(romsxgm_ptr[voice_data[index1].wv_end]);
                            value4 = ((int32_t)(READ_LE_UINT16(rom_ptr) << 17)) >> 17;
                            voice_data[index1].wv_un3_hi = rom_ptr[10] & 7;

                            voice_data[index1].field_0C[2] = value4;
                        }
                        else
                        {
                            rom_ptr = &(romsxgm_ptr[voice_data[index1].wv_end]);
                            value4 = ((int32_t)(READ_LE_UINT16(rom_ptr) << 17)) >> 17;
                            voice_data[index1].wv_un3_hi = rom_ptr[10] & 7;

                            voice_data[index1].field_0C[3] = value4;
                            voice_data[index1].field_0C[2] = value4 - ((((int32_t)(READ_LE_UINT16(&(romsxgm_ptr[voice_data[index1].wv_start & ~1])) << 16)) >> 25) << voice_data[index1].wv_un3_hi);
                        }
                    }
                    else
                    {
                        value5 = READ_LE_UINT16(&(romsxgm_ptr[voice_data[index1].wv_pos]));
                        voice_data[index1].field_0C[0] = voice_data[index1].field_0C[2];
                        voice_data[index1].field_0C[1] = voice_data[index1].field_0C[3];
                        voice_data[index1].wv_un3_hi += dword_C00342C0[value5 & 3];
                        voice_data[index1].field_0C[2] = voice_data[index1].field_0C[1] + ((((int32_t)(value5 << 23)) >> 25) << voice_data[index1].wv_un3_hi);
                        voice_data[index1].field_0C[3] = voice_data[index1].field_0C[2] + ((((int32_t)(value5 << 16)) >> 25) << voice_data[index1].wv_un3_hi);
                    }
                }
            }

            value7 = voice_data[index1].field_0C[value2 & 1];
            value7 += ((int32_t)((voice_data[index1].field_0C[(value2 & 1) + 1] - value7) * (voice_data[index1].wv_fpos & 0x3FF))) >> 10;
            value6 = ((int32_t)(15 * voice_data[index1].field_2C + voice_data[index1].field_38)) >> 4;
            value7 = ((int32_t)(value7 * value6)) >> 12;

            voice_data[index1].field_2C = value6;
            voice_data[index1].wv_fpos += voice_data[index1].v_freq;
            left += value7 >> voice_data[index1].field_30;
            right += value7 >> voice_data[index1].field_34;
        }

        if (is_reverb_enabled == 1)
        {
            reverb_value1 = (left + right) >> 3;

            reverb_value2 = reverb_data_ptr[reverb_data_index & 0x7FFF];
            reverb_data_ptr[(reverb_data_index + 500) & 0x7FFF] = reverb_value1 - (reverb_value2 >> 1);
            reverb_value1 = (reverb_value1 >> 1) + reverb_value2;

            reverb_value2 = reverb_data_ptr[(reverb_data_index + 501) & 0x7FFF];
            reverb_data_ptr[(reverb_data_index + 826) & 0x7FFF] = reverb_value1 - (reverb_value2 >> 1);
            reverb_value1 = (reverb_value1 >> 1) + reverb_value2;

            reverb_value2 = reverb_data_ptr[(reverb_data_index + 827) & 0x7FFF];
            reverb_data_ptr[(reverb_data_index + 1038) & 0x7FFF] = reverb_value1 - (reverb_value2 >> 1);
            reverb_value1 = (reverb_value1 >> 1) + reverb_value2;

            reverb_value2 = reverb_data_ptr[(reverb_data_index + 1039) & 0x7FFF];
            reverb_data_ptr[(reverb_data_index + 1176) & 0x7FFF] = reverb_value1 - (reverb_value2 >> 1);
            reverb_value1 = (reverb_value1 >> 1) + reverb_value2;

            reverb_value3 = reverb_value1 >> 1;

            reverb_value4 = reverb_data_ptr[(reverb_data_index + 1177) & 0x7FFF] - ((96 * reverb_data_ptr[(reverb_data_index + 1179) & 0x7FFF]) >> 8);
            reverb_data_ptr[(reverb_data_index + 1178) & 0x7FFF] = reverb_value4 >> 3;
            reverb_data_ptr[(reverb_data_index + 3177) & 0x7FFF] = reverb_value4 + reverb_value3;

            reverb_value4 = reverb_data_ptr[(reverb_data_index + 3178) & 0x7FFF] - ((97 * reverb_data_ptr[(reverb_data_index + 3180) & 0x7FFF]) >> 8);
            reverb_data_ptr[(reverb_data_index + 3179) & 0x7FFF] = reverb_value4 >> 3;
            reverb_data_ptr[(reverb_data_index + 5118) & 0x7FFF] = reverb_value4 + reverb_value3;

            left += (reverb_data_ptr[(reverb_data_index + 1179) & 0x7FFF] + reverb_data_ptr[(reverb_data_index + 3335) & 0x7FFF]) >> reverb_shift;
            right += (reverb_data_ptr[(reverb_data_index + 1339) & 0x7FFF] + reverb_data_ptr[(reverb_data_index + 3180) & 0x7FFF]) >> reverb_shift;

            reverb_data_index = (reverb_data_index + 1) & 0x7FFF;
        }

        if (left > 32767)
        {
            left = 32767;
        }
        else if (left <= -32767)
        {
            left = -32767;
        }

        if (right > 32767)
        {
            right = 32767;
        }
        else if (right <= -32767)
        {
            right = -32767;
        }

        ((int16_t *)output_ptr)[2 * index2] = left;
        ((int16_t *)output_ptr)[2 * index2 + 1] = right;
    }
}

static int32_t InitializeMidiDataBuffer(void)
{
    midi_data_write_index = 0;
    midi_data_read_index = 0;
    return 0;
}

static int32_t EMPTY_DeinitializeMidiDataBuffer(void)
{
    return 0;
}

static void AddByteToMidiDataBuffer(uint8_t value)
{
    uint32_t write_index;

    write_index = midi_data_write_index;
    midi_data_buffer[write_index] = value;
    midi_data_write_index = (write_index + 1) & 0xFFFF;
}

static uint8_t GetValueFromMidiDataBuffer(void)
{
    uint32_t write_index, read_index;
    int index;
    uint32_t event_time;
    uint32_t time_2;
    uint8_t result;

    write_index = midi_data_write_index;
    read_index = midi_data_read_index;
    if (write_index == read_index)
    {
        return 0xFF;
    }

    event_time = 0;
    for (index = 0; index < 4; index++)
    {
        event_time |= midi_data_buffer[read_index] << (8 * index);
        read_index = (read_index + 1) & 0xFFFF;

        if (write_index == read_index)
        {
            midi_data_read_index = read_index;
            return 0xFF;
        }
    }

    time_2 = (system_time_1 >= 600000) ? (system_time_1 - 600000) : 0;

    if ((system_time_1 + 600000 <= event_time) || (time_2 >= event_time))
    {
        AllVoicesSoundsOff();
        midi_data_read_index = 0;
        midi_data_write_index = 0;
        return 0xFF;
    }

    /*if (event_time + 100 > system_time_1)
    {
        return 0xFF;
    }*/

    result = midi_data_buffer[read_index];
    midi_data_read_index = (read_index + 1) & 0xFFFF;
    return result;
}

static int32_t InitializePhase(void)
{
    processing_phase = 0;
    return 0;
}

static int32_t EMPTY_DeinitializePhase(void)
{
    return 0;
}

static void voice_set_panpot(Voice_Data *voice_data_ptr)
{
    voice_data_ptr->field_34 = sub_C0036FB0(voice_data_ptr->v_panpot >> 8);
    voice_data_ptr->field_30 = sub_C0036FB0(voice_data_ptr->v_panpot & 0x1F);
}

static void voice_set_flags(Voice_Data *voice_data_ptr)
{
    uint32_t offset1;

    offset1 = rom_change_bank(10, (voice_data_ptr->index >> 8) + voice_get_index(voice_data_ptr, voice_data_ptr->index & 0xFF));
    offset1 += 4 * (voice_data_ptr->vflags & VFLAG_Mask07);

    if ((voice_data_ptr->vflags & VFLAG_MaskC0) == VFLAG_Value80)
    {
        offset1 += 32;
    }

    voice_data_ptr->field_48 = rom_read_word_at(offset1);
    voice_data_ptr->field_4A = rom_read_word();
    voice_data_ptr->vflags = (voice_data_ptr->vflags & VFLAG_NotMask07) | (voice_data_ptr->field_48 & 7);
}

static void voice_set_flags2(Voice_Data *voice_data_ptr)
{
    uint32_t offset1;
    uint16_t value1;
    int32_t value2;
    int32_t value3;

    offset1 = rom_change_bank(11, (voice_data_ptr->pgm_f14 >> 8) + voice_get_index(voice_data_ptr, voice_data_ptr->pgm_f14 & 0xFF));
    offset1 += (voice_data_ptr->vflags & VFLAG_Mask38) >> 1;

    if ((voice_data_ptr->vflags & VFLAG_MaskC0) == VFLAG_Value80)
    {
        offset1 += 32;
    }

    value1 = rom_read_word_at(offset1);
    value1 = ((voice_data_ptr->v_velocity * (value1 >> 8)) & 0xFF00) | (value1 & 0xFF);
    voice_data_ptr->v_vol = value1;

    if ((((voice_data_ptr->vflags & VFLAG_Mask38) >> 3) == value1) && (voice_data_ptr->field_52 == 0))
    {
        voice_data_ptr->note_number = 0xFF;
        return;
    }

    value2 = rom_read_word() >> 8;
    if ((value2 & 0xE0) == 0x20)
    {
        value3 = (value2 & 0x1F) << 8;
    }
    else
    {
        value3 = value2;
        if ((value2 & 0xE0) != 0)
        {
            value2 = (value2 >> 5) + 6;
            value3 = (value3 & 0x1F) + 32;
        }
        else
        {
            value2 >>= 2;
            value3 &= 3;
        }
        value3 <<= value2;
    }

    if (value3 > 0x7FFF)
    {
        value3 = 0x7FFF;
    }

    voice_data_ptr->vflags = (voice_data_ptr->vflags & VFLAG_NotMask38) | ((voice_data_ptr->v_vol & 7) << 3);
    voice_data_ptr->field_50 = value3;
}

static void voice_set_amp(Voice_Data *voice_data_ptr)
{
    int32_t value0;

    value0 = channel_data[voice_data_ptr->channel_num_2 >> 1].expression * channel_data[voice_data_ptr->channel_num_2 >> 1].volume;
    value0 = ((int32_t)(value0 * value0)) >> 13;
    voice_data_ptr->vol = ((int32_t)(value0 * voice_data_ptr->wv_un3_lo)) >> 7;

    voice_set_panpot(voice_data_ptr);
}

// Note: phase processing has to do with envelope states over time. do not over-process or the
//       states will happen either too quickly or too slowly.
VLSG_API_(void) ProcessPhase(void)
{
    int phase, index, value;
    Channel_Data *channel;

    phase = processing_phase & 7;
    processing_phase++;

    switch ( phase )
    {
        case 0:
            sub_C0037140();

            for (index = 0; index < maximum_polyphony; index++)
            {
                if (voice_data[index].note_number != 255)
                {
                    voice_data[index].field_54 += dword_C0032188[voice_data[index].pgm_f10 + 112];
                }
            }

            break;

        case 1:
            sub_C0037140();
            sub_C0036FE0();
            break;

        case 2:
            sub_C0037140();
            break;

        case 3:
            sub_C0037140();

            for (index = 0; index < maximum_polyphony; index++)
            {
                if (voice_data[index].note_number != 255)
                {
                    channel = &(channel_data[voice_data[index].channel_num_2 >> 1]);
                    value = voice_data[index].pgm_f0E + channel->channel_pressure + channel->modulation;
                    if (value > 127)
                    {
                        value = 127;
                    }
                    else if (value < 0)
                    {
                        value = 0;
                    }

                    voice_set_freq(&(voice_data[index]), (int16_t)(voice_data[index].base_freq + (((int32_t)(value * (voice_data[index].field_54 >> 8))) >> 7) + (voice_data[index].field_4C >> 3)));
                }
            }

            break;

        case 4:
            for (index = 0; index < maximum_polyphony; index++)
            {
                if (voice_data[index].note_number != 255)
                {
                    voice_set_amp(&(voice_data[index]));
                }
            }

            sub_C0037140();
            break;

        case 5:
            sub_C0037140();
            sub_C0036FE0();
            break;

        case 6:
            sub_C0037140();
            break;

        case 7:
            sub_C0037140();

            for (index = 0; index < maximum_polyphony; index++)
            {
                if (voice_data[index].note_number != 255)
                {
                    channel = &(channel_data[voice_data[index].channel_num_2 >> 1]);
                    value = voice_data[index].pgm_f0E + channel->channel_pressure + channel->modulation;
                    if (value > 127)
                    {
                        value = 127;
                    }
                    else if (value < 0)
                    {
                        value = 0;
                    }

                    voice_set_freq(&(voice_data[index]), (int16_t)(voice_data[index].base_freq + (((int32_t)(value * (voice_data[index].field_54 >> 8))) >> 7) + (voice_data[index].field_4C >> 3)));
                }
            }

            break;

        default:
            break;
    }
}

static int32_t sub_C0036FB0(int16_t value3)
{
    int32_t value1, value2;

    value1 = 0;
    value2 = 16;
    do
    {
        if (value2 < value3)
        {
            break;
        }
        value1++;
        value2 >>= 1;
    } while (value2);
    return value1;
}

static void sub_C0036FE0(void) // ADSR envelope-related
{
    int index;
    int32_t value1, value2, value3;

    for (index = 0; index < maximum_polyphony; index++)
    {
        if (voice_data[index].note_number == 255) continue;

        value1 = voice_data[index].field_48;
        value2 = voice_data[index].field_4C;
        if (value1 > value2)
        {
            value3 = value2 + voice_data[index].field_4A;
            if (value3 > 32767)
            {
                value3 = 32767;
            }

            if (value1 > value3)
            {
                voice_data[index].field_4C = value3;
                continue;
            }
        }
        else
        {
            value3 = value2 - voice_data[index].field_4A;
            if (value3 < -32767)
            {
                value3 = -32767;
            }

            if (value1 < value3)
            {
                voice_data[index].field_4C = value3;
                continue;
            }
        }

        voice_data[index].field_4C = value1;

        voice_set_flags(&(voice_data[index]));
    }
}

static void sub_C0037140(void)  // ADSR envelope-related
{
    int index, choice, index2;
    int32_t value1, value2, value3;

    for (index = 0; index < maximum_polyphony; index++)
    {
        if (voice_data[index].note_number == 255) continue;

        value1 = voice_data[index].field_52;
        value2 = voice_data[index].field_50;
        value3 = voice_data[index].v_vol & 0xFF00;

        if (value3 > value1)
        {
            value1 += value2;
            if (value1 > 32767)
            {
                value1 = 32767;
            }

            choice = (value3 <= value1)?1:0;
        }
        else
        {
            value1 -= value2;
            if (value1 < -32767)
            {
                value1 = -32767;
            }

            choice = (value3 >= value1)?1:0;
        }

        if (choice)
        {
            voice_data[index].field_52 = value3;
            index2 = (value3 & 0x7fff) >> 11;
            voice_data[index].field_28 = word_C00342D0[index2] + (((int32_t)((word_C00342D0[index2 + 1] - word_C00342D0[index2]) * (value3 & 0x07ff))) >> 11);
            voice_set_flags2(&(voice_data[index]));
        }
        else
        {
            voice_data[index].field_52 = value1;
            index2 = (value1 & 0x7fff) >> 11;
            voice_data[index].field_28 = word_C00342D0[index2] + (((int32_t)((word_C00342D0[index2 + 1] - word_C00342D0[index2]) * (value1 & 0x07ff))) >> 11);
        }

        voice_data[index].field_38 = ((int32_t)(voice_data[index].field_28 * voice_data[index].vol)) >> 14;
    }
}

static int32_t InitializeStructures(void)
{
    int index;

    for (index = 0; index < MAX_VOICES; index++)
    {
        voice_data[index].note_number = 255;
    }

    for (index = 0; index < MIDI_CHANNELS; index++)
    {
        channel_data[index].program_change = 0;
        channel_data[index].pitch_bend = 0;
        channel_data[index].channel_pressure = 0;
        channel_data[index].modulation = 0;
        channel_data[index].volume = 100;
        channel_data[index].pan = 0;
        channel_data[index].expression = 127;
        channel_data[index].chflags &= ~CHFLAG_Sustain;
        channel_data[index].pitch_bend_sense = 512;
        channel_data[index].fine_tune = 0;
        channel_data[index].coarse_tune = 0;
        channel_data[index].parameter_number_LSB = 255;
        channel_data[index].parameter_number_MSB = 255;
        channel_data[index].data_entry_MSB = 0;
        channel_data[index].data_entry_LSB = 0;
    }

    for (index = 0; index < MIDI_CHANNELS; index++)
    {
        ProgramChange(&(program_data[index * 2]), 0);
    }

    return 0;
}

static int32_t EMPTY_DeinitializeStructures(void)
{
    return 0;
}

static void ResetAllControllers(Channel_Data *channel_data_ptr)
{
    channel_data_ptr->expression = 127;
    channel_data_ptr->pitch_bend = 0;
    channel_data_ptr->channel_pressure = 0;
    channel_data_ptr->parameter_number_LSB = 255;
    channel_data_ptr->modulation = 0;
    channel_data_ptr->parameter_number_MSB = 255;
    channel_data_ptr->data_entry_MSB = 0;
    channel_data_ptr->data_entry_LSB = 0;
    channel_data_ptr->chflags &= ~CHFLAG_Sustain;
}

static void ResetChannel(Channel_Data *channel_data_ptr)
{
    channel_data_ptr->volume = 100;
    channel_data_ptr->program_change = 0;
    channel_data_ptr->expression = 127;
    channel_data_ptr->pitch_bend_sense = 512;
    channel_data_ptr->chflags &= ~CHFLAG_Sustain;
    channel_data_ptr->pitch_bend = 0;
    channel_data_ptr->channel_pressure = 0;
    channel_data_ptr->modulation = 0;
    channel_data_ptr->parameter_number_LSB = 255;
    channel_data_ptr->pan = 0;
    channel_data_ptr->parameter_number_MSB = 255;
    channel_data_ptr->fine_tune = 0;
    channel_data_ptr->data_entry_MSB = 0;
    channel_data_ptr->coarse_tune = 0;
    channel_data_ptr->data_entry_LSB = 0;
}

static uint32_t rom_change_bank(uint32_t bank, int32_t index)
{
    const uint8_t *address1;
    uint32_t offset1;
    int32_t offset2;

    address1 = &(romsxgm_ptr[4 * bank + 65588]);
    offset1 = (READ_LE_UINT16(address1 + 2) << 8) + (READ_LE_UINT16(address1) >> 8);
    offset2 = 4 + index * (int16_t)READ_LE_UINT16(romsxgm_ptr + offset1 + 2);

    rom_offset = offset1 + offset2;
    return rom_offset;
}

static uint16_t rom_read_word(void)
{
    uint16_t result;

    result = READ_LE_UINT16(romsxgm_ptr + rom_offset);
    rom_offset += 2;
    return result;
}

static int16_t rom_read_word_at(uint32_t offset)
{
    rom_offset = offset + 2;
    return (int16_t)READ_LE_UINT16(romsxgm_ptr + offset);
}

#if defined(VLSG_BUILD_DLL)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

BOOL WINAPI DllEntryPoint(HINSTANCE hinst, DWORD reason, void* context)
{
    return TRUE;
}
#endif

