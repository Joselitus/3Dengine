#include "InsectLegs.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

using namespace glm;
using namespace std;

InsectLegs::InsectLegs(const Pose &bindLegs, const InsectLegParams &p)
    : bind(bindLegs), target(bindLegs), params(p) {
  points = previous = bind;
  int base = 0;
  for (size_t l = 0; l < bind.size(); l++) {
    vector<float> legLengths;
    for (size_t j = 0; j + 1 < bind[l].size(); j++) {
      legLengths.push_back(length(bind[l][j + 1] - bind[l][j]));
      bones.push_back(RigBone("leg" + to_string(l) + "_" + to_string(j), base + (int)j,
                              base + (int)j + 1));
    }
    lengths.push_back(legLengths);
    base += (int)bind[l].size();
  }
  flatten(bind, bindFlat);
}

void InsectLegs::flatten(const vector<vector<vec3>> &from, vector<vec3> &to) const {
  to.clear();
  for (const auto &leg : from)
    to.insert(to.end(), leg.begin(), leg.end());
}

void InsectLegs::setBody(const mat4 &bodyToWorld) {
  lastBody = started ? body : bodyToWorld;
  body = bodyToWorld;
}

void InsectLegs::step(double dt) {
  if (!started) { // at rest where the pose wants them
    for (size_t l = 0; l < target.size(); l++)
      for (size_t j = 0; j < target[l].size(); j++)
        points[l][j] = previous[l][j] = vec3(body * vec4(target[l][j], 1.0f));
    started = true;
    lastBody = body;
    return;
  }
  // (a scaled body has scaled legs)
  float scale = length(vec3(body[0]));
  int n = std::max(1, std::min(60, (int)std::ceil(dt / params.subStep)));
  float h = (float)dt / n;
  float keep = std::max(0.0f, 1.0f - params.damping * h);
  for (int k = 1; k <= n; k++) {
    // the body moves smoothly through the frame (its matrix interpolated)
    float t = (float)k / n;
    mat4 B = lastBody * (1.0f - t) + body * t;
    for (size_t l = 0; l < points.size(); l++) {
      vector<vec3> &P = points[l], &Q = previous[l];
      P[0] = Q[0] = vec3(B * vec4(target[l][0], 1.0f)); // the hip goes with the body
      for (size_t j = 1; j < P.size(); j++) {
        vec3 goal = vec3(B * vec4(target[l][j], 1.0f));
        float k2 = params.stiffness[std::min<size_t>(j - 1, 3)];
        vec3 accel = k2 * (goal - P[j]) - vec3(0.0f, params.gravity, 0.0f);
        vec3 now = P[j];
        P[j] += (P[j] - Q[j]) * keep + accel * h * h;
        Q[j] = now;
      }
      // the segments keep their lengths (the hip does not give)
      for (int it = 0; it < params.iterations; it++) {
        for (size_t j = 0; j + 1 < P.size(); j++) {
          vec3 d = P[j + 1] - P[j];
          float len = length(d);
          if (len < 1e-6f)
            continue;
          vec3 fix = d * ((len - lengths[l][j] * scale) / len);
          if (j == 0) {
            P[j + 1] -= fix;
          } else {
            P[j] += fix * 0.5f;
            P[j + 1] -= fix * 0.5f;
          }
        }
      }
      // and stay above the floor
      if (floor) {
        for (size_t j = 1; j < P.size(); j++) {
          float ground;
          if (floor(P[j].x, P[j].z, ground) && P[j].y < ground + params.footClearance * scale) {
            P[j].y = ground + params.footClearance * scale;
            Q[j].y = P[j].y;
          }
        }
      }
    }
  }
}

void InsectLegs::boneGlobals(const vec3 &origin, map<string, mat4> &out) const {
  vector<vec3> flat;
  flatten(points, flat);
  rigBoneGlobals(bindFlat, bones, flat, origin, out);
}

void InsectLegs::segmentTransforms(vector<mat4> &out) const {
  // the joints in the body's frame
  mat4 toBody = inverse(body);
  vector<vec3> flat;
  for (const auto &leg : points)
    for (const vec3 &p : leg)
      flat.push_back(vec3(toBody * vec4(p, 1.0f)));
  map<string, mat4> globals;
  rigBoneGlobals(bindFlat, bones, flat, vec3(0.0f), globals);
  out.clear();
  for (const RigBone &b : bones) // (a model made at its bind place: from there)
    out.push_back(globals[b.name] * translate(mat4(1.0f), -bindFlat[b.from]));
}
