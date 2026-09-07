#define NOMINMAX
#include "PlayScene.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <variant>
#include <cmath>

#include "Camera/Controller/CameraController.hpp"
#include "Camera/PlayerCamera.hpp"
#include "GameObject/Player/Player.h"
#include "Laser/Laser.hpp"
#include "Light/LightManager.hpp"
#include "Input.hpp"
#include "Json/JsonParams.hpp"
#include "Line.hpp"
#include "Math/MathUtils.hpp"
#include "Pattern/Singleton.hpp"
#include "Screen/Screen.hpp"
#include "Texture/TextureManager.hpp"
#include "Time/Time.hpp"
#include "Tower/MainTower.hpp"

#ifdef _DEBUG
#include "imgui_internal.h"
#endif

PlayScene::PlayScene() = default;
PlayScene::~PlayScene() = default;

void PlayScene::LoadStageConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Stage", "BattleStage")) return;
    const auto groups = json->GetGroups("BattleStage");
    const auto layout = groups.find("Layout");
    if (layout == groups.end()) return;
    const auto read = [&](const char* _key, float _fallback) {
        const auto entry = layout->second.find(_key);
        if (entry == layout->second.end()) return _fallback;
        float value = _fallback;
        if (const auto number = std::get_if<float>(&entry->second)) value = *number;
        else if (const auto integer = std::get_if<int32_t>(&entry->second)) value = static_cast<float>(*integer);
        return std::isfinite(value) ? value : _fallback;
    };
    stageSize_ = std::max(read("Size", stageSize_), 20.0f);
    towerMargin_ = std::clamp(read("TowerMargin", towerMargin_), 2.0f, stageSize_ * 0.5f - 5.0f);
    fenceHeight_ = std::max(read("FenceHeight", fenceHeight_), 0.1f);
    wallBounce_ = std::clamp(read("WallBounce", wallBounce_), 0.0f, 1.0f);
}

void PlayScene::Initialize() {
    LoadStageConfig();
    const float halfSize = stageSize_ * 0.5f;
    const float towerPosition = halfSize - towerMargin_;
    constexpr Vector3 mainTowerPosition{0.0f, 0.0f, 0.0f};
    constexpr Vector3 shadowLightOffset{0.0f, 10.0f, 0.0f};

    Singleton<TextureManager>::GetInstance()->Load("skybox.dds");

    player_ = std::make_unique<Player>();
    player_->Initialize();
    // 画面手前（-Z）に出現。高さは床と同じ0。
    player_->SetPosition(mainTowerPosition + Vector3{0.0f, 0.0f, -8.0f});
    player_->SetInput(input_);
    player_->EnableGrappleMovement();
    player_->SetStageBoundary(halfSize, wallBounce_);
    playerCamera_ = std::make_unique<PlayerCamera>();
    playerCamera_->Initialize(*player_);
    Singleton<LightManager>::GetInstance()->SetPosition(
        player_->GetPosition() + shadowLightOffset);

    towerManager_ = std::make_unique<TowerManager>();
    towerManager_->Initialize();

    assistedTower_ = nullptr;
    MainTower* mainTower = towerManager_->AddMainTower(mainTowerPosition);

    // 5×5の等間隔配置。中央はメインタワーなので通常タワーを重ねない。
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column < 5; ++column) {
            if (row == 2 && column == 2) continue;
            const float x = -towerPosition + static_cast<float>(column) * towerPosition * 0.5f;
            const float z = -towerPosition + static_cast<float>(row) * towerPosition * 0.5f;
            towerManager_->AddTower({x, 0.0f, z});
        }
    }

    laser_ = std::make_unique<Laser>();
    laser_->Initialize(Particle());
    laser_->SetStart(player_.get(), player_->GetModelOffset().y);
    laser_->ClearTarget();

    // スコアと制限時間は EnemyManager より先に用意し、撃破報酬の加算先として渡しておく
    scoreManager_ = std::make_unique<ScoreManager>();
    scoreManager_->Initialize();

    survivalTimeManager_ = std::make_unique<SurvivalTimeManager>();
    survivalTimeManager_->Initialize();

    comboManager_ = std::make_unique<ComboManager>();
    comboManager_->Initialize();

    // タワーHPゲージは MainTower の HP を読むだけなので、タワーを渡しておく
    towerHpGauge_ = std::make_unique<TowerHpGauge>();
    towerHpGauge_->Initialize();
    towerHpGauge_->SetTarget(mainTower_);

    // リザルトはシーンを跨がず、この画面の上に重ねて出す
    resultOverlay_ = std::make_unique<ResultOverlay>();
    resultOverlay_->Initialize();

    enemyManager_ = std::make_unique<EnemyManager>();
    enemyManager_->Initialize(Particle());
    enemyManager_->SetTargetPosition(mainTowerPosition.x, mainTowerPosition.z);
    enemyManager_->SetScoreManager(scoreManager_.get());
    enemyManager_->SetComboManager(comboManager_.get());
    // 敵に到達されたときダメージを受けるタワーを渡す
    enemyManager_->SetMainTower(mainTower_);

    floor_ = std::make_unique<Model>();
    floor_->Initialize("plane");
    floor_->SetTexture("white_x16.png");
    floor_->SetColor({0.5f, 0.5f, 0.5f, 1.0f});
    floor_->SetTranslate({0.0f, 0.0f, 0.0f});
    floor_->SetRotate({-1.5707963f, 0.0f, 0.0f});
    floor_->SetScale({halfSize, halfSize, 1.0f});

    const float halfHeight = fenceHeight_ * 0.5f;
    const std::array<Vector3, 4> fencePositions{{
        {0.0f, halfHeight, -halfSize}, {0.0f, halfHeight, halfSize},
        {-halfSize, halfHeight, 0.0f}, {halfSize, halfHeight, 0.0f}
    }};
    // Planeは片面なので表裏を用意し、ステージの内外どちらからでも見えるようにする。
    for (size_t side = 0; side < fencePositions.size(); ++side) {
        for (size_t face = 0; face < 2; ++face) {
            auto& fence = fences_[side * 2 + face];
            fence = std::make_unique<Model>();
            fence->Initialize("plane");
            fence->SetTexture("white_x16.png");
            fence->SetColor({1.0f, 0.4f, 0.05f, 0.4f});
            fence->SetScale({halfSize, halfHeight, 1.0f});
            fence->SetTranslate(fencePositions[side]);
            fence->SetRotate({0.0f, (side < 2 ? 0.0f : MathUtils::F_PI * 0.5f)
                + static_cast<float>(face) * MathUtils::F_PI, 0.0f});
            fence->Update();
        }
    }

    mouseCursor_ = std::make_unique<Line>();
    mouseCursor_->Initialize();
    mouseCursor_->SetName("MouseCursor");
    mouseCursor_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    cursorVisible_ = false;
}

