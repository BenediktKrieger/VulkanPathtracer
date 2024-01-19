#pragma once
#include <chrono>
#include <vk_types.h>

class Camera
{
public:
    enum Type{
        eTrackBall,
        eFirstPerson
    };
    Camera();
	Camera(Type type, SDL_Window* window, uint32_t width, uint32_t height, glm::vec3 eye, glm::vec3 center);
    void update();
    glm::vec3 getPosition();
    glm::vec3 getDirection();
    void updateSize(uint32_t width, uint32_t height);
    glm::mat4 getView();
    void handleInputEvent(const SDL_Event *event);
    bool changed = false;
private:
	glm::mat4 _viewMatrix;
    glm::vec3 _eye;
    glm::vec3 _center;
    glm::vec3 _forward;
	glm::vec2 _oldPos, _newPos;
    glm::vec2 _angle = glm::vec2(0.0f);
    uint32_t _width, _height;
    double _timeout = 0.0;
    double _lastTime = 0.0;
    SDL_Window* _window;
    float _radius;
    float _xpos;
    float _ypos;
    Type _type;
    bool _buttonState_W;
    bool _buttonState_A;
    bool _buttonState_S;
    bool _buttonState_D;
    bool _buttonState_C;
    bool _buttonState_LCTRL;
    bool _buttonState_SPACE;
    bool _buttonState_MOUSELEFT;
};