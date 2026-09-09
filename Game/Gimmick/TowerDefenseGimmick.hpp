#ifndef TOWER_DEFENSE_GIMMICK_HPP_
#define TOWER_DEFENSE_GIMMICK_HPP_

#include <cstdint>
#include <memory>

#include "Gimmick/IGimmick.hpp"
#include "Math/Vector2.hpp"
#include "Sprite.hpp"

class MainTower;

/// @brief メインタワー以外のタワー1つを敵スポナー化し、指定数を倒すまで敵が湧き続けるギミック
/// @note 指定時間内に必要撃破数へ届かなければ失敗になる
class TowerDefenseGimmick final : public IGimmick {
    enum class Phase {
        Warning,  //!< 赤矢印で対象タワーを案内している間。まだ敵は湧かない
        Spawning  //!< 敵が湧き、制限時間と撃破数を判定している間
    };

    GimmickContext context_{};
    GimmickState state_ = GimmickState::Ready;
    MainTower* targetTower_ = nullptr;
    Phase phase_ = Phase::Warning;

    float warningElapsedSeconds_ = 0.0f;
    float warningDurationSeconds_ = 3.0f;
    std::unique_ptr<Sprite> warningArrow_;
    Vector2 warningArrowSize_{32.0f, 32.0f};
    bool warningArrowVisible_ = false;

    float elapsedTime_ = 0.0f;
    float timeLimitSeconds_ = 25.0f;
    int32_t requiredKillCount_ = 10;
    int32_t killCount_ = 0;
    float killRadius_ = 8.0f;
    float spawnIntervalSeconds_ = 1.5f;
    float spawnElapsedSeconds_ = 0.0f;
    float spawnRadius_ = 3.0f;

public:
    void Initialize(const GimmickContext& _context) override;
    void Update(float _deltaTime) override;
    void Draw() const override;
    GimmickType GetType() const override { return GimmickType::TowerDefense; }
    GimmickState GetState() const override { return state_; }

    void Debug() override;

private:
    void LoadConfig();
    void SaveConfig() const;
    void UpdateWarningArrow();
    void CollectKillsInRange();
    void Finish(GimmickState _result);
};

#endif // TOWER_DEFENSE_GIMMICK_HPP_
