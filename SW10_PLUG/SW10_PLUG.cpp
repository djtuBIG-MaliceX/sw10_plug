#include "SW10_PLUG.h"

#if defined(CLAP_API) && (defined(__GNUC__) || defined(__clang__))
// MinGW/Clang portability fix (no iPlug2 submodule edit). GCC/Clang reject
// __attribute__((dllexport)) on a namespace-scope `const` definition — even with a prior
// extern declaration (unlike MSVC, which tolerates it). Upstream defines the CLAP entry
// points as `CLAP_EXPORT const clap_plugin_*_t x = {...}`. We locally expand CLAP_EXPORT
// to `extern __attribute__((dllexport))` (external linkage + export) so those two
// definitions in IPlug_include_in_plug_src.h (next include) are valid and exported. All of
// the CLAP SDK's own CLAP_EXPORT uses (many on static-inline helpers) were already processed
// through SW10_PLUG.h above (header-guarded), so this stays local to the entry points.
#undef CLAP_EXPORT
#define CLAP_EXPORT extern __attribute__((dllexport))
#endif

#include "IPlug_include_in_plug_src.h"

#if defined(CLAP_API) && (defined(__GNUC__) || defined(__clang__))
#undef CLAP_EXPORT
#if defined(_WIN32) || defined(__CYGWIN__)
#define CLAP_EXPORT __attribute__((dllexport))
#else
#define CLAP_EXPORT __attribute__((visibility("default")))
#endif
#endif

#include <sstream>
#include <cstring>
#include <algorithm>

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

// -----------------------------------------------------------------------------
// GM level-1 program names (program change values 0-127 on melodic channels)
const char* kGMProgramNames[128] = {
  "Acoustic Grand Piano",   "Bright Acoustic Piano",  "Electric Grand Piano",   "Honky-tonk Piano",
  "Electric Piano 1",       "Electric Piano 2",       "Harpsichord",            "Clavinet",
  "Celesta",                "Glockenspiel",           "Music Box",              "Vibraphone",
  "Marimba",                "Xylophone",              "Tubular Bells",          "Dulcimer",
  "Drawbar Organ",          "Percussive Organ",       "Rock Organ",             "Church Organ",
  "Reed Organ",             "Accordion",              "Harmonica",              "Tango Accordion",
  "Acoustic Guitar (nylon)","Acoustic Guitar (steel)","Electric Guitar (jazz)", "Electric Guitar (clean)",
  "Electric Guitar (muted)","Overdriven Guitar",       "Distortion Guitar",      "Guitar harmonics",
  "Acoustic Bass",          "Electric Bass (finger)", "Electric Bass (pick)",   "Fretless Bass",
  "Slap Bass 1",            "Slap Bass 2",            "Synth Bass 1",           "Synth Bass 2",
  "Violin",                 "Viola",                  "Cello",                  "Contrabass",
  "Tremolo Strings",        "Pizzicato Strings",      "Orchestral Harp",        "Timpani",
  "String Ensemble 1",      "String Ensemble 2",      "SynthStrings 1",         "SynthStrings 2",
  "Choir Aahs",             "Voice Oohs",             "Synth Voice",            "Orchestra Hit",
  "Trumpet",                "Trombone",               "Tuba",                   "Muted Trumpet",
  "French Horn",            "Brass Section",          "SynthBrass 1",           "SynthBrass 2",
  "Soprano Sax",            "Alto Sax",               "Tenor Sax",              "Baritone Sax",
  "Oboe",                   "English Horn",           "Bassoon",                "Clarinet",
  "Piccolo",                "Flute",                  "Recorder",               "Pan Flute",
  "Blown Bottle",           "Shakuhachi",             "Whistle",                "Ocarina",
  "Lead 1 (square)",        "Lead 2 (sawtooth)",      "Lead 3 (calliope)",      "Lead 4 (chiff)",
  "Lead 5 (charang)",       "Lead 6 (voice)",         "Lead 7 (fifths)",        "Lead 8 (bass+lead)",
  "Pad 1 (new age)",        "Pad 2 (warm)",           "Pad 3 (polysynth)",      "Pad 4 (choir)",
  "Pad 5 (bowed)",          "Pad 6 (metallic)",       "Pad 7 (halo)",           "Pad 8 (sweep)",
  "FX 1 (rain)",            "FX 2 (soundtrack)",      "FX 3 (crystal)",         "FX 4 (atmosphere)",
  "FX 5 (brightness)",      "FX 6 (goblins)",         "FX 7 (echoes)",          "FX 8 (sci-fi)",
  "Sitar",                  "Banjo",                  "Shamisen",               "Koto",
  "Kalimba",                "Bag pipe",               "Fiddle",                 "Shanai",
  "Tinkle Bell",            "Agogo",                  "Steel Drums",            "Woodblock",
  "Taiko Drum",             "Melodic Tom",            "Synth Drum",             "Reverse Cymbal",
  "Guitar Fret Noise",      "Breath Noise",           "Seashore",               "Bird Tweet",
  "Telephone Ring",         "Helicopter",             "Applause",               "Gunshot"
};

