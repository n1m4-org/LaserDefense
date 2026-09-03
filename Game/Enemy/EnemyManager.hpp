#ifndef ENEMY_MANAGER_HPP_
#define ENEMY_MANAGER_HPP_

#include <cstdint>
#include <memory>
#include <vector>

#include "Enemy.hpp"
#include "Math/Vector2.hpp"
#include "Timer/Timer.hpp"

class ScoreManager;

class EnemyManager final {
    std::vector<std::unique_ptr<Enemy>> enemies_;
    Timer spawnTimer_{std::chrono::milliseconds{3000}};

    std::string modelName_{"Cube"};
    Vector3 modelScale_{0.5f, 0.5f, 0.5f};
    Vector3 modelOffset_{0.0f, 0.5f, 0.0f};
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
    Vector2 spawnRange_{90.0f, 90.0f};
    Vector2 spawnExcludeRange_{10.0f, 10.0f};

    /// 敵1体を倒したときの獲得スコア。Enemy.json の "Score" / "Value" で変更できる
    int32_t scoreValue_ = 100;
    /// タワーに接触して消滅した敵にもスコアを与えるか。同 "AwardOnTowerHit" で変更できる
    bool awardScoreOnTowerHit_ = false;
    /// スコアの加算先。未設定(nullptr)ならスコア加算は行われない
    ScoreManager* scoreManager_ = nullptr;

public:
    ~EnemyManager();

    void Initialize();
    void SetTargetPosition(float _x, float _z);

    /// @brief 撃破スコアの加算先を設定する
    /// @param _scoreManager スコアを加算する ScoreManager（所有権は持たない）
    /// @note シーン側で ScoreManager を生成したあとに一度呼ぶだけでよい
    void SetScoreManager(ScoreManager* _scoreManager) { scoreManager_ = _scoreManager; }

    void Update(float _deltaTime);
    void Draw() const;

private:
    void LoadConfig();
    void SpawnEnemy(const Vector3& _position);

    /// @brief 撃破された敵のスコアを回収して ScoreManager へ加算する
    void CollectScore();
};

#endif // ENEMY_MANAGER_HPP_
