#ifndef GAME_SOUND_HPP_
#define GAME_SOUND_HPP_

/// ゲーム内で鳴らす効果音をまとめて扱う。
///
/// 実ファイルと音量は GameSound.cpp の SE_DEFINITIONS が持つので、
/// 素材の差し替えと音量調整は呼び出し側に触れずそこだけで完結する。
/// Enemy や TowerManager など、シーンから遠い場所からも鳴らすため名前空間で公開している。
namespace GameSound {
    /// 効果音の種類。GameSound.cpp の SE_DEFINITIONS と並び順を一致させること
    enum class Se {
        Decide,           //!< 決定音。タイトルの開始とタイトルへの復帰で共用
        EnemyHit,         //!< 敵にレーザーが当たった
        EnemyDeath,       //!< 敵を倒した
        TowerDamage,      //!< メインタワーが被弾した
        TowerSwitch,      //!< メインタワーが切り替わった
        LaserConnect,     //!< レーザーがタワーへ接続した
        ResultTransition, //!< リザルトへ移った
        Dash,             //!< ダッシュした
        SwitchWarning,    //!< メインタワー切替の予告
        LaserLoop,        //!< レーザー接続中に鳴らし続ける音
        TitleBgm,         //!< タイトルの BGM
        PlayBgm,          //!< ゲーム中の BGM

        Count
    };

    /// @brief 全ての効果音を読み込む
    /// @note 2回目以降の呼び出しは何もしない。読み込みに失敗した音は無音になるだけで進行は止まらない
    void Load();

    /// @brief 効果音を1回鳴らす
    /// @param _se 鳴らす効果音
    /// @param _pitch 再生ピッチ。1.0 が原音。連続で鳴る音は少し散らすと単調にならない
    void Play(Se _se, float _pitch = 1.0f);

    /// @brief 効果音をループ再生し始める
    /// @param _se 鳴らす効果音
    /// @note 既に鳴っていれば何もしないので、状態を持たず毎フレーム呼んでよい
    void StartLoop(Se _se);

    /// @brief ループ再生を止める
    /// @param _se 止める効果音
    /// @note 鳴っていなければ何もしないので、状態を持たず毎フレーム呼んでよい
    void StopLoop(Se _se);

    /// @brief 鳴り終わった再生を回収する
    /// @note 毎フレーム呼ぶこと。呼ばないと鳴り終わった再生がミキサー内に残り続ける
    void Update();

    /// @brief 鳴っている効果音を全て止める。ループ再生も含む
    void StopAll();
} // namespace GameSound

#endif // GAME_SOUND_HPP_
