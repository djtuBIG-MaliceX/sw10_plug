#include "SW10_PLUG.h"
#include "IPlug_include_in_plug_src.h"
#include "LFO.h"
#include "timeapi.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "VLSG.h"
#ifdef __cplusplus
}
#endif

static const char* arg_rom = "ROMSXGM.BIN";

static uint32_t outbuf_counter;

static uint8_t* rom_address;
//static uint8_t wav_buffer[65536];  // NOTE: SAMPLES ARE int16_t stereo interleaved!

static unsigned int timediv;

static int frequency = 2, polyphony = 3, reverb_effect = 1;


static struct timespec start_time;

static inline void WRITE_LE_UINT16(uint8_t* ptr, uint16_t value)
{
  ptr[0] = value & 0xff;
  ptr[1] = (value >> 8) & 0xff;
}

static inline void WRITE_LE_UINT32(uint8_t* ptr, uint32_t value)
{
  ptr[0] = value & 0xff;
  ptr[1] = (value >> 8) & 0xff;
  ptr[2] = (value >> 16) & 0xff;
  ptr[3] = (value >> 24) & 0xff;
}

// Needed if using DLL (not linking to winmm)
static uint32_t VLSG_GetTime(void)
{
  struct timespec _tp;

  clock_gettime(0, &_tp);

  return ((_tp.tv_sec - start_time.tv_sec) * 1000) + ((_tp.tv_nsec - start_time.tv_nsec) / 1000000);
}

static uint32_t VLSG_CALLTYPE lsgGetTime()
{
//#ifdef __WINDOWS_MM__
//  return ::timeGetTime();
//#else
  return VLSG_GetTime();
//#endif
}

//FWdecl
static uint8_t* load_rom_file(const char* romname);

static void handleDllPath() {
  char path[MAX_PATH];
  HMODULE hm = NULL;

  if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    (LPCSTR)&load_rom_file, &hm) == 0)
  {
    int ret = GetLastError();
    fprintf(stderr, "GetModuleHandle failed, error = %d\n", ret);
    // Return or however you want to handle an error.
  }
  if (GetModuleFileName(hm, path, sizeof(path)) == 0)
  {
    int ret = GetLastError();
    fprintf(stderr, "GetModuleFileName failed, error = %d\n", ret);
    // Return or however you want to handle an error.
  }

  // The path variable should now contain the full filepath for this DLL.
}

// TODO move to C++ private function
static uint8_t* load_rom_file(const char* romname)
{
  FILE* f;
  uint8_t* mem;

  f = fopen(romname, "rb");
  if (f == NULL)
  {
    return NULL;
  }

  mem = (uint8_t*)malloc(2 * 1024 * 1024);
  if (mem == NULL)
  {
    fclose(f);
    return NULL;
  }

  if (fread(mem, 1, 2 * 1024 * 1024, f) != 2 * 1024 * 1024)
  {
    free(mem);
    fclose(f);
    return NULL;
  }

  return mem;
}

static void lsgWrite(uint8_t* event, unsigned int length)
{
  const DWORD time = lsgGetTime();
  const BYTE* p = reinterpret_cast<const BYTE*>(event);
  for (; length > 0; length--, p++) {
    VLSG_Write(&time, 4);
    VLSG_Write(p, 1);
  }
}

static int start_synth(void)
{
  rom_address = load_rom_file(arg_rom);
  if (rom_address == NULL) {
    fprintf(stderr, "Error opening ROM file: %s\n", arg_rom);
    return -1;
  }

  // set function GetTime
  VLSG_SetFunc_GetTime(lsgGetTime);

  // set frequency
  VLSG_SetParameter(PARAMETER_Frequency, frequency);

  // set polyphony
  VLSG_SetParameter(PARAMETER_Polyphony, 0x10 + polyphony);

  // set reverb effect
  VLSG_SetParameter(PARAMETER_Effect, 0x20 + reverb_effect);

  // set address of ROM file
  VLSG_SetParameter(PARAMETER_ROMAddress, (uintptr_t)rom_address);

  // set output buffer - not used as being used from IPlug2 directly
  /*outbuf_counter = 0;
  memset(wav_buffer, 0, 65536);
  VLSG_SetParameter(PARAMETER_OutputBuffer, (uintptr_t)wav_buffer);*/

  // start playback
  VLSG_PlaybackStart();

  return 0;
}

