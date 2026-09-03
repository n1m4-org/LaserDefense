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
    bool awardsScoreOnTowerHit_ = false;    //!< タワーに接触して消滅した場合もスコアを与えるか
    bool scorePending_ = false;             //!< スコアが未回収か（二重加算を防ぐためのフラグ）
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

    /// @brief 撃破時に得られるスコアを設定する
    /// @param _score            1体あたりの獲得スコア
    /// @param _awardOnTowerHit  タワーに接触して消えた場合もスコアを与えるか
    /// @note 通常は EnemyManager が JSON の値を渡すので、点数を変えたい場合は
    ///       Assets/Data/Enemy/Enemy.json の "Score" を書き換えるだけでよい
    void SetScoreValue(int32_t _score, bool _awardOnTowerHit = false) {
        scoreValue_ = _score;
        awardsScoreOnTowerHit_ = _awardOnTowerHit;
    }

    /// @brief 1体あたりの獲得スコアを取得する
    int32_t GetScoreValue() const { return scoreValue_; }

    /// @brief 未回収のスコアを持っているか
    bool IsScorePending() const { return scorePending_; }

    /// @brief 未回収のスコアを受け取る（1体につき1回だけ0以外を返す）
    /// @return 加算すべきスコア。未撃破または回収済みなら0
    /// @note 呼び出した時点で回収済みになるため、二重加算されることはない
    int32_t TakeScoreReward();

    /// @brief この敵を撃破状態にする
    /// @param _awardsScore この撃破をスコア加算の対象にするか
    void Kill(bool _awardsScore = true);

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
