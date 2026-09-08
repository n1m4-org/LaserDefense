#define NOMINMAX
#include "PlayScene.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <variant>
#include <vector>

#include "Camera/Controller/CameraController.hpp"
#include "Camera/PlayerCamera.hpp"
#include "GameObject/Player/Player.h"
#include "Laser/Laser.hpp"
#include "Light/LightManager.hpp"
#include "Input.hpp"
#include "Json/JsonParams.hpp"
#include "Math/MathUtils.hpp"
#include "Pattern/Singleton.hpp"
#include "Screen/Screen.hpp"
#include "Texture/TextureManager.hpp"
#include "Time/Time.hpp"
#include "Tower/MainTower.hpp"
#include "Ui/UiAnimPresets.hpp"
#include "src/ParticleSystem/ParticleSystem.hpp"

#ifdef _DEBUG
#include "imgui_internal.h"
#endif

namespace {
    /// リザルトのUIで使うキャンバス名(Assets/Data/UI/Result.json)
    constexpr const char* RESULT_CANVAS_NAME = "Result";
    /// 「タイトルへ戻る」ボタンに割り当てるアクションキー
    constexpr const char* ACTION_TO_TITLE = "Result.ToTitle";
    constexpr const char* PLAYER_SPEED_EFFECT_TEMPLATE = "PlayerSpeedEffect";
    constexpr const char* PLAYER_SPEED_EFFECT_SPAWN = "PlayerSpeedEffectSpawn";
    constexpr const char* PLAYER_DASH_EFFECT_TEMPLATE = "PlayerDashEffect";
    constexpr const char* PLAYER_DASH_EFFECT_SPAWN = "PlayerDashEffectSpawn";

    /// リザルトが出てから戻る操作を受け付けるまでの秒数
    /// @note タワーが落ちた瞬間はレーザーのために左クリックを押していることが多く、
    ///       すぐ受け付けると成績を見る前にタイトルへ飛んでしまう
    constexpr float RESULT_RETURN_DELAY = 1.2f;
} // namespace

PlayScene::PlayScene() = default;
PlayScene::~PlayScene() {
    playerSpeedEffectHandle_.Stop();
}

void PlayScene::InitializePlayerSpeedEffect() {
    const auto particleSystem = Particle();
    if (!particleSystem) return;

    playerSpeedParticleState_ = std::make_shared<PlayerSpeedParticleState>();
    const std::weak_ptr<PlayerSpeedParticleState> state = playerSpeedParticleState_;
    particleSystem->RegisterSpawnFunc(PLAYER_SPEED_EFFECT_SPAWN,
        [state](const Vector3& _center, Vector3& _position, Vector3& _velocity) {
            const auto effectState = state.lock();
            if (!effectState) {
                _position = _center;
                _velocity = {};
                return;
            }
            const Vector3 direction = effectState->direction;
            const Vector3 side{-direction.z, 0.0f, direction.x};
            _position = _center - direction * MathUtils::Random(0.35f, 0.75f)
                + side * MathUtils::Random(-0.2f, 0.2f)
                + Vector3{0.0f, MathUtils::Random(-0.12f, 0.12f), 0.0f};
            _velocity = direction * MathUtils::Random(-3.0f, -1.0f)
                + side * MathUtils::Random(-0.6f, 0.6f);
        });

    ParticleSystem::EmitterConfig emitter;
    emitter.texture = "white_x16.png";
    emitter.frequency = 0.025f;
    emitter.duration = std::numeric_limits<float>::max();
    emitter.spawnCount = 1;
    emitter.size = {0.14f, 0.14f, 0.14f};
    emitter.particleLifetime = 0.2f;
    emitter.spawnFuncKey = PLAYER_SPEED_EFFECT_SPAWN;
    emitter.colorKeys = {
        GradientKey<Vector4>{0.0f, {0.7f, 0.9f, 1.0f, 0.9f}},
        GradientKey<Vector4>{1.0f, {0.2f, 0.55f, 1.0f, 0.0f}}
    };
    emitter.sizeKeys = {
        GradientKey<Vector3>{0.0f, {0.14f, 0.14f, 0.14f}},
        GradientKey<Vector3>{1.0f, {0.03f, 0.03f, 0.03f}}
    };

    ParticleSystem::Template speedEffect;
    speedEffect.emitters.push_back(emitter);
    particleSystem->Register(PLAYER_SPEED_EFFECT_TEMPLATE, speedEffect, true);
}

