#ifndef ANIMATION
#define ANIMATION

#include <glm/glm.hpp>
#include <string>

#define FRAME 0.041666666667


float TimeToFrame(float time);


glm::vec2 FramesToTime(glm::vec2 frames);

// Named range of frames of an animation, with a priority. Not used yet:
// Skeleton always plays the first animation of the file.
class Animation
{
     public:
         std::string name;
         float start_time; 
         float end_time;   
         int priority;   

         Animation();
         Animation(std::string in_name, glm::vec2 times, int in_priority);
};
#endif