// The GM spec names no drum kits; the SW-10 ROM carries kit banks right after the
// melodic set (programs 128+). These are UI labels for those kit banks; on the drum
// channel a program change selects the kit rather than an instrument.
const char* kDrumKitNames[8] = {
  "Standard Kit", "Room Kit",      "Power Kit",   "Orchestra Kit",
  "Kit 5",        "Kit 6",         "Kit 7",       "Kit 8",
};

static const char* kChannelOptionLabels[16] = {
  "1", "2", "3", "4", "5", "6", "7", "8",
  "9", "10", "11", "12", "13", "14", "15", "16"
};

// -----------------------------------------------------------------------------
/** Framed text control that can have its string pushed to it by the delegate via
    kMsgTagProgramName, and (optionally) opens a popup menu on click. The menu is
    rebuilt fresh by the click handler each time (IPopupMenu has no clear-items API),
    and kept alive in a control member so async menu back-ends stay valid. */
class MenuTextControl : public ITextControl
{
public:
  using ClickFunc = std::function<void(MenuTextControl&, IPopupMenu&)>;

  MenuTextControl(const IRECT& bounds, const char* str, const IText& text,
                  ClickFunc onClick = nullptr, bool drawFrame = true)
  : ITextControl(bounds, str, text)
  , mOnClick(onClick)
  , mDrawFrame(drawFrame)
  {}

  void Draw(IGraphics& g) override
  {
    if (mDrawFrame) {
      // insets sized relative to the control so they stay proportional at any UI scale
      const float hi = mRECT.H();
      const IRECT r = mRECT.GetPadded(-1.f);
      g.FillRoundRect(IColor(255, 240, 238, 232), r);
      g.DrawRoundRect(COLOR_BLACK.WithOpacity(0.45f), r);

      IRECT textR = mRECT.GetPadded(-std::max(4.f, hi * 0.12f));
      g.DrawText(mText, GetStr(), textR);

      // simple disclosure arrow
      const float ax = mRECT.R - std::max(6.f, hi * 0.14f), ay = mRECT.MH();
      const float aw = std::max(3.f, hi * 0.11f);
      const IColor ac = mText.mFGColor.WithOpacity(0.8f);
      g.DrawLine(ac, ax - aw, ay - aw * 0.6f, ax, ay + aw);
      g.DrawLine(ac, ax, ay + aw, ax + aw, ay - aw * 0.6f);
    }
    else {
      g.DrawText(mText, GetStr(), mRECT);
    }
  }
  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (!mOnClick) return;

    mMenu = std::make_unique<IPopupMenu>("");
    mOnClick(*this, *mMenu);
    GetUI()->CreatePopupMenu(*this, *mMenu, mRECT.L, mRECT.B);
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    if (msgTag == kMsgTagProgramName && pData && dataSize > 0) {
      SetStr(static_cast<const char*>(pData)); // WDL_String copies
      SetDirty(false);
    }
  }

private:
  ClickFunc mOnClick;
  bool mDrawFrame;
  std::unique_ptr<IPopupMenu> mMenu;
};

