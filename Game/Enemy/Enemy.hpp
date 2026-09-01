#ifndef ENEMY_HPP_
#define ENEMY_HPP_

#include "GameObject/GameObject.hpp"

class Enemy final : public GameObject {
public:
    enum class State {
        Spawn,
        Move,
        Death
    };

private:
    std::string modelName_{"Cube"};
    Vector3 modelScale_{0.5f, 0.5f, 0.5f};
    Vector3 modelOffset_{0.0f, 0.5f, 0.0f};
    Vector4 modelColor_{1.0f, 0.0f, 0.0f, 1.0f};
    Vector3 targetPosition_{};
    float moveSpeed_ = 1.0f;
    State state_ = State::Spawn;
    float spawnAnimationTime_ = 0.0f;
    float spawnAnimationDuration_ = 1.0f;
    float spawnStartScale_ = 0.1f;
    float spawnRotations_ = 2.0f;
    bool moveDuringSpawnAnimation_ = false;
    float deathAnimationTime_ = 0.0f;
    float deathAnimationDuration_ = 1.0f;
    float deathPeakScale_ = 1.3f;
    float deathEndScale_ = 0.01f;
    float deathExpandRatio_ = 0.4f;
    bool deathAnimationFinished_ = false;

public:
    void SetAppearance(const std::string& _modelName, const Vector3& _scale,
                       const Vector3& _offset, const Vector4& _color);
    void SetMovement(const Vector3& _targetPosition, float _moveSpeed);
    void SetSpawnAnimation(float _duration, float _startScale,
                           float _rotations, bool _moveDuringAnimation);
    void SetDeathAnimation(float _duration, float _peakScale,
                           float _endScale, float _expandRatio);
    State GetState() const { return state_; }
    bool IsAlive() const { return state_ != State::Death; }
    bool IsDeathAnimationFinished() const { return deathAnimationFinished_; }
    void Kill();

    void Initialize() override;
    void Update(float _deltaTime) override;
    void Draw() override;

private:
    void UpdateMovement(float _deltaTime);
    void UpdateSpawnAnimation(float _deltaTime);
    void UpdateDeathAnimation(float _deltaTime);
    bool IsSpawnAnimationPlaying() const;
};

#endif // ENEMY_HPP_
