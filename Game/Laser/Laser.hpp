#ifndef LASER_HPP_
#define LASER_HPP_

#include <memory>
#include <vector>

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
    std::vector<Endpoint> targets_;

public:
    Laser();
    ~Laser();

    void Initialize();
    void Update();
    void Draw() const;

    void SetStart(const GameObject* _object, float _lineHeight = kDefaultLineHeight);
    void AddTarget(const GameObject* _object, float _lineHeight = kDefaultLineHeight);
    void RemoveTarget(const GameObject* _object);
    void ClearTargets();
};

#endif // LASER_HPP_
