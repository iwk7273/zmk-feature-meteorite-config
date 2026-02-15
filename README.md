# zmk-feature-meteorite-config

Meteorite向けの拡張機能をまとめたZMKモジュールです。実装の詳細ではなく、ユーザーがどう使うかにフォーカスした使い方ガイドを記載します。

## できること
- 設定変更用の`&ccfg`ビヘイビア（CPI/スクロール/回転/OSなどの操作）
- OSモードに応じてキーを切り替えるビヘイビア群（`&mck`/`&mmk`/`&mosk`/`&mck_or_kp`）
- BluetoothスロットとOSモードの同時切替（`&mbt`）
- スマートトグル（`behavior-smart-toggle` + `&mst`）
- 特定レイヤーが有効な時だけスクロール処理に差し替える入力プロセッサ

## 導入
1. モジュール追加
- 既にワークスペース内にある場合は`ZMK_EXTRA_MODULES`にパスを追加します。
- 別リポジトリに置く場合は`west.yml`に追加してください。

2. Kconfig設定
- `CONFIG_ZMK_CUSTOM_CONFIG=y`
- 永続化したい場合は`CONFIG_SETTINGS=y`

3. `keymap`で必要な`dtsi`/`dt-bindings`を`#include`
```dts
#include <behaviors/custom_config.dtsi>
#include <behaviors/meteorite_custom_key.dtsi>
#include <behaviors/meteorite_bt_os.dtsi>
#include <behaviors/meteorite_mod_key.dtsi>
#include <behaviors/meteorite_os_key.dtsi>
#include <behaviors/meteorite_smart_toggle.dtsi>
#include <dt-bindings/zmk/custom_config.h>
#include <dt-bindings/zmk/meteorite_custom_keys.h>
#include <dt-bindings/zmk/meteorite_bt_os.h>
```

## 使い方

### 1) Custom Config操作 (`&ccfg`)
`&ccfg`は1パラメータです。レイヤーに割り当てて操作します。
```dts
&ccfg C_CPI_DN
&ccfg C_CPI_UP
&ccfg C_SDIV_DN
&ccfg C_SDIV_UP
&ccfg C_ROT_DN
&ccfg C_ROT_UP
&ccfg C_SCALE_TOG
&ccfg C_SCRH_TOG
&ccfg C_SCRV_TOG
&ccfg C_SCRL_SCALE_TOG
&ccfg C_SCRL2_UP
&ccfg C_OS_TOG
&ccfg C_OS_WIN
&ccfg C_OS_MAC
&ccfg C_SAVE
&ccfg C_RESET
```

各操作の説明
- `C_CPI_DN`: CPIを1段階下げます（200単位で循環）。
- `C_CPI_UP`: CPIを1段階上げます（200単位で循環）。
- `C_SDIV_DN`: スクロール分割値（scroll_div）を1段階下げます。
- `C_SDIV_UP`: スクロール分割値（scroll_div）を1段階上げます。
- `C_ROT_DN`: センサー回転角を1段階下げます（プリセット角度で循環）。
- `C_ROT_UP`: センサー回転角を1段階上げます（プリセット角度で循環）。
- `C_SCALE_TOG`: ポインタ移動のスケーリングON/OFFを切り替えます。
- `C_SCRH_TOG`: 横スクロール反転のON/OFFを切り替えます。
- `C_SCRV_TOG`: 縦スクロール反転のON/OFFを切り替えます。
- `C_SCRL_SCALE_TOG`: スクロールのスケーリングON/OFFを切り替えます。
- `C_SCRL2_UP`: スクロールレイヤー2を次のレイヤーへ進めます（レイヤー数で循環）。
- `C_OS_TOG`: OSモードをWin/Macでトグルします。
- `C_OS_WIN`: OSモードをWindowsに固定します。
- `C_OS_MAC`: OSモードをMacに固定します。
- `C_SAVE`: 現在の設定を保存します。
- `C_RESET`: 設定をデフォルトに戻します（DTS/既定値に従う）。

補足
- `C_SCRL1_UP`は動作しません（スクロールレイヤー1はデフォルト固定）。

