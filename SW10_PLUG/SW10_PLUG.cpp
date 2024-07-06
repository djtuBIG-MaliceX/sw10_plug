#include "SW10_PLUG.h"
#include "IPlug_include_in_plug_src.h"
#include <sstream>

static struct timespec start_time;

static const char* arg_rom = "ROMSXGM.BIN";
static uint32_t outbuf_counter;
static uint8_t* rom_address;


static uint32_t lsgGetTime()
{
//#ifdef __WINDOWS_MM__
//  // Requires #include timeapi.h but doesn't work for WinXP
//  return ::timeGetTime();
//#else
  struct timespec _tp;

  clock_gettime(0, &_tp);

  return ((_tp.tv_sec - start_time.tv_sec) * 1000) + ((_tp.tv_nsec - start_time.tv_nsec) / 1000000);
//#endif
}

SW10_PLUG::SW10_PLUG(const InstanceInfo& info)
  : iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets)),
  vlsgInstance(std::make_unique<VLSG>()),
  bufferMode(1),
  wav_buffer(std::make_unique<uint8_t[]>(262144)) // 256KB buffer, ok for 88200Hz?
{
  start_synth();

  // TODO remap params
  GetParam(kParamSampleRate)->InitEnum("SampleRate", frequency, { "11025", "22050", "44100", "16538", "48000" });
  GetParam(kParamPolyphony)->InitEnum("Polyphony", polyphony, {"24", "32", "48", "64", "128", "256"});
  GetParam(kParamReverbMode)->InitEnum("Reverb Mode", reverb_effect, { "Off", "Reverb 1", "Reverb 2" });
  GetParam(kParamPitchBendRange)->InitInt("P.Bend Rng", 2, 0, 127, "semitones", IParam::kFlagsNone, "ADSR");
  GetParam(kParamVelocityFunction)->InitInt("Velocity Curve", 6, 0, 11, "", IParam::kFlagsNone, "ADSR");

  //GetParam(kParamSustain)->InitDouble("Sustain", 50., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
  //GetParam(kParamRelease)->InitDouble("Release", 10., 2., 1000., 0.1, "ms", IParam::kFlagsNone, "ADSR");
  //GetParam(kParamBufferRenderMode)->InitEnum("Render Mode", 1, {"Off", "Low Latency", "Original Driver"});
  //GetParam(kParamLFORateHz)->InitFrequency("LFO Rate", 1., 0.01, 40.);
  //GetParam(kParamLFORateTempo)->InitEnum("LFO Rate", LFO<>::k1, {LFO_TEMPODIV_VALIST});
  //GetParam(kParamLFORateMode)->InitBool("LFO Sync", true);
  //GetParam(kParamLFODepth)->InitPercentage("LFO Depth");
    
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };
  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_WHITE);
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableMultiTouch(true);
    
#ifdef OS_WEB
    pGraphics->AttachPopupMenuControl();
#endif

//    pGraphics->EnableLiveEdit(true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    const IRECT b = pGraphics->GetBounds().GetPadded(-20.f);
    IRECT keyboardBounds = b.GetFromBottom(100);
    IRECT wheelsBounds = keyboardBounds.ReduceFromLeft(100.f).GetPadded(-10.f);
    auto kbd = new IVKeyboardControl(keyboardBounds);
    kbd->SetNoteRange(36, 96);
    pGraphics->AttachControl(kbd, kCtrlTagKeyboard);
    pGraphics->AttachControl(new IWheelControl(wheelsBounds.FracRectHorizontal(0.5)), kCtrlTagBender);
    pGraphics->AttachControl(new IWheelControl(wheelsBounds.FracRectHorizontal(0.5, true), IMidiMsg::EControlChangeMsg::kModWheel));