static void stop_synth(void)
{
  VLSG_PlaybackStop();
  //munmap(rom_address, ROMSIZE); // TODO unload
}



SW10_PLUG::SW10_PLUG(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  start_synth();

  // TODO remap params
  GetParam(kParamGain)->InitDouble("Gain", 100., 0., 100.0, 0.01, "%");
  GetParam(kParamNoteGlideTime)->InitMilliseconds("Note Glide Time", 0., 0.0, 30.);
  GetParam(kParamAttack)->InitDouble("Attack", 10., 1., 1000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamDecay)->InitDouble("Decay", 10., 1., 1000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamSustain)->InitDouble("Sustain", 50., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
  GetParam(kParamRelease)->InitDouble("Release", 10., 2., 1000., 0.1, "ms", IParam::kFlagsNone, "ADSR");
  GetParam(kParamLFOShape)->InitEnum("LFO Shape", LFO<>::kTriangle, {LFO_SHAPE_VALIST});
  GetParam(kParamLFORateHz)->InitFrequency("LFO Rate", 1., 0.01, 40.);
  GetParam(kParamLFORateTempo)->InitEnum("LFO Rate", LFO<>::k1, {LFO_TEMPODIV_VALIST});
  GetParam(kParamLFORateMode)->InitBool("LFO Sync", true);
  GetParam(kParamLFODepth)->InitPercentage("LFO Depth");
    
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };
  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_GRAY);
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableMultiTouch(true);
    
#ifdef OS_WEB
    pGraphics->AttachPopupMenuControl();
#endif

//    pGraphics->EnableLiveEdit(true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    const IRECT b = pGraphics->GetBounds().GetPadded(-20.f);
    const IRECT lfoPanel = b.GetFromLeft(300.f).GetFromTop(200.f);
    IRECT keyboardBounds = b.GetFromBottom(300);
    IRECT wheelsBounds = keyboardBounds.ReduceFromLeft(100.f).GetPadded(-10.f);
    pGraphics->AttachControl(new IVKeyboardControl(keyboardBounds), kCtrlTagKeyboard);
    pGraphics->AttachControl(new IWheelControl(wheelsBounds.FracRectHorizontal(0.5)), kCtrlTagBender);
    pGraphics->AttachControl(new IWheelControl(wheelsBounds.FracRectHorizontal(0.5, true), IMidiMsg::EControlChangeMsg::kModWheel));
