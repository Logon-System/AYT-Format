// AYT CLI player — C++17, requires SDL2
// Tronic / Siko / Longshot
// GPA + Logon Sytem

// Build:
// Under linux:
// g++ -std=c++17 -O2 -pipe -s -DNOMINMAX  ayt-player-cli.cpp -o ayt-player -lSDL2
//
// Under Windows/Mingw:
//  g++ -std=c++17 -O2 -pipe -s ayt-player-cli.cpp -o ayt-player.exe -lmingw32 -lSDL2main -lSDL2  -Wl,-Bstatic -lstdc++ -static-libgcc


#define _CRT_SECURE_NO_WARNINGS

#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <iomanip>

using TWord = uint16_t;
int verbosity = 1;

// Helpers
template <typename T> static inline T clampv(T v, T lo, T hi) {
  return std::clamp(v, lo, hi);
}
template <typename T> static inline T maxi(T a, T b) { return (a > b) ? a : b; }

// ========================
// AYT Parser
// ========================

struct TAYTHeader {
  uint8_t Magic = 0, B0 = 0, B1 = 0, N = 0;
  TWord PtrFirst = 0, PtrLoop = 0, PtrInit = 0, T2 = 0;
  uint8_t PF = 0;
  double masterClock = 1000000.0f;
  int FrameCount = 0;
  int LoopFrame = 0;
  std::vector<int> PresentRegs;
  std::array<std::vector<uint8_t>, 16>
      Regs; // Registres [0..15][frame 0..FrameCount-1]
};

struct TConstInit {
  int RegIndex;
  int Value;
};

class TAYTParser {
  std::ifstream F;

  // Contenu binaire du fichier pour faciliter le parsing
  std::vector<uint8_t> FileData;

  uint16_t ReadU16LE(size_t off) const {
    if (off + 1 >= FileData.size())
      throw std::runtime_error("ReadU16LE: EOF");
    return uint16_t(FileData[off]) | (uint16_t(FileData[off + 1]) << 8);
  }

public:
  TAYTHeader header;

