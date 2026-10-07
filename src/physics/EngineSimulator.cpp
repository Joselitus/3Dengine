#include "EngineSimulator.h"

#include <algorithm>
#include <cmath>

constexpr float EngineSimulator::IDLE_RPM;
constexpr float EngineSimulator::REDLINE_RPM;

namespace {
// rpm per m/s of road speed in each gear: the RV tops out at 20 m/s, so the
// last gear is ~2700 rpm at full speed
const float RATIOS[] = {500.0f, 270.0f, 186.0f, 135.0f};
const int GEARS = 4;
const float UPSHIFT_RPM = 3000.0f;
const float DOWNSHIFT_RPM = 1200.0f;
const float REV_FREE = 0.5f;     // fraction of the rev range reached revving with the clutch slipping
const float RISE_RATE = 2500.0f; // rpm/s climbing, at full throttle
const float FALL_RATE = 1600.0f; // rpm/s dropping with the throttle released
const float STOP_RATE = 1100.0f; // rpm/s spinning down when switched off

// the starting sequence
const float CRANK_RPM = 170.0f;  // the starter on a weak battery
const float FLARE_RPM = 1250.0f; // the engine catches and shoots up
const float CATCH_TIME = 3.6f;   // stumbling around the idle after it catches
}

float EngineSimulator::random01() {
  seed = seed * 1664525u + 1013904223u;
  return (float)(seed >> 8) / 16777216.0f;
}

void EngineSimulator::enter(Phase next, float length) {
  phase = next;
  phaseTime = 0.0f;
  phaseLength = length;
}

void EngineSimulator::start(bool fuel) {
  if (phase != Phase::Off)
    return;
  attempt = 0;
  gaveUp = false;
  failAll = !fuel;
  // an old engine: often it does not catch the first time
  float r = random01();
  failures = r < 0.35f ? 0 : (r < 0.75f ? 1 : 2);
  // the key, the solenoid (a recording already has its pause)
  enter(Phase::KeyDelay, crankTime > 0.0f ? 0.0f : random(0.45f, 0.8f));
}

float EngineSimulator::crankLength(bool fails) {
  if (crankTime > 0.0f)
    return crankTime;
  return fails ? random(1.6f, 2.6f) : random(1.1f, 2.0f);
}

// A failed attempt is over: the driver tries again after a pause, or gives up
void EngineSimulator::endAttempt() {
  if (failAll && attempt >= 2) { // enough: the driver lets the key go
    gaveUp = true;
    stop();
  } else {
    enter(Phase::Pause, random(0.7f, 1.3f));
  }
}

void EngineSimulator::stop() {
  phase = Phase::Off;
  starter = 0.0f;
  fire = 0.0f;
}

void EngineSimulator::updateStart(double dt, bool fuel) {
  float dtf = (float)dt;
  phaseTime += dtf;
  float t = phaseTime;
  // the battery gets weaker with each attempt
  float battery = 1.0f - 0.14f * attempt;
  switch (phase) {
  case Phase::KeyDelay:
    rpm = 0.0f;
    starter = 0.0f;
    fire = 0.0f;
    if (t >= phaseLength) {
      bool fails = failAll || attempt < failures;
      enter(Phase::Cranking, crankLength(fails));
    }
    break;
  case Phase::Cranking: {
    starter = 1.0f;
    fire = 0.0f;
    // spins up in a moment, then sags as the battery tires
    float spin = std::min(1.0f, t / 0.3f);
    float sag = 1.0f - 0.18f * t / phaseLength;
    rpm = CRANK_RPM * battery * spin * sag * (1.0f + 0.04f * (random01() - 0.5f));
    if (t >= phaseLength) {
      bool fails = failAll || attempt < failures;
      if (fails && crankTime > 0.0f)
        endAttempt(); // the recording has no sputter
      else if (fails)
        enter(Phase::Sputter, random(0.5f, 0.9f)); // it almost starts and dies
      else
        enter(Phase::Catching, CATCH_TIME);
    }
    break;
  }
  case Phase::Sputter: {
    starter = 1.0f;
    // a few weak, uneven firings that die out
    float left = 1.0f - t / phaseLength;
    fire = failAll ? 0.0f : std::max(0.0f, 0.55f * left * (0.4f + random01()));
    rpm = CRANK_RPM * battery * (1.0f + 0.5f * fire * random01());
    if (t >= phaseLength)
      endAttempt();
    break;
  }
  case Phase::Pause:
    starter = 0.0f;
    fire = 0.0f;
    rpm = std::max(0.0f, rpm - 400.0f * dtf);
    if (t >= phaseLength) {
      attempt++;
      bool fails = failAll || attempt < failures;
      enter(Phase::Cranking, crankLength(fails));
    }
    break;
  default:
    break;
  }
}

