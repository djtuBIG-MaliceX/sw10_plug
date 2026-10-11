#pragma once

#include <atomic>

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
#include "VLSG.h"

const int kNumPresets = 1;

#ifdef _MSC_VER
// MSVC-only shim: pre-C++11 / older MSVC lacked clock_gettime. MinGW provides a real
// POSIX clock_gettime (<ctime>, force-included by the MinGW portability prelude), so
// defining it here would collide and the i64/__int64 literals below are MSVC-only.
int clock_gettime(int, struct timespec* spec)      //C-file part
{
  __int64 wintime; GetSystemTimeAsFileTime((FILETIME*)&wintime);
  wintime -= 116444736000000000i64;  //1jan1601 to 1jan1970
  spec->tv_sec = wintime / 10000000i64;           //seconds
  spec->tv_nsec = wintime % 10000000i64 * 100;      //nano-seconds
  return 0;
}
#endif


enum EParams
{
  kParamPolyphony = 0,
  kParamReverbMode,
  kParamPitchBendRange,
  kParamVelocityFunction,
  kParamBufferRenderMode,
  kNumParams
};

enum EControlTags
{
  kCtrlTagMeter = 0,
  kCtrlTagKeyboard,
  kCtrlTagBender,
  kCtrlTagModWheel,
  kCtrlTagChannelButtons, // valIdx = channel
  kCtrlTagProgramCaption, // shows GM program name of selected channel
  kCtrlTagVolumeSlider,
  kCtrlTagPanSlider,
  kCtrlTagExpressionSlider,
  kCtrlTagModulationSlider,
  kCtrlTagChannelLabel,
  kNumCtrlTags
};

// Arbitrary message tags (delegate -> UI; handled by MenuTextControl::OnMsgFromDelegate)
enum ECustomMsgTags
{
  kMsgTagSelectChannel = 0x53434853, // 'SCHS' (reserved for future UI-originated channel select)
  kMsgTagProgramName,                // NUL-terminated display string pushed to a text control
  kMsgTagChannelActivity,            // uint32_t bitmask of MIDI channels with new traffic (tab flash)
};

using namespace iplug;
using namespace igraphics;

// GM1 melodic program names (program change 0-127 on channels 1-9, 11-16)
extern const char* kGMProgramNames[128];
// Drum kit names for the drum channel (maps to ROM programs 128-135)
extern const char* kDrumKitNames[8];

/** IWheelControl always transmits on channel 1; this subclass transmits on the currently
    selected MIDI channel (pitch bend or a linked CC such as the mod wheel). */
class ChannelWheelControl : public IWheelControl
{
public:
  ChannelWheelControl(const IRECT& bounds, IMidiMsg::EControlChangeMsg cc, const int* pSelectedChannel, int initBendRange = 12)
  : IWheelControl(bounds, cc, initBendRange)
  , mpSelectedChannel(pSelectedChannel)
  {
    // Replace the base action function with a channel-aware one
    SetActionFunction([this, cc](IControl* pControl) {
      IMidiMsg msg;
      if (cc == IMidiMsg::EControlChangeMsg::kNoCC) // pitchbend
        msg.MakePitchWheelMsg((pControl->GetValue() * 2.) - 1., *mpSelectedChannel);
      else
        msg.MakeControlChangeMsg(cc, pControl->GetValue(), *mpSelectedChannel);

      pControl->GetDelegate()->SendMidiMsgFromUI(msg);
    });
  }

private:
  const int* mpSelectedChannel;
};

/** Per-channel mirror of the synth state, kept in sync from ProcessMidiMsg/SysEx so the
    UI can show (and initialise its widgets from) any of the 16 MIDI channels. */
struct ChannelUiState
{
  int program = 0;      // GM program 0-127 (drum kit 0-7 on the drum channel)
  int volume = 100;     // CC7, 0-127
  int pan = 64;         // CC10, 0-127 (64 = centre)
  int expression = 127; // CC11, 0-127
  int modulation = 0;   // CC1, 0-127
  int bendRange = 2;    // pitch-bend semitones (RPN 0/0 + Data MSB), GM default 2
};

/** Latched RPN selector (CC101/CC100) per MIDI channel; audio-thread only. -1 = none set. */
struct RpnState
{
  int msb = -1;
  int lsb = -1;
};

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
  void OnUIClose() override;

#if IPLUG_EDITOR
  // IEditorDelegate: retargets every UI-originated channel-voice message (on-screen
  // keyboard, qwerty input, wheels, faders, program popup) to the selected channel.
  void SendMidiMsgFromUI(const IMidiMsg& msg) override;

  /** Drains the audio->UI queues. Called from OnIdle AND pumped by an always-live
      control animation, so metering/state keeps updating even in hosts whose idle
      callbacks never fire (iPlug2's OnIdle rides a thread-message timer bound to the
      constructing thread, which some hosts do not pump). */
  void UITick();
#endif

private:
  std::unique_ptr<VLSG> vlsgInstance;
  IPeakAvgSender<2> mMeterSender;
  IMidiQueue mMidiQueue;
  IMidiQueueBase<ISysEx> mSysExQueue;
  std::unique_ptr<uint8_t[]> wav_buffer; // NOTE: SAMPLES ARE int16_t stereo interleaved!
  int bufferMode;
  int polyphony = 5;
  int reverb_effect = 0;

  // UI channel management
  ChannelUiState mChannelUi[MIDI_CHANNELS];
  RpnState mRpn[MIDI_CHANNELS];          // audio-thread RPN latching (CC101/CC100/CC6)
  std::atomic<uint32_t> mChActivityMask{ 0 }; // channels with new MIDI traffic (bit = ch), consumed in UITick
  int mSelectedChannel = 0;
  int mUiStateDirty = 0;      // selected-channel state changed, push to UI on next idle
  int mKeyboardOctave = 0;    // keyboard octave shift, in octaves from the base range
  int mSampleRateHint = 0;    // last rate (Hz) applied to VLSG / shown in the UI
  int mShownPoly = -1;        // last polyphony rendered into the UI label (debounce)
  int mShownRate = -1;        // last sample-rate rendered into the UI label (debounce)
  size_t wavBufferCapacityBytes = 0; // actual allocation size of wav_buffer

  // Render bookkeeping (mode-2 legacy ring consumer; was function-local statics before)
  int mRenderedFramesQueued = 0;
  uint32_t mRenderReadPos = 0;   // int16-element cursor into the wav_buffer ring
  uint32_t mRingSamples = 0;     // cached VLSG::GetOutputBufferSamples() at last reset

  std::atomic<int> mPolyCount{ 0 };   // written on the audio thread, read in OnIdle
  ITextControl* polyIndicator = nullptr;
  ITextControl* rateIndicator = nullptr;

  uint8_t* load_rom_file(const char* romname);
  void lsgWrite(uint8_t* event, unsigned int length, int offset = 0);
  int start_synth(void);
  void stop_synth(void);
  char* handleDllPath(const char* romname);

  void UpdateChannelUiState(const IMidiMsg& msg);
  void ResetChannelUiState();
  void PushChannelStateToUI();
  static const char* GetProgramNameStr(int channel, int program);
  void SendPitchBendRPN(int channel, int semitones);
};