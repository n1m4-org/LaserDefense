#ifndef ENEMY_HPP_
#define ENEMY_HPP_

#include <cstdint>

#include "GameObject/GameObject.hpp"
#include "Collision/Collider.h"
#include "Combat/AttackHit.hpp"
#include "Sprite.hpp"

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
    float maxHp_ = 10.0f;
    float hp_ = 10.0f;
    float knockbackBrake_ = 5.0f;
    Vector3 knockbackVelocity_{};
    Sprite hpBarBackground_;
    Sprite hpBarFill_;
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
    int32_t scoreValue_ = 100;              //!< この敵を倒したときに加算されるスコア
    float timeBonusSeconds_ = 3.0f;         //!< この敵を倒したときに加算される制限時間（秒）
    bool awardsRewardOnTowerHit_ = false;   //!< タワーに接触して消滅した場合も報酬を与えるか
    bool rewardPending_ = false;            //!< 撃破報酬が未回収か（二重加算を防ぐためのフラグ）
    bool towerReachPending_ = false;        //!< タワーへ到達したことが未処理か（コンボを切るのに使う）
    std::unique_ptr<Collision::Collider> collider_;
    Vector3 colliderOffset_{};

public:
    void SetHealth(float _maxHp, float _knockbackBrake);
    float GetHp() const { return hp_; }
    float GetMaxHp() const { return maxHp_; }
    void TakeDamage(const AttackHit& _hit);
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
    void SetColliderOffset(const Vector3& _offset) { colliderOffset_ = _offset; }
    const Vector3& GetColliderOffset() const { return colliderOffset_; }

    /// @brief 撃破時に得られる報酬（スコア・制限時間）を設定する
    /// @param _score            1体あたりの獲得スコア
    /// @param _timeBonusSeconds 1体あたりの制限時間の加算秒数
    /// @param _awardOnTowerHit  タワーに接触して消えた場合も報酬を与えるか
    /// @note 通常は EnemyManager が JSON の値を渡すので、報酬を変えたい場合は
    ///       Assets/Data/Enemy/Enemy.json の "Score" / "TimeBonus" を書き換えるだけでよい
    void SetDefeatReward(int32_t _score, float _timeBonusSeconds,
                         bool _awardOnTowerHit = false) {
        scoreValue_ = _score;
        timeBonusSeconds_ = _timeBonusSeconds;
        awardsRewardOnTowerHit_ = _awardOnTowerHit;
    }

    /// @brief 1体あたりの獲得スコアを取得する
    int32_t GetScoreValue() const { return scoreValue_; }

    /// @brief 1体あたりの制限時間の加算秒数を取得する
    float GetTimeBonusSeconds() const { return timeBonusSeconds_; }

    /// @brief タワーへ到達したことを受け取る（1体につき1回だけ true を返す）
    /// @return タワーへ到達されていれば true。レーザーで倒した場合や処理済みなら false
    /// @note コンボを途切れさせる「ミス」の判定に使う
    bool ConsumeTowerReach();

    /// @brief 未回収の撃破報酬を持っているか
    bool IsRewardPending() const { return rewardPending_; }

    /// @brief 未回収の撃破報酬を受け取る（1体につき1回だけ true を返す）
    /// @return 報酬を渡すべきなら true。未撃破または回収済みなら false
    /// @note 呼び出した時点で回収済みになるため、二重加算されることはない。
    ///       実際の値は GetScoreValue() / GetTimeBonusSeconds() から取る
    bool ConsumeDefeatReward();

    /// @brief この敵を撃破状態にする
    /// @param _awardsReward この撃破を報酬（スコア・時間）の対象にするか
    void Kill(bool _awardsReward = true);

    void Initialize() override;
    void Update(float _deltaTime) override;
    void Draw() override;

private:
    void DrawHpBar();
    void UpdateMovement(float _deltaTime);
    void UpdateSpawnAnimation(float _deltaTime);
    void UpdateDeathAnimation(float _deltaTime);
    void UpdateCollider();
    void OnCollisionTrigger(const Collision::Collider* _other);
    bool IsSpawnAnimationPlaying() const;
};

#endif // ENEMY_HPP_