void EngineSimulator::updateDriving(double dt, float speed, float throttle) {
  float dtf = (float)dt;
  float v = std::fabs(speed);
  // automatic gearbox (reversing stays in first)
  if (speed < -0.5f) {
    gear = 0;
  } else {
    if (gear < GEARS - 1 && v * RATIOS[gear] > UPSHIFT_RPM)
      gear++;
    while (gear > 0 && v * RATIOS[gear] < DOWNSHIFT_RPM)
      gear--;
  }

  float wheelRpm = v * RATIOS[gear];
  float freeRpm = IDLE_RPM + throttle * (REDLINE_RPM * REV_FREE - IDLE_RPM);
  // pushing the pedal while slow revs the engine up beyond the wheels (slipping clutch)
  float target = std::max(wheelRpm, freeRpm);
  // the engine labours: a push on the pedal raises it a little over the wheels
  target += throttle * 180.0f;

  float rate = target > rpm ? RISE_RATE * (0.3f + 0.7f * throttle) : FALL_RATE;

  if (phase == Phase::Catching) {
    // it catches and flares up, then falls back hunting around the idle, stumbling now and then
    float k = phaseTime / phaseLength; // 0..1
    float settle = std::exp(-phaseTime * 1.1f);
    hunt += dtf * 5.0f;
    float hunting = (1.0f - k) * 55.0f * std::sin(hunt) * (0.6f + 0.4f * std::sin(hunt * 0.37f));
    float idleTarget = IDLE_RPM + (FLARE_RPM - IDLE_RPM) * settle + hunting;
    // a stumble: the revs sag for a moment
    if (dip <= 0.0f && random01() < 0.9f * dtf && k < 0.9f)
      dip = 1.0f;
    dip = std::max(0.0f, dip - dtf * 3.5f);
    idleTarget -= 90.0f * dip * (1.0f - k);
    target = std::max(target, idleTarget);
    rate = std::max(rate, 3500.0f); // the first flare is quick
    // the starter lets go once it has caught: its overrun is heard as it drops out
    starter = phaseTime < 0.35f ? 1.0f : 0.0f;
    fire = std::min(1.0f, 0.5f + phaseTime * 1.5f) - 0.6f * dip * (1.0f - k);
    fire = std::max(0.25f, fire);
  } else {
    fire = 1.0f;
    starter = 0.0f;
    target = std::max(target, IDLE_RPM);
  }
  target = std::min(std::max(target, 0.0f), REDLINE_RPM);

  if (rpm < IDLE_RPM * 0.9f && phase != Phase::Catching)
    rate = std::max(rate, 2200.0f);
  if (target > rpm)
    rpm = std::min(target, rpm + rate * dtf);
  else
    rpm = std::max(target, rpm - rate * dtf);

  float revFraction = (rpm - IDLE_RPM) / (REDLINE_RPM - IDLE_RPM);
  load = std::min(1.0f, throttle * (0.5f + 0.5f * std::max(revFraction, 0.0f)));
}

void EngineSimulator::update(double dt, bool fuel, float speed, float throttle) {
  float dtf = (float)dt;
  throttle = std::min(std::max(throttle, 0.0f), 1.0f);
  switch (phase) {
  case Phase::Off:
    rpm = std::max(0.0f, rpm - STOP_RATE * dtf);
    load = 0.0f;
    starter = 0.0f;
    fire = 0.0f;
    gear = 0;
    return;
  case Phase::KeyDelay:
  case Phase::Cranking:
  case Phase::Sputter:
  case Phase::Pause:
    load = 0.0f;
    updateStart(dt, fuel);
    return;
  case Phase::Stalling:
    // out of fuel while running: it coughs and dies
    phaseTime += dtf;
    starter = 0.0f;
    load = 0.0f;
    fire = std::max(0.0f, 0.5f * (1.0f - phaseTime / phaseLength)) * random01();
    rpm = std::max(0.0f, rpm - 700.0f * dtf);
    if (phaseTime >= phaseLength)
      stop();
    return;
  case Phase::Catching:
  case Phase::Running:
    if (!fuel) {
      enter(Phase::Stalling, 1.2f);
      return;
    }
    if (phase == Phase::Catching) {
      phaseTime += dtf;
      if (phaseTime >= phaseLength)
        enter(Phase::Running, 0.0f);
    }
    updateDriving(dt, speed, throttle);
    break;
  }
}
