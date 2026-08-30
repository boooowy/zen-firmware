# zen-hud/

[zen-hud](https://github.com/boooowy/zen-hud)（ZEN の状態を表示する macOS の
オーバーレイ）が読むファイルを置くディレクトリです。ファームウェアのビルドには
一切関与しません。

## keymap.json

`config/keymap.keymap` から `tools/build_keymap_json.py` が生成します。
レイヤーごとのキー表示、物理配置、コンボの定義とタイミングが入っています。

キーマップは Keymap Editor の bot が main へ直接コミットしてくるので、
`.github/workflows/keymap-json.yml` が push のたびに再生成してコミットします。
**手で編集しないでください。**

### なぜ `config/` に置いていないのか

`config/` は ZMK と Keymap Editor のものです。Keymap Editor は
`config/*.json` をレイアウト定義として読み、保存のたびに自分で
`config/keymap.json` を書きます。ここに別の内容のファイルを置いたところ、
エディタがリポジトリを開けなくなりました（`info must define "layouts"`）。
