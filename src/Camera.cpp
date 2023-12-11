#include "Camera.h"

Camera::Camera(){

}

Camera::Camera(Type type, SDL_Window* window, uint32_t width, uint32_t height, glm::vec3 eye, glm::vec3 center)
{
    _xpos = 0.0f;
    _ypos = 0.0f;
    _radius = glm::length(eye - center);
    _type = type;
    _center = center;
    _eye = eye;
    _forward = glm::normalize(_center - _eye);
    if(type == Type::eFirstPerson) {
        _angle = glm::vec2(atan2(_forward.x, _forward.z), asin(_forward.y) + 1.570796f);
    } else {
        glm::vec3 _eyeNorm = glm::normalize(_eye);
        _angle = glm::vec2(atan2(_eyeNorm.x, _eyeNorm.z), asin(-_eyeNorm.y) + 1.570796f);
    }
    _width = width;
    _height = height;
    _oldPos = glm::vec2(width / 2.0f, height / 2.0f);
    _newPos = glm::vec2(width / 2.0f, height / 2.0f);
    _window = window;
    _lastTime = SDL_GetTicks() / 1000.0;
    _buttonState_W = false;
    _buttonState_A = false;
    _buttonState_S = false;
    _buttonState_D = false;
    _buttonState_C = false;
    _buttonState_LCTRL = false;
    _buttonState_SPACE = false;
    _buttonState_MOUSELEFT = false;
    update();
}

void Camera::handleInputEvent(const SDL_Event *event)
{
    switch(event->type) {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event->button.button == SDL_BUTTON_LEFT) {
                _buttonState_MOUSELEFT = true;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event->button.button == SDL_BUTTON_LEFT) {
                _buttonState_MOUSELEFT = false;
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            _xpos = event->motion.x;
            _ypos = event->motion.y;
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if(_type == eTrackBall) {
                _radius -= (float) (event->wheel.y * 0.1);
                _radius = glm::max((float)_radius, 0.000001f);
                changed = true;
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event->key.keysym.sym == SDLK_w) {
                _buttonState_W = true;
            } else if (event->key.keysym.sym == SDLK_a) {
                _buttonState_A = true;
            } else if (event->key.keysym.sym == SDLK_s) {
                _buttonState_S = true;
            } else if (event->key.keysym.sym == SDLK_d) {
                _buttonState_D = true;
            } else if (event->key.keysym.sym == SDLK_c) {
                _buttonState_C = true;
            } else if (event->key.keysym.sym == SDLK_LCTRL) {
                _buttonState_LCTRL = true;
            } else if (event->key.keysym.sym == SDLK_SPACE) {
                _buttonState_SPACE = true;
            }
            break;
        case SDL_EVENT_KEY_UP:
            if (event->key.keysym.sym == SDLK_w) {
                _buttonState_W = false;
            } else if (event->key.keysym.sym == SDLK_a) {
                _buttonState_A = false;
            } else if (event->key.keysym.sym == SDLK_s) {
                _buttonState_S = false;
            } else if (event->key.keysym.sym == SDLK_d) {
                _buttonState_D = false;
            } else if (event->key.keysym.sym == SDLK_c) {
                _buttonState_C = false;
            } else if (event->key.keysym.sym == SDLK_LCTRL) {
                _buttonState_LCTRL = false;
            } else if (event->key.keysym.sym == SDLK_SPACE) {
                _buttonState_SPACE = false;
            }
            break;
    }
}

void Camera::update(){
    double time_seconds = SDL_GetTicks() / 1000.0;
    double delta_time = time_seconds - _lastTime;
    _lastTime = time_seconds;

    if (_buttonState_MOUSELEFT) {
        _newPos = glm::vec2(_xpos, _ypos);
        float xAngle = (_newPos.x - _oldPos.x) / _width * 2 * 3.141592f;
        float yAngle = (_newPos.y - _oldPos.y) / _height * 3.141592f;
        _angle -= glm::vec2(xAngle, yAngle);
        _angle.y = glm::max(glm::min(_angle.y, 3.141591f), 0.000001f);
        changed = true;
    }
    _oldPos = glm::vec2(_xpos, _ypos);

    if (_buttonState_C && _timeout <= 0.00001){
        if(_type == eTrackBall){
            _type = eFirstPerson;
            _forward = glm::normalize(_center - _eye);
            _forward.y = 0.0f;
            _forward = glm::normalize(_forward);
            _angle.x += 3.141592f;
            changed = true;
        }
        else{
            _type = eTrackBall;
            _radius = glm::length(_eye);
            glm::vec3 _eyeNorm = glm::normalize(_eye);
            _angle = glm::vec2(atan2(_eyeNorm.x, _eyeNorm.z), asin(-_eyeNorm.y) + 1.570796f);
            changed = true;
        }
        _timeout = 0.3;
    }
    if(_timeout >= 0.0)
        _timeout -= delta_time;
    if(_type == eTrackBall){
        _eye.x = _radius * sin(_angle.y) * sin(_angle.x);
        _eye.y = _radius * cos(_angle.y);
        _eye.z = _radius * sin(_angle.y) * cos(_angle.x);

        _viewMatrix = glm::lookAt(_eye, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
    }
    else if(_type == eFirstPerson){
        glm::vec3 up = glm::vec3(0.0, 1.0, 0.0);
        glm::vec3 down = -up;
        glm::vec3 forward = glm::normalize(glm::vec3(_forward.x, 0.0f, _forward.z));
        glm::vec3 backwards = -1.f * forward;
        glm::vec3 left = glm::cross(up, forward);
        glm::vec3 right = -1.f * left;
        if (_buttonState_W){
            _eye += forward * (float) delta_time * 3.f;
            changed = true;
        }
        if (_buttonState_A){
            _eye += left * (float) delta_time * 3.f;
            changed = true;
        }
        if (_buttonState_S){
            _eye += backwards * (float) delta_time * 3.f;
            changed = true;
        }
	    if (_buttonState_D){
            _eye += right * (float) delta_time * 3.f;
            changed = true;
        }
        if (_buttonState_LCTRL){
            _eye += down * (float) delta_time * 3.f;
            changed = true;
        }
        if (_buttonState_SPACE){
            _eye += up * (float) delta_time * 3.f;
            changed = true;
        }
        _radius = glm::length(_eye);

        _forward.x = sin(3.141592f - _angle.y) * sin(_angle.x);
        _forward.y = cos(3.141592f - _angle.y);
        _forward.z = sin(3.141592f - _angle.y) * cos(_angle.x);

        _viewMatrix = glm::lookAt(_eye, _eye + _forward, glm::vec3(0.0, 1.0, 0.0));
    }
}

void Camera::updateSize(uint32_t width, uint32_t height)
{
    _width = width;
    _height = height;
    changed = true;
}

glm::mat4 Camera::getView(){
    return _viewMatrix;
}