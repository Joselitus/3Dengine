#ifndef ENGINE_SIMULATOR
#define ENGINE_SIMULATOR

#include <cstdint>

// The speed of a car's engine and its starting (no OpenGL, no audio): an
// automatic gearbox, a flywheel and the starter. Each frame it gets the road
// speed and the throttle and gives the revolutions per minute.
//
// start() begins what an old, tired engine does when the key is turned: a
// pause (the key, the solenoid), the starter cranking the engine slowly on a
// weak battery, maybe a few attempts that sputter and die (a random number of
// them), and at last the engine catches, flares up and hunts around the idle
// for some seconds before it settles. isRunning() tells that it has caught
// (from then on the vehicle can use it). With no fuel it never catches: after
// the attempts it gives up (hasGivenUp()).
//
// Running, it idles at IDLE_RPM, climbs when the driver accelerates (it can rev
// freely while the vehicle is slow, the clutch slipping), follows the wheels
// in gear (so it rises with speed and falls at each upshift) and, stopped with
// stop(), spins down to 0. getLoad() is how hard it is working (0..1).
// getStarter() (0..1) is how engaged the starter motor is and getFire() (0..1)
// the share of firings that happen (0 cranking, less while it sputters), for
// the sound.
class EngineSimulator {
public:
  static constexpr float IDLE_RPM = 600.0f;
  static constexpr float REDLINE_RPM = 4200.0f;

  enum class Phase { Off, KeyDelay, Cranking, Sputter, Pause, Catching, Running, Stalling };

private:
  Phase phase = Phase::Off;
  float phaseTime = 0.0f;
  float phaseLength = 0.0f;
  int attempt = 0;          // 0-based, of this start
  int failures = 0;         // attempts that will fail before one works
  bool failAll = false;     // no fuel
  bool gaveUp = false;
  float rpm = 0.0f;
  float load = 0.0f;
  float starter = 0.0f;
  float fire = 0.0f;
  float dip = 0.0f;         // a stumble of the idle just now, 0..1
  float hunt = 0.0f;        // phase of the idle's hunting
  int gear = 0;             // index into the gear table (reverse uses the first one)
  uint32_t seed = 2463534242u;

  float random01();
  float random(float from, float to) { return from + (to - from) * random01(); }
  void enter(Phase next, float length);
  void updateStart(double dt, bool fuel);
  void updateDriving(double dt, float speed, float throttle);

public:
  // Turns the key: the starting sequence begins (from Off only)
  void start(bool fuel);
  // Switches the engine off
  void stop();
  // fuel: there is some left. speed: m/s along the vehicle (negative =
  // reversing). throttle: 0..1 (the pedal, in either direction).
  void update(double dt, bool fuel, float speed, float throttle);

  Phase getPhase() const { return phase; }
  // The engine has caught and works (it may still be stumbling)
  bool isRunning() const { return phase == Phase::Catching || phase == Phase::Running; }
  // The key is turned: starting, running or about to stall
  bool isStarting() const {
    return phase == Phase::KeyDelay || phase == Phase::Cranking || phase == Phase::Sputter ||
           phase == Phase::Pause;
  }
  // Every attempt failed (no fuel): the driver let the key go. Read once (it resets).
  bool hasGivenUp() { bool g = gaveUp; gaveUp = false; return g; }
  float getRpm() const { return rpm; }
  float getLoad() const { return load; }
  float getStarter() const { return starter; }
  float getFire() const { return fire; }
  int getGear() const { return gear + 1; }
  void reset() { *this = EngineSimulator(); }
};

#endif
