#ifndef GAME_SAMPLE_SCENE_HPP
#define GAME_SAMPLE_SCENE_HPP
#include "IScene.hpp"

class GameSampleScene : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Draw() override;
};

inline void GameSampleScene::Initialize() { }

inline void GameSampleScene::Update() { }

inline void GameSampleScene::Draw() { }

#endif
