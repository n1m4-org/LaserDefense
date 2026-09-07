#ifndef ROUTE_GIMMICK_HPP_
#define ROUTE_GIMMICK_HPP_

#include "Gimmick/IGimmick.hpp"

class RouteGimmick final : public IGimmick {
    GimmickContext context_{};
    GimmickState state_ = GimmickState::Ready;

public:
    void Initialize(const GimmickContext& _context) override;
    void Update(float _deltaTime) override;
    void Draw() const override;
    GimmickType GetType() const override { return GimmickType::Route; }
    GimmickState GetState() const override { return state_; }
};

#endif // ROUTE_GIMMICK_HPP_
