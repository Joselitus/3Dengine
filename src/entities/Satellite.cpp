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

// The hinge the dish tilts on, in the frame of antenna_base.obj (printed by
// assets/antenna/split_antenna.py): on the base's south edge (+z), at the
// dish's lowest point
static const vec3 HINGE(0.0f, 0.203f, 0.780f);
// The zenith the dish was modelled at (also printed by split_antenna.py): it
// starts like that
#define MODELLED_ZENITH 45.0f
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

Satellite::Satellite(shared_ptr<Model> dish, shared_ptr<Model> base,
                     vec3 ground, float size)
    : GameObject(dish), mount(make_shared<GameObject>(base)), ground(ground),
      size(size) {
  setScale(size);
  mount->setScale(size);
  zenith = targetZenith = MODELLED_ZENITH; // as it was modelled
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
// around x (its hinge) brings it down towards north (-z); turning that by the
// azimuth around y (clockwise from above, hence the minus) gives the heading.
// See getPointing() for the resulting vector. The base turns by the azimuth
// too, and carries the hinge round with it.
void Satellite::applyOrientation() {
  mat4 turn = rotate(mat4(1.0f), -radians(azimuth), vec3(0.0f, 1.0f, 0.0f));
  mount->setPosition(ground.x, ground.y, ground.z);
  mount->setRotation(turn);
  vec3 hinge = ground + vec3(turn * vec4(HINGE * size, 0.0f));
  setPosition(hinge.x, hinge.y, hinge.z);
  setRotation(rotate(turn, -radians(zenith), vec3(1.0f, 0.0f, 0.0f)));
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

  panel.add(new UILabel("Norte = -z", UITheme::MUTED));
}

void Satellite::teleport(const vec3 &position) {
  // `position` is where the head (its hinge) goes: the base goes as far
  ground += position - this->position;
  GameObject::teleport(position);
  applyOrientation();
}

void Satellite::turn(float radians) {
  // The azimuth goes clockwise seen from above
  float degrees = glm::degrees(radians);
  azimuth = wrap360(azimuth - degrees);
  targetAzimuth = wrap360(targetAzimuth - degrees);
  applyOrientation();
}

float Satellite::getHeading() const { return -glm::radians(azimuth); }

void Satellite::getProperties(std::vector<Property> &properties) {
  GameObject::getProperties(properties);
  properties.push_back(Property::info("Apunta a", [this]() {
    return format("azimut %.1f, cenit %.1f grados", azimuth, zenith);
  }));
  properties.push_back(Property::number(
      "Azimut objetivo", 0.0f, 360.0f, 1.0f, [this]() { return targetAzimuth; },
      [this](float v) { setTargetAzimuth(v); }, "grados"));
  properties.push_back(Property::number(
      "Cenit objetivo", 0.0f, MAX_ZENITH, 1.0f, [this]() { return targetZenith; },
      [this](float v) { setTargetZenith(v); }, "grados"));
  properties.push_back(Property::number(
      "Velocidad de giro", MIN_SLEW_RATE, MAX_SLEW_RATE, 1.0f,
      [this]() { return slewRate; }, [this](float v) { setSlewRate(v); }, "grados/s"));
}