void PlayScene::Update() {
    constexpr Vector3 shadowLightOffset{0.0f, 10.0f, 0.0f};

    input_.Update();

    const float deltaTime = Time::GetDeltaTime();

    // タワーが落ちたらリザルトへ。シーンは切り替えず画面の上へシートを重ねるだけなので、
    // 負けた瞬間の状況がそのまま背景として残る
    if (mainTower_ && mainTower_->IsDestroyed() && !resultOverlay_->IsActive()) {
        survivalTimeManager_->SetCounting(false);
        // ゲーム中の UI は畳む。文字はスプライトより手前に描かれる仕組みなので、
        // 残すと暗幕が効かず、リザルトより明るいまま浮いてしまう
        towerHpGauge_->SetVisible(false);
        scoreManager_->SetVisible(false);
        survivalTimeManager_->SetVisible(false);
        comboManager_->SetVisible(false);
        resultOverlay_->Show(survivalTimeManager_->GetElapsedSeconds(),
                             scoreManager_->GetScore());
    }

    // リザルト中はゲーム側へ渡す経過時間を 0 にして進行だけを止める。
    // Update 自体は呼び続けるので描画に必要な行列は保たれ、背景は静止画として残る
    const bool playing = !resultOverlay_->IsActive();
    const float gameDelta = playing ? deltaTime : 0.0f;

    // 選択判定・カーソル・描画に同じカメラ行列を使う。

    if (playing) {
        UpdateTowerSelection();
    } else {
        // リザルト中は操作を受け付けない。掴んでいたレーザーとカーソルを外しておく
        laser_->ClearTarget();
        towerManager_->SetHoveredTower(nullptr);
        cursorVisible_ = false;
    }

    playerCamera_->Update(*player_, deltaTime);
    towerManager_->Update(deltaTime);
    if (MainTower* switchedMainTower = towerManager_->ConsumeMainTowerSwitch()) {
        const Vector3& target = switchedMainTower->GetPosition();
        enemyManager_->SetTargetPosition(target.x, target.z);
        enemyManager_->SetMainTower(switchedMainTower);
        towerHpGauge_->SetTarget(switchedMainTower);
    }
    UpdateTowerSelection();

    player_->SetGrappleTarget(laser_->GetConnectedTarget());
    player_->Update(gameDelta);
    const Vector3& playerVelocity = player_->GetVelocity();
    const float playerSpeed = std::hypot(playerVelocity.x, playerVelocity.z);
    laser_->UpdateSpeedMultipliers(playerSpeed, player_->GetSwingMaxSpeed());
    Singleton<LightManager>::GetInstance()->SetPosition(
        player_->GetPosition() + shadowLightOffset);
    enemyManager_->Update(gameDelta);
    laser_->Update();
    scoreManager_->Update(gameDelta);
    survivalTimeManager_->Update(gameDelta);
    comboManager_->Update(gameDelta);
    towerHpGauge_->Update(gameDelta);
    floor_->Update();
    for (const auto& fence : fences_) fence->Update();

    // リザルトだけは止めていない実時間で動かす
    resultOverlay_->Update(deltaTime);
    DrawHud();
}

