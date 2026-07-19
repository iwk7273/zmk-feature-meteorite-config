# zmk-feature-meteorite-config

Meteorite向けの拡張機能をまとめたZMKモジュール。

## できること
- 設定変更用の`&ccfg`ビヘイビア（CPI/スクロール/OS切り替え）。設定値はキーボードに保存される。
- OSモードに応じてキーを切り替えるビヘイビア群（`&mck`/`&mmk`/`&mosk`）
- BluetoothスロットとOSモードの同時切替（`&mbt`）
- スマートトグル（`behavior-smart-toggle`）
- Meteorite40向け入力プロセッサ（motion scaler / sensor rotation / scroll transform / scroll layer gate）

## 導入
1. モジュール追加
- `west.yml`にリポジトリを追加する。
例（`west.yml`）
```yaml
manifest:
  remotes:
    - name: iwk7273
      url-base: https://github.com/iwk7273
  projects:
    - name: zmk-feature-meteorite-config
      remote: iwk7273
      revision: feat/meteorite-custom-config-rpc
      path: modules/zmk-feature-meteorite-config
```

3. `keymap`で必要な`dtsi`/`dt-bindings`を`#include`
```dts
#include <behaviors/custom_config.dtsi>
#include <behaviors/meteorite_custom_key.dtsi>
#include <behaviors/meteorite_bt_os.dtsi>
#include <behaviors/meteorite_mod_key.dtsi>
#include <behaviors/meteorite_os_key.dtsi>
#include <dt-bindings/zmk/custom_config.h>
#include <dt-bindings/zmk/meteorite_custom_keys.h>
#include <dt-bindings/zmk/meteorite_bt_os.h>
```

## 使い方
### 1) Custom Config操作 (`&ccfg`)
`&ccfg`は「設定操作」を実行するためのビヘイビア。`C_*`の操作コードを指定してキーに割り当てる。
ZMK Studioでは「meteorite custom config」として表示される。パラメータは一覧から選択し、`Select config op`（値0）のままだと何もしない。`C_SCRL1_UP`は一覧に出ない。

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

Custom Config一覧（パラメータ/キー/説明）
| パラメータ | キー | 説明 |
| --- | --- | --- |
| CPI | `C_CPI_DN` | CPIを1段階下げる（`ZMK_CUSTOM_CONFIG_CPI_STEP`単位で循環）。 |
| CPI | `C_CPI_UP` | CPIを1段階上げる（`ZMK_CUSTOM_CONFIG_CPI_STEP`単位で循環）。 |
| スクロール分割値（scroll_div） | `C_SDIV_DN` | スクロール分割値を1段階下げる（`ZMK_CUSTOM_CONFIG_SCROLL_DIV_STEP`単位で循環）。 |
| スクロール分割値（scroll_div） | `C_SDIV_UP` | スクロール分割値を1段階上げる（`ZMK_CUSTOM_CONFIG_SCROLL_DIV_STEP`単位で循環）。 |
| センサー回転角 | `C_ROT_DN` | センサー回転角を1段階下げる（プリセット角度で循環）。 |
| センサー回転角 | `C_ROT_UP` | センサー回転角を1段階上げる（プリセット角度で循環）。 |
| ポインタ移動スケーリング | `C_SCALE_TOG` | スケーリングON/OFFを切り替える。 |
| スクロール反転（横） | `C_SCRH_TOG` | 横スクロール反転のON/OFFを切り替える。 |
| スクロール反転（縦） | `C_SCRV_TOG` | 縦スクロール反転のON/OFFを切り替える。 |
| スクロールスケーリング | `C_SCRL_SCALE_TOG` | OFF（Linear）/ON（Adaptive）を切り替える。 |
| スクロールレイヤー1 | `C_SCRL1_UP` | 動作しない（レイヤー1はデフォルト固定）。 |
| スクロールレイヤー2 | `C_SCRL2_UP` | レイヤー2を次のレイヤーへ進める（レイヤー数で循環）。 |
| OSモード | `C_OS_TOG` | OSモードをWin/Macでトグルする。 |
| OSモード | `C_OS_WIN` | OSモードをWindowsに固定する。 |
| OSモード | `C_OS_MAC` | OSモードをMacに固定する。 |
| 設定保存 | `C_SAVE` | 現在の設定を保存する。 |
| 設定リセット | `C_RESET` | 設定をデフォルトに戻す（DTS/既定値に従う）。 |

開発メモ: opcode の処理・名前・ZMK Studio metadata は `include/zmk/custom_config_ops.def` を正本にしています。DTS/keymap 用の数値 `#define` は `include/dt-bindings/zmk/custom_config.h` に残し、C ビルド時に `.def` の値と一致することを検査します。
CPI/scroll_div の段階幅と上限は `include/zmk/custom_config_axes.h` を正本にしています。

