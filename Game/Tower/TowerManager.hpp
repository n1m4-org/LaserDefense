#ifndef TOWER_MANAGER_HPP_
#define TOWER_MANAGER_HPP_

#include <memory>
#include <vector>

#include "Tower.hpp"

class MainTower;

class TowerManager final {
    std::vector<std::unique_ptr<Tower>> towers_;
    std::vector<MainTower*> towerCandidates_;
    std::vector<MainTower*> mainTowers_;
    float mainTowerSwitchInterval_ = 30.0f;
    float mainTowerSwitchTime_ = 0.0f;
    float switchWarningLeadSeconds_ = 2.0f;
    float switchWarningBlinkInterval_ = 0.5f;
    std::size_t nextCandidateIndex_ = 0;
    bool mainTowerSwitched_ = false;
    MainTower* switchWarningTower_ = nullptr;
    MainTower* nextWarningTower_ = nullptr;

public:
    void Initialize();
    Tower* AddTower(const Vector3& _position);
    MainTower* AddMainTower(const Vector3& _position);
    void Update(float _deltaTime);
    void Draw() const;
    Tower* PickTower(const Vector3& _origin, const Vector3& _direction, float _length) const;
    void SetHoveredTower(const Tower* _tower);
    void SetConnectedTower(const Tower* _tower);
    MainTower* ConsumeMainTowerSwitch();
    const std::vector<MainTower*>& GetMainTowers() const { return mainTowers_; }

private:
    void LoadConfig();
    void UpdateMainTowerSwitch(float _deltaTime);
    void UpdateSwitchWarning();
    MainTower* FindNextSubTower();
};

#endif // TOWER_MANAGER_HPP_
