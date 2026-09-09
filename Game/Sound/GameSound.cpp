#include "GameSound.hpp"

#include <array>
#include <cstddef>
#include <vector>

#include "Handle.hpp"
#include "Log.hpp"
#include "PlaybackHandle.hpp"

namespace {
    /// 1つの効果音の定義
    struct SeDefinition {
        const char* path; //!< 実行ファイルからの相対パス。Audio だけは他のアセットと違いフルパスで指定する
        float volume;     //!< 既定の音量。ここを触れば呼び出し側を変えずにバランスを取れる
    };

    /// GameSound::Se と並び順を一致させること
    constexpr std::array<SeDefinition, static_cast<std::size_t>(GameSound::Se::Count)> SE_DEFINITIONS = {{
        {"Assets/Audio/decide.mp3",        0.80f}, // Decide
        {"Assets/Audio/enemy_hit.mp3",     0.50f}, // EnemyHit
        {"Assets/Audio/enemy_death.mp3",   0.35f}, // EnemyDeath  小さめ
        {"Assets/Audio/tower_damage.mp3",  0.70f}, // TowerDamage
        {"Assets/Audio/tower_switch.mp3",  0.80f}, // TowerSwitch
        {"Assets/Audio/laser_connect.mp3", 0.60f}, // LaserConnect
        {"Assets/Audio/result.mp3",        0.90f}, // ResultTransition
        {"Assets/Audio/dash.mp3",          0.55f}, // Dash
        {"Assets/Audio/warning.mp3",       0.65f}, // SwitchWarning
        {"Assets/Audio/laser_loop.mp3",    0.25f}, // LaserLoop  小さめ
        {"Assets/Audio/maou_bgm_cyber45.mp3",   0.40f}, // TitleBgm
        {"Assets/Audio/maou_bgm_neorock80.mp3", 0.35f}, // PlayBgm  効果音に埋もれない程度に控えめ
        {"Assets/Audio/maou_se_onepoint15.mp3",         0.80f}, // GimmickClear
        {"Assets/Audio/maou_se_battle_explosion08.mp3", 0.85f}, // GimmickTowerDamage
        {"Assets/Audio/maou_se_sound_switch01.mp3",     0.60f}, // GimmickColorStep
        {"Assets/Audio/maou_se_8bit26.mp3",             0.22f}, // GimmickOrbitLoop  鳴りっぱなしなので小さめ
        {"Assets/Audio/maou_se_8bit24.mp3",             0.70f}, // GimmickEnemySpawn
    }};

    std::array<Audio::Handle, SE_DEFINITIONS.size()> handles{};

    /// 再生中のハンドル。鳴り終わったものを Update() で回収するために持つ
    std::vector<Audio::PlaybackHandle> playbacks;

    /// ループ再生中のハンドル。単発とは寿命が違うので分けて持つ
    std::array<Audio::PlaybackHandle, SE_DEFINITIONS.size()> loops{};

    bool loaded = false;
} // namespace

void GameSound::Load() {
    if (loaded) return;

    for (std::size_t index = 0; index < handles.size(); ++index) {
        if (const Audio::Result result = handles[index].Load(SE_DEFINITIONS[index].path); !result) {
            // 読み込めなくても Play() が無音になるだけなので、記録して進む
            Log::Send(Log::Level::ERR, "[GameSound] " + result.what());
        }
    }

    loaded = true;
}

void GameSound::Play(const Se _se, const float _pitch) {
    const auto index = static_cast<std::size_t>(_se);
    if (index >= handles.size()) return;

    const Audio::PlaybackHandle playback = handles[index].Play(SE_DEFINITIONS[index].volume, _pitch);
    if (playback.IsValid()) playbacks.push_back(playback);
}

void GameSound::StartLoop(const Se _se) {
    const auto index = static_cast<std::size_t>(_se);
    if (index >= handles.size()) return;
    // 既に鳴っているなら鳴らし直さない。呼び出し側が状態を持たずに済む
    if (loops[index].IsPlaying()) return;

    loops[index] = handles[index].Play(SE_DEFINITIONS[index].volume, 1.0f, 0.0f, true);
}

void GameSound::StopLoop(const Se _se) {
    const auto index = static_cast<std::size_t>(_se);
    if (index >= loops.size()) return;
    if (!loops[index].IsValid()) return;

    loops[index].Stop();
    loops[index] = {};
}

void GameSound::Update() {
    // 鳴り終わっただけの再生はミキサー側で回収されないため、こちらから Stop() して片付ける。
    // ここに積むのはループもポーズもしない単発の効果音だけなので、
    // 「鳴っていない = 鳴り終わった」と判断してよい
    std::erase_if(playbacks, [](const Audio::PlaybackHandle& _playback) {
        if (_playback.IsPlaying()) return false;
        _playback.Stop();
        return true;
    });
}

void GameSound::StopAll() {
    for (const Audio::PlaybackHandle& playback : playbacks) {
        playback.Stop();
    }
    playbacks.clear();

    for (Audio::PlaybackHandle& loop : loops) {
        if (!loop.IsValid()) continue;
        loop.Stop();
        loop = {};
    }
}
