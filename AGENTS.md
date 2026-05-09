# AGENTS.md

このリポジトリは Meteorite40 固有機能をまとめた ZMK module です。
custom config の状態管理、Meteorite behaviors、入力 processor、rotary encoder dispatcher を提供します。

## 作業方針

- module 内の変更は `src/`、`include/`、`dts/`、`Kconfig`、README を中心に行います。
- ZMK 本体の Studio RPC handler は `../zmk` fork 側で実装します。この module は custom config state と behavior API を提供します。
- editor から直接 DTS / Kconfig / `.conf` を編集させる前提の機能は追加しません。実機編集は firmware の RPC / metadata / settings 公開面を通します。
- settings key や struct layout を変える場合は、既存 NVS からの migration / backward compatible load を必ず検討してください。
- upstream ZMK module として再利用しやすいよう、Meteorite40 固有の key position は可能な限り devicetree property で外に出します。

## Branch / Fork 運用

- `origin` は Meteorite module fork (`iwk7273/zmk-feature-meteorite-config`) として扱います。
- 現行の Meteorite Studio 連携 branch は `feat/meteorite-custom-config-rpc` です。
- `zmk-config-meteorite40/config/west.yml` はこの branch を参照します。
- branch 名や commit を変えたら、config repo の west manifest を必ず更新してください。
- `main` へ取り込む場合は、取り込み後に config repo の revision を `main` または新しい正本 branch へ切り替えます。

## Custom Config

- `src/custom_feature.c` は `current`、`saved`、`defaults`、`dirty` の状態モデルを持ちます。
- `zmk_custom_config_set()` は RAM 上の current を更新し、dirty を更新します。settings への保存は `zmk_custom_config_save()` に分離します。
- `zmk_custom_config_discard()` は current を saved へ戻します。
- `zmk_custom_config_reset_settings()` は defaults を適用し、settings の保存値を削除します。
- `C_SAVE` は save RPC と同じ永続化経路に寄せます。
- OS mode 変更も他の config 操作と同じく即時保存しません。永続化する場合は `C_SAVE` または editor の Save を使います。
- `scroll_layer_1` は現行仕様では default scroll layer 固定です。変更可能にする場合は sanitize / metadata / editor 表示を同時に見直してください。

## Rotary Encoder

- この module は rotary encoder 用の `&met_enc` / `&mmsc` behavior を提供しません。encoder は keymap 側の `sensor-bindings` と ZMK fork の `Layer.sensor_bindings` RPC で編集します。
- `zmk,behavior-sensor-rotate-var` の `tap-ms` は DTS 固定値です。Studio/editor から編集する対象は behavior と `param1` / `param2` のみです。
- encoder action は専用 settings を持たず、keymap binding として保存・破棄・リセットします。
- slot order は現在 `sensor 0 CW`, `sensor 0 CCW`, `sensor 1 CW`, `sensor 1 CCW` です。順序を変える場合は README、ZMK RPC metadata、editor test を更新してください。
- 旧 encoder slot / pseudo slot / `ConfigState.encoderSlots` 経路は使いません。

## 関連リポジトリ

- `../zmk`: Studio RPC subsystem と settings reset hook。
- `../zmk-studio-messages`: Meteorite RPC schema。
- `../zmk-config-meteorite40`: shield / keymap / layout / west manifest。
- `../meteorite-keymap-editor`: RPC metadata を表示・編集する browser editor。

複数 repo にまたがる変更は、repo ごとに責務を分けて commit してください。

## 検証

- module 単体ではなく、`zmk-config-meteorite40` の firmware build で確認します。
- custom config の変更は、default load、settings load、save、discard、reset、behavior operation 経由の更新を確認対象にします。
- encoder 変更は、keymap 末尾 slot、physical layout pseudo slot、`sensor-bindings`、editor の encoder panel が同じ slot order を見ていることを確認します。
