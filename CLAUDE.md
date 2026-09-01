# CLAUDE.md

このファイルは、このリポジトリでコードを操作する際のClaude Code (claude.ai/code) へのガイダンスを提供します。

## ビルドコマンド

これはVisual Studio 2022を使用した2層ビルドシステムのC++20 DirectX12ベースのゲームエンジンです。

```bash
# ソリューション全体をビルド
msbuild GameTemplate.sln /p:Configuration=Debug /p:Platform=x64

# エンジンライブラリのみをビルド
msbuild Engine/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64

# ゲーム実行ファイルのみをビルド  
msbuild Game/Game.vcxproj /p:Configuration=Debug /p:Platform=x64

# ゲームを実行
.\out\x64\Debug\Game\Game.exe

# クリーンビルド
msbuild GameTemplate.sln /t:Clean /p:Configuration=Debug /p:Platform=x64
```

## アーキテクチャ概要

### コアフレームワーク構造
- **エンジンライブラリ** (`Engine/`) - 静的ライブラリ（.lib）としてコンパイルされるコアシステム
- **ゲーム実行ファイル** (`Game/`) - Engine.libとリンクするゲーム実装
- **コンポーネントベースアーキテクチャ** 専門的なマネージャークラスを使用

### 主要コンポーネント
- `Framework.hpp` - メインアプリケーションループと初期化
- `IGame` - ゲーム実装のベースインターフェース（`Game/MyGame.cpp`で継承）
- `IScene` - ゲームシーンのベースインターフェース
- `SceneSwitcher` - シーン遷移の管理
- `DirectXAdapter` - DirectX12の初期化とデバイス管理
- `ResourceRepository` - アセットの読み込みとキャッシュシステム

### マネージャークラス
- `CameraManager` - カメラシステム管理
- `LightManager` - ライティングシステム
- `TextureManager` - テクスチャの読み込みと管理
- `ModelRepository` / `MeshRepository` - 3Dモデルアセットのキャッシュ

### アセットパイプライン
アセットは`Assets/`ディレクトリから読み込まれます：
- **モデル**: `Assets/Resources/`内の`.gltf`、`.obj`、`.blend`ファイル
- **テクスチャ**: `.png`ファイル
- **シェーダー**: `Assets/Shaders/`内の`.hlsl`ファイル
- **フォント**: `Assets/Fonts/`内の`.ttf`ファイル
- **データ**: `Assets/Data/`内のJSON設定ファイル

サポートされているローダー: `GltfLoader`（アニメーション付き）、`ObjLoader`

### 開発ワークフロー
1. `Game/MyGame.cpp`で`IGame`を継承してゲームロジックを実装
2. `IScene`を継承してシーンを作成
3. `Game/SceneFactory.cpp`でシーンを登録
4. レンダリング、入力、アセット管理にはFrameworkマネージャークラスを使用
5. アセットはResourceRepositoryによって自動的にキャッシュされます

## コーディング規約
- **C++20標準**
- **4スペースインデント**
- **UTF-8エンコーディング、CRLF改行**
- 直接DirectX呼び出しよりもFrameworkマネージャーを使用
- コンポーネントベースデザインパターンを優先

## 依存関係
- **DirectX 12**（Windows 10 SDK必須）
- **vcpkg** パッケージ管理用
- **imgui** デバッグUI用
- **DirectXTex** テクスチャ処理用
- **lwlog** ログ出力用

## よくある問題
- Windows 10 SDKとVisual Studio 2022 v143ツールセットがインストールされていることを確認
- ビルド順序: EngineプロジェクトがGameプロジェクトより先にコンパイルされる必要があります
- 自動読み込みのため、アセットは正しい`Assets/`サブディレクトリに配置する必要があります