#ifndef LASER_HPP_
#define LASER_HPP_

#include <memory>

#include "Math/Vector3.hpp"

class GameObject;
class Line;

class Laser final {
    static constexpr float kDefaultLineHeight = 0.5f;

    struct Endpoint {
        const GameObject* object = nullptr;
        float lineHeight = kDefaultLineHeight;
    };

    std::unique_ptr<Line> line_;
    Endpoint start_{};
    Endpoint target_{};

public:
    Laser();
    ~Laser();

    void Initialize();
    void Update();
    void Draw() const;

    void SetStart(const GameObject* _object, float _lineHeight = kDefaultLineHeight);
    void SetTarget(const GameObject* _object, float _lineHeight = kDefaultLineHeight);
    void ClearTarget();
};

#endif // LASER_HPP_
