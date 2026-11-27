# ZEN ファームウェア

ZEN用のファームウェアです。**ZEN Configurator** に対応しています。

- セットアップガイド: https://e24-gh.github.io/zen/firmware.html
- ZEN Configurator: https://e24-gh.github.io/zen/configurator/

Configuratorは、ZENをUSBでつないでブラウザから設定する画面です。キー配置とトラックボールの設定を、GitHubを使わずに変更できます。Mac / PC の **Chrome または Edge** でお使いください。Safari と、iPhone / iPad には対応していません。

## 2つの使い方

### A: コンボ／マクロを使わない方

GitHubの操作は必要ありません。はじめて設定する方は、まずこちらで大丈夫です。

1. [セットアップガイド](https://e24-gh.github.io/zen/firmware.html) からファームウェアのZIPをダウンロードします
2. 右手と左手の対応コントローラへUF2ファイルを書き込みます
3. 右手をUSBでつなぎ、Configuratorを開いて設定します

このリポジトリをforkする必要はありません。配布しているZIPは、このリポジトリのソースから作成したものです。

### B: コンボ／マクロを設定している方、またはこれから設定する方

コンボとマクロはファームウェアに焼き込まれるため、Configuratorからキーボードへ直接送ることができません。設定ファイルを編集して、ファームウェアを作り直します。

1. このリポジトリを自分のGitHubアカウントへforkします
2. `config/keymap.keymap` を編集します
3. GitHub Actionsでファームウェアがbuildされるのを待ちます
4. `firmware` のダウンロードボタンからzipファイルをダウンロードします
5. UF2ファイルを書き込みます

キー配置そのものは、この方法で書き込んだあとでもConfiguratorから変更できます。

コンボは、Configuratorの「コンボ」タブで `config/keymap.keymap` を読み込み、画面上でキーを選んで編集し、書き出せます。**マクロはこのタブでは編集できません。** `config/keymap.keymap` をGitHub上で直接編集してください。

> **キー配置はConfigurator、コンボとマクロは `config/keymap.keymap`。**
> 同じキーを両方で変更すると、Configuratorで設定した内容が優先されます。
> `config/keymap.keymap` 側の変更を反映させたいときは、先にConfiguratorの
> 「出荷時の設定に戻す」を実行してください。

## 書き込むファイル

ZIP および GitHub Actions で作成される `firmware` には、以下のUF2ファイルが含まれます。

右手 — お使いの構成に合うものを1つ

| ファイル | 構成 |
| --- | --- |
| `zen_right_trackball_pmw3610_central.uf2` | PMW3610（標準） |
| `zen_right_trackball_paw3222_central.uf2` | PAW3222 |
| `zen_right_trackpad_central.uf2` | トラックパッド |

左手 — お使いの構成に合うものを1つ

| ファイル | 構成 |
| --- | --- |
| `zen_left_peripheral.uf2` | トラックパッドなし（標準） |
| `zen_left_trackpad_peripheral.uf2` | トラックパッドあり |

標準構成なら、右手は PMW3610 用、左手はトラックパッドなし用の2つです。

このほかに、設定リセット用の `settings_reset.uf2` が入っています。通常は使いません。

## ZEN HUD を使う

[ZEN HUD](https://github.com/boooowy/zen-hud) は、有効なレイヤーや押されているキー、バッテリーを
macOS の画面に表示するアプリです。使うには、右手に通常版の代わりにテレメトリ対応の UF2 を書き込みます。

- `zen_right_trackball_pmw3610_central_telemetry.uf2`
- `zen_right_trackball_paw3222_central_telemetry.uf2`
- `zen_right_trackpad_central_telemetry.uf2`

左手はそのままで構いません。手順は [docs/zen-hud-setup.md](docs/zen-hud-setup.md) を参照してください。

HUD は `config/keymap.keymap` から作る `zen-hud/keymap.json` を使ってキーの名前を表示します。
Configurator でキー配置を変えると、HUD の表示は古いままになります。
キー配置を変えるときは、`config/keymap.keymap` を直してください。

## 設定を元に戻したいとき

Configuratorの「プリセット」→「出荷時の設定に戻す」で、Configuratorから変更したキー配置とマウス操作をキーボードから消せます。ファームウェアに入っている元の設定に戻ります。無線の登録は消えません。

無線接続そのものがうまくいかない場合は、`settings_reset.uf2` を使います。こちらは無線の登録も消えます。

1. PCやMacの接続設定から `ZEN` を削除します
2. `settings_reset.uf2` を書き込みます
3. USBを外します
4. ZENの電源を入れます
5. もう一度通常ファームウェアを書き込みます
6. 接続設定から `ZEN` を再接続します

## ZMK Studio について

このファームウェアでは ZMK Studio を無効にしています（`CONFIG_ZMK_STUDIO=n`）。Configuratorと同じUSBの口を取り合うため、両立できません。キー配置の編集はConfiguratorをお使いください。

どうしても ZMK Studio を使いたい場合は、forkしてご自分でビルドすれば切り替えられます。**そのかわりConfiguratorは使えなくなります。** 切り替える前に、Configuratorの「出荷時の設定に戻す」を実行しておいてください。切り替えたあとはConfiguratorにつなげないため、この操作ができなくなります。

1. `boards/shields/zen/zen_right.conf` の `CONFIG_ZMK_STUDIO=n` を `CONFIG_ZMK_STUDIO=y` に変更します
2. `boards/shields/zen/zen_left.conf` も同じように変更します
3. GitHub Actionsでbuildし、左右に書き込みます

変更するのはこの2行だけです。Configurator用の仕組みは自動的に外れます。あとからConfiguratorを使いたくなった場合は、同じ2行を `CONFIG_ZMK_STUDIO=n` に戻して、もう一度ビルドして書き込んでください。

## 以前のリポジトリをお使いの方へ

[zen-firmware](https://github.com/E24-GH/zen-firmware) で作成したファームウェアはConfiguratorに対応していません。

キー配置やコンボ、マクロをカスタマイズしている場合は、以前のforkの `config/keymap.keymap` の内容をこちらへコピーして移してください。キー配置もコンボもマクロも同じファイルに入っているため、ファイルごと移せばそのまま引き継がれます。

カスタマイズしていない場合は、ZIPをそのまま書き込んでください（上の **A**）。無線の登録とトラックボールの設定は残ります。

`config/keymap.keymap` 以外（`boards/shields/zen/*.conf` など）も変更している場合は、その差分もあわせてご確認ください。

## 対応する構成

**トラックボールは右手のみ**に取り付けられます。センサーは PMW3610（標準）と PAW3222 に対応しています。

**トラックパッドは左右どちらにも**取り付けられます。右手に付ける場合はトラックボールの代わりになります。

| | 取り付けられるもの |
| --- | --- |
| 右手 | トラックボール（PMW3610 / PAW3222）、またはトラックパッド |
| 左手 | なし、またはトラックパッド |

標準構成は、右手がPMW3610トラックボール、左手がなしです。

## ライセンス

MIT
