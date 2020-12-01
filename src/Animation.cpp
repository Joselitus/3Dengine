#include "Animation.h"

static float TimeToFrame(float time) {
     float frame = FRAME;
     return time / frame;
}


static glm::vec2 FramesToTime(glm::vec2 frames) {
     float frame = FRAME;
     return frames * frame;
}

Animation::Animation() {
 start_time = end_time = priority = 0; //Set everything to zero...
}

Animation::Animation(std::string in_name, glm::vec2 times, int in_priority) {
 name = in_name;
 start_time = times.x;
 end_time = times.y;
 priority = in_priority;
}