#### ZMK Studio RPC 連携

`CONFIG_ZMK_STUDIO_RPC=y` かつ対応 ZMK fork を使う場合、Meteorite custom config は `meteorite` RPC subsystem から編集できます。
対応 firmware は `core.getDeviceInfo` の `capabilities` に `meteorite.config` を返します。editor はこの capability がある場合だけ `meteorite` subsystem を呼び出すため、未対応 firmware では Custom Config view を表示しません。

- `getConfigState` は `fields/current/saved/defaults/dirty` を返します。editor はこの metadata を正本にし、raw 数値だけをハードコードしません。
- `setConfig` は RAM 上の `current` だけを更新し、settings へは保存しません。保存は `saveChanges` または `&ccfg C_SAVE` で行います。
- `discardChanges` は `current` を最後に保存された `saved` へ戻します。
- `core.resetSettings` では ZMK 側の `ZMK_RPC_SUBSYSTEM_SETTINGS_RESET` に登録された Meteorite reset hook が呼ばれ、保存済み settings は削除され、DTS/既定値へ戻ります。
- `C_OS_TOG` / `C_OS_WIN` / `C_OS_MAC` は OS モードだけを `CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE` 後に自動保存します。他の config 値に未保存差分があっても巻き込んで保存しません。
- `scroll_layer_1` は現行 firmware では default scroll layer 固定です。RPC metadata では `readOnly` と `fixedReason` を返し、editor から変更可能に見せません。
- `TimingConfig` は mod-tap / layer-tap の tapping term と idle / deep sleep timeout を保持する optional RPC submessage です。timeout の `0` は Off を意味し、両方が有効なときは deep sleep が idle 以上でなければなりません。
- timing 値は custom config payload の末尾に append-only で保存します。古い保存データは既定値で補完され、Studio の Apply 後に Save すると本体 NVS に永続化されます。

### 2) OS切替系ビヘイビア
#### OSに応じた単体修飾キー（`&mck`）
OSで入れ替えたい単体の修飾キーに使う。WinではCtrl、MacではCmdなど、OSモードに応じて修飾キーを切り替えられる。
用途に合わせて任意の組み合わせの修飾キーを切り替えられるよう、以下の内容をデフォルトで用意済み。
ZMK Studioでは「meteorite OS-Switch Mod (single)」として表示される。

設定値一覧
| 値 | 説明 |
| --- | --- |
| `M_OS_CTRL_CMD` | Win: Ctrl / Mac: Cmd |
| `M_OS_ALT_OPT` | Win: Alt / Mac: Option |
| `M_OS_ALT_CTRL` | Win: Alt / Mac: Ctrl |
| `M_OS_WIN_CTRL` | Win: Win / Mac: Ctrl |
| `M_OS_WIN_OPT` | Win: Win / Mac: Option |
| `M_OS_ALT_CMD` | Win: Alt / Mac: Cmd |

```dts
&mck M_OS_CTRL_CMD
&mck M_OS_ALT_CMD
```

#### タップ/ホールド切替（meteorite OS-Switch-mod-tap）
ZMK標準のビヘイビアを利用し、`&mck`をbindingsのホールド側に置くことで、タップは通常キー、ホールドはOSで入れ替わる修飾キーにできる。

```dts
mck_mt: mck_mt {
    compatible = "zmk,behavior-hold-tap";
    #binding-cells = <2>;
    tapping-term-ms = <150>;
    bindings = <&mck>, <&kp>;
    display-name = "meteorite OS-Switch-mod-tap";
};
```

#### OSに応じた修飾キー + 任意キー（`&mmk`）
OSで入れ替わる修飾キーと、通常キーを同時に押す用途に使う（Win/Opt + Leftなど）。
ZMK Studioでは「meteorite OS-Switch Mod+Key」として表示される。パラメータ1はOS-Switch Mod一覧、パラメータ2は任意キー。

```dts
&mmk M_OS_CTRL_CMD TAB
&mmk M_OS_ALT_CMD ESCAPE
```

#### Win/Macのキーをペアで指定（`&mosk`）
任意のWin用キーとMac用キーをペアで設定し、OSに合わせて切り替える。
ZMK Studioでは「meteorite OS-Switch Key (Win/Mac pair)」として表示される。パラメータ1がWin用キー、パラメータ2がMac用キー。

```dts
&mosk HOME LG(LEFT)
&mosk END  LG(RIGHT)
```

