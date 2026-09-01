#include "MyGame.hpp"

#include "Factory/PostEffectFactory.hpp"
#include "Scene/EnemyTestScene.hpp"

void MyGame::Initialize(GameEngine::Config& _config) {
    // ゲーム固有の設定（Assets/Config/App.cnf に含まれないもの）
    _config.defaultScene = "EnemyTest";

    // cnf の値をゲーム側で強制上書きしたい場合はここで設定する
    // 例: _config.fps = 120;

    Register();

    // PostEffectFactoryを登録
    SetPostEffectFactory(std::make_unique<PostEffectFactory>());
}

void MyGame::Register(){
    RegisterScene<EnemyTestScene>("EnemyTest");
}