void PlayScene::InitializePlayerDashEffect() {
    const auto particleSystem = Particle();
    if (!particleSystem) return;

    particleSystem->RegisterSpawnFunc(PLAYER_DASH_EFFECT_SPAWN,
        [](const Vector3& _center, Vector3& _position, Vector3& _velocity) {
            const float angle = MathUtils::Random(0.0f, 6.2831853f);
            const Vector3 direction{std::cos(angle), 0.0f, std::sin(angle)};
            _position = _center + direction * MathUtils::Random(0.0f, 0.18f)
                + Vector3{0.0f, MathUtils::Random(-0.1f, 0.1f), 0.0f};
            _velocity = direction * MathUtils::Random(5.0f, 10.0f)
                + Vector3{0.0f, MathUtils::Random(0.5f, 2.0f), 0.0f};
        });

    ParticleSystem::EmitterConfig emitter;
    emitter.texture = "white_x16.png";
    emitter.frequency = 0.0f;
    emitter.duration = 0.0f;
    emitter.spawnCount = 18;
    emitter.size = {0.4f, 0.4f, 0.4f};
    emitter.particleLifetime = 0.8f;
    emitter.spawnFuncKey = PLAYER_DASH_EFFECT_SPAWN;
    emitter.colorKeys = {
        GradientKey<Vector4>{0.0f, {1.0f, 0.95f, 0.15f, 1.0f}},
        GradientKey<Vector4>{1.0f, {1.0f, 0.95f, 0.15f, 0.0f}}
    };

    ParticleSystem::Template dashEffect;
    dashEffect.emitters.push_back(emitter);
    particleSystem->Register(PLAYER_DASH_EFFECT_TEMPLATE, dashEffect, true);
}

void PlayScene::EmitPlayerDashEffect() {
    const auto particleSystem = Particle();
    if (!particleSystem) return;
    particleSystem->Emit(
        PLAYER_DASH_EFFECT_TEMPLATE, player_->GetPosition() + player_->GetModelOffset());
}

