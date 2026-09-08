#ifndef GIMMICK_TUNING_SCENE_HPP_
#define GIMMICK_TUNING_SCENE_HPP_

#include <memory>

#include "Gimmick/GimmickManager.hpp"
#include "IScene.hpp"
#include "Model.hpp"
#include "Scene/Input/GameSceneInput.hpp"
#include "Tower/TowerManager.hpp"

class Player;
class PlayerCamera;

class GimmickTuningScene final : public IScene {
    GameSceneInput input_{};
    std::unique_ptr<Player> player_;
    std::unique_ptr<PlayerCamera> playerCamera_;
    std::unique_ptr<TowerManager> towerManager_;
    std::unique_ptr<GimmickManager> gimmickManager_;
    std::unique_ptr<Model> floor_;

public:
    GimmickTuningScene();
    ~GimmickTuningScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Debug() override;
};

#endif // GIMMICK_TUNING_SCENE_HPP_