// -----------------------------------------------------------------------------
SW10_PLUG::SW10_PLUG(const InstanceInfo& info)
  : iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets)),
  vlsgInstance(std::make_unique<VLSG>()),
  bufferMode(1),
  wav_buffer(std::make_unique<uint8_t[]>(262144)) // floor size; OnReset grows it per host rate
{
  wavBufferCapacityBytes = 262144;
  start_synth();

  GetParam(kParamPolyphony)->InitEnum("Polyphony", polyphony, {"24", "32", "48", "64", "128", "256"});
  GetParam(kParamReverbMode)->InitEnum("Reverb Mode", reverb_effect, { "Off", "Reverb 1", "Reverb 2" });
  GetParam(kParamPitchBendRange)->InitInt("P.Bend Rng", 2, 0, 127, "semitones", IParam::kFlagsNone, "ADSR");
  GetParam(kParamVelocityFunction)->InitInt("Velocity Curve", 6, 0, 11, "", IParam::kFlagsNone, "ADSR");
  GetParam(kParamBufferRenderMode)->InitEnum("Render Mode", 1, {"Off", "Low Latency", "Original Driver"});

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

    // HiDPI: the window was scaled by GetScaleForScreen(), so GetBounds() comes back in
    // PHYSICAL device px. Every design constant below is authored for the logical
    // 1024x520 window and must be multiplied by sc, otherwise the whole layout
    // collapses into the top-left corner at scale>1. (Ratio to FULL logical width,
    // then padded proportionally too.)
    const IRECT rawBounds = pGraphics->GetBounds();
    const float sc = rawBounds.W() / 1024.f;
    auto S = [sc](double v) { return static_cast<float>(v * sc); };
    const IRECT b = rawBounds.GetPadded(-S(8));

    const IRECT tabRow  = b.GetFromTop(S(32));                             // MIDI channel selector
    const IRECT strip   = b.GetReducedFromTop(S(40)).GetFromTop(S(120));   // program + mixers
    const IRECT knobRow = b.GetReducedFromTop(S(164)).GetFromTop(S(82));   // global knobs + info
    const IRECT kbdAll  = b.GetReducedFromTop(S(250));                     // octave bar + wheels|keys
    const IRECT kbdBar  = kbdAll.GetFromTop(S(26));
    const IRECT band    = kbdAll.GetReducedFromTop(S(30));
    const IRECT wheelCol= band.GetFromLeft(S(150)).GetPadded(-S(6));       // wheels side-by-side, left of keys
    const IRECT keysR   = band.GetReducedFromLeft(S(150));

    const IVStyle style = DEFAULT_STYLE.WithDrawShadows(false)
                        .WithLabelText(IText(S(12)))
                        .WithValueText(IText(S(13)));
    auto sendCC = [this](int cc) {
      return [this, cc](IControl* pCaller) {
        IMidiMsg msg;
        msg.MakeControlChangeMsg(static_cast<IMidiMsg::EControlChangeMsg>(cc), pCaller->GetValue(), mSelectedChannel);
        SendMidiMsgFromUI(msg);
      };
    };

    // ---- MIDI channel selector (all 16 channels) ------------------------------------
    pGraphics->AttachControl(new MenuTextControl(tabRow.GetFromLeft(S(64)), "MIDI CH",
      IText(S(13), EAlign::Center, COLOR_BLACK), nullptr, /*drawFrame*/ false));

    std::vector<const char*> chOptions(kChannelOptionLabels, kChannelOptionLabels + 16);
    auto pTabs = new IVTabSwitchControl(tabRow.GetReducedFromLeft(S(72)),
      [this](IControl* pCaller) {
        mSelectedChannel = pCaller->As<IVTabSwitchControl>()->GetSelectedIdx();
        PushChannelStateToUI();   // wheels/sliders/program caption follow the channel
      },
      chOptions, "", style.WithValueText(IText(S(12))));
    pTabs->SetValue(static_cast<double>(mSelectedChannel) / 15.0);
    pGraphics->AttachControl(pTabs, kCtrlTagChannelButtons);

    // ---- Selected-channel panel: name + GM program ----------------------------------
    const IRECT progR = strip.GetFromLeft(S(300));

    pGraphics->AttachControl(new MenuTextControl(progR.GetFromTop(S(26)), "Channel 1",
      IText(S(14), EAlign::Center, COLOR_BLACK), nullptr, /*drawFrame*/ false), kCtrlTagChannelLabel);

    auto pCaption = new MenuTextControl(
      progR.GetReducedFromTop(S(32)).GetCentredInside(progR.W() - S(12), S(54)),
      "Acoustic Grand Piano", IText(S(16), EAlign::Near, COLOR_BLACK),
      [this](MenuTextControl&, IPopupMenu& menu) {
        const int ch = mSelectedChannel;   // freeze channel for this popup session
        if (ch == 9) {
          for (int i = 0; i < 8; i++)
            menu.AddItem(kDrumKitNames[i]);
          menu.SetChosenItemIdx(mChannelUi[ch].program & 7);
        } else {
          for (int i = 0; i < 128; i++)
            menu.AddItem(kGMProgramNames[i]);
          menu.SetChosenItemIdx(mChannelUi[ch].program & 127);
        }
        menu.SetFunction([this, ch](IPopupMenu* pMenu) {
          const int program = pMenu->GetChosenItemIdx();
          if (program < 0) return;

          IMidiMsg msg;   // helper exists at IPlugMidi.h MakeProgramChange
          msg.MakeProgramChange(program, ch);
          SendMidiMsgFromUI(msg);
        });
      }, /*drawFrame*/ true);
    pGraphics->AttachControl(pCaption, kCtrlTagProgramCaption);

    // ---- Per-channel mixers: Volume / Pan / Expression / Modulation (CC 7/10/11/1) --
    const IRECT sliderR = strip.GetReducedFromLeft(S(316)).GetFromLeft(S(4 * 78));
    struct SliderSpec { const char* label; int cc; int tag; };
    const SliderSpec specs[4] = {
      { "Vol", 7, kCtrlTagVolumeSlider }, { "Pan", 10, kCtrlTagPanSlider },
      { "Expr", 11, kCtrlTagExpressionSlider }, { "Mod", 1, kCtrlTagModulationSlider },
    };
    for (int i = 0; i < 4; i++) {
      auto pSlider = new IVSliderControl(sliderR.GetGridCell(i, 1, 4).GetPadded(-S(7)),
        kNoParameter, specs[i].label, style);
      pSlider->SetActionFunction(sendCC(specs[i].cc));
      pGraphics->AttachControl(pSlider, specs[i].tag);
    }

    // ---- Bender + mod wheel — side by side, left of the keys (original placement) ---
    const IRECT benderR = wheelCol.GetGridCell(0, 1, 2).GetPadded(-S(3));
    const IRECT modR    = wheelCol.GetGridCell(1, 1, 2).GetPadded(-S(3));

    pGraphics->AttachControl(new MenuTextControl(benderR.GetFromTop(S(16)), "Bend",
      IText(S(12), EAlign::Center, COLOR_BLACK), nullptr, false));
    pGraphics->AttachControl(new MenuTextControl(modR.GetFromTop(S(16)), "Mod",
      IText(S(12), EAlign::Center, COLOR_BLACK), nullptr, false));

    pGraphics->AttachControl(new ChannelWheelControl(benderR.GetReducedFromTop(S(18)),
      IMidiMsg::EControlChangeMsg::kNoCC, &mSelectedChannel, GetParam(kParamPitchBendRange)->Value()),
      kCtrlTagBender);
    pGraphics->AttachControl(new ChannelWheelControl(modR.GetReducedFromTop(S(18)),
      IMidiMsg::EControlChangeMsg::kModWheel, &mSelectedChannel), kCtrlTagModWheel);

    // ---- Master meter: right column, from under the tab row down to the keyboard ----
    const IRECT meterR = IRECT(b.R - S(108), b.T + S(42), b.R - S(6), kbdAll.T - S(4));
    pGraphics->AttachControl(new IVLEDMeterControl<2>(meterR), kCtrlTagMeter);

    // ---- Global knobs ---------------------------------------------------------------
    const IRECT knobsR = knobRow.GetReducedFromRight(S(280));
    pGraphics->AttachControl(new IVKnobControl(knobsR.GetGridCell(0, 1, 5).GetPadded(-S(8)), kParamPolyphony, "Polyphony"), kNoTag, "RenderMode")->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(knobsR.GetGridCell(1, 1, 5).GetPadded(-S(8)), kParamReverbMode, "Reverb"), kNoTag, "Reverb")->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(knobsR.GetGridCell(2, 1, 5).GetPadded(-S(8)), kParamBufferRenderMode, "Render Mode"), kNoTag, "RenderMode")->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(knobsR.GetGridCell(3, 1, 5).GetPadded(-S(8)), kParamPitchBendRange, "P.Bend Rng"), kNoTag, "ADSR")->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(knobsR.GetGridCell(4, 1, 5).GetPadded(-S(8)), kParamVelocityFunction, "Vel. Curve"), kNoTag, "ADSR")->DisablePrompt(false);

    // Voices + host sample-rate readout (pushed from OnIdle, main thread only)
    const IRECT infoR = IRECT(knobRow.R - S(268), knobRow.T + S(4), knobRow.R - S(118), knobRow.B);
    polyIndicator = new ITextControl(infoR.GetFromTop(S(32)), "Voices: 0",
      IText(S(14), EAlign::Center, COLOR_BLACK));
    pGraphics->AttachControl(polyIndicator);
    rateIndicator = new ITextControl(infoR.GetReducedFromTop(S(36)), "SR: - Hz",
      IText(S(14), EAlign::Center, COLOR_BLACK));
    pGraphics->AttachControl(rateIndicator);
    mShownPoly = -1;
    mShownRate = -1;

    // ---- Keyboard with octave shift --------------------------------------------------
    static const char* kNoteNames[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

    auto pKbd = new ChannelKeyboardControl(keysR, 36, 96, &mSelectedChannel);
    pKbd->SetShowNotesAndVelocity(true);
    pGraphics->AttachControl(pKbd, kCtrlTagKeyboard);

    ITextControl* pOctLabel = new ITextControl(kbdBar.GetReducedFromLeft(S(36)).GetFromLeft(S(150)),
      "C2 - C7", IText(S(13), EAlign::Center, COLOR_BLACK));
    pGraphics->AttachControl(pOctLabel);

    auto shiftOctave = [this, pGraphics, pOctLabel](int delta) {
      mKeyboardOctave = Clip(mKeyboardOctave + delta, -3, 2);
      const int lo = Clip(36 + mKeyboardOctave * 12, 0, 127);
      const int hi = Clip(96 + mKeyboardOctave * 12, lo + 12, 127);
      if (auto p = pGraphics->GetControlWithTag(kCtrlTagKeyboard))
        p->As<IVKeyboardControl>()->SetNoteRange(lo, hi);
      pOctLabel->SetStrFmt(32, "%s%d - %s%d", kNoteNames[lo % 12], lo / 12 - 1,
                                           kNoteNames[hi % 12], hi / 12 - 1);
    };

    pGraphics->AttachControl(new IVButtonControl(kbdBar.GetFromLeft(S(30)),
      [shiftOctave](IControl*) { shiftOctave(-1); }, "<", style));
    pGraphics->AttachControl(new IVButtonControl(kbdBar.GetReducedFromLeft(S(192)).GetFromLeft(S(30)),
      [shiftOctave](IControl*) { shiftOctave(1); }, ">", style));

    pGraphics->AttachControl(new MenuTextControl(kbdBar.GetReducedFromLeft(S(240)), "Click the program name to change instrument, or right-click the bender for its range",
      IText(S(12), EAlign::Near, COLOR_BLACK.WithOpacity(0.6f)), nullptr, false));

    pGraphics->SetQwertyMidiKeyHandlerFunc([pGraphics](const IMidiMsg& msg) {
                                              pGraphics->GetControlWithTag(kCtrlTagKeyboard)->As<IVKeyboardControl>()->SetNoteFromMidi(msg.NoteNumber(), msg.StatusMsg() == IMidiMsg::kNoteOn);
                                           });

    // Initial UI state from the channel mirror (host may have set programs before the UI opened)
    PushChannelStateToUI();
  };
