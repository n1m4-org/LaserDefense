#ifndef ENEMY_MANAGER_HPP_
#define ENEMY_MANAGER_HPP_

#include <memory>
#include <vector>

#include "Enemy.hpp"
#include "Math/Vector2.hpp"
#include "Timer/Timer.hpp"

class EnemyManager final {
    std::vector<std::unique_ptr<Enemy>> enemies_;
    Timer spawnTimer_{std::chrono::milliseconds{3000}};

    std::string modelName_{"Cube"};
    Vector3 modelScale_{0.5f, 0.5f, 0.5f};
    Vector3 modelOffset_{0.0f, 0.25f, 0.0f};
    Vector4 modelColor_{1.0f, 0.0f, 0.0f, 1.0f};
    Vector3 targetPosition_{};
    float moveSpeed_ = 1.0f;
    float spawnAnimationDuration_ = 1.0f;
    float spawnStartScale_ = 0.1f;
    float spawnRotations_ = 2.0f;
    bool moveDuringSpawnAnimation_ = false;
    float deathAnimationDuration_ = 1.0f;
    float deathPeakScale_ = 1.3f;
    float deathEndScale_ = 0.01f;
    float deathExpandRatio_ = 0.4f;
    float spawnIntervalSeconds_ = 3.0f;
    Vector2 spawnRange_{20.0f, 20.0f};

public:
    ~EnemyManager();

    void Initialize();
    void SetTargetPosition(float _x, float _z);
    void Update(float _deltaTime);
    void Draw() const;

private:
    void LoadConfig();
    void SpawnEnemy(const Vector3& _position);
};

#endif // ENEMY_MANAGER_HPP_