### 3) Smart Toggle
Smart Toggleは「押し続けている間は修飾キーを押したまま」「素早く繰り返すとトグル」に切り替わる挙動を作るためのビヘイビア。
例えば、WinではAlt/Tab、MacではCmd/Tabでアプリを切り替える用途に使える。1つのキーで「修飾キーの長押し」と「Tabによる切り替え」を行える。
`&mck`をbindingsのホールド側に利用することで、WinではAlt+Tab、MacではCmd+Tabのように切り替えられる。
ZMK Studioでは`display-name`が「meteorite Smart toggle (Alt/Cmd+Tab)」として表示される。

```dts
/ {
    behaviors {
        alt_cmd_tab: alt_cmd_tab {
            compatible = "zmk,behavior-smart-toggle";
            #binding-cells = <0>;
            display-name = "meteorite Smart toggle (Alt/Cmd+Tab)";
            bindings = <&mck M_OS_ALT_CMD>, <&kp TAB>;
            position-bindings = <12 13>;
            position-binding-behaviors = <&kp LS(TAB)>, <&kp TAB>;
        };
    };
};

&alt_cmd_tab
```

- トグル中に他のキーを押すと切り替えは解除され、押したキーは送信されない（抑止される）。
- `position-bindings`はトグル中に別の挙動をさせたいキー位置を指定する。`position-binding-behaviors`は対応する挙動を並べ、長さは`position-bindings`と一致させる。
- 上の例では位置`12/13`（D/F）を押したときに`LS(TAB)`/`TAB`を送るようにして、切り替え中の挙動をカスタムしている。

#### meteorite OS-Switch-mod-tap (smart toggle)
`behavior-hold-tap`で`&mck`（ホールド）と`&alt_cmd_tab`（タップ）を組み合わせたラッパー。1つのキーで「OS依存の修飾キー長押し」と「Alt/Cmd+Tabの切り替え」を使い分ける。
ZMK Studioではラッパーの`display-name`が「meteorite OS-Switch-mod-tap (smart toggle)」として表示される。
```dts
mck_mt_st: mck_mt_st {
    compatible = "zmk,behavior-hold-tap";
    #binding-cells = <2>;
    flavor = "tap-preferred";
    tapping-term-ms = <150>;
    bindings = <&mck>, <&alt_cmd_tab>; // ホールド, タップ
    display-name = "meteorite OS-Switch-mod-tap (smart toggle)";
};
```

### 4) BT + OS 同時切替 (`&mbt`)
指定したBTスロットに切り替えつつ、OSモードも同時に切り替える。端末ごとにOSが異なる運用（例: BT0=Win、BT1=Mac）に便利。
ZMK Studioでは「meteorite BT+OS select」として表示される。パラメータ1はBTスロット（`M_BT0`〜`M_BT4`）、パラメータ2はOS（`M_OS_WIN`/`M_OS_MAC`）。
BTプロファイルとOSモードはどちらも `CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE` 後に保存される。OSモード保存はOS値だけを対象にし、他のCustom Config未保存差分は保存しない。
```dts
&mbt M_BT0 M_OS_WIN
&mbt M_BT1 M_OS_MAC
```

### 5) Meteorite入力プロセッサ
Meteorite40のトラックボール処理は、このモジュール内の入力プロセッサをDTSで組み合わせる。
Custom Configが有効な場合、CPI以外のポインタ/スクロール設定も `zmk_custom_config_*()` の値が優先される。

| compatible | 用途 |
| --- | --- |
| `zmk,input-processor-meteorite-motion-scaler` | 物理速度で正規化した4種類のポインター応答を適用する。 |
| `zmk,input-processor-meteorite-sensor-rotation` | センサーのX/Y移動を設定角度で回転する。 |
| `zmk,input-processor-meteorite-scroll-transform` | raw X/Y入力から物理速度を求め、スクロール応答、蓄積、軸選択、wheel変換を一括処理する。 |
| `zmk,input-processor-meteorite-xy-clipper` | 旧構成向け。スクロール時のX/Y入力を蓄積し、支配的な軸だけを出力する。 |
| `zmk,input-processor-meteorite-scroll-layer` | 特定レイヤーが有効な時だけスクロール用入力プロセッサへ流す。 |

#### ポインタープロファイルV2

`zmk,input-processor-meteorite-motion-scaler`はフレーム内のX/Y countsをCPIとreport intervalで正規化し、`speed_mm_s = hypot(dx, dy) * 25400 / (cpi * dt_ms)`で求めた物理速度からQ16 gainを線形補間する。絶対出力を一定値へ飽和させず、profileごとのgain上限だけを適用する。フレーム全体の速度はsync時に確定するため、算出・平滑化したgainは次フレームへ適用する。

