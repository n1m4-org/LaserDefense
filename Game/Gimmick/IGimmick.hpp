#ifndef I_GIMMICK_HPP_
#define I_GIMMICK_HPP_

class EnemyManager;
class Player;
class TowerManager;

enum class GimmickType {
    Route,
    TowerDefense,
    TowerOrbit
};

enum class GimmickState {
    Ready,
    Active,
    Success,
    Failed
};

struct GimmickContext {
    Player* player = nullptr;
    TowerManager* towerManager = nullptr;
    EnemyManager* enemyManager = nullptr;
};

class IGimmick {
public:
    virtual ~IGimmick() = default;

    virtual void Initialize(const GimmickContext& _context) = 0;
    virtual void Update(float _deltaTime) = 0;
    virtual void Draw() const = 0;
    virtual GimmickType GetType() const = 0;
    virtual GimmickState GetState() const = 0;

    bool IsFinished() const {
        const GimmickState state = GetState();
        return state == GimmickState::Success || state == GimmickState::Failed;
    }
};

#endif // I_GIMMICK_HPP_