  void LoadFromFile(const std::string &path) {
    // 1. Charger tout le fichier
    F = std::ifstream(path, std::ios::binary | std::ios::ate);
    if (!F)
      throw std::runtime_error("Fichier introuvable: " + path);

    size_t fileSize = F.tellg();
    FileData.resize(fileSize);
    F.seekg(0);
    F.read(reinterpret_cast<char *>(FileData.data()), fileSize);

    if (fileSize < 14)
      throw std::runtime_error("Fichier AYT trop court");

    // 2. Parser le header (index 0 à 13)
    header.Magic = FileData[0];
    header.B0 = FileData[1];
    header.B1 = FileData[2];
    header.N = FileData[3];
    header.PtrFirst = ReadU16LE(4);
    header.PtrLoop = ReadU16LE(6);
    header.PtrInit = ReadU16LE(8);
    header.T2 = ReadU16LE(10);
    header.PF = FileData[12]; // Freq + Plateform

    struct platformDev {
      std::string name;
      double masterClock;
    };

    const platformDev platformFrequencies[9] = {
        {"CPC", 1000000.0},         {"ORIC", 1000000.0},
        {"ZXUNO", 1750000.0},       {"PENTAGON", 1750000.0},
        {"TIMEXTS2068", 1764000.0}, {"ZX128", 1773400.0},
        {"MSX", 1789772.5},         {"ATARI", 2000000.0},
        {"VG5000", 1000000.0}};

    const int pf = header.PF & 31;

    if (pf < 9) {
      header.masterClock = platformFrequencies[pf].masterClock;
    }

    const size_t pattDeb = 14;

    // 3. Déterminer les registres présents
    std::array<bool, 14> present{};
    for (int r = 0; r <= 7; r++)
      if (header.B1 & (1 << (7 - r)))
        present[r] = true;
    for (int r = 8; r <= 13; r++)
      if (header.B0 & (1 << (7 - (r - 8))))
        present[r] = true;

    int presentCount = 0;
    for (int r = 0; r < 14; ++r) {
      if (present[r]) {
        header.PresentRegs.push_back(r);
        presentCount++;
      }
    }

    if (presentCount == 0)
      throw std::runtime_error("Invalid register count");

    // 4. Extraire les blocs de données (Pattern) et de séquences (Sequence)
    const size_t pattLen = size_t(header.PtrFirst) - pattDeb;
    if (header.PtrFirst < pattDeb || size_t(header.PtrFirst) > fileSize)
      throw std::runtime_error("Invalid PattStart/PtrFirst values");

    if (pattDeb + pattLen > fileSize)
      throw std::runtime_error("Invalid fileSize");

    const uint8_t *patt = FileData.data() + pattDeb;

    const int seqWords = int(header.T2) - presentCount;
    const size_t seqLen = size_t(seqWords) * 2;
    const size_t seqStart = size_t(header.PtrFirst);

    if (seqStart + seqLen > fileSize)
      throw std::runtime_error("Invalid fileSize");

    const uint8_t *seq = FileData.data() + seqStart;

    // 5. Extract Registers Initialization constants 
    std::map<int, int> constMap;
    size_t p = header.PtrInit; //seqLen + size_t(presentCount) * 2;

     while (p + 1 < fileSize) {
      const int a = FileData[p++];
      if (a == 0xFF)
        break;
      const int b = FileData[p++];
      constMap[a & 0xFF] = b & 0xFF;

    //std::cout<< p << " R"<<a <<"="<< b<<std::endl;

    }

    // 6. Décompression/Expansion des données dans header.Regs
    const int N = header.N | 0;
    const int P = presentCount;
    const int blocks = seqWords / P;
    header.FrameCount = blocks * N;

    if (header.FrameCount <= 0)
      throw std::runtime_error("Invalid frame count");

    // Initialisation des 14 registres (R0 à R13)
    for (int r = 0; r <= 12; r++) {
      const int c = constMap.count(r) ? constMap.at(r) : 0;
      header.Regs[r].assign(header.FrameCount, c);
    }
    const int c13 = constMap.count(13) ? constMap.at(13) : 0xFF;
    header.Regs[13].assign(header.FrameCount, c13);

    // Les registres 14 et 15 ne sont pas utilisés dans AYT/YM
    header.Regs[14].assign(header.FrameCount, 0);
    header.Regs[15].assign(header.FrameCount, 0);

    size_t sp = 0; // Pointeur dans le buffer de séquence
    for (int b = 0; b < blocks; b++) {
      for (int idx = 0; idx < P; idx++) {
        const int r = header.PresentRegs[idx];
        const size_t off =
            uint16_t(seq[sp]) | (uint16_t(seq[sp + 1]) << 8); // ReadU16LE
        sp += 2;

        if (off + N > pattLen) {
          throw std::runtime_error("Pattern Pointer overflow");
        }

        // Copier N octets (slice) du Pattern vers le tableau de registres
        for (int n = 0; n < N; n++) {
          header.Regs[r][b * N + n] = patt[off + n];
        }
      }
    }

    // 7. Calcul du LoopFrame
    header.LoopFrame = 0;
    if (header.PtrLoop >= header.PtrFirst) {
      const size_t bytesFromFirst =
          size_t(header.PtrLoop) - size_t(header.PtrFirst);
      const size_t blockIdx =
          std::lround(double(bytesFromFirst) / double(P * 2));
      if (blockIdx * size_t(P * 2) == bytesFromFirst) {
        header.LoopFrame = int(blockIdx * N);
      }
    }
    header.LoopFrame = clampv(header.LoopFrame, 0, header.FrameCount);
  }

  // Méthodes pour l'interface Player
  int Frames() const { return header.FrameCount; }
  uint8_t GetRegFrame(int reg, int frame) const {
    return header.Regs[size_t(reg)][size_t(frame)];
  }
  int LoopFrame() const { return header.LoopFrame; }

  void PrintHeader() const {
    std::cout << "AYT Loaded (Version " << (int)header.Magic << ")"
              << std::endl;
    std::cout << "Frame N (chunk size): " << (int)header.N << "" << std::endl;
    std::cout << "Blocks/Sequence words: "
              << (header.T2 - header.PresentRegs.size()) / 2 << " (x"
              << header.PresentRegs.size() << " regs)" << std::endl;
    std::cout << "Total Frames: " << header.FrameCount << "" << std::endl;
    std::cout << "Loop Frame: " << header.LoopFrame << "" << std::endl;
    std::cout << "Present Registers (R): ";
    for (int r : header.PresentRegs)
      std::cout << r << " ";
    std::cout << "" << std::endl;

    std::cout << "MasterClock : " << std::fixed << header.masterClock << " Hz"
              << std::endl;
  }
};

// ========================
// Cœur AY (TAYChip) - Identique à la version YM
// (Pas de changement requis)
// ========================
class TAYChip {
  // ... (Corps de la classe TAYChip comme dans ymconsoleplayer.cpp) ...
  uint8_t R[16]{};

  int TonePeriod[3]{1, 1, 1};
  int ToneCnt[3]{0, 0, 0};
  int ToneOut[3]{0, 0, 0};

  int NoisePeriod{1};
  int NoiseCnt{0};
  uint32_t NoiseLFSR{0x1FFFF}; // 17 bits, seed non nul
  int NoiseOut{1};

  int EnvPeriod{1};
  int EnvCnt{0};
  int EnvShape{0}; // R13
  int EnvLevel{0}; // 0..31
  bool EnvHold{false}, EnvAlt{false}, EnvAtk{false}, EnvCont{false};

  std::array<float, 32> DAC{};

  void RecalcTonePeriods() {
    TonePeriod[0] = maxi(1, int(R[0] | ((R[1] & 0x0F) << 8)));
    TonePeriod[1] = maxi(1, int(R[2] | ((R[3] & 0x0F) << 8)));
    TonePeriod[2] = maxi(1, int(R[4] | ((R[5] & 0x0F) << 8)));
  }
  void RecalcNoisePeriod() { NoisePeriod = maxi(1, int(R[6] & 0x1F)); }
  void RecalcEnvPeriod() { EnvPeriod = maxi(1, int(R[0x0B] | (R[0x0C] << 8))); }
  void EnvReload() {
    EnvShape = R[0x0D] & 0x0F;
    EnvCont = (EnvShape & 0x08) != 0;
    EnvAtk = (EnvShape & 0x04) != 0;
    EnvAlt = (EnvShape & 0x02) != 0;
    EnvHold = (EnvShape & 0x01) != 0;
    EnvLevel = EnvAtk ? 0 : 31;
  }
  void EnvStep() {
    if (EnvAtk) {
      if (EnvLevel < 31)
        ++EnvLevel;
    } else {
      if (EnvLevel > 0)
        --EnvLevel;
    }

    bool hitTop = (EnvAtk && EnvLevel == 31);
    bool hitBot = (!EnvAtk && EnvLevel == 0);
    if (hitTop || hitBot) {
      if (!EnvCont) {
        EnvLevel = EnvAtk ? 31 : 0;
        return;
      }
      if (EnvHold)
        return;
      if (EnvAlt)
        EnvAtk = !EnvAtk;
      else
        EnvLevel = EnvAtk ? 0 : 31;
    }
  }

public:
  TAYChip(bool UseYMTable, int /*MasterClock*/) {
    setDAC(false);
    Reset();
  }

  void setDAC(bool UseYMTable) {
    static constexpr float AY_DAC[32] = {
        0.0000000f, 0.0000000f, 0.0099947f, 0.0099947f, 0.0144503f, 0.0144503f,
        0.0210575f, 0.0210575f, 0.0307012f, 0.0307012f, 0.0455482f, 0.0455482f,
        0.0644999f, 0.0644999f, 0.1073625f, 0.1073625f, 0.1265888f, 0.1265888f,
        0.2049897f, 0.2049897f, 0.2922103f, 0.2922103f, 0.3728389f, 0.3728389f,
        0.4925307f, 0.4925307f, 0.6353246f, 0.6353246f, 0.8055848f, 0.8055848f,
        1.0000000f, 1.0000000f};
    static constexpr float YM_DAC[32] = {
        0.0000000f, 0.0000000f, 0.0046540f, 0.0077211f, 0.0109560f, 0.0139620f,
        0.0169986f, 0.0200198f, 0.0243687f, 0.0296941f, 0.0350652f, 0.0403906f,
        0.0485389f, 0.0583352f, 0.0680552f, 0.0777752f, 0.0925154f, 0.1110857f,
        0.1297475f, 0.1484855f, 0.1766690f, 0.2115511f, 0.2463874f, 0.2811017f,
        0.3337301f, 0.4004273f, 0.4673838f, 0.5344320f, 0.6351720f, 0.7580072f,
        0.8799268f, 1.0000000f};
    const float *src = UseYMTable ? YM_DAC : AY_DAC;
    for (int i = 0; i < 32; ++i)
      DAC[size_t(i)] = src[i];
  }