#endif
}


char* SW10_PLUG::handleDllPath(const char* romname) {
  static char path[MAX_PATH] = "";
  HMODULE hm = nullptr;

  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    (LPCSTR)&lsgGetTime, &hm) == 0)
  {
    int ret = GetLastError();
    fprintf(stderr, "GetModuleHandle failed, error = %d\n", ret);
    return path;
  }
  if (GetModuleFileNameA(hm, path, sizeof(path)) == 0)
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
    f = fopen(romname, "rb");
  }
  if (f == nullptr)
    return nullptr;

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
  if (rom_address == nullptr) return; // VLSG dereferences ROM data in ProcessMidiData
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

  // Provisional rate so PlaybackStart allocates; OnReset() retargets to the host's
  // real sample rate (GetSampleRate) before any audio is processed.
  vlsgInstance->VLSG_SetFrequency(44100);

  // set polyphony
  vlsgInstance->VLSG_SetParameter(PARAMETER_Polyphony, 0x10 + polyphony);

  // set reverb effect
  vlsgInstance->VLSG_SetParameter(PARAMETER_Effect, 0x20 + reverb_effect);

  // set address of ROM file
  vlsgInstance->VLSG_SetParameter(PARAMETER_ROMAddress, (uintptr_t)rom_address);

  // set output buffer
  outbuf_counter = 0;
  memset(wav_buffer.get(), 0, wavBufferCapacityBytes);
  vlsgInstance->VLSG_SetParameter(PARAMETER_OutputBuffer, (uintptr_t)wav_buffer.get());

  // start playback
  vlsgInstance->VLSG_PlaybackStart();

  return 0;
}