//    pGraphics->AttachControl(new IVMultiSliderControl<4>(b.GetGridCell(0, 2, 2).GetPadded(-30), "", DEFAULT_STYLE, kParamAttack, EDirection::Vertical, 0.f, 1.f));
    const IRECT controls = b.GetGridCell(1, 2, 2);
    pGraphics->AttachControl(new IVKnobControl(controls.GetGridCell(0, 2, 6).GetCentredInside(90), kParamGain, "Gain"));
    pGraphics->AttachControl(new IVKnobControl(controls.GetGridCell(1, 2, 6).GetCentredInside(90), kParamNoteGlideTime, "Glide"));
    const IRECT sliders = controls.GetGridCell(2, 2, 6).Union(controls.GetGridCell(3, 2, 6)).Union(controls.GetGridCell(4, 2, 6));
    pGraphics->AttachControl(new IVSliderControl(sliders.GetGridCell(0, 1, 4).GetMidHPadded(30.), kParamAttack, "Attack"));
    pGraphics->AttachControl(new IVSliderControl(sliders.GetGridCell(1, 1, 4).GetMidHPadded(30.), kParamDecay, "Decay"));
    pGraphics->AttachControl(new IVSliderControl(sliders.GetGridCell(2, 1, 4).GetMidHPadded(30.), kParamSustain, "Sustain"));
    pGraphics->AttachControl(new IVSliderControl(sliders.GetGridCell(3, 1, 4).GetMidHPadded(30.), kParamRelease, "Release"));
    pGraphics->AttachControl(new IVLEDMeterControl<2>(controls.GetFromRight(100).GetPadded(-30)), kCtrlTagMeter);
    
    pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 0, 2, 3).GetCentredInside(60), kParamLFORateHz, "Rate"), kNoTag, "LFO")->Hide(true);
    pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 0, 2, 3).GetCentredInside(60), kParamLFORateTempo, "Rate"), kNoTag, "LFO")->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 1, 2, 3).GetCentredInside(60), kParamLFODepth, "Depth"), kNoTag, "LFO");
    pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 2, 2, 3).GetCentredInside(60), kParamLFOShape, "Shape"), kNoTag, "LFO")->DisablePrompt(false);
    pGraphics->AttachControl(new IVSlideSwitchControl(lfoPanel.GetGridCell(1, 0, 2, 3).GetFromTop(30).GetMidHPadded(20), kParamLFORateMode, "Sync", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false).WithWidgetFrac(0.5f).WithDrawShadows(false), false), kNoTag, "LFO");
    pGraphics->AttachControl(new IVDisplayControl(lfoPanel.GetGridCell(1, 1, 2, 3).Union(lfoPanel.GetGridCell(1, 2, 2, 3)), "", DEFAULT_STYLE, EDirection::Horizontal, 0.f, 1.f, 0.f, 1024), kCtrlTagLFOVis, "LFO");
    
    pGraphics->AttachControl(new IVGroupControl("LFO", "LFO", 10.f, 20.f, 10.f, 10.f));
    
    pGraphics->AttachControl(new IVButtonControl(keyboardBounds.GetFromTRHC(200, 30).GetTranslated(0, -30), SplashClickActionFunc,
      "Show/Hide Keyboard", DEFAULT_STYLE.WithColor(kFG, COLOR_WHITE).WithLabelText({15.f, EVAlign::Middle})))->SetAnimationEndActionFunction(
      [pGraphics](IControl* pCaller) {
        static bool hide = false;
        pGraphics->GetControlWithTag(kCtrlTagKeyboard)->Hide(hide = !hide);
        pGraphics->Resize(PLUG_WIDTH, hide ? PLUG_HEIGHT / 2 : PLUG_HEIGHT, pGraphics->GetDrawScale());
    });
//#ifdef OS_IOS
//    if(!IsOOPAuv3AppExtension())
//    {
//      pGraphics->AttachControl(new IVButtonControl(b.GetFromTRHC(100, 100), [pGraphics](IControl* pCaller) {
//                               dynamic_cast<IGraphicsIOS*>(pGraphics)->LaunchBluetoothMidiDialog(pCaller->GetRECT().L, pCaller->GetRECT().MH());
//                               SplashClickActionFunc(pCaller);
//                             }, "BTMIDI"));
//    }
//#endif
    
    pGraphics->SetQwertyMidiKeyHandlerFunc([pGraphics](const IMidiMsg& msg) {
                                              pGraphics->GetControlWithTag(kCtrlTagKeyboard)->As<IVKeyboardControl>()->SetNoteFromMidi(msg.NoteNumber(), msg.StatusMsg() == IMidiMsg::kNoteOn);
                                           });
  };
#endif
}

#if IPLUG_DSP
void SW10_PLUG::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  //mDSP.ProcessBlock(nullptr, outputs, 2, nFrames, mTimeInfo.mPPQPos, mTimeInfo.mTransportIsRunning);

  VLSG_BufferVst(outbuf_counter, outputs, nFrames);
  outbuf_counter += nFrames;

  // CBF aligning the pcm writes to the on-demand frame shifts.
  /*for (int i = 0, j = 0; j < nFrames; ) {
    outputs[0][j] = (((int16_t*)wav_buffer)[(outbuf_counter + i++) & 32767]) / 32768.0;
    outputs[1][j++] = (((int16_t*)wav_buffer)[(outbuf_counter + i++) & 32767]) / 32768.0;
    ++outbuf_counter;
  }*/
  
  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
}

