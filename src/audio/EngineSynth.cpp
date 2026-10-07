#include "EngineSynth.h"

#include <algorithm>
#include <cmath>

namespace {
const float PI = 3.14159265f;
const float SPEED_OF_SOUND = 343.0f;
const float PIPE_LENGTH[3] = {1.5f, 1.8f, 3.4f}; // m: the two banks' headers and the common tailpipe
const float PIPE_REFLECTION = 0.62f;       // inverted at the open end
const float PIPE_DAMPING = 0.30f;          // in-loop low-pass (smaller = darker)
const float TAIL_REFLECTION = 0.45f;       // the tailpipe's open end
const float REDLINE = 4200.0f;
// 1-8-4-3-6-5-7-2: the cylinder (0-based) firing at each 90 degrees
const int FIRING_ORDER[8] = {0, 7, 3, 2, 5, 4, 6, 1};
// odd cylinders (1,3,5,7) are one bank and even ones the other
inline int bankOf(int cylinder) { return cylinder % 2; }

inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }
}

EngineSynth::EngineSynth() {
  for (int i = 0; i < 8; i++) {
    cylinderAmp[i] = 1.0f;
    cylinderAngle[i] = 1000.0f; // not firing
  }
  for (int b = 0; b < 3; b++) {
    for (int i = 0; i < PIPE_SIZE; i++)
      pipe[b][i] = 0.0f;
    pipeDamp[b] = 0.0f;
    // the wave goes to the end of the pipe and back
    pipeDelay[b] = std::min(PIPE_SIZE - 1, (int)(2.0f * PIPE_LENGTH[b] / SPEED_OF_SOUND * RATE));
  }
  // The silencer's low resonances (a big truck muffler)
  makeResonator(silencer[0], 42.0f, 2.5f);
  makeResonator(silencer[1], 85.0f, 2.5f);
  makeResonator(silencer[2], 165.0f, 2.0f);
  for (int i = 0; i < 3; i++)
    cutoffState[i] = 0.0f;
  makeResonator(starterBody, 620.0f, 4.0f);
}

float EngineSynth::noise() {
  seed = seed * 1664525u + 1013904223u;
  return (float)(int32_t)seed / 2147483648.0f;
}

// A band-pass resonator (RBJ biquad, constant 0 dB peak)
void EngineSynth::makeResonator(Resonator &r, float freq, float q) {
  float w = 2.0f * PI * freq / RATE;
  float alpha = std::sin(w) / (2.0f * q);
  float a0 = 1.0f + alpha;
  r.b0 = alpha / a0;
  r.b2 = -alpha / a0;
  r.a1 = -2.0f * std::cos(w) / a0;
  r.a2 = (1.0f - alpha) / a0;
  r.z1 = r.z2 = 0.0f;
}

