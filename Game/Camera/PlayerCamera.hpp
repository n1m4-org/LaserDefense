#ifndef PLAYER_CAMERA_HPP_
#define PLAYER_CAMERA_HPP_

#include "Math/Vector3.hpp"

class Player;

class PlayerCamera final {
    Vector3 focus_{};
    float followSpeed_ = 8.0f;
    float distance_ = 50.0f;
    float minDistance_ = 50.0f;
    float maxDistance_ = 100.0f;
    float speedDistanceFactor_ = 0.02857143f;
    float zoomSpeed_ = 3.0f;

public:
    void Initialize(const Player& _player);
    void Update(const Player& _player, float _deltaTime);

private:
    void LoadConfig();
    float GetTargetDistance(const Player& _player) const;
    void ApplyCamera() const;
};

#endif // PLAYER_CAMERA_HPP_