void SW10_PLUG::stop_synth(void)
{
  polyIndicator = nullptr;
  rateIndicator = nullptr;
  vlsgInstance->VLSG_PlaybackStop();
  //munmap(rom_address, ROMSIZE); // TODO unload
}

void SW10_PLUG::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  if (rom_address == nullptr) {
    // ROM failed to load (start_synth returned -1): VLSG would dereference null
    // ROM data. Stay silent instead of crashing (also keeps ROM-less CI smoke alive).
    for (int ch = 0; ch < 2; ch++)
      for (int i = 0; i < nFrames; i++)
        outputs[ch][i] = 0.0;
    mMidiQueue.Flush(nFrames);
    mSysExQueue.Flush(nFrames);
    return;
  }

  if (bufferMode == 1) {
    // Attempt 1 - directly render as requested to output buffer (without respecting internal timer code)
    const int32_t poly = vlsgInstance->VLSG_BufferVst(outbuf_counter, outputs, nFrames, mMidiQueue, mSysExQueue);
    mPolyCount.store(poly, std::memory_order_relaxed);
    mMidiQueue.Flush(nFrames);
    mSysExQueue.Flush(nFrames);
  } else if (bufferMode == 2) {
    // Attempt 2 - render the chunks based on the engine's native chunk size (which now
    // follows the host sample rate), but only dequeue on demand. Ring layout sizes come
    // from VLSG so they stay correct at any rate (was hardcoded 1024/32768 = 44.1kHz only).
    uint32_t ringSamples = mRingSamples;
    if (ringSamples == 0) {
      ringSamples = mRingSamples = vlsgInstance->GetOutputBufferSamples();
    }
    int16_t* ring = reinterpret_cast<int16_t*>(wav_buffer.get());

    if (mRenderedFramesQueued <= 0) {
      vlsgInstance->VLSG_Buffer(outbuf_counter);
      mRenderedFramesQueued += vlsgInstance->GetLegacyChunkSamples();
      ++outbuf_counter;
    }

    for (int frameIdx = 0; frameIdx < nFrames; ++frameIdx) {
      if (mRenderedFramesQueued <= 0) {
        const int32_t poly = vlsgInstance->VLSG_Buffer(outbuf_counter);
        mPolyCount.store(poly, std::memory_order_relaxed);
        mRenderedFramesQueued += vlsgInstance->GetLegacyChunkSamples();
        ++outbuf_counter;
      }
      outputs[0][frameIdx] = ring[mRenderReadPos] / 32768.0;
      if (++mRenderReadPos >= ringSamples) mRenderReadPos = 0;
      outputs[1][frameIdx] = ring[mRenderReadPos] / 32768.0;
      if (++mRenderReadPos >= ringSamples) mRenderReadPos = 0;
      --mRenderedFramesQueued;
    }
  }
  
  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
}

