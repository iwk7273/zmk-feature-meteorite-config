# AGENTS.md

このリポジトリは Meteorite40 固有 firmware 機能をまとめた ZMK module です。
custom config の状態管理、Meteorite behaviors、smart-toggle、Meteorite input processors を提供します。

## 作業方針

- module 内の変更は `src/`、`include/`、`dts/`、`Kconfig`、`README.md` を中心に行います。
- ZMK 本体の Studio RPC handler は `../zmk` fork 側で実装します。この module は custom config state と behavior/input processor API を提供します。
- editor から直接 DTS / Kconfig / `.conf` を編集させる前提の機能は追加しません。実機編集は firmware の RPC / metadata / settings 公開面を通します。
- settings key や `struct zmk_custom_config` の layout を変える場合は、既存 NVS からの migration / backward-compatible load を必ず検討してください。
- Meteorite40 固有の key position や layer 前提は、可能な限り devicetree property と metadata で外へ出します。

## Branch / Fork 運用

- `origin` は Meteorite module fork (`iwk7273/zmk-feature-meteorite-config`) として扱います。
- 現行の Meteorite Studio 連携 branch は `feat/meteorite-custom-config-rpc` です。
- `zmk-config-meteorite40/config/west.yml` はこの branch を参照します。
- branch 名や revision を変えたら、config repo の west manifest も更新してください。
- `main` へ取り込む場合は、取り込み後に config repo の revision を `main` または新しい正本 branch へ切り替えます。

## Custom Config

- `src/custom_feature.c` は `current`、`saved`、`defaults`、`dirty` の状態モデルを持ちます。
- `zmk_custom_config_set()` は RAM 上の current だけを更新し、settings 保存は `zmk_custom_config_save()` に分離します。
- `zmk_custom_config_discard()` は current を saved へ戻します。
- `zmk_custom_config_reset_settings()` は defaults を適用し、settings の保存値を削除します。
- `C_SAVE` は save RPC と同じ永続化経路に寄せます。
- OS mode 変更も他の config 操作と同じく即時保存しません。永続化する場合は `C_SAVE` または editor の Save を使います。
- `scroll_layer_1` は現行仕様では default scroll layer 固定です。変更可能にする場合は sanitize / metadata / editor 表示を同時に見直してください。

## Input Processors

- Meteorite40 の pointer / scroll 処理は `src/input_processors/` を正本にします。
- `meteorite_motion_scaler.c` は cursor scaling と scroll scaling の両方を担当し、`custom-config-scaling` で参照する custom config field を切り替えます。
- `meteorite_sensor_rotation.c` は `zmk_custom_config_rotation_deg()` と連動します。
- `meteorite_xy_clipper.c` は scroll divisor と scroll reverse 設定に連動します。
- `meteorite_scroll_layer.c` は scroll layer 設定に連動し、有効レイヤー時だけ下流 processor へ流します。
- 旧 standalone repo (`zmk-feature-scaling`、`zmk-feature-sensor_rotation`、`zmk-feature-xy_clipper`) は参照用です。実装変更はこの repo に入れます。

## Rotary Encoder

- この module は rotary encoder 用の `&met_enc` / `&mmsc` behavior を提供しません。
- encoder は keymap 側の `sensor-bindings` と ZMK fork の `Layer.sensor_bindings` RPC で編集します。
- `zmk,behavior-sensor-rotate-var` の `tap-ms` は DTS 固定値です。Studio/editor から編集する対象は behavior と `param1` / `param2` のみです。
- encoder action は専用 settings を持たず、keymap binding として保存・破棄・リセットします。
- 旧 encoder slot / pseudo slot / `ConfigState.encoderSlots` 経路は使いません。

## 関連リポジトリ

- `../zmk`: Studio RPC subsystem と settings reset hook。
- `../zmk-studio-messages`: Meteorite RPC schema。
- `../zmk-config-meteorite40`: shield / keymap / layout / west manifest。
- `../meteorite-keymap-editor`: RPC metadata を表示・編集する browser editor。

複数 repo にまたがる変更は、repo ごとに責務を分けて commit してください。

## 検証

- module 単体ではなく、`zmk-config-meteorite40` の firmware build で確認します。
- custom config 変更は default load、settings load、save、discard、reset、behavior operation 経路を確認対象にします。
- input processor 変更は cursor movement、scroll layer、scroll divisor、rotation、scaling、scroll scaling を確認対象にします。
