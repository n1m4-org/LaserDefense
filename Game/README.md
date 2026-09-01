# DarkRun (Game Layer)

このREADMEは `Game/` ディレクトリの実装内容に限定した説明です。  
エンジン実装の説明は含めず、ゲーム固有ロジックのみを対象にしています。

## 概要
- タイトル: `DarkRun`
- ジャンル: 3Dアクション
- 初期シーン: `title`
- 目標: 敵を倒して進行し、クリアまたはゲームオーバーに遷移

設定・シーン登録は [MyGame.cpp](D:/repos/PortfolioGame/Game/MyGame.cpp) で行っています。

## ゲーム層の主な責務
- シーン進行: タイトル / ゲーム / リザルトの遷移
- プレイヤー制御: 移動・攻撃・スキル
- 敵制御: 行動ロジック、追従・接敵
- 戦闘管理: コライダー連携、ダメージ、HP/撃破数UI
- 演出管理: カメラワーク、ポストエフェクト適用、UI表示

## 実装上のアピールポイント
### 1. 振る舞い差し替え可能な移動設計
- `IMovementBehavior` を基点に移動ロジックを差し替え
- `Walk / Dash / Flash` を同一インターフェースで扱える
- 優先度で実行挙動を切り替える構成

対象:
- [IMovementBehavior.hpp](D:/repos/PortfolioGame/Game/Player/Movement/IMovementBehavior.hpp)
- [Movement.hpp](D:/repos/PortfolioGame/Game/Player/Movement/Movement.hpp)
- [Movement.cpp](D:/repos/PortfolioGame/Game/Player/Movement/Movement.cpp)

### 2. Command ベースの敵行動拡張
- `ICommand` で移動命令を抽象化
- 敵ごとにコマンドを差し替えて行動を変更可能

対象:
- [ICommand.hpp](D:/repos/PortfolioGame/Game/Command/ICommand.hpp)
- [ToTargetCommand.hpp](D:/repos/PortfolioGame/Game/Command/Move/ToTargetCommand.hpp)

### 3. スキルエンティティの独立管理
- `ISkillEntity` 経由で生成・更新・寿命管理
- 新スキルを追加しやすい構造

対象:
- [ISkillEntity.hpp](D:/repos/PortfolioGame/Game/Skill/ISkillEntity.hpp)
- [SkillManager.hpp](D:/repos/PortfolioGame/Game/Skill/SkillManager.hpp)
- [BlackHole.hpp](D:/repos/PortfolioGame/Game/Skill/BlackHole/BlackHole.hpp)

### 4. ポストエフェクトのゲーム側注入
- ゲーム層で `PostEffectFactory` を登録し、演出を差し替え可能
- シーンや状態に応じた演出の切り替えに対応

対象:
- [PostEffectFactory.cpp](D:/repos/PortfolioGame/Game/PostEffectFactory.cpp)
- [PostEffectFactory.hpp](D:/repos/PortfolioGame/Game/Factory/PostEffectFactory.hpp)

## ディレクトリガイド (Game)
- `Scene/`: 各シーン実装
- `Player/`: プレイヤー本体、入力、移動モジュール
- `Enemy/`: 敵本体、群管理、行動制御
- `Skill/`: スキル実体と管理
- `Ui/`: HUD/メニュー/ガイド表示
- `Command/`: 行動コマンド実装
- `Performance/`: Intro/Outro など演出系

## 拡張しやすいポイント
1. 新移動の追加: `IMovementBehavior` 実装を追加して `Movement` に登録
2. 新スキルの追加: `ISkillEntity` 継承クラスを実装して `SkillManager` へ追加
3. 新演出の追加: `PostEffectFactory` に生成分岐を追加
4. 新シーンの追加: `IScene` 継承クラスを作成し `MyGame::Register()` に登録

## 実行時の確認観点 (ゲーム側)
- シーン遷移が正しくつながるか (`title -> game -> result`)
- プレイヤー移動優先度が意図通りか (Dash/Flashの割り込み)
- スキル生成と寿命破棄が正常か
- UI (HP/キル数/ガイド/ポーズ) が状態同期できているか