//    pGraphics->AttachControl(new IVMultiSliderControl<4>(b.GetGridCell(0, 2, 2).GetPadded(-30), "", DEFAULT_STYLE, kParamAttack, EDirection::Vertical, 0.f, 1.f));
    const IRECT controls = b.GetGridCell(0, 4, 3);
    pGraphics->AttachControl(new IVKnobControl(controls.GetGridCell(0, 1, 4).GetCentredInside(90), kParamSampleRate, "Sample Rate"), kNoTag, "RenderMode")->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(controls.GetGridCell(1, 1, 4).GetCentredInside(90), kParamPolyphony, "Polyphony"), kNoTag, "RenderMode")->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(controls.GetGridCell(2, 1, 4).GetCentredInside(90), kParamReverbMode, "Reverb"), kNoTag, "Reverb")->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(controls.GetGridCell(3, 1, 4).GetCentredInside(90), kParamBufferRenderMode, "RenderMode"), kNoTag, "RenderMode")->DisablePrompt(false);
    const IRECT sliders = b.GetGridCell(1, 4, 3); //.Union(controls.GetGridCell(3, 2, 6)).Union(controls.GetGridCell(4, 1, 4));
    pGraphics->AttachControl(new IVSliderControl(sliders.GetGridCell(0, 1, 4), kParamPitchBendRange, "P.Bend Rng"));
    pGraphics->AttachControl(new IVSliderControl(sliders.GetGridCell(1, 1, 4), kParamVelocityFunction, "Vel. Curve"));
    //pGraphics->AttachControl(new IVSliderControl(sliders.GetGridCell(2, 1, 4).GetMidHPadded(30.), kParamSustain, "Sustain"));
    //pGraphics->AttachControl(new IVSliderControl(sliders.GetGridCell(3, 1, 4).GetMidHPadded(30.), kParamRelease, "Release"));
    pGraphics->AttachControl(new IVLEDMeterControl<2>(b.GetFromRight(100).GetPadded(-5).GetReducedFromBottom(100)), kCtrlTagMeter);
      
    pGraphics->AttachControl(new IVButtonControl(keyboardBounds.GetFromTRHC(200, 30).GetTranslated(0, -300), SplashClickActionFunc,
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


char* SW10_PLUG::handleDllPath(const char* romname) {
  static char path[MAX_PATH] = "";
  HMODULE hm = nullptr;

  if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    (LPCSTR)&lsgGetTime, &hm) == 0)
  {
    int ret = GetLastError();
    fprintf(stderr, "GetModuleHandle failed, error = %d\n", ret);
    return path;
  }
  if (GetModuleFileName(hm, path, sizeof(path)) == 0)
  {
    int ret = GetLastError();
    fprintf(stderr, "GetModuleFileName failed, error = %d\n", ret);
    return path;
  }

  // The path variable should now contain the full filepath for this DLL.
  std::string s1(path);
  std::stringstream ss;

  ss << s1.substr(0, s1.find_last_of("\\/")) << "\\";
  strcpy(path, ss.str().c_str());
  strcat(path, romname);
  return path;
}

uint8_t* SW10_PLUG::load_rom_file(const char* romname)
{
  FILE* f;
  uint8_t* mem;

  // Open in same dir as DLL
  char* dirPath = handleDllPath(romname);
  f = fopen(dirPath, "rb");
  if (f == nullptr)
  {
    //Fallback - open in current process CWD
    if (f = fopen(romname, "rb"))
      return nullptr;
  }

  // Original ROM always 2MB. If using custom ROM, this may need to change.
  mem = (uint8_t*)malloc(2 * 1024 * 1024);
  if (mem == nullptr)
  {
    fclose(f);
    return nullptr;
  }

  // If ROM read was not exactly 2MB, fail loading.
  if (fread(mem, 1, 2 * 1024 * 1024, f) != 2 * 1024 * 1024)
  {
    free(mem);
    fclose(f);
    return nullptr;
  }

  return mem;
}

void SW10_PLUG::lsgWrite(uint8_t* event, unsigned int length, int offset)
{
  const uint32_t time = lsgGetTime();
  const uint8_t* p = reinterpret_cast<const BYTE*>(event);

  // Old method
  for (; length > 0; length--, p++) {
    vlsgInstance->VLSG_Write(&time, 4);
    vlsgInstance->VLSG_Write(p, 1);
  }
  vlsgInstance->ProcessMidiData();
}

int SW10_PLUG::start_synth(void)
{
  rom_address = load_rom_file(arg_rom);
  if (rom_address == NULL) {
    fprintf(stderr, "Error opening ROM file: %s\n", arg_rom);
    return -1;
  }

  // set function GetTime
  vlsgInstance->VLSG_SetFunc_GetTime(lsgGetTime);

  // set frequency
  vlsgInstance->VLSG_SetParameter(PARAMETER_Frequency, frequency);

  // set polyphony
  vlsgInstance->VLSG_SetParameter(PARAMETER_Polyphony, 0x10 + polyphony);

  // set reverb effect
  vlsgInstance->VLSG_SetParameter(PARAMETER_Effect, 0x20 + reverb_effect);

  // set address of ROM file
  vlsgInstance->VLSG_SetParameter(PARAMETER_ROMAddress, (uintptr_t)rom_address);

  // set output buffer
  outbuf_counter = 0;
  memset(wav_buffer.get(), 0, 131072);
  vlsgInstance->VLSG_SetParameter(PARAMETER_OutputBuffer, (uintptr_t)wav_buffer.get());

  // start playback
  vlsgInstance->VLSG_PlaybackStart();

  return 0;
}

void SW10_PLUG::stop_synth(void)
{
  vlsgInstance->VLSG_PlaybackStop();
  //munmap(rom_address, ROMSIZE); // TODO unload
}

void SW10_PLUG::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  static int renderedSampleQueueSize = 0;
  static uint16_t renderOffset = 0;

  if (bufferMode == 1) {
    // Attempt 1 - directly render as requested to output buffer (without respecting internal timer code)
    vlsgInstance->VLSG_BufferVst(outbuf_counter, outputs, nFrames, mMidiQueue, mSysExQueue);
    mMidiQueue.Flush(nFrames);
    mSysExQueue.Flush(nFrames);
  } else if (bufferMode == 2) {
    // Attempt 2 - render the chunks based on existing hard-coded sample sizes, but only dequeue on demand.
    if (renderedSampleQueueSize <= 0) {
      vlsgInstance->VLSG_Buffer(outbuf_counter);
      renderedSampleQueueSize += 1024;  //outbuf_size_para (uint8_t)
      ++outbuf_counter;
    }

    for (int sampleIdx = 0, frameIdx = 0; frameIdx < nFrames; ) {
      if (renderedSampleQueueSize <= 0) {
        vlsgInstance->VLSG_Buffer(outbuf_counter);
        renderedSampleQueueSize += 1024;
        ++outbuf_counter;
      }
      outputs[0][frameIdx] = (((int16_t*)wav_buffer.get())[renderOffset++ & 32767]) / 32768.0;
      outputs[1][frameIdx++] = (((int16_t*)wav_buffer.get())[renderOffset++ & 32767]) / 32768.0;
      --renderedSampleQueueSize;
    }
  }
  
  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
}

