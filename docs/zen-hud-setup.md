# ZEN HUD を使うためのファームウェアの準備

[ZEN HUD](https://github.com/boooowy/zen-hud) は、ZEN の状態（有効なレイヤー、押されているキー、
バッテリー、接続先）を macOS の画面に表示するアプリです。表示に使う情報はキーボードが Bluetooth で
送ります。そのため、右手側に**テレメトリ対応のファームウェア**を書き込む必要があります。

ここではその準備を説明します。アプリ自体のインストールは、ZEN HUD の README を参照してください。

## 変わること・変わらないこと

- 変わるのは**右手（central）だけ**です。左手のファームウェアはそのままで構いません。
- テレメトリ対応のファームウェアは、通常版に `zen-telemetry` snippet を足しただけのものです。キー入力の処理はどちらでも同じです。
- 通常版の UF2 は今までどおり作られます。何か問題があれば、通常版を書き直すだけで元に戻せます。
- テレメトリは HID と同じ Bluetooth 接続を使います。ケーブルも、追加のペアリングも要りません。

## 手順

### 1. このリポジトリを fork する

[boooowy/zen-firmware](https://github.com/boooowy/zen-firmware) を自分のアカウントへ fork します。
[E24-GH/zen-firmware](https://github.com/E24-GH/zen-firmware) を既に fork して使っている場合は、
[既存の fork に取り込む](#既存の-fork-に取り込む) を参照してください。

fork したら、そのリポジトリの **Actions** タブを開いて、ワークフローを有効にします。
fork 直後の GitHub Actions は無効になっているためです。

### 2. キーマップを入れる

いつもどおり Keymap Editor で fork を開いて、キー配置を編集・保存します。
それまで使っていたキーマップがある場合は、`config/keymap.keymap` を置き換えても構いません。

### 3. keymap.json が作られたことを確認する

HUD は、キーに表示する文字を `zen-hud/keymap.json` から読みます。このファイルは
`.github/workflows/keymap-json.yml`（**Update keymap.json**）が `config/keymap.keymap` から生成し、
リポジトリへコミットします。

- `config/keymap.keymap` を push するたびに、自動で再生成されます。
- **fork した直後の `zen-hud/keymap.json` は作者のキーマップのものです。** 一度キーマップを保存するか、
  Actions タブで **Update keymap.json → Run workflow** を手動で実行してください。
- `github-actions[bot]` の `chore: regenerate zen-hud/keymap.json` というコミットが現れれば成功です。
- ワークフローが `Permission denied` で失敗する場合は、**Settings → Actions → General → Workflow permissions**
  で **Read and write permissions** を選んでください。

### 4. テレメトリ版の UF2 を書き込む

GitHub Actions のビルドが終わると、`firmware` の zip に次のファイルが含まれます。
右手のハードウェアに合うものを、**右手だけ**に書き込んでください。

| 右手のハードウェア | UF2 | 確認状況 |
|---|---|---|
| トラックボール PMW3610（標準） | `zen_right_trackball_pmw3610_central_telemetry.uf2` | 実機で確認済み |
| トラックボール PAW3222 | `zen_right_trackball_paw3222_central_telemetry.uf2` | ビルドのみ・実機未確認 |
| トラックパッド | `zen_right_trackpad_central_telemetry.uf2` | ビルドのみ・実機未確認 |

実機未確認の構成で試した方は、結果を Issue で教えてもらえると助かります。

UF2 を書き込んでもペアリング情報は消えないので、通常は Mac と接続し直す必要はありません。
HUD がキーボードを見つけられないときは、ZEN の電源を入れ直してください。それでも駄目なら、
[うまく接続できない場合](../README.md#うまく接続できない場合) の手順を試してください。

### 5. ZEN HUD に自分の keymap.json を指定する

ZEN HUD の **設定 → キーマップ** で取得元を **GitHub** にし、URL に自分の fork を指定します。

```text
https://raw.githubusercontent.com/<ユーザー名>/zen-firmware/main/zen-hud/keymap.json
```

URL を入力して Return を押すと取得します。既定の URL は作者のキーマップです。そのままにすると、
自分のものではないキー表示になります。

- fork を private にしている場合は、URL からは取得できません。keymap.json を手元にダウンロードして、
  取得元を**ローカルファイル**にしてください。
- リポジトリの keymap.json が更新されても、書き込み済みのファームウェアは変わりません。
  キーマップを変えたら、ファームウェアも書き直してください。そうしないと、HUD の表示と実機がずれます。

### 6. 実機に切り替える

ZEN HUD の **メニューバー → データの取得元 → キーボード（Bluetooth）** を選びます。
初回は macOS から Bluetooth の許可を求められます。

## レイヤー名とトラックボール速度を表示する

HUD はレイヤーを `2 layer_2` のように、番号とノード名で表示します。
`config/keymap.keymap` のレイヤーに `display-name` を付けると、その名前を表示します。

```dts
layer_7 {
    display-name = "speed A";
    bindings = < ... >;
};
```

`speed <何か>`（大文字小文字は問いません）という名前のレイヤーは、トラックボールの速度プリセットとして
扱われます。そのレイヤーが有効な間、ステータス行に `ball A` のように表示されます。
複数有効なときは、番号の大きいレイヤーが優先です。

`display-name` を書き足したら、Keymap Editor で一度保存し、書き足した名前が消えていないか確かめてください。

## 通常版に戻す

右手に、名前に `_telemetry` が付いていない UF2（例: `zen_right_trackball_pmw3610_central.uf2`）を書き込みます。
左手や Mac 側の設定を変える必要はありません。

## 既存の fork に取り込む

E24-GH/zen-firmware を fork して、独自に手を入れている場合です。

作者の fork は途中に個人的なキーマップの変更を多く含むので、`git cherry-pick` でコミットを拾うより、
次のファイルを持ってくる方が確実です。

```sh
git remote add zen-hud https://github.com/boooowy/zen-firmware.git
git fetch zen-hud
git checkout zen-hud/main -- \
  src/zen_telemetry.c src/zen_telemetry.h \
  src/zen_telemetry_ble.c src/zen_telemetry_hosts.c \
  snippets/zen-telemetry \
  tools/build_keymap_json.py .github/workflows/keymap-json.yml \
  zen-hud/README.md docs/zen-telemetry-protocol.md docs/zen-hud-setup.md
```

加えて、次の 3 か所は手で書き足してください（既存の内容と混ざるため、ファイルごと上書きしないでください）。

| ファイル | 書き足す内容 |
|---|---|
| `Kconfig` | `menuconfig ZEN_TELEMETRY` から `endif # ZEN_TELEMETRY` までのブロック |
| `CMakeLists.txt` | `if(CONFIG_SHIELD_ZEN_LEFT OR CONFIG_SHIELD_ZEN_RIGHT)` の中に `zephyr_library_sources_ifdef(CONFIG_ZEN_TELEMETRY ...)` と `zephyr_library_sources_ifdef(CONFIG_ZEN_TELEMETRY_BLE ...)` の 2 行 |
| `build.yaml` | `*_central_telemetry` の 3 エントリのうち、必要なもの |

テレメトリは ZMK 本体に手を入れていません。ZMK のイベントを購読するだけなので、キーマップや独自の
behavior とは衝突しないはずです。push したら、[手順 3](#3-keymapjson-が作られたことを確認する) 以降に進んでください。

## 対応するバージョン

ファームウェアと HUD は、[docs/zen-telemetry-protocol.md](zen-telemetry-protocol.md) のワイヤ形式で通信します。
このファイルが両者の取り決めの正本です。

| 項目 | 形式 |
|---|---|
| `events` / `snapshot` | protocol v1 |
| `profiles`（接続先ごとのホスト名） | v2。この characteristic を持たない古いファームウェアでも HUD は動きます |
| `zen-hud/keymap.json` | schema 1 |

動作を確認した組み合わせは、zen-firmware `0d95f19` と zen-hud `ffbc8c4` 以降です。
