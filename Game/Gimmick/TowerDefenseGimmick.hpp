#ifndef TOWER_DEFENSE_GIMMICK_HPP_
#define TOWER_DEFENSE_GIMMICK_HPP_

#include "Gimmick/IGimmick.hpp"

class TowerDefenseGimmick final : public IGimmick {
    GimmickContext context_{};
    GimmickState state_ = GimmickState::Ready;

public:
    void Initialize(const GimmickContext& _context) override;
    void Update(float _deltaTime) override;
    void Draw() const override;
    GimmickType GetType() const override { return GimmickType::TowerDefense; }
    GimmickState GetState() const override { return state_; }
};

#endif // TOWER_DEFENSE_GIMMICK_HPP_