void PlayScene::Draw() {
    player_->Draw();
    enemyManager_->Draw();
    towerManager_->Draw();
    laser_->Draw();
    floor_->Draw();
    for (const auto& fence : fences_) fence->Draw();


    // UI は 3D の描画がすべて終わったあとに重ねる

    if (cursorVisible_) mouseCursor_->Draw();
}

void PlayScene::UpdateTowerSelection() {
    const auto mouse = Singleton<Input>::GetInstance();
    const auto screen = Singleton<Screen>::GetInstance();
    const auto camera = Singleton<CameraController>::GetInstance()->GetActive();
    const Vector2 position = mouse->GetMousePosition();
    const float width = screen->Width();
    const float height = screen->Height();

    bool mouseAvailable = camera && width > 0.0f && height > 0.0f
        && position.x >= 0.0f && position.x < width
        && position.y >= 0.0f && position.y < height;
    // フォーカス喪失時も接続を解除する（Input側が前フレーム値を保持する場合への対策）。
    DWORD foregroundProcess = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
    const bool hasFocus = foregroundProcess == GetCurrentProcessId();
    mouseAvailable = mouseAvailable && hasFocus;
#ifdef _DEBUG
    if (const auto* context = ImGui::GetCurrentContext(); context && context->HoveredWindow) {
        mouseAvailable = mouseAvailable && std::string(context->HoveredWindow->Name) == "Scene";
    }
#endif

    mouseCursor_->Clear();
    cursorVisible_ = mouseAvailable;
    Tower* hovered = nullptr;
    if (mouseAvailable) {
        const Matrix4x4 inverseViewProjection = camera->GetViewProjection().Inverse();
        const auto unproject = [&](float _x, float _y, float _depth) {
            return MathUtils::Matrix::Transform(
                Vector3{2.0f * _x / width - 1.0f, 1.0f - 2.0f * _y / height, _depth},
                inverseViewProjection);
        };
        const Vector3 nearPosition = unproject(position.x, position.y, 0.0f);
        const Vector3 farPosition = unproject(position.x, position.y, 1.0f);
        const Vector3 rayDirection = farPosition - nearPosition;
        hovered = towerManager_->PickTower(nearPosition, rayDirection.Normalize(), rayDirection.Length());

        // スクリーン上で半径5pxの丸を、手前の平面に逆投影して描画する。
        constexpr int segments = 24;
        constexpr float radius = 5.0f;
        for (int i = 0; i < segments; ++i) {
            const float a = 2.0f * MathUtils::F_PI * static_cast<float>(i) / segments;
            const float b = 2.0f * MathUtils::F_PI * static_cast<float>(i + 1) / segments;
            mouseCursor_->AddLine(
                unproject(position.x + std::cos(a) * radius, position.y + std::sin(a) * radius, 0.01f),
                unproject(position.x + std::cos(b) * radius, position.y + std::sin(b) * radius, 0.01f));
        }
        mouseCursor_->Update();
    }

    if (hovered) assistedTower_ = hovered;
    // カーソルがSceneやタワーから外れても保持。ボタン解放またはフォーカス喪失で解除。
    if (!hasFocus || !mouse->IsMousePress(0)) {
        laser_->ClearTarget();
    } else if (mouseAvailable && mouse->IsMouseTrigger(0)) {
        // 押し始めたTowerを保持。ドラッグで別Towerへ乗り換えない。
        if (assistedTower_) laser_->SetTarget(assistedTower_);
        else laser_->ClearTarget();
    }
}


void PlayScene::DrawHud() {

    // 接続中は選択オーバーレイを解除し、接続解除後に選択アシスト表示へ戻す。
    towerManager_->SetHoveredTower(
        laser_->GetConnectedTarget() ? nullptr : assistedTower_);
    towerManager_->SetConnectedTower(
        static_cast<const Tower*>(laser_->GetConnectedTarget()));


    towerHpGauge_->Draw();
    scoreManager_->Draw();
    survivalTimeManager_->Draw();
    comboManager_->Draw();

    // 暗幕はいちばん最後。ここまでに積んだ UI ごと暗くして、シートを最前面に置く
    resultOverlay_->Draw();
}