void EngineSynth::generate(float *out, size_t frames) {
  const float rpmGoal = targetRpm.load();
  const float loadGoal = targetLoad.load();
  const float starterGoal = targetStarter.load();
  const float fireGoal = targetFire.load();
  const float follow = 1.0f - std::exp(-1.0f / (0.03f * RATE));      // rpm: ~30 ms
  const float loadFollow = 1.0f - std::exp(-1.0f / (0.08f * RATE));  // load: ~80 ms

  for (size_t n = 0; n < frames; n++) {
    rpm += (rpmGoal - rpm) * follow;
    load += (loadGoal - load) * loadFollow;
    float starterBefore = starter;
    starter += (starterGoal - starter) * 0.01f; // (the solenoid is quick)
    fire = fireGoal;
    // the solenoid pulls in and lets go
    if (starterBefore < 0.5f && starter >= 0.5f) {
      clickEnv = 1.0f;
      clunkEnv = 1.0f;
    } else if (starterBefore >= 0.5f && starter < 0.5f) {
      clickEnv = 0.6f;
      clunkEnv = 0.4f;
    }
    if (rpm < 20.0f && starter < 0.01f && clickEnv < 0.001f) { // stopped: silence (and the pipes empty out)
      out[n] = 0.0f;
      pipeWrite = (pipeWrite + 1) % PIPE_SIZE;
      for (int b = 0; b < 3; b++)
        pipe[b][pipeWrite] = 0.0f;
      continue;
    }
    float revs = clampf((rpm - 500.0f) / (REDLINE - 500.0f), 0.0f, 1.0f);   // 0 idle .. 1 redline
    float effort = clampf(0.55f + 0.45f * load, 0.0f, 1.0f);      // how hard each firing is

    // --- the crankshaft, with its irregularity (it speeds up and slows down a little)
    wobble += (noise() * 0.5f - wobble) * 0.0008f;
    // cranking drags at each compression (the starter slows down) and a worn engine is uneven
    float strain = starter * (1.0f - fire);
    float compression = 0.5f + 0.5f * std::cos(2.0f * PI * std::fmod(cycle, 90.0f) / 90.0f);
    float degPerSample = rpm / 60.0f * 360.0f / RATE * (1.0f + 0.35f * wobble * (1.2f - revs)) *
                         (1.0f - 0.30f * strain * compression); // (the starter has inertia: it only ripples)
    float before = cycle;
    cycle += degPerSample;
    if (cycle >= 720.0f)
      cycle -= 720.0f;
    for (int c = 0; c < 8; c++)
      cylinderAngle[c] += degPerSample;
    // a cylinder fires every time the cycle passes one of its 90 degree marks
    int markBefore = (int)(before / 90.0f), markNow = (int)(cycle / 90.0f);
    if (markNow != markBefore) {
      int c = FIRING_ORDER[markNow];
      cylinderAngle[c] = 0.0f;
      mechEnv = 1.0f;
      // each firing a little different; the idle lopes more
      float lope = 0.40f * (1.0f - revs) + 0.07f; // (a big cam)
      cylinderAmp[c] = 1.0f + lope * noise() * 1.4f;
      // it does not fire: only a weak chuff of air (the compression) goes out
      if ((noise() * 0.5f + 0.5f) > fire)
        cylinderAmp[c] = 2.4f;
    }

    // --- the exhaust pulses of each bank: a fast rise and a slower fall, fixed in crank angle
    float width = 48.0f - 22.0f * effort; // degrees: sharper with load
    float bankIn[2] = {0.0f, 0.0f};
    for (int c = 0; c < 8; c++) {
      float a = cylinderAngle[c];
      if (a > 6.0f * width)
        continue;
      float x = a / width;
      bankIn[bankOf(c)] += cylinderAmp[c] * x * std::exp(1.0f - x);
    }

    // --- the pipes: y = in - reflection * lowpass(y delayed)
    pipeWrite = (pipeWrite + 1) % PIPE_SIZE;
    float collector = 0.0f;
    for (int b = 0; b < 2; b++) {
      int read = (pipeWrite - pipeDelay[b] + PIPE_SIZE) % PIPE_SIZE;
      pipeDamp[b] += (pipe[b][read] - pipeDamp[b]) * PIPE_DAMPING;
      float y = bankIn[b] * effort - PIPE_REFLECTION * pipeDamp[b];
      pipe[b][pipeWrite] = y;
      collector += y;
    }
    // both banks join in one tailpipe
    {
      int read = (pipeWrite - pipeDelay[2] + PIPE_SIZE) % PIPE_SIZE;
      pipeDamp[2] += (pipe[2][read] - pipeDamp[2]) * PIPE_DAMPING;
      float y = collector - TAIL_REFLECTION * pipeDamp[2];
      pipe[2][pipeWrite] = y;
      collector = 0.5f * (collector + y);
    }

    // --- the silencer: deep resonances added to the collector's sound
    float resonant = 0.0f;
    static const float RESONANCE_GAIN[3] = {1.9f, 1.5f, 0.8f};
    for (int i = 0; i < 3; i++) {
      Resonator &r = silencer[i];
      float y = r.b0 * collector + r.z1;
      r.z1 = r.z2 - r.a1 * y;           // (b1 = 0)
      r.z2 = r.b2 * collector - r.a2 * y;
      resonant += RESONANCE_GAIN[i] * y;
    }
    float sound = 0.7f * collector + resonant;

    // the low-pass opens with the load and the revs: dull at idle, roaring flat out
    float cutoff = 230.0f + 150.0f * starter + 1400.0f * load + 500.0f * revs;
    float k = 1.0f - std::exp(-2.0f * PI * cutoff / RATE);
    for (int i = 0; i < 3; i++) {
      cutoffState[i] += (sound - cutoffState[i]) * k;
      sound = cutoffState[i];
    }

    // --- intake and mechanical noise: low rumble that follows the load and the firing
    float white = noise();
    float firingBeat = 0.55f + 0.45f * std::sin(cycle * PI / 90.0f * 1.0f); // 4 per revolution
    intakeLow += (white - intakeLow) * 0.06f;
    intakeLow2 += (intakeLow - intakeLow2) * 0.06f;
    mechLow += (white - mechLow) * 0.22f;
    sound += (0.9f * intakeLow2 * (0.2f + 0.8f * load) * (0.3f + 0.7f * revs) * firingBeat) * 5.5f;
    sound += 0.05f * (mechLow - 0.2f * intakeLow) * (0.4f + 0.6f * revs);


    // noise between ~0.8 and 2.5 kHz: the mechanical sound of an old engine and the starter's whirr
    float rough = noise();
    brightHi += (rough - brightHi) * 0.22f;
    brightHi2 += (brightHi - brightHi2) * 0.22f;
    brightLo += (rough - brightLo) * 0.13f;
    brightLo2 += (brightLo - brightLo2) * 0.13f;
    // and a low band (100-300 Hz): the engine block's growl
    growlHi += (rough - growlHi) * 0.040f;
    growlLo += (rough - growlLo) * 0.014f;
    float growlBand = (growlHi - growlLo) * 9.0f;
    float brightBand = (brightHi2 - brightLo2) * 4.0f;

    // --- the starter motor. Cranking is mostly the engine's own rhythm: a dull thump at each
    // compression (4 per crank turn on a V8) that makes the crank speed ripple, with the
    // starter's whirr riding on it (its pitch dips at every compression) and the ring gear's tone
    float starterSound = 0.0f;
    float engaged = 1.0f - std::min(std::max((rpm - 260.0f) / 160.0f, 0.0f), 1.0f); // it drops out once the engine runs
    if (markNow != markBefore && starter > 0.5f)
      thumpEnv = 1.0f;
    {
      float crankRevs = std::min(degPerSample * RATE / 360.0f, 6.0f); // turns per second, ripple included
      motorSpeed += (crankRevs - motorSpeed) * 0.004f;               // ~6 ms: follows the dips
      motorPhase += 2.0f * PI * motorSpeed * 190.0f / RATE;          // armature whirr: ~500 Hz at 2.7 turns/s
      gearPhase += 2.0f * PI * motorSpeed * 130.0f / RATE;           // 130 flywheel teeth
      if (motorPhase > 2.0f * PI * 64.0f) motorPhase -= 2.0f * PI * 64.0f;
      if (gearPhase > 2.0f * PI * 64.0f) gearPhase -= 2.0f * PI * 64.0f;
      float whirr = std::sin(motorPhase) + 0.45f * std::sin(2.0f * motorPhase) +
                    0.2f * std::sin(3.0f * motorPhase);
      float gear = std::sin(gearPhase) + 0.3f * std::sin(2.0f * gearPhase);
      // the whirr gets louder and deeper as the compression loads the motor
      float loadAmp = 0.75f + 0.5f * compression;
      // the thump of each compression: low-passed noise and a short low sine
      thumpEnv *= 0.9993f;
      thumpLow += (rough - thumpLow) * 0.045f;
      thumpPhase += 2.0f * PI * 78.0f / RATE;
      if (thumpPhase > 2.0f * PI) thumpPhase -= 2.0f * PI;
      float thump = thumpEnv * (2.2f * thumpLow + 0.9f * std::sin(thumpPhase));
      // soft brush noise, not a hiss
      float body = 0.30f * loadAmp * whirr + 0.12f * gear + 1.3f * thump + 0.25f * brightBand;
      Resonator &r = starterBody; // the starter's casing
      float y = r.b0 * body + r.z1;
      r.z1 = r.z2 - r.a1 * y;
      r.z2 = r.b2 * body - r.a2 * y;
      starterSound = starter * engaged * (body + 0.5f * y);
      starterLow += (starterSound - starterLow) * 0.5f;
      starterSound = starterLow;
    }
    // the engine block's growl while it is turned over (only with the starter engaged)
    mechEnv *= 0.9965f;
    sound += growlBand * (0.5f + 1.5f * mechEnv) * std::min(1.0f, rpm / 250.0f) * 0.9f * starter;
    // solenoid: a sharp click and a low clunk
    clickEnv *= 0.985f;
    clunkEnv *= 0.9992f;
    clunkPhase += 2.0f * PI * 85.0f / RATE;
    if (clunkPhase > 2.0f * PI) clunkPhase -= 2.0f * PI;
    starterSound += 0.9f * clickEnv * noise() + 0.8f * clunkEnv * std::sin(clunkPhase);
    sound += 1.3f * starterSound;

    // --- block the DC the pulses carry, then saturate a little
    float dc = sound - dcX + 0.997f * dcY;
    dcX = sound;
    dcY = dc;
    out[n] = std::tanh(0.8f * dc) * 0.9f;
  }
}
