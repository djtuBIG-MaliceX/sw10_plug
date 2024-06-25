#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"

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
  kParamGain = 0,
  kParamNoteGlideTime,
  kParamAttack,
  kParamDecay,
  kParamSustain,
  kParamRelease,
  kParamLFOShape,
  kParamLFORateHz,
  kParamLFORateTempo,
  kParamLFORateMode,
  kParamLFODepth,
  kNumParams
};

#if IPLUG_DSP
// will use EParams in SW10_PLUG_DSP.h
#include "SW10_PLUG_DSP.h"
#endif

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

#if IPLUG_DSP // http://bit.ly/2S64BDd
public:
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
  void OnParamChange(int paramIdx) override;
  void OnParamChangeUI(int paramIdx, EParamSource source) override;
  void OnIdle() override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

private:
  SW10_PLUGDSP<sample> mDSP {16};
  IPeakAvgSender<2> mMeterSender;
  ISender<1> mLFOVisSender;
#endif
};
