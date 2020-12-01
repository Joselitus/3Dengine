#ifndef ANIMATION
#define ANIMATION

#include <glm/glm.hpp>
#include <string>

#define FRAME 0.041666666667


static float TimeToFrame(float time);


static glm::vec2 FramesToTime(glm::vec2 frames);

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