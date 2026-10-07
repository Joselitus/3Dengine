#ifndef ENGINE_SYNTH
#define ENGINE_SYNTH

#include <atomic>
#include <cstdint>

#include "AudioGenerator.h"

// A big V8 (a 90s Ford 460 / Chevy 454 of a motorhome: low idle with a lumpy cam,
// long pulses, one big muffler) synthesized in real time, after the physically informed
// approach of procedural engine simulators (no recordings, no pitch-shifting):
//  - a crankshaft turning at the current rpm (with a little irregularity) fires
//    the eight cylinders in the cross-plane order 1-8-4-3-6-5-7-2, every 90
//    degrees of the 720-degree cycle; each firing is an exhaust pressure pulse
//    whose shape is fixed in crank angle (so it follows the rpm by itself),
//    sharper and stronger with load, and a little different every time;
//  - the cylinders feed two banks, each with its own exhaust pipe (a delay
//    line with an inverted, damped end reflection: the pipe's resonances), which
//    gives the V8 its uneven burble (the banks fire 270-180-90-180 degrees apart);
//  - the banks meet in a Y-pipe into one long tailpipe, and the muffler adds low
//    resonances and a low-pass that opens with the load and the revs;
//  - intake and mechanical noise ride on top, and a little saturation.
// Starting: with the starter engaged the crank turns slowly and drags at each
// compression, the cylinders that do not fire just pump air (a weak chuff), the
// starter makes its whine (commutator) and the growl of its pinion on the
// flywheel's teeth, a worn one grinds and rattles, and the solenoid clunks when
// it engages and lets go. setFire() < 1 makes some firings fail (it sputters).
// setRpm/setLoad may be called from the game thread while it plays.
class EngineSynth : public AudioGenerator {
public:
  static constexpr unsigned int RATE = 44100;

private:
  std::atomic<float> targetRpm{0.0f};
  std::atomic<float> targetLoad{0.0f};
  std::atomic<float> targetStarter{0.0f}; // how engaged the starter motor is, 0..1
  std::atomic<float> targetFire{1.0f};    // share of the firings that happen, 0..1

  // audio thread state
  float rpm = 0.0f, load = 0.0f;
  float cycle = 0.0f;           // crank angle in the 4-stroke cycle, degrees 0..720
  float wobble = 0.0f;          // slow random speed fluctuation of the crank
  float cylinderAmp[8];         // strength of each cylinder's last firing
  float cylinderAngle[8];       // crank degrees since each cylinder fired
  uint32_t seed = 12345u;

  static const int PIPE_SIZE = 2048;
  float pipe[3][PIPE_SIZE];     // exhaust pipes: the two banks' and the common tailpipe (delay lines)
  float pipeDamp[3];            // low-pass inside the pipe loop
  int pipeWrite = 0;
  int pipeDelay[3];

  struct Resonator { float b0, b2, a1, a2, z1, z2; };
  Resonator silencer[3];
  float cutoffState[3];         // the low-pass of the collector
  float dcX = 0.0f, dcY = 0.0f; // DC blocker
  // the starter: an old, worn one (see generate)
  float starter = 0.0f, fire = 1.0f;
  float commutatorPhase = 0.0f, gearPhase = 0.0f, whineJitter = 0.0f;
  float grind = 0.0f;            // a grinding tooth, decaying
  float clickEnv = 0.0f, clunkEnv = 0.0f, clunkPhase = 0.0f;
  float starterLow = 0.0f, motorSpeed = 0.0f, motorPhase = 0.0f;
  float thumpEnv = 0.0f, thumpLow = 0.0f, thumpPhase = 0.0f;
  float growlHi = 0.0f, growlLo = 0.0f;
  float brightHi = 0.0f, brightHi2 = 0.0f, brightLo = 0.0f, brightLo2 = 0.0f, mechEnv = 0.0f; // mid-high noise and the ticks of the valve train
  Resonator starterBody;         // the metal ringing
  float intakeLow = 0.0f, intakeLow2 = 0.0f, mechLow = 0.0f;

  float noise();                // white, -1..1
  void makeResonator(Resonator &r, float freq, float q);

public:
  EngineSynth();
  void setRpm(float value) { targetRpm.store(value); }
  void setLoad(float value) { targetLoad.store(value); }
  void setStarter(float value) { targetStarter.store(value); }
  void setFire(float value) { targetFire.store(value); }

  unsigned int sampleRate() const override { return RATE; }
  void generate(float *out, size_t frames) override;
};

#endif