  void Reset() {
    std::fill(std::begin(R), std::end(R), 0);
    for (int i = 0; i < 3; ++i) {
      TonePeriod[i] = 1;
      ToneCnt[i] = 0;
      ToneOut[i] = 0;
    }
    NoisePeriod = 1;
    NoiseCnt = 0;
    NoiseLFSR = 0x1FFFF;
    NoiseOut = 1;
    EnvPeriod = 1;
    EnvCnt = 0;
    EnvShape = 0;
    EnvLevel = 0;
    EnvHold = false;
    EnvAlt = false;
    EnvAtk = false;
    EnvCont = false;
  }

  void WriteReg(int Index, int Value) {
    Index &= 0x0F;
    Value &= 0xFF;
    R[Index] = uint8_t(Value);
    switch (Index) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
      RecalcTonePeriods();
      break;
    case 6:
      RecalcNoisePeriod();
      break;
    case 0x0B:
    case 0x0C:
      RecalcEnvPeriod();
      break;
    case 0x0D:
      EnvReload();
      break;
    default:
      break;
    }
  }

  void Tick() {
    // tones
    for (int i = 0; i < 3; ++i)
      if (ToneCnt[i] == 0)
        ToneCnt[i] = TonePeriod[i];
    for (int i = 0; i < 3; ++i) {
      if (--ToneCnt[i] == 0)
        ToneOut[i] ^= 1;
    }

    // noise
    if (NoiseCnt == 0)
      NoiseCnt = NoisePeriod;
    if (--NoiseCnt == 0) {
      if (((NoiseLFSR ^ (NoiseLFSR >> 3)) & 1u) != 0u)
        NoiseLFSR = (NoiseLFSR >> 1) | 0x10000u;
      else
        NoiseLFSR = (NoiseLFSR >> 1);
      NoiseOut = int(NoiseLFSR & 1u);
    }

    // envelope
    if (EnvCnt == 0)
      EnvCnt = EnvPeriod;
    if (--EnvCnt == 0)
      EnvStep();
  }

  void MixSample(float &LOut, float &ROut) {
    auto ChanEnabled = [](int regVal, int bit) -> bool {
      return (((regVal >> bit) & 1) == 0);
    }; // 0=ON
    auto EnvLevelDAC = [this]() -> int { return clampv(EnvLevel, 0, 31); };
    auto FixVolIdx = [](int v) -> int {
      v = clampv(v, 0, 15);
      return v * 2 + 1;
    };

    float mixL = 0.f, mixR = 0.f;
    for (int i = 0; i < 3; ++i) {
      int gateTone = ChanEnabled(int(R[7]), i) ? 1 : 0;
      int gateNoise = ChanEnabled(int(R[7]), i + 3) ? 1 : 0;

      int chanOut = 1;
      if (gateTone)
        chanOut &= ToneOut[i];
      if (gateNoise)
        chanOut &= (1 - NoiseOut);

      int envActive = (R[8 + i] & 0x10);
      int volIdx = envActive ? EnvLevelDAC() : FixVolIdx(R[8 + i] & 0x0F);
      float vol = DAC[size_t(volIdx)];
      float c = float((2 * chanOut - 1)) * vol;

      if (i == 0)
        mixL += c;
      else if (i == 1) {
        mixL += c * 0.5f;
        mixR += c * 0.5f;
      } else
        mixR += c;
    }
    LOut = clampv(mixL * 0.6f, -1.0f, 1.0f);
    ROut = clampv(mixR * 0.6f, -1.0f, 1.0f);
  }
};

// ========================
// High-pass 20 Hz (THighPass) - Identique
// ========================
struct THighPass {
  double Alpha{0.0};
  double PrevInL{0.0}, PrevOutL{0.0};
  double PrevInR{0.0}, PrevOutR{0.0};
  void Init(int SampleRate, double CutHz = 20.0) {
    double dt = 1.0 / double(SampleRate);
    double RC = 1.0 / (2.0 * M_PI * CutHz);
    Alpha = RC / (RC + dt);
    PrevInL = PrevOutL = PrevInR = PrevOutR = 0.0;
  }
  void Process(double &L, double &R) {
    double yL = Alpha * (PrevOutL + L - PrevInL);
    double yR = Alpha * (PrevOutR + R - PrevInR);
    PrevInL = L;
    PrevOutL = yL;
    PrevInR = R;
    PrevOutR = yR;
    L = yL;
    R = yR;
  }
};

// ========================
// Player (TAYTPlayer)
// Adapté pour utiliser TAYTParser
// ========================
class TAYTPlayer {
  const TAYTParser *P{nullptr};
  TAYChip AY;
  int FFrameRate{50}; // Le format AYT ne stocke pas le frame rate, on prend
                      // 50Hz par défaut
  int FMasterClock{1000000};
  int FFrameCount{0};
  int FLoopFrame{0};

