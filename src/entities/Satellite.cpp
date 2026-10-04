#include "Satellite.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>

#include "UIButton.h"
#include "UILabel.h"
#include "UIPanel.h"
#include "UIRow.h"
#include "UISlider.h"

using namespace std;
using namespace glm;

// Proportions of the post
#define POST_HEIGHT 0.8f
#define POST_WIDTH 0.18f
// Below this difference (degrees) an axis counts as on target
#define ON_TARGET 0.01f

// Angle in [0, 360)
static float wrap360(float degrees) {
  degrees = fmodf(degrees, 360.0f);
  return degrees < 0.0f ? degrees + 360.0f : degrees;
}

// Signed difference `to - from` the short way round, in [-180, 180)
static float shortestTurn(float from, float to) {
  return wrap360(to - from + 180.0f) - 180.0f;
}

// Moves `value` towards `value + difference`, no more than maxStep
static float approach(float value, float difference, float maxStep) {
  return value + fmaxf(-maxStep, fminf(maxStep, difference));
}

static string format(const char *pattern, ...) {
  char text[128];
  va_list args;
  va_start(args, pattern);
  vsnprintf(text, sizeof(text), pattern, args);
  va_end(args);
  return text;
}

Satellite::Satellite(shared_ptr<Model> cube, vec3 ground, float size)
    : GameObject(cube), mount(make_shared<GameObject>(cube)) {
  mount->setPosition(ground.x, ground.y + POST_HEIGHT / 2, ground.z);
  // A non-uniform scale fits in the rotation matrix
  mount->setRotation(glm::scale(mat4(1.0f), vec3(POST_WIDTH, POST_HEIGHT,
                                                 POST_WIDTH)));
  setScale(size);
  // High enough that the tilted head never sinks into the post
  base = ground + vec3(0.0f, POST_HEIGHT + size * 0.75f, 0.0f);
  setPosition(base.x, base.y, base.z);
  applyOrientation();
}

void Satellite::pointAt(float azimuth, float zenith) {
  targetAzimuth = wrap360(azimuth);
  targetZenith = fmaxf(0.0f, fminf(MAX_ZENITH, zenith));
}

void Satellite::setSlewRate(float degreesPerSecond) {
  slewRate = fmaxf(MIN_SLEW_RATE, fminf(MAX_SLEW_RATE, degreesPerSecond));
}

bool Satellite::isSlewing() const {
  return fabsf(shortestTurn(azimuth, targetAzimuth)) > ON_TARGET ||
         fabsf(targetZenith - zenith) > ON_TARGET;
}

vec3 Satellite::getPointing() const {
  float a = radians(azimuth), z = radians(zenith);
  return vec3(sinf(z) * sinf(a), cosf(z), -sinf(z) * cosf(a));
}

void Satellite::update(double dt) {
  GameObject::update(dt);
  float maxStep = slewRate * (float)dt;
  azimuth = wrap360(
      approach(azimuth, shortestTurn(azimuth, targetAzimuth), maxStep));
  zenith = approach(zenith, targetZenith - zenith, maxStep);
  applyOrientation();
}

// The head starts with its boresight (+y) up. Tilting it by the zenith
// around x brings it down towards north (-z); turning that by the azimuth
// around y (clockwise from above, hence the minus) gives the heading.
// See getPointing() for the resulting vector.
void Satellite::applyOrientation() {
  mat4 m = rotate(mat4(1.0f), -radians(azimuth), vec3(0.0f, 1.0f, 0.0f));
  setRotation(rotate(m, -radians(zenith), vec3(1.0f, 0.0f, 0.0f)));
}

void Satellite::buildInterface(UIPanel &panel) {
  panel.add(new UILabel(
      [this]() { return format("Azimut actual: %.1f deg", azimuth); }));
  panel.add(new UILabel(
      [this]() { return format("Cenit actual:  %.1f deg", zenith); }));
  panel.add(new UILabel(
      [this]() { return format("Elevacion: %.1f deg", getElevation()); },
      UITheme::MUTED));
  panel.add(new UILabel(
      [this]() {
        // + 0.0f turns -0.0 (shown as "-0.00") into 0.0
        vec3 p = round(getPointing() * 100.0f) / 100.0f + 0.0f;
        return format("Direccion: (%.2f, %.2f, %.2f)", p.x, p.y, p.z);
      },
      UITheme::MUTED));
  panel.add(new UILabel(
      [this]() {
        return string("Estado: ") +
               (isSlewing() ? "girando" : "apuntando");
      },
      UITheme::ACCENT));

  panel.add(new UISlider(
      "Azimut objetivo", 0.0f, 360.0f, 0.5f,
      [this]() { return targetAzimuth; },
      [this](float v) { setTargetAzimuth(v == 360.0f ? 0.0f : v); }, "deg"));
  panel.add(new UISlider(
      "Cenit objetivo", 0.0f, MAX_ZENITH, 0.5f,
      [this]() { return targetZenith; },
      [this](float v) { setTargetZenith(v); }, "deg"));
  panel.add(new UISlider(
      "Velocidad de giro", MIN_SLEW_RATE, MAX_SLEW_RATE, 1.0f,
      [this]() { return slewRate; }, [this](float v) { setSlewRate(v); },
      "deg/s"));

  UIRow *presets = panel.add(new UIRow());
  presets->add(new UIButton("Cenit", [this]() { pointAt(targetAzimuth, 0); }));
  presets->add(new UIButton("Norte 45", [this]() { pointAt(0, 45); }));
  presets->add(new UIButton("Parar", [this]() { stop(); }));

  panel.add(new UILabel("Norte = -z. Esc: cerrar", UITheme::MUTED));
}

void Satellite::teleport(const vec3 &position) {
  vec3 delta = position - this->position;
  GameObject::teleport(position);
  base += delta;
  mount->translate(delta);
}

void Satellite::turn(float radians) {
  // The azimuth goes clockwise seen from above
  float degrees = glm::degrees(radians);
  azimuth = wrap360(azimuth - degrees);
  targetAzimuth = wrap360(targetAzimuth - degrees);
  applyOrientation();
}

float Satellite::getHeading() const { return -glm::radians(azimuth); }
