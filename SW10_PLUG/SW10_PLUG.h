#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
#include "VLSG.h"

const int kNumPresets = 1;

int clock_gettime(int, struct timespec* spec)      //C-file part
{
  __int64 wintime; GetSystemTimeAsFileTime((FILETIME*)&wintime);
  wintime -= 116444736000000000i64;  //1jan1601 to 1jan1970
  spec->tv_sec = wintime / 10000000i64;           //seconds
  spec->tv_nsec = wintime % 10000000i64 * 100;      //nano-seconds
  return 0;
}


enum EParams
{
  kParamSampleRate = 0,
  kParamPolyphony,
  kParamReverbMode,
  kParamPitchBendRange,
  kParamVelocityFunction,
  kParamSustain,
  kParamRelease,
  kParamBufferRenderMode,
  kParamLFORateHz,
  kParamLFORateTempo,
  kParamLFORateMode,
  kParamLFODepth,
  kNumParams
};

enum EControlTags
{
  kCtrlTagMeter = 0,
  kCtrlTagLFOVis,
  kCtrlTagScope,
  kCtrlTagRTText,
  kCtrlTagKeyboard,
  kCtrlTagBender,
  kNumCtrlTags
};

using namespace iplug;
using namespace igraphics;

class SW10_PLUG final : public Plugin
{
public:
  SW10_PLUG(const InstanceInfo& info);

public:
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void ProcessSysEx(const ISysEx& msg) override;
  void OnReset() override;
  void OnParamChange(int paramIdx) override;
  void OnParamChangeUI(int paramIdx, EParamSource source) override;
  void OnIdle() override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

private:
  std::unique_ptr<VLSG> vlsgInstance;
  IPeakAvgSender<2> mMeterSender;
  IMidiQueue mMidiQueue;
  IMidiQueueBase<ISysEx> mSysExQueue;
  std::unique_ptr<uint8_t[]> wav_buffer; // NOTE: SAMPLES ARE int16_t stereo interleaved!
  int bufferMode;
  int frequency = 2;
  int polyphony = 4;
  int reverb_effect = 0;

  uint8_t* load_rom_file(const char* romname);
  void lsgWrite(uint8_t* event, unsigned int length, int offset = 0);
  int start_synth(void);
  void stop_synth(void);
  char* handleDllPath(const char* romname);
};