void PlayScene::UpdatePlayerSpeedEffect(float _speed, float _maxSpeed, float _deltaTime) {
    const auto particleSystem = Particle();
    const Vector3 velocity = player_->GetVelocity();
    const float horizontalSpeed = std::hypot(velocity.x, velocity.z);
    const bool emit = particleSystem && playerSpeedParticleState_
        && std::isfinite(_deltaTime) && _deltaTime > 0.0f
        && std::isfinite(_speed) && std::isfinite(_maxSpeed) && _maxSpeed > 0.0001f
        && _speed > _maxSpeed * 0.3f && horizontalSpeed > 0.0001f;
    if (!emit) {
        playerSpeedEffectHandle_.Stop();
        playerSpeedEffectHandle_ = {};
        return;
    }

    playerSpeedParticleState_->direction = {
        velocity.x / horizontalSpeed, 0.0f, velocity.z / horizontalSpeed};
    const Vector3 emitterPosition = player_->GetPosition() + player_->GetModelOffset();
    if (!playerSpeedEffectHandle_.IsValid()) {
        playerSpeedEffectHandle_ = particleSystem->Emit(
            PLAYER_SPEED_EFFECT_TEMPLATE, emitterPosition);
    }
    playerSpeedEffectHandle_.SetPosition(emitterPosition);
}

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
    // リザルトから戻る先。Change()を呼んだタイミングで切り替わる
    next_ = "Title";

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
    InitializePlayerSpeedEffect();
    InitializePlayerDashEffect();
    playerCamera_ = std::make_unique<PlayerCamera>();
    playerCamera_->Initialize(*player_);
    Singleton<LightManager>::GetInstance()->SetPosition(
        player_->GetPosition() + shadowLightOffset);

    towerManager_ = std::make_unique<TowerManager>();
    towerManager_->Initialize();

    assistedTower_ = nullptr;
    mainTower_ = towerManager_->AddMainTower(mainTowerPosition);
    std::vector<Vector3> towerPositions{mainTowerPosition};
    towerPositions.reserve(9);

    // 3×3の等間隔配置。中央をメインタワーとし、合計9本にする。
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            if (row == 1 && column == 1) continue;
            const float x = -towerPosition + static_cast<float>(column) * towerPosition;
            const float z = -towerPosition + static_cast<float>(row) * towerPosition;
            const Vector3 position{x, 0.0f, z};
            towerManager_->AddTower(position);
            towerPositions.push_back(position);
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

    // タワーHPゲージへ共有HPを所有するTowerManagerを渡す
    towerHpGauge_ = std::make_unique<TowerHpGauge>();
    towerHpGauge_->Initialize();
    towerHpGauge_->SetTarget(towerManager_.get());

    // リザルトはシーンを跨がず、この画面の上に重ねて出す
    resultOverlay_ = std::make_unique<ResultOverlay>();
    resultOverlay_->Initialize();

    SetupResultCanvas();
    mainTowerIndicator_ = std::make_unique<MainTowerIndicator>();
    mainTowerIndicator_->Initialize();

    enemyManager_ = std::make_unique<EnemyManager>();
    enemyManager_->Initialize(Particle());
    enemyManager_->SetTargetPosition(mainTowerPosition.x, mainTowerPosition.z);
    enemyManager_->SetSpawnExclusionPositions(towerPositions);
    enemyManager_->SetScoreManager(scoreManager_.get());
    enemyManager_->SetComboManager(comboManager_.get());
    // 敵に到達されたときダメージを受けるタワーを渡す
    enemyManager_->SetTowerManager(towerManager_.get());

    gimmickManager_ = std::make_unique<GimmickManager>();
    gimmickManager_->Initialize(GimmickContext{
        player_.get(), towerManager_.get(), enemyManager_.get()});

    shockwave_ = std::make_unique<Model>();
    shockwave_->Initialize("plane");
    shockwave_->SetTexture("circle2.png");
    shockwave_->SetRotate({-MathUtils::F_PI * 0.5f, 0.0f, 0.0f});
    shockwaveTime_ = SHOCKWAVE_DURATION;

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

    for (std::size_t i = 0; i < reticleFills_.size(); ++i) {
        const bool horizontal = i < 2;
        reticleOutlines_[i].Initialize("white_x16.png");
        reticleOutlines_[i].SetAnchorPoint({0.5f, 0.5f});
        reticleOutlines_[i].SetSize(
            horizontal ? Vector2{10.0f, 5.0f} : Vector2{5.0f, 10.0f});
        reticleOutlines_[i].SetColor({0.0f, 0.0f, 0.0f, 1.0f});

        reticleFills_[i].Initialize("white_x16.png");
        reticleFills_[i].SetAnchorPoint({0.5f, 0.5f});
        reticleFills_[i].SetSize(
            horizontal ? Vector2{8.0f, 3.0f} : Vector2{3.0f, 8.0f});
        reticleFills_[i].SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    }
    reticlePositionInitialized_ = false;
    reticleHoverProgress_ = 0.0f;
    cursorVisible_ = false;
    clickTowerGuide_.Initialize("Click Tower", 0.0f, 0.0f, 22.0f);
    clickTowerGuide_.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    clickTowerGuide_.SetVisible(false);
    clickTowerGuideElapsed_ = 0.0f;
    clickTowerGuideVisible_ = false;
    dashInputGuide_.Initialize("RightClick", 32.0f, 0.0f, 26.0f);
    dashInputGuide_.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    dashActionGuide_.Initialize("Dash", 32.0f, 0.0f, 38.0f);
    dashActionGuide_.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    dashCooldownGaugeFrame_.Initialize("white_x16.png");
    dashCooldownGaugeFrame_.SetAnchorPoint({0.0f, 0.0f});
    dashCooldownGaugeFrame_.SetColor({0.0f, 0.0f, 0.0f, 0.62f});
    dashCooldownGauge_.Initialize("white_x16.png");
    dashCooldownGauge_.SetAnchorPoint({0.0f, 0.0f});
    dashCooldownGauge_.SetColor({0.05f, 0.35f, 1.0f, 0.72f});
    dashCooldownRatio_ = 1.0f;
    dashCooldownFlashTime_ = 0.0f;
}