void SW10_PLUG::OnIdle()
{
  mMeterSender.TransmitData(*this);

  if (mUiStateDirty) {
    mUiStateDirty = 0;
    PushChannelStateToUI();
  }

  // Debounced main-thread-only updates of the audio-thread counters (never touch the
  // IControl strings directly from ProcessBlock anymore).
  const int poly = mPolyCount.load(std::memory_order_relaxed);
  if (polyIndicator && poly != mShownPoly) {
    mShownPoly = poly;
    polyIndicator->SetStrFmt(24, "Voices: %d", poly);
  }
  if (rateIndicator && mSampleRateHint != mShownRate) {
    mShownRate = mSampleRateHint;
    rateIndicator->SetStrFmt(24, "SR: %d Hz", mSampleRateHint);
  }
}

void SW10_PLUG::OnReset()
{
  // The host owns the sample rate: hand it straight to the synth engine (which scales
  // its chunk cadence, envelope timing and reverb delays from the reference rate).
  const double sr = GetSampleRate();
  const unsigned rate = (sr > 100.0) ? static_cast<unsigned>(sr + 0.5) : 44100u;

  mMeterSender.Reset(sr > 0.0 ? sr : 44100.0);
  mMidiQueue.Resize(GetBlockSize());
  mSysExQueue.Resize(GetBlockSize());

  if (rom_address) {
    vlsgInstance->VLSG_SetFrequency(rate);

    // Grow the legacy ring buffer if this rate's slot layout needs more room than we
    // currently have, then re-hand the (possibly new) pointer to the engine.
    const uint32_t need = vlsgInstance->GetOutputBufferBytes();
    if (need > wavBufferCapacityBytes) {
      auto grown = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[need]);
      if (grown) {
        wav_buffer = std::move(grown);
        wavBufferCapacityBytes = need;
      }
    }

    memset(wav_buffer.get(), 0, wavBufferCapacityBytes);
    vlsgInstance->VLSG_SetParameter(PARAMETER_OutputBuffer, (uintptr_t)wav_buffer.get());
  }

  mSampleRateHint = static_cast<int>(rate);

  // Restart render bookkeeping cleanly at the new geometry.
  mRingSamples = vlsgInstance->GetOutputBufferSamples();
  mRenderedFramesQueued = 0;
  mRenderReadPos = 0;
  outbuf_counter = 0;

  ResetChannelUiState();
  mUiStateDirty = 1;   // refresh UI (rate label + channel state) on next idle
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
}

