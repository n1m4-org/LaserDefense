#ifndef GIMMICK_MANAGER_HPP_
#define GIMMICK_MANAGER_HPP_

#include <memory>

#include "Gimmick/IGimmick.hpp"

class GimmickManager final {
    GimmickContext context_{};
    std::unique_ptr<IGimmick> activeGimmick_;
    float spawnInterval_ = 30.0f;
    float spawnTime_ = 0.0f;
    float routeWeight_ = 1.0f;
    float towerDefenseWeight_ = 1.0f;
    float towerOrbitWeight_ = 1.0f;

public:
    void Initialize(const GimmickContext& _context);
    void Update(float _deltaTime);
    void Draw() const;

    const IGimmick* GetActiveGimmick() const { return activeGimmick_.get(); }

private:
    void LoadConfig();
    void StartRandomGimmick();
    std::unique_ptr<IGimmick> CreateGimmick(GimmickType _type) const;
};

#endif // GIMMICK_MANAGER_HPP_