void PlayScene::Update() {
    constexpr Vector3 shadowLightOffset{0.0f, 10.0f, 0.0f};

    input_.Update();

    const float deltaTime = Time::GetDeltaTime();

    // タワーが落ちたらリザルトへ。シーンは切り替えず画面の上へシートを重ねるだけなので、
    // 負けた瞬間の状況がそのまま背景として残る
    if (towerManager_->IsDestroyed() && !resultOverlay_->IsActive()) {
        survivalTimeManager_->SetCounting(false);
        // ゲーム中の UI は畳む。文字はスプライトより手前に描かれる仕組みなので、
        // 残すと暗幕が効かず、リザルトより明るいまま浮いてしまう
        towerHpGauge_->SetVisible(false);
        scoreManager_->SetVisible(false);
        survivalTimeManager_->SetVisible(false);
        comboManager_->SetVisible(false);
        resultOverlay_->Show(survivalTimeManager_->GetElapsedSeconds(),
                             scoreManager_->GetScore());
        resultElapsed_ = 0.0f;
        returnAccepting_ = false;
    }

    // リザルト中はゲーム側へ渡す経過時間を 0 にして進行だけを止める。
    // Update 自体は呼び続けるので描画に必要な行列は保たれ、背景は静止画として残る
    const bool playing = !resultOverlay_->IsActive();
    const float gameDelta = playing ? deltaTime : 0.0f;
    clickTowerGuideElapsed_ = std::min(
        clickTowerGuideElapsed_ + gameDelta, CLICK_TOWER_GUIDE_DURATION);
    if (playing) {
        const Vector2 mousePosition = Singleton<Input>::GetInstance()->GetMousePosition();
        if (std::isfinite(mousePosition.x) && std::isfinite(mousePosition.y)) {
            if (!reticlePositionInitialized_) {
                reticlePosition_ = mousePosition;
                reticlePositionInitialized_ = true;
            } else {
                const float blend = 1.0f - std::exp(
                    -RETICLE_FOLLOW_SPEED * std::min(gameDelta, 0.1f));
                reticlePosition_.x += (mousePosition.x - reticlePosition_.x) * blend;
                reticlePosition_.y += (mousePosition.y - reticlePosition_.y) * blend;
            }
        }
    }

    // 選択判定・カーソル・描画に同じカメラ行列を使う。

    if (!playing) {
        // リザルト中は操作を受け付けない。掴んでいたレーザーとカーソルを外しておく
        laser_->ClearTarget();
        towerManager_->SetHoveredTower(nullptr);
        assistedTower_ = nullptr;
        cursorVisible_ = false;
        clickTowerGuideVisible_ = false;
    }

    playerCamera_->Update(*player_, deltaTime);
    towerManager_->Update(gameDelta);
    if (MainTower* switchedMainTower = towerManager_->ConsumeMainTowerSwitch()) {
        // 衝撃波と敵の移動先に使う、現在の防衛対象を更新する。
        mainTower_ = switchedMainTower;
        const Vector3& target = switchedMainTower->GetPosition();
        if (playing) {
            enemyManager_->ApplyShockwave(target, SHOCKWAVE_RADIUS, SHOCKWAVE_SPEED);
            shockwaveTime_ = 0.0f;
            shockwave_->SetTranslate(target + Vector3{0.0f, 0.05f, 0.0f});
        }
        enemyManager_->SetTargetPosition(target.x, target.z);
    }
    if (playing) UpdateTowerSelection(gameDelta);

    player_->SetGrappleTarget(laser_->GetConnectedTarget());
    player_->Update(gameDelta);
    if (player_->ConsumeDashTriggered()) {
        EmitPlayerDashEffect();
    }
    dashCooldownRatio_ = player_->GetDashCooldownRatio();
    dashCooldownFlashTime_ = std::max(dashCooldownFlashTime_ - gameDelta, 0.0f);
    if (player_->ConsumeDashCooldownCompleted()) {
        dashCooldownFlashTime_ = DASH_COOLDOWN_FLASH_DURATION;
    }
    const Vector3& playerVelocity = player_->GetVelocity();
    const float playerSpeed = std::hypot(playerVelocity.x, playerVelocity.z);
    UpdatePlayerSpeedEffect(playerSpeed, player_->GetSwingMaxSpeed(), gameDelta);
    laser_->UpdateSpeedMultipliers(playerSpeed, player_->GetSwingMaxSpeed());
    enemyManager_->Update(gameDelta);
    gimmickManager_->Update(gameDelta);
    laser_->Update();
    scoreManager_->Update(gameDelta);
    survivalTimeManager_->Update(gameDelta);
    comboManager_->Update(gameDelta);
    towerHpGauge_->Update(gameDelta);
    const auto& defenseTargets = towerManager_->GetMainTowers();
    mainTowerIndicator_->Update(
        playing && !defenseTargets.empty() ? defenseTargets.front() : nullptr);
    if (shockwaveTime_ < SHOCKWAVE_DURATION) {
        const float t = shockwaveTime_ / SHOCKWAVE_DURATION;
        const float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        const float size = 0.1f + (SHOCKWAVE_RADIUS * 2.0f - 0.1f) * eased;
        shockwave_->SetScale({size, size, 1.0f});
        shockwave_->SetColor({1.0f, 0.5f, 0.0f, 1.0f - t});
        shockwave_->Update();
        shockwaveTime_ = std::min(shockwaveTime_ + gameDelta, SHOCKWAVE_DURATION);
    }
    floor_->Update();
    for (const auto& fence : fences_) fence->Update();

    // リザルトだけは止めていない実時間で動かす
    resultOverlay_->Update(deltaTime);
    UpdateResultReturn(deltaTime);
    DrawHud();
}