void SW10_PLUG::OnUIClose()
{
  polyIndicator = nullptr;
  rateIndicator = nullptr;
}

void SW10_PLUG::ProcessMidiMsg(const IMidiMsg& msg)
{
  TRACE;

  UpdateChannelUiState(msg);   // keep the UI channel mirror current (both render modes)

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

    SendMidiMsg(msg);
    break;
  
  case IMidiMsg::kNoteOff:
    data[0] = 0x80 | chan;
    data[1] = key;
    data[2] = vel;
    length = 3;

    lsgWrite(data, length);

    SendMidiMsg(msg);
    break;

  case IMidiMsg::kControlChange: {
    uint8_t cc = msg.ControlChangeIdx();
    uint8_t ccVal = (uint8_t)(msg.ControlChange(msg.ControlChangeIdx()) * 127) & 0x7F; // assuming 7-bit
    data[0] = 0xB0 | chan;
    data[1] = cc & 0x7f;
    data[2] = ccVal & 0x7f;

    length = 3;

    lsgWrite(data, length);

    SendMidiMsg(msg);
    break;
  }

  case IMidiMsg::kProgramChange:
    data[0] = 0xC0 | chan;
    data[1] = prog;
    length = 2;

    lsgWrite(data, length);

    SendMidiMsg(msg);
    break;

  case IMidiMsg::kChannelAftertouch: {
    uint8_t aft = msg.ChannelAfterTouch() & 0x7F;
    data[0] = 0xD0 | chan;
    data[1] = aft;
    length = 2;

    lsgWrite(data, length);

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
  auto value = GetParam(paramIdx)->Value();
  switch (paramIdx) {
    case kParamPolyphony:
      polyphony = value;
      vlsgInstance->VLSG_SetParameter(PARAMETER_Polyphony, 0x10 + polyphony);
      break;
    case kParamBufferRenderMode:
      bufferMode = value;
      // Fresh consumer state so a mode switch never reads stale ring positions.
      mRenderedFramesQueued = 0;
      mRenderReadPos = 0;
      outbuf_counter = 0;
      break;
    case kParamReverbMode:
      reverb_effect = value;
      vlsgInstance->VLSG_SetParameter(PARAMETER_Effect, 0x20 + reverb_effect);
      break;
    case kParamPitchBendRange:
      SendPitchBendRPN(mSelectedChannel, value);
      break;
    case kParamVelocityFunction:
      vlsgInstance->VLSG_SetParameter(PARAMETER_VelocityFunc, 0x40 + value);
      break;
  }
}

void SW10_PLUG::OnParamChangeUI(int paramIdx, EParamSource source)
{
  if (auto pGraphics = GetUI())
  {
    /* reserved for future per-control UI reactions */
  }
}

bool SW10_PLUG::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  if (ctrlTag == kCtrlTagBender && msgTag == IWheelControl::kMessageTagSetPitchBendRange && pData)
  {
    const int bendRange = *static_cast<const int*>(pData);   // wheel sends sizeof(int)
    SendPitchBendRPN(mSelectedChannel, bendRange);
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
void SW10_PLUG::SendPitchBendRPN(int channel, int semitones)
{
  // Send 0-0-bendRange RPN event on the given channel
  uint8_t data[3];
  const uint8_t status = 0xB0 | (static_cast<uint8_t>(channel) & 0xF);

  data[0] = status;
  data[1] = 100 & 0x7f;
  data[2] = 0 & 0x7f;
  lsgWrite(data, 3);

  data[0] = status;
  data[1] = 101 & 0x7f;
  data[2] = 0 & 0x7f;
  lsgWrite(data, 3);

  data[0] = status;
  data[1] = 6 & 0x7f;
  data[2] = static_cast<uint8_t>(semitones) & 0x7f;
  lsgWrite(data, 3);
}

void SW10_PLUG::UpdateChannelUiState(const IMidiMsg& msg)
{
  const int ch = msg.Channel();
  if (ch < 0 || ch >= MIDI_CHANNELS) return;

  bool touched = false;
  switch (msg.StatusMsg())
  {
    case IMidiMsg::kProgramChange:
      mChannelUi[ch].program = msg.Program() & 0x7F;
      touched = true;
      break;
    case IMidiMsg::kControlChange: {
      const int idx = msg.ControlChangeIdx();
      const int val = static_cast<int>(msg.ControlChange(static_cast<IMidiMsg::EControlChangeMsg>(idx)) * 127.0);
      switch (idx) {
        case 1:  mChannelUi[ch].modulation = val; touched = true; break;
        case 7:  mChannelUi[ch].volume      = val; touched = true; break;
        case 10: mChannelUi[ch].pan         = val; touched = true; break;
        case 11: mChannelUi[ch].expression  = val; touched = true; break;
        default: break;
      }
      break;
    }
    default:
      break;
  }

  // Benign race on this int flag/mirror by design: writes happen on the audio thread,
  // the UI polls it in OnIdle (main thread). Worst case is one frame of stale display.
  if (touched && ch == mSelectedChannel)
    mUiStateDirty = 1;
}

void SW10_PLUG::ResetChannelUiState()
{
  for (int ch = 0; ch < MIDI_CHANNELS; ch++)
    mChannelUi[ch] = ChannelUiState();   // GM defaults (program 0, vol 100, pan centre...)
}

void SW10_PLUG::PushChannelStateToUI()
{
  auto pGraphics = GetUI();
  if (!pGraphics) return;

  const ChannelUiState st = mChannelUi[mSelectedChannel];  // snapshot the benign-race mirror

  auto pushSlider = [&](int tag, double v) {
    if (auto p = pGraphics->GetControlWithTag(tag))
      p->SetValueFromDelegate(v);
  };
  pushSlider(kCtrlTagVolumeSlider, st.volume / 127.0);
  pushSlider(kCtrlTagPanSlider, st.pan / 127.0);
  pushSlider(kCtrlTagExpressionSlider, st.expression / 127.0);
  pushSlider(kCtrlTagModulationSlider, st.modulation / 127.0);

  char buf[96];
  snprintf(buf, sizeof(buf), "%s", GetProgramNameStr(mSelectedChannel, st.program));
  SendControlMsgFromDelegate(kCtrlTagProgramCaption, kMsgTagProgramName, static_cast<int>(strlen(buf)) + 1, buf);

  snprintf(buf, sizeof(buf), "Channel %d%s", mSelectedChannel + 1,
           (mSelectedChannel == 9) ? "  -  Drum kit" : "");
  SendControlMsgFromDelegate(kCtrlTagChannelLabel, kMsgTagProgramName, static_cast<int>(strlen(buf)) + 1, buf);
}

const char* SW10_PLUG::GetProgramNameStr(int channel, int program)
{
  if (channel == 9)   // MIDI channel 10 = drums; program picks the kit
    return kDrumKitNames[program & 7];
  return kGMProgramNames[program & 127];
}
