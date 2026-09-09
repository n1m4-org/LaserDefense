#ifndef GIMMICK_MANAGER_HPP_
#define GIMMICK_MANAGER_HPP_

#include <memory>
#include <optional>

#include "Gimmick/IGimmick.hpp"

class GimmickManager final {
    GimmickContext context_{};
    std::unique_ptr<IGimmick> activeGimmick_;
    float spawnInterval_ = 30.0f;
    float spawnTime_ = 0.0f;
    float routeWeight_ = 1.0f;
    float towerDefenseWeight_ = 1.0f;
    float towerOrbitWeight_ = 1.0f;
    bool debugPaused_ = false;
    std::optional<GimmickType> pendingStart_;

public:
    void Initialize(const GimmickContext& _context);
    void Update(float _deltaTime);
    void Draw() const;
    void Debug();

    const IGimmick* GetActiveGimmick() const { return activeGimmick_.get(); }

private:
    void LoadConfig();
    void StartRandomGimmick();
    void StartGimmick(GimmickType _type);
    std::unique_ptr<IGimmick> CreateGimmick(GimmickType _type) const;
};

#endif // GIMMICK_MANAGER_HPP_