  int FCurFrame{0};
  double FSampleInFrm{0.0};
  double FSmpPerFrm{0.0};
  bool FApplyRegs{true};

  int FSampleRate{48000};
  double FChipTicksPerSample{1.0f};
  double FChipTicksPerSampleCumul{0.0f};
  THighPass HP;

  std::atomic<bool> FIsPlaying{false};

public:
  // Le constructeur reçoit le TAYTParser au lieu du TYMParser
  TAYTPlayer(const TAYTParser *Parser, int SampleRate, int OverrideClock,
             bool UseYMDAC)
      : P(Parser), AY(UseYMDAC, OverrideClock > 0 ? OverrideClock : 1000000) {
    FFrameRate = 50;

    if (OverrideClock > 0)
      FMasterClock = OverrideClock;
    else
      FMasterClock = 1000000; // MasterClock par défaut

    FFrameCount = Parser->Frames();
    FLoopFrame = Parser->LoopFrame();

    FSampleRate = clampv(SampleRate, 8000, 192000);
    FSmpPerFrm = double(FSampleRate) / double(FFrameRate);

    FChipTicksPerSample = maxi(1.0, (FMasterClock / 8.0) / FSampleRate);
                                            
    //std::cout<<"FChipTicksPerSample = "<< FChipTicksPerSample <<std::endl;
    //std::cout<<"FSmpPerFrm = "<< FSmpPerFrm <<std::endl;
    
    HP.Init(FSampleRate, 20.0);

    Start();
  }

  void Start() {
    FCurFrame = 0;
    FSampleInFrm = 0.0;
    FApplyRegs = true;
    FIsPlaying = true;
    AY.Reset();
    FChipTicksPerSampleCumul=0;
  }

  void Stop() { FIsPlaying = false; }

  bool IsPlaying() const { return FCurFrame < FFrameCount; }
  std::atomic<bool> &IsPlayingAtomic() { return FIsPlaying; }

  void FillBufferPCM16(void *outBuf, int frames) {
    auto *p = reinterpret_cast<int16_t *>(outBuf);
    for (int i = 0; i < frames; ++i) {
      if (FApplyRegs) {
        if (FCurFrame >= FFrameCount) {
          if (FLoopFrame < FFrameCount)
            FCurFrame = FLoopFrame;
          else {
            FIsPlaying = false;
            std::fill(p, p + (frames - i) * 2, 0);
            return;
          }
        }
        // Boucle sur les 14 registres (0 à 13) 
        for (int rr = 0; rr <= 13; ++rr) {
          uint8_t vreg = P->GetRegFrame(rr, FCurFrame);
          if (verbosity>1) std::cout <<  std::hex << std::setw(2) << std::setfill('0') << (int)vreg << " ";

          if (rr == 13 && vreg == 0xFF)
            continue; // R13=0xFF est un skip en YM
          AY.WriteReg(rr, vreg);
          
        }
        

        if (verbosity>1) std::cout << std::endl;
        FApplyRegs = false;
      }

      FChipTicksPerSampleCumul+=FChipTicksPerSample;

      while (FChipTicksPerSampleCumul>1.0f) 
      {
        AY.Tick();
        FChipTicksPerSampleCumul--;
      }

      float Lf = 0.f, Rf = 0.f;
      AY.MixSample(Lf, Rf);
      double Ld = double(Lf), Rd = double(Rf);
      HP.Process(Ld, Rd);

      int li = clampv(int(std::lround(Ld * 32767.0)), -32768, 32767);
      int ri = clampv(int(std::lround(Rd * 32767.0)), -32768, 32767);
      *p++ = static_cast<int16_t>(li);
      *p++ = static_cast<int16_t>(ri);

      FSampleInFrm += 1.0;
      if (FSampleInFrm >= FSmpPerFrm) {
        FSampleInFrm -= FSmpPerFrm;
        ++FCurFrame;
        FApplyRegs = true;
      }
    }
  }

  int EffectiveClock() const { return FMasterClock; }
};