void SW10_PLUG::OnIdle()
{
  mMeterSender.TransmitData(*this);
  mLFOVisSender.TransmitData(*this);
}

void SW10_PLUG::OnReset()
{
  mDSP.Reset(GetSampleRate(), GetBlockSize());
  mMeterSender.Reset(GetSampleRate());
}

void SW10_PLUG::ProcessMidiMsg(const IMidiMsg& msg)
{
  TRACE;
  
  IMidiMsg::EStatusMsg status = msg.StatusMsg();

  // TODO port to msg.mData1/2 calls
  uint8_t data[12];
  int length;
  auto sampOffset = msg.mOffset; // TODO how to use this?
  uint8_t chan = msg.Channel() & 0xF;
  uint8_t key = msg.NoteNumber() & 0x7F;
  uint8_t vel = msg.Velocity() & 0x7F;
  uint8_t prog = msg.Program() & 0x7F;

  switch (status)
  {
  case IMidiMsg::kNoteOn:
    data[0] = 0x90 | chan;
    data[1] = key;
    data[2] = vel;
    length = 3;

    lsgWrite(data, length);

    printf("Note ON, channel:%d note:%d velocity:%d\n", chan, key, vel);
    SendMidiMsg(msg);
    break;

  case IMidiMsg::kNoteOff:
    data[0] = 0x80 | chan;
    data[1] = key;
    data[2] = vel;
    length = 3;

    lsgWrite(data, length);

    printf("Note OFF, channel:%d note:%d velocity:%d\n", chan, key, vel);
    SendMidiMsg(msg);
    break;

  //case IMidiMsg::kPolyAftertouch:
  //  // Not used by CASIO SW-10
  //  data[0] = 0xA0 | chan;
  //  data[1] = key;
  //  data[2] = vel; /// check msg.PolyAftertouch()
  //  length = 3;

  //  lsgWrite(data, length);

  //  printf("Keypress, channel:%d note:%d velocity:%d\n", chan, key, vel);
  //  SendMidiMsg(msg);
  //  break;

  case IMidiMsg::kControlChange: {
    uint8_t cc = msg.ControlChangeIdx();
    uint8_t ccVal = (uint8_t)(msg.ControlChange(msg.ControlChangeIdx()) * 127) & 0x7F; // assuming 7-bit
    data[0] = 0xB0 | chan;
    data[1] = cc & 0x7f;
    data[2] = ccVal & 0x7f;

    length = 3;

    lsgWrite(data, length);

    printf("Controller, channel:%d param:%d value:%d\n", chan, key, vel);
    SendMidiMsg(msg);
    break;
  }


    /** TODO: does IPlug support 14-bit CCs?
    if (event->data.control.param >= 0 && event->data.control.param < 32)
    {
      data[0] = 0xB0 | event->data.control.channel;
      data[1] = event->data.control.param;
      data[2] = (event->data.control.value >> 7) & 0x7f;
      data[3] = event->data.control.param + 32;
      data[4] = event->data.control.value & 0x7f;
      length = 5;

      lsgWrite(data, length);
    }

    printf("Controller 14-bit, channel:%d param:%d value:%d\n", chan, key, vel);
    SendMidiMsg(msg);
    break;
    */

  case IMidiMsg::kProgramChange:
    data[0] = 0xC0 | chan;
    data[1] = prog;
    length = 2;

    lsgWrite(data, length);

    printf("Program change: channel:%d value:%d\n", chan, prog);
    SendMidiMsg(msg);
    break;

  case IMidiMsg::kChannelAftertouch: {
    uint8_t aft = msg.ChannelAfterTouch() & 0x7F;
    data[0] = 0xD0 | chan;
    data[1] = aft;
    length = 2;

    lsgWrite(data, length);

    printf("Channel pressure: channel:%d value:%d\n", chan, aft);
    SendMidiMsg(msg);
    break;
  }
  case IMidiMsg::kPitchWheel: {
    auto pbVal = msg.PitchWheel();
    int pbValInt = (int)(pbVal * 8192) + 8192;

    if (pbValInt > 16384) {
      pbValInt = 16383;
    }
    else if (pbValInt < 0) {
      pbValInt = -8192;
    }

    data[0] = 0xE0 | chan;
    data[1] = (pbValInt) & 0x7f;
    data[2] = (pbValInt >> 7) & 0x7f;
    length = 3;

    lsgWrite(data, length);

    printf("Pitch bend: channel:%d value:%d\n", chan, pbVal);
    SendMidiMsg(msg);
    break;

    // TODO IPlug mapping of NRPN/RPN handling
  }
  //case IMidiMsg::SND_SEQ_EVENT_NONREGPARAM:
  //  // Not used by CASIO SW-10
  //  data[0] = 0xB0 | event->data.control.channel;
  //  data[1] = 0x63; // NRPN MSB
  //  data[2] = (event->data.control.param >> 7) & 0x7f;
  //  data[3] = 0x62; // NRPN LSB
  //  data[4] = event->data.control.param & 0x7f;
  //  data[5] = 0x06; // data entry MSB
  //  data[6] = (event->data.control.value >> 7) & 0x7f;
  //  data[7] = 0x26; // data entry LSB
  //  data[8] = event->data.control.value & 0x7f;
  //  length = 9;

  //  lsgWrite(data, length);

  //  printf("NRPN, channel:%d param:%d value:%d\n", event->data.control.channel, event->data.control.param, event->data.control.value);
  //  SendMidiMsg(msg);
  //  break;

  //case SND_SEQ_EVENT_REGPARAM:
  //  data[0] = 0xB0 | event->data.control.channel;
  //  data[1] = 0x65; // RPN MSB
  //  data[2] = (event->data.control.param >> 7) & 0x7f;
  //  data[3] = 0x64; // RPN LSB
  //  data[4] = event->data.control.param & 0x7f;
  //  data[5] = 0x06; // data entry MSB
  //  data[6] = (event->data.control.value >> 7) & 0x7f;
  //  data[7] = 0x26; // data entry LSB
  //  data[8] = event->data.control.value & 0x7f;
  //  length = 9;

  //  lsgWrite(data, length);

  //  printf("RPN, channel:%d param:%d value:%d\n", event->data.control.channel, event->data.control.param, event->data.control.value);
  //  SendMidiMsg(msg);
  //  break;

    // TODO - check IPlugMidi::ISysEx struct and where it's handled
  /*case IMidiMsg::kSysex:
    length = event->data.ext.len;

    lsgWrite(event->data.ext.ptr, length);

    printf("SysEx (fragment) of size %d\n", event->data.ext.len);

    break;*/

  default:
    msg.PrintMsg();
    break;
  }
  
//handle:
  //mDSP.ProcessMidiMsg(msg);
  //SendMidiMsg(msg);
}

void SW10_PLUG::OnParamChange(int paramIdx)
{
  mDSP.SetParam(paramIdx, GetParam(paramIdx)->Value());
}

void SW10_PLUG::OnParamChangeUI(int paramIdx, EParamSource source)
{
  if (auto pGraphics = GetUI())
  {
    if (paramIdx == kParamLFORateMode)
    {
      const auto sync = GetParam(kParamLFORateMode)->Bool();
      pGraphics->HideControl(kParamLFORateHz, sync);
      pGraphics->HideControl(kParamLFORateTempo, !sync);
    }
  }
}

bool SW10_PLUG::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  if(ctrlTag == kCtrlTagBender && msgTag == IWheelControl::kMessageTagSetPitchBendRange)
  {
    const int bendRange = *static_cast<const int*>(pData);
    //mDSP.mSynth.SetPitchBendRange(bendRange);
    
  }
  
  return false;
}
#endif