void PlayScene::Draw() {
    player_->Draw();
    enemyManager_->Draw();
    towerManager_->Draw();
    laser_->Draw();
    floor_->Draw();
    if (shockwaveTime_ < SHOCKWAVE_DURATION) shockwave_->Draw();
    gimmickManager_->Draw();
    for (const auto& fence : fences_) fence->Draw();


    // UI は 3D の描画がすべて終わったあとに重ねる

    if (cursorVisible_) {
        for (auto& outline : reticleOutlines_) outline.Draw();
        for (auto& fill : reticleFills_) fill.Draw();
    }
    if (!resultOverlay_->IsActive()) {
        dashCooldownGaugeFrame_.Draw();
        dashCooldownGauge_.Draw();
    }
}

void PlayScene::UpdateTowerSelection(float _deltaTime) {
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

    cursorVisible_ = mouseAvailable;
    clickTowerGuideVisible_ = mouseAvailable
        && clickTowerGuideElapsed_ < CLICK_TOWER_GUIDE_DURATION;
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

        const float hoverStep = std::max(_deltaTime, 0.0f) / RETICLE_HOVER_DURATION;
        reticleHoverProgress_ = hovered
            ? std::min(reticleHoverProgress_ + hoverStep, 1.0f)
            : std::max(reticleHoverProgress_ - hoverStep, 0.0f);
        const float hoverEase = reticleHoverProgress_ * reticleHoverProgress_
            * (3.0f - 2.0f * reticleHoverProgress_);
        const float offsetDistance = 9.0f + (6.0f - 9.0f) * hoverEase;
        const float rotation = MathUtils::F_PI * 0.25f * hoverEase;
        const Vector4 reticleColor{
            1.0f + (0.2f - 1.0f) * hoverEase,
            1.0f,
            1.0f + (0.35f - 1.0f) * hoverEase,
            1.0f};
        const float rotationCos = std::cos(rotation);
        const float rotationSin = std::sin(rotation);
        const std::array<Vector2, 4> offsets{{
            {-rotationCos * offsetDistance, -rotationSin * offsetDistance},
            {rotationCos * offsetDistance, rotationSin * offsetDistance},
            {rotationSin * offsetDistance, -rotationCos * offsetDistance},
            {-rotationSin * offsetDistance, rotationCos * offsetDistance}
        }};
        for (std::size_t i = 0; i < offsets.size(); ++i) {
            const Vector2 reticlePartPosition = reticlePosition_ + offsets[i];
            reticleOutlines_[i].SetPosition(reticlePartPosition);
            reticleFills_[i].SetPosition(reticlePartPosition);
            reticleOutlines_[i].SetRotation(rotation);
            reticleFills_[i].SetRotation(rotation);
            reticleFills_[i].SetColor(reticleColor);
            reticleOutlines_[i].Update();
            reticleFills_[i].Update();
        }

        clickTowerGuide_.SetPosition(
            reticlePosition_.x + 18.0f, reticlePosition_.y - 11.0f);
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


void PlayScene::SetupResultCanvas() {
    // アニメーションとアクションは Setup(=JSON読み込み)より先に登録する。
    // 読み込み時に ShowAnim / Events のキーから解決されるため、
    // 後から登録しても JSON の指定が効かない
    UiAnimPresets::RegisterAll(resultCanvas_);
    resultCanvas_.RegisterAction(ACTION_TO_TITLE, [this] { RequestReturnToTitle(); });

    resultCanvas_.Setup(RESULT_CANVAS_NAME);

    // 読み込み直後は表示状態なので、リザルトが出るまで閉じておく
    resultCanvas_.SetActive(false);
    resultElapsed_ = 0.0f;
    returnAccepting_ = false;
}

void PlayScene::UpdateResultReturn(float _deltaTime) {
    if (!resultOverlay_->IsActive()) return;

    resultElapsed_ += std::max(_deltaTime, 0.0f);

    if (!returnAccepting_) {
        // 受け付ける前は UI も出さない。出ていない案内を押せてしまう状態を作らない
        if (resultElapsed_ < RESULT_RETURN_DELAY) return;
        returnAccepting_ = true;
        // 一度 Inactive を挟むと Show から始まり、出現アニメとカーソルの初期化が走る
        resultCanvas_.SetActive(false);
        resultCanvas_.SetActive(true);
        return;
    }

    // ボタンを狙わなくても、スペースか左クリックだけで戻れるようにしておく
    if (input_.IsDecide()) RequestReturnToTitle();
}

void PlayScene::RequestReturnToTitle() {
    resultCanvas_.SetActive(false);
    Change();
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
    mainTowerIndicator_->Draw();
    clickTowerGuide_.SetVisible(clickTowerGuideVisible_);
    clickTowerGuide_.Draw();

    const float screenHeight = Singleton<Screen>::GetInstance()->Height();
    const bool showControlGuide = !resultOverlay_->IsActive();
    dashInputGuide_.SetPosition(32.0f, screenHeight - 100.0f);
    dashInputGuide_.SetVisible(showControlGuide);
    dashInputGuide_.Draw();
    dashActionGuide_.SetPosition(32.0f, screenHeight - 68.0f);
    dashActionGuide_.SetVisible(showControlGuide);
    dashActionGuide_.Draw();
    dashCooldownGaugeFrame_.SetPosition({18.0f, screenHeight - 114.0f});
    dashCooldownGaugeFrame_.SetSize({164.0f, 100.0f});
    dashCooldownGaugeFrame_.Update();

    dashCooldownGauge_.SetPosition({20.0f, screenHeight - 112.0f});
    dashCooldownGauge_.SetSize({160.0f * std::clamp(dashCooldownRatio_, 0.0f, 1.0f), 96.0f});
    const float flashRatio = DASH_COOLDOWN_FLASH_DURATION > 0.0f
        ? std::clamp(dashCooldownFlashTime_ / DASH_COOLDOWN_FLASH_DURATION, 0.0f, 1.0f)
        : 0.0f;
    dashCooldownGauge_.SetColor({
        0.05f + 0.95f * flashRatio,
        0.35f + 0.65f * flashRatio,
        1.0f,
        0.72f + 0.28f * flashRatio
    });
    dashCooldownGauge_.Update();

    // 暗幕はいちばん最後。ここまでに積んだ UI ごと暗くして、シートを最前面に置く
    resultOverlay_->Draw();
}