// ========================
// Callback audio SDL - Identique
// ========================
// Utilise l'interface FillBufferPCM16 du player
static void audioCallback(void *userdata, Uint8 *stream, int len) {
  // Le player doit maintenant être TAYTPlayer
  TAYTPlayer *player = reinterpret_cast<TAYTPlayer *>(userdata);
  int frames_to_fill = len / sizeof(int16_t) / 2;

  if (player->IsPlayingAtomic().load()) {
    player->FillBufferPCM16(stream, frames_to_fill);
  } else {
    std::memset(stream, 0, len);
  }
}

// ========================
// SDL Audio Runner - Identique
// ========================
static void PlayViaSDL(TAYTPlayer &Player, int SampleRate) {
  SDL_AudioSpec want, have;

  SDL_zero(want);
  want.freq = SampleRate;
  want.format = AUDIO_S16SYS;
  want.channels = 2;
  want.samples = 2048;
  want.callback = audioCallback;
  want.userdata = &Player;

  SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);

  if (dev == 0) {
    throw std::runtime_error("SDL_OpenAudioDevice failed: " +
                             std::string(SDL_GetError()));
  }

  if (verbosity>1) std::cout << std::setw(2) << std::setfill('0');

  SDL_PauseAudioDevice(dev, 0);

  std::cin.get();

  Player.Stop();
  SDL_Delay(200);

  SDL_PauseAudioDevice(dev, 1);
  SDL_CloseAudioDevice(dev);
}

static std::string getArgValue(int argc, char **argv, const std::string &name,
                               const std::string &defVal) {
  std::string key = "-" + name + "=";
  for (int i = 1; i < argc; ++i) {
    const char *s = argv[i];
    if (s[0] == '-') {
      int ik = 1;
      if (s[1] == '-')
        ik++;

      const char *eq = std::strchr(s, '=');
      if (eq) {
        std::string k(s + ik, eq - (s + ik));
        std::string v(eq + 1);
        std::string kLOW = k;
        std::transform(kLOW.begin(), kLOW.end(), kLOW.begin(), ::tolower);
        std::string nameLOW = name;
        std::transform(nameLOW.begin(), nameLOW.end(), nameLOW.begin(),
                       ::tolower);
        if (kLOW == nameLOW)
          return v;
      }
    }
  }
  return defVal;
}

static int getArg(int argc, char **argv, const std::string &name) {
  std::string key = "-" + name;
  int cnt = 0;
  for (int i = 1; i < argc; ++i) {
    if (key == argv[i])
      cnt++;
  }
  return cnt;
}

// ========================
// Main
// ========================
int main(int argc, char **argv) {


  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    std::cerr << "Error: SDL_Init failed: " << SDL_GetError() << std::endl;
    return 2;
  }

  try {
    if (argc < 2) {
      std::cout
          << "Usage: " << (argc > 0 ? argv[0] : "ayt-player")
          << " <fichier.ayt> [--rate=48000] [--dac=ym|ay] [--clock=1773400] "
             "[--quiet]"
          << std::endl;
      SDL_Quit();
      return 1;
    }

    std::string fileName = argv[1];
    {
      std::ifstream f(fileName, std::ios::binary);
      if (!f)
        throw std::runtime_error("Unable to find file : " + fileName);
    }

    std::string rateStr = getArgValue(argc, argv, "rate", "48000");
    int rate = clampv(std::atoi(rateStr.c_str()), 8000, 192000);

    std::string dacStr = getArgValue(argc, argv, "dac", "ay");
    std::string dacLOW = dacStr;
    std::transform(dacLOW.begin(), dacLOW.end(), dacLOW.begin(), ::tolower);
    bool UseYMDAC = (dacLOW != "ay");

    std::string clockStr = getArgValue(argc, argv, "clock", "");
    int ForceClk = clampv(std::atoi(clockStr.c_str()), 0, 10000000);

    if (getArg(argc, argv, "quiet") > 0)
      verbosity = 0;
    if (getArg(argc,argv,"v")>0) verbosity ++;

    // Remplacement de TYMParser par TAYTParser
    TAYTParser parser;
    parser.LoadFromFile(fileName);

    if (verbosity > 0)
      parser.PrintHeader(); // Affiche les infos AYT

    int EffClock = (ForceClk > 0) ? ForceClk : parser.header.masterClock;

    // Remplacement de TYMPlayer par TAYTPlayer
    TAYTPlayer player(&parser, rate, EffClock, UseYMDAC);

    if (verbosity > 0)
      std::cout << "Currently Playing... (CTRL+C or Return to stop)"
                << std::endl;
    PlayViaSDL(player, rate);

    SDL_Quit();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    SDL_Quit();
    return 2;
  }
}