| 保存値 | Profile | LUT `(mm/s, gain)` | rise / fall |
| ---: | --- | --- | --- |
| 0 | Standard | `(0,.40) (15,.41) (30,1) (50,1.75) (80,2.35) (120,2.75) (160,3) (200,3.4)` | 18 / 9 ms |
| 1 | Stable | `(0,.30) (25,.45) (50,1) (90,1.25) (140,1.5) (200,1.7) (250,1.8)` | 24 / 12 ms |
| 2 | Responsive | `(0,.55) (10,.70) (20,1) (40,1.75) (70,2.5) (95,3) (120,3.4) (200,4.2)` | 12 / 6 ms |
| 3 | Custom | `(0,Start) (30,Precision) (90,Fast) (200,Flick)` | 18 / 9 ms |

Standardの定常target gainは、800 CPI・12 msで旧Classic
（`max-output=300`、`half-input=50`、`exponent-tenths=12`）と比較し、
5〜75 countsの整数入力で出力差が最大約8.7%に収まるよう調整している。

- Q16 remainderを常に次イベントへ持ち越し、1 count以下の低速出力を切り捨てない。
- reset直後は選択profileの最低gainから開始する。
- Customは0 / 30 / 90 / 200 mm/sの固定速度点で選んだpercent gainを線形補間する。既定値は40 / 100 / 240 / 340%で、各点はFirmwareが公開する選択肢から単調非減少となる組み合わせだけを受け付ける。
- 120 msを超える停止、90度以上の方向転換、CPI・有効状態・profile・Custom gain変更、設定読込・reset、Ball Profile切替でgain、フレーム履歴、remainderを破棄する。
- `scaling_mode`は従来どおり0=無効、1=有効だけを表し、profile番号には転用しない。`pointer_profile`とCustom gainはCustom Config NVS payloadの末尾へappendされ、旧payloadはStandardと既定Custom gainで補完される。storage schema versionは4を維持する。
- Custom Config無効時のDTS fallbackは`scaling-mode`、`pointer-profile`、`cpi`、`default-dt-ms`で指定する。

#### スクロールプロファイルV2

新しいSCROLL chainでは`zmk,input-processor-meteorite-scroll-transform`を単独で使う。従来の`xy_clipper -> scroll_motion_scaler -> to_wheel`のように整数wheel stepへ変換してからgainを掛けず、raw countsにgainを適用した後でQ16 accumulatorからwheel stepを取り出す。このため、ゆっくりした1 count入力も失わずに蓄積できる。

- `Linear`（保存値0）: 常に1.0x。速度による加速を行わない。
- `Adaptive`（保存値1）: 30 mm/sまでは1.0xを維持し、50 / 80 / 110 / 145 / 180 mm/sで約1.15 / 1.45 / 1.85 / 2.4 / 3.0xとなる固定LUTを線形補間する。
- 速度は`hypot(dx, dy) * 25400 / (cpi * dt_ms)`で計算するため、同じ物理速度ならCPIやreport intervalが変わっても近い応答になる。
- Adaptive gainは上昇16 ms、下降8 ms相当で平滑化する。120 msを超える停止、方向反転、設定変更、Ball Profile切替では履歴とremainderをリセットする。
- `scroll_div`はgainとは独立したstep閾値として引き続き使う。値を小さくすると速く、大きくすると細かくなる。
- 既存NVS fieldは変更しない。旧OFFはLinear、旧ONはAdaptiveとして安全に移行し、旧firmwareへ戻しても0/1の範囲で読み込める。

スクロールレイヤーゲートの例:
```dts
/ {
    input_processors {
        scroll_layer: scroll_layer {
            compatible = "zmk,input-processor-meteorite-scroll-layer";
            input-processors = <&some_scroll_proc>;
            layer-1 = <1>;
            layer-2 = <2>;
        };
    };
};
```
`CONFIG_ZMK_CUSTOM_CONFIG=y`の場合、`layer-1/2`は`custom_config`の設定値が優先される。

### 6) Rotary encoder
このモジュールはロータリーエンコーダー用の `&met_enc` / `&mmsc` behavior を提供しない。
Meteorite40 のエンコーダー動作は keymap 側の `sensor-bindings` に `zmk,behavior-sensor-rotate-var` behavior を割り当て、対応 ZMK fork の `Layer.sensor_bindings` / `setLayerSensorBinding` RPC で編集する。

`tap-ms` は DTS 固定値で、実行時や Studio RPC からは変更しない。Studio/editor で編集する対象は behavior と `param1` / `param2` のみ。
