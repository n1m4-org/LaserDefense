#ifndef PLAYER_CAMERA_HPP_
#define PLAYER_CAMERA_HPP_

#include "Math/Vector3.hpp"

class Player;

class PlayerCamera final {
    Vector3 focus_{};
    float followSpeed_ = 8.0f;
    float distance_ = 50.0f;

public:
    void Initialize(const Player& _player);
    void Update(const Player& _player, float _deltaTime);

private:
    void LoadConfig();
    void ApplyCamera() const;
};

#endif // PLAYER_CAMERA_HPP_
