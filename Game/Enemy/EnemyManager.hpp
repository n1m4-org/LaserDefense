#ifndef ENEMY_MANAGER_HPP_
#define ENEMY_MANAGER_HPP_

#include <cstdint>
#include <memory>
#include <vector>

#include "Enemy.hpp"
#include "Math/Vector2.hpp"
#include "Timer/Timer.hpp"

class ScoreManager;
class TimeLimitManager;
class ComboManager;
class MainTower;

class EnemyManager final {
    std::vector<std::unique_ptr<Enemy>> enemies_;
    Timer spawnTimer_{std::chrono::milliseconds{2000}};

    std::string modelName_{"Cube"};
    Vector3 modelScale_{0.5f, 0.5f, 0.5f};
    Vector3 modelOffset_{0.0f, 0.5f, 0.0f};
    Vector4 modelColor_{1.0f, 0.0f, 0.0f, 1.0f};
    Vector3 targetPosition_{};
    float moveSpeed_ = 1.0f;
    float maxHp_ = 10.0f;
    float knockbackBrake_ = 5.0f;
    float spawnAnimationDuration_ = 1.0f;
    float spawnStartScale_ = 0.1f;
    float spawnRotations_ = 2.0f;
    bool moveDuringSpawnAnimation_ = false;
    float deathAnimationDuration_ = 1.0f;
    float deathPeakScale_ = 1.3f;
    float deathEndScale_ = 0.01f;
    float deathExpandRatio_ = 0.4f;
    float spawnIntervalSeconds_ = 2.0f;
    int32_t spawnCount_ = 4;
    Vector2 spawnRange_{150.0f, 150.0f};
    Vector2 spawnExcludeRange_{30.0f, 30.0f};

    /// 敵1体を倒したときの獲得スコア。Enemy.json の "Score" / "Value" で変更できる
    int32_t scoreValue_ = 100;
    /// 敵1体を倒したときの制限時間の加算秒数。同 "TimeBonus" / "Seconds" で変更できる
    float timeBonusSeconds_ = 3.0f;
    /// 敵1体がタワーへ到達したときにタワーへ与えるダメージ。同 "TowerDamage" / "Value" で変更できる
    float towerDamage_ = 10.0f;
    /// タワーに接触して消滅した敵にも報酬を与えるか。同 "Score" / "AwardOnTowerHit" で変更できる
    bool awardRewardOnTowerHit_ = false;
    /// スコアの加算先。未設定(nullptr)ならスコア加算は行われない
    ScoreManager* scoreManager_ = nullptr;
    /// 制限時間の加算先。未設定(nullptr)なら時間加算は行われない
    TimeLimitManager* timeLimitManager_ = nullptr;
    /// コンボの加算先。未設定(nullptr)ならコンボは数えられず、倍率は常に1になる
    ComboManager* comboManager_ = nullptr;
    /// ダメージを与えるメインタワー。未設定(nullptr)ならタワーHPは減らない
    MainTower* mainTower_ = nullptr;

public:
    ~EnemyManager();

    void Initialize();
    void SetTargetPosition(float _x, float _z);

    /// @brief 撃破スコアの加算先を設定する
    /// @param _scoreManager スコアを加算する ScoreManager（所有権は持たない）
    /// @note シーン側で ScoreManager を生成したあとに一度呼ぶだけでよい
    void SetScoreManager(ScoreManager* _scoreManager) { scoreManager_ = _scoreManager; }

    /// @brief 撃破時に増える制限時間の加算先を設定する
    /// @param _timeLimitManager 時間を加算する TimeLimitManager（所有権は持たない）
    /// @note シーン側で TimeLimitManager を生成したあとに一度呼ぶだけでよい
    void SetTimeLimitManager(TimeLimitManager* _timeLimitManager) {
        timeLimitManager_ = _timeLimitManager;
    }

    /// @brief 連続撃破を数える ComboManager を設定する
    /// @param _comboManager コンボを加算する ComboManager（所有権は持たない）
    /// @note 設定するとスコアにコンボ倍率が掛かるようになる
    void SetComboManager(ComboManager* _comboManager) { comboManager_ = _comboManager; }

    /// @brief 敵に到達されたときダメージを受けるメインタワーを設定する
    /// @param _mainTower ダメージを与えるタワー（所有権は持たない）
    /// @note 設定すると、敵1体が到達するたびに Enemy::GetTowerDamage() 分の HP が減る
    void SetMainTower(MainTower* _mainTower) { mainTower_ = _mainTower; }

    void Update(float _deltaTime);
    void Draw() const;

private:
    void LoadConfig();
    void SpawnEnemy(const Vector3& _position);

    /// @brief 撃破された敵の報酬を回収し、スコアと制限時間へ加算する
    void CollectDefeatRewards();
};

#endif // ENEMY_MANAGER_HPP_
