#include "MyGame.hpp"

#include "Factory/PostEffectFactory.hpp"
#include "PostProcess/BoxBlur/BoxBlur.hpp"
#include "PostProcess/Grayscale/Grayscale.hpp"
#include "PostProcess/Vignette/Vignette.hpp"
#include "Scene/GameSampleScene.hpp"
#include "Scene/GimmickTuningScene.hpp"
#include "Scene/PlayScene.hpp"
#include "Scene/TitleScene.hpp"

void MyGame::Initialize(GameEngine::Config& _config) {
    // ゲーム固有の設定（Assets/Config/App.cnf に含まれないもの）

    _config.defaultScene = "Title";


    // cnf の値をゲーム側で強制上書きしたい場合はここで設定する
    // 例: _config.fps = 120;

    Register();

    // PostEffectを登録
    RegisterPostEffect<Vignette>("Vignette");
    RegisterPostEffect<Grayscale>("Grayscale");
    RegisterPostEffect<BoxBlur>("BoxBlur");
}

void MyGame::Register() {
    RegisterScene<TitleScene>("Title");
    RegisterScene<PlayScene>("Play");
    RegisterScene<GameSampleScene>("GameSample");
    RegisterScene<GimmickTuningScene>("GimmickTuning");
}