void SW10_PLUG::OnIdle()
{
  mMeterSender.TransmitData(*this);
}

void SW10_PLUG::OnReset()
{
  // TODO reset VLSG synth state
  mMeterSender.Reset(GetSampleRate());
  mMidiQueue.Resize(GetBlockSize());
  mSysExQueue.Resize(GetBlockSize());
}

void SW10_PLUG::ProcessSysEx(const ISysEx& msg)
{
  TRACE;

  int length = msg.mSize;
  uint8_t *data = (uint8_t*)(msg.mData);

  if (bufferMode == 1) {
    mSysExQueue.Add(msg);
  } else {
    lsgWrite(data, length);
  }
  
  printf("SysEx (fragment) of size %d\n", length);
}

void SW10_PLUG::ProcessMidiMsg(const IMidiMsg& msg)
{
  TRACE;

  // Only for low latency mode
  if (bufferMode == 1) {
    mMidiQueue.Add(msg);
    return;
  }

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

    lsgWrite(data, length, sampOffset);

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
      pbValInt = 0;
    }

    data[0] = 0xE0 | chan;
    data[1] = (pbValInt) & 0x7f;
    data[2] = (pbValInt >> 7) & 0x7f;
    length = 3;

    lsgWrite(data, length);

    printf("Pitch bend: channel:%d value:%d\n", chan, pbVal);
    SendMidiMsg(msg);
    break;
  }

  default:
    msg.PrintMsg();
    break;
  }
  // Fuck it, process every single event.
  vlsgInstance->ProcessMidiData();
}

void SW10_PLUG::OnParamChange(int paramIdx)
{
  //mDSP.SetParam(paramIdx, GetParam(paramIdx)->Value());
  auto value = GetParam(paramIdx)->Value();
  switch (paramIdx) {
    case kParamSampleRate:
      frequency = value;
      vlsgInstance->VLSG_SetParameter(PARAMETER_Frequency, frequency);
      break;
    case kParamPolyphony:
      polyphony = value;
      vlsgInstance->VLSG_SetParameter(PARAMETER_Polyphony, 0x10 + polyphony);
      break;
    case kParamBufferRenderMode:
      bufferMode = value;
      break;
    case kParamReverbMode:
      reverb_effect = value;
      vlsgInstance->VLSG_SetParameter(PARAMETER_Effect, 0x20 + reverb_effect);
      break;
    case kParamPitchBendRange: {
      // Send 0-0-bendRange RPN event
      uint8_t data[3];
      data[0] = 0xB0 | 0;
      data[1] = 100 & 0x7f;
      data[2] = 0 & 0x7f;
      lsgWrite(data, 3);

      data[0] = 0xB0 | 0;
      data[1] = 101 & 0x7f;
      data[2] = 0 & 0x7f;
      lsgWrite(data, 3);

      data[0] = 0xB0 | 0;
      data[1] = 6 & 0x7f;
      data[2] = (uint8_t)value & 0x7f;
      lsgWrite(data, 3);
      break;
    }
    case kParamVelocityFunction:
      vlsgInstance->VLSG_SetParameter(PARAMETER_VelocityFunc, 0x40 + value);
      break;
  }
}

void SW10_PLUG::OnParamChangeUI(int paramIdx, EParamSource source)
{
  if (auto pGraphics = GetUI())
  {
    /*if (paramIdx == kParamLFORateMode)
    {
      const auto sync = GetParam(kParamLFORateMode)->Bool();
      pGraphics->HideControl(kParamLFORateHz, sync);
      pGraphics->HideControl(kParamLFORateTempo, !sync);
    }*/
  }
}

bool SW10_PLUG::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  if(ctrlTag == kCtrlTagBender && msgTag == IWheelControl::kMessageTagSetPitchBendRange)
  {
    auto bendRange = *static_cast<const uint8_t*>(pData);

    // Send 0-0-bendRange RPN event
    uint8_t data[3];
    data[0] = 0xB0 | 0;
    data[1] = 100 & 0x7f;
    data[2] = 0 & 0x7f;
    lsgWrite(data, 3);

    data[0] = 0xB0 | 0;
    data[1] = 101 & 0x7f;
    data[2] = 0 & 0x7f;
    lsgWrite(data, 3);

    data[0] = 0xB0 | 0;
    data[1] = 6 & 0x7f;
    data[2] = bendRange & 0x7f;
    lsgWrite(data, 3);
  }
  
  return false;
}