初期値
- CPI: `trackball`の`cpi`から算出（200刻み）。未定義なら1000。
- スクロール分割値: `xy_clipper`の`threshold`から算出（5刻み）。未定義なら20。
- 回転角: `sensor_rotation`の`rotation-angle`に最も近い角度。未定義なら30度。
- スクロール反転: `xy_clipper`の`invert-x/invert-y`。未定義ならX=ON、Y=OFF。
- スケーリング: `motion_scaler`の`scaling-mode`、スクロール側は`scroll_motion_scaler`の`scaling-mode`。未定義ならOFF。
- スクロールレイヤー: `scroll_layer_defaults.layers` または `scroll_layer_gate.layer-1/2`。未定義なら`0/0`。
- OSモード: `custom_config_defaults.os-mode`。未定義ならWin（0）。

### 2) OS切替系ビヘイビア
#### `&mck`（OSに応じた単体モディファイア）
```dts
&mck M_OS_CTRL_CMD
&mck M_OS_ALT_CMD
```

#### `&mmk`（OSに応じたモディファイア + 任意キー）
```dts
&mmk M_OS_CTRL_CMD TAB
&mmk M_OS_ALT_CMD ESCAPE
```

#### `&mosk`（Win/Macのキーをペアで指定）
```dts
&mosk HOME LG(LEFT)
&mosk END  LG(RIGHT)
```

#### `&mck_or_kp`（カスタム or 通常キー）
```dts
// OS切替モディファイア（OS依存）
&mck_or_kp MCK_TAP_PARAM(M_OS_ALT_CMD)

// 通常キー
&mck_or_kp TAB

// Alt/Cmd+Tab（Smart Toggle利用）
&mck_or_kp MCK_TAP_PARAM_ALT_CMD_TAB
```
`MCK_TAP_PARAM_ALT_CMD_TAB`を使う場合は`alt-cmd-tab`の設定が必要です。

#### `zmk,behavior-os-switch`（OSに応じた任意ビヘイビア切替）
`dtsi`は用意されていないので、必要に応じて自分で定義します。
```dts
/ {
    behaviors {
        os_switch: os_switch {
            compatible = "zmk,behavior-os-switch";
            #binding-cells = <0>;
            bindings = <&kp LGUI>, <&kp LCTRL>; // Win/Macで切替
        };
    };
};
```

### 3) Smart Toggle
`behavior-smart-toggle`を定義し、`&mst`に紐づけます。
```dts
/ {
    behaviors {
        alt_cmd_tab: alt_cmd_tab {
            compatible = "zmk,behavior-smart-toggle";
            #binding-cells = <0>;
            bindings = <&mck M_OS_ALT_CMD>, <&kp TAB>;
            position-bindings = <12 13>;
            position-binding-behaviors = <&kp LS(TAB)>, <&kp TAB>;
        };
    };
};

&mst {
    alt-cmd-tab = <&alt_cmd_tab>;
};
```
`position-bindings`はトグル中に別の挙動をさせたいキーを指定します。

### 4) BT + OS 同時切替 (`&mbt`)
```dts
&mbt M_BT0 M_OS_WIN
&mbt M_BT1 M_OS_MAC
```

### 5) スクロールレイヤー入力プロセッサ
特定レイヤーが有効な時だけ、スクロール用入力プロセッサに流します。
```dts
/ {
    input_processors {
        scroll_layer: scroll_layer {
            compatible = "zmk,input-processor-scroll-layer";
            input-processors = <&some_scroll_proc>;
            layer-1 = <1>;
            layer-2 = <2>;
        };
    };
};
```
`CONFIG_ZMK_CUSTOM_CONFIG=y`の場合、`layer-1/2`は`custom_config`の設定値が優先されます。

### 6) デフォルト値の設定
OSモードとスクロールレイヤーの初期値をDTSで指定できます。
```dts
/ {
    custom_config_defaults: custom_config_defaults {
        compatible = "zmk,custom-config-defaults";
        os-mode = <1>; // 0: Win, 1: Mac
    };

    scroll_layer_defaults: scroll_layer_defaults {
        compatible = "zmk,custom-scroll-layers";
        layers = <1 2>;
    };
};
```

## 参考
- `zmk-feature-meteorite-config/dts/bindings/` に各ビヘイビアのプロパティが記載されています。
- 使用例は `zmk-config-meteorite40/config/meteorite40_low.keymap` を参照してください。
