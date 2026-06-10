# Zynq + MAX2771 移植記録

greta-oto GNSS受信機をXilinx Zynqプラットフォーム（MAX2771 RFフロントエンドIC使用）で動作させるための追加・修正内容をまとめたドキュメントです。

---

## 追加・修正ファイル一覧

```
greta-oto/
├── BB_HW/
│   ├── rom_init/                      ← 新規追加（生成ファイル）
│   │   ├── b1c_legendre.coe/.mem
│   │   ├── l1c_legendre.coe/.mem
│   │   └── memory_code.coe/.mem
│   └── rtl/
│       └── xilinx/                    ← 新規追加（RTLラッパー群）
│           ├── gnss_top_axi.v
│           ├── max2771_if.v
│           ├── xilinx_clk_gate.v
│           ├── xilinx_rom_wrapper.v
│           └── constraints_template.xdc
├── Firmware/
│   └── Abstract/
│       └── HWCtrl_Zynq.c              ← 新規追加（Zynq向けHWアクセス層）
└── tools/
    └── gen_rom_init.py                ← 新規追加（ROMデータ生成スクリプト）
```

---

## 1. PL（FPGA）側の追加ファイル

### 1.1 `BB_HW/rtl/xilinx/gnss_top_axi.v`

**目的:** `gnss_top` のカスタムパラレルバスを AXI4-Lite スレーブに変換するラッパーモジュール。Vivado IP Integrator でのブロックデザインに組み込む最上位PLモジュール。

**主な仕様:**
- AXI4-Lite スレーブ（16ビットバイトアドレス = 64KB空間）
- 書き込み: AW/Wチャネルを独立バッファし、両方揃い次第 `host_cs + host_wr` を1サイクルアサート
- 読み出し: `host_cs + host_rd` を1サイクルアサート後、`gnss_top` の登録出力が確定する1サイクル待機（RD\_WAIT）してから RVALID を返す
- `max2771_if` を内部でインスタンス化（`adc_clk` は外部ポートとして残す）

**接続方法（Vivado ブロックデザイン）:**
```
Zynq PS GP0 AXI Master
  → AXI Interconnect
    → gnss_top_axi.S_AXI_*
MAX2771 CLKOUT → gnss_top_axi.adc_clk
MAX2771 I[1:0] → gnss_top_axi.max_i
MAX2771 Q[1:0] → gnss_top_axi.max_q
gnss_top_axi.irq → Zynq PS IRQ_F2P[0]
```

---

### 1.2 `BB_HW/rtl/xilinx/max2771_if.v`

**目的:** MAX2771の出力信号を `gnss_top` が期待する8ビット形式に変換する単純なアダプタ。

**信号フォーマット変換:**

| 信号 | ビット | 意味 |
|---|---|---|
| `max_i[1]` | `adc_data[7]` | I チャネル 符号ビット（1=負） |
| `max_i[0]` | `adc_data[6]` | I チャネル 大きさビット |
| `2'b00` | `adc_data[5:4]` | ゼロパディング |
| `max_q[1]` | `adc_data[3]` | Q チャネル 符号ビット |
| `max_q[0]` | `adc_data[2]` | Q チャネル 大きさビット |
| `2'b00` | `adc_data[1:0]` | ゼロパディング |

`gnss_top` のサンプルフォーマット（`if_data/description.txt` 参照）: 4ビット符号付き大きさ表現（上位4ビット=I、下位4ビット=Q）。MAX2771の2ビット出力（符号+大きさ各1ビット）をこの形式にマッピングする。

---

### 1.3 `BB_HW/rtl/xilinx/xilinx_clk_gate.v`

**目的:** ASICのラッチベースクロックゲーティングセルをFPGA向けに置き換え。

**問題:** `backend_wrapper/clock_gating.v` の `gated_clock_wrapper` は `always @(*) if (!clk_in) en_latch = en | te;` というラッチ推論を使用しており、Xilinx合成で問題が生じる可能性がある。

**解決策:** Xilinx `BUFGCE` プリミティブを使用したグリッチフリークロックゲーティング。

**使用箇所:**
- `tracking_engine.v`: 4チャネル分（`gated_clk[0..3]`）
- `ae_core.v`: 1個（AE動作中のみクロック供給）

**Vivadoプロジェクトへの追加方法:** `backend_wrapper/clock_gating.v` の代わりにこのファイルを追加する（同時に両方を含めないこと）。

---

### 1.4 `BB_HW/rtl/xilinx/xilinx_rom_wrapper.v`

**目的:** 3つのROMを `xpm_memory_sprom`（Xilinxパラメータ化マクロ）に置き換えたラッパー。

**背景:** `sprom.v` の `mem` 配列は「他のモジュールで初期化する」とコメントされているが、合成時にROM内容が失われる問題がある。

**置き換え対象モジュール:**

| モジュール名 | サイズ | 初期化ファイル |
|---|---|---|
| `b1c_legendre_rom_640x16_wrapper` | 640 × 16 bit | `b1c_legendre.mem` |
| `l1c_legendre_rom_640x16_wrapper` | 640 × 16 bit | `l1c_legendre.mem` |
| `memory_code_rom_12800x32_wrapper` | 12800 × 32 bit | `memory_code.mem` |

**Vivadoへの組み込み方:** `backend_wrapper/mem_wrapper.v` のROM部分（上記3モジュール）の代わりに使用。RAM部分（`spram` 使用のもの）は `mem_wrapper.v` をそのまま使用してよい（Vivadoが自動的にBRAMとして推論）。

**`.mem` ファイルの配置:** Vivadoプロジェクトディレクトリ（`.xpr` ファイルと同じフォルダ）に `BB_HW/rom_init/*.mem` をコピーする、またはパラメータ `MEMORY_INIT_FILE` にフルパスを記載する。

---

### 1.5 `BB_HW/rtl/xilinx/constraints_template.xdc`

**目的:** Zynq + MAX2771 向けタイミング・ピン制約のテンプレート。

**記載内容:**
- `adc_clk`（MAX2771 CLKOUT）のクロック定義（デフォルト: 4.096 MHz = 243.584 ns周期）
- `adc_clk` とシステムクロックの非同期クロックグループ宣言
- MAX2771 I/Q/CLKOUTピンの `PACKAGE_PIN` / `IOSTANDARD` プレースホルダ
- MAX2771データの入力遅延制約（`set_input_delay` 参照）
- PPSパルス出力、EVENT\_MARKピンのプレースホルダ
- MAX2771 SPI ピン（PS MIO経由の場合は不要）

**使用方法:** `IOxx_xx` となっている箇所を実際のボードの回路図に合わせて修正する。

---

## 2. ROM初期化データ（`BB_HW/rom_init/`）

**生成スクリプト:** `tools/gen_rom_init.py`

**データソース:**

| 出力ファイル | ソース |
|---|---|
| `b1c_legendre.coe/.mem` | `HWModel/src/WeilPrn.cpp::CWeilPrn::LegendreB1C[640]` |
| `l1c_legendre.coe/.mem` | `HWModel/src/WeilPrn.cpp::CWeilPrn::LegendreL1C[640]` |
| `memory_code.coe/.mem` | `HWModel/inc/E1_code.h::GalE1Code[100][128]` |

**ファイル形式:**
- `.coe`: Xilinx Block Memory Generator IP 用（`memory_initialization_radix=16;` ヘッダ付き）
- `.mem`: `xpm_memory_sprom` の `MEMORY_INIT_FILE` パラメータ用（`$readmemh` 互換、1行1ワード、0埋め16進数）

**再生成方法:**
```bash
cd /path/to/greta-oto
python3 tools/gen_rom_init.py
```

---

## 3. PS（ARM）側の追加ファイル

### 3.1 `Firmware/Abstract/HWCtrl_Zynq.c`

**目的:** `HWCtrl_HW.c` のZynqバレメタル/FreeRTOS向け実装。`HWCtrl.h` インタフェースを満たす。

**ハードウェアアドレス（要確認・変更）:**

| 定数 | デフォルト値 | 説明 |
|---|---|---|
| `GNSS_BASE_ADDR` | `0x43C00000` | Vivado Address Editorで割り当てたベースアドレス |
| `GNSS_IRQ_ID` | `61` | IRQ\_F2P[0] に対応するGIC割り込みID |
| `MAX2771_SPI_ID` | `XPAR_XSPIPS_0_DEVICE_ID` | SPI1の場合は `_1_` に変更 |

**レジスタアクセス:**
- `Xil_In32` / `Xil_Out32` を使用
- `Address` 引数はDWORDオフセット（`gnss_top` firmware 全体で統一されている単位）を受け取り、内部で `<< 2` してバイトアドレスに変換

**MAX2771 SPI初期化（`EnableRF()`）:**
- `XSpiPs_CfgInitialize` → `XSpiPs_SetOptions`（マスターモード、CPOL=0 CPHA=0）→ `XSpiPs_SetClkPrescaler`（〜6 MHz SCLK）の順に初期化
- `max2771_init_gps_l1()` が以下の順にレジスタを書き込む:
  1. `PLLCONF` (0x3): PLL設定（1575.42 MHzターゲット）
  2. `DIV` (0x4): Nディバイダ
  3. `FDIV` (0x5): 小数部（整数Nモードでは0）
  4. `CLK` (0x7): CLKOUTをXTALスルー（16.368 MHz）
  5. `CONF3` (0x2): PGAゲイン・フィルタ帯域
  6. `CONF2` (0x1): 2ビットI/Q出力モード
  7. `CONF1` (0x0): LNA・ミキサ・IFフィルタ有効化
  8. `STRM` (0x6): ストリーミング無効

**デフォルトのMAX2771設定ターゲット:**

| 項目 | 値 |
|---|---|
| 基準クリスタル | 16.368 MHz |
| RF入力 | GPS L1 (1575.42 MHz) |
| IF出力 | ≒4.092 MHz |
| CLKOUT (adc_clk) | 16.368 MHz |
| 出力ビット幅 | 2ビット I + 2ビット Q（符号+大きさ） |

> **注意:** レジスタ値（`MAX2771_CONF1_VAL` 等）は参考値です。実際のボードのクリスタル周波数、フィルタ帯域、LNAゲイン等に応じてMAX2771データシートを参照して調整してください。

---

## 4. Vivadoプロジェクト構築手順

### 4.1 ソースファイルの追加

以下のファイルを **追加する（上書き対象）**:

```
BB_HW/rtl/address.v                     ← グローバルインクルードとしてマーク
BB_HW/rtl/mem_model/spram.v
BB_HW/rtl/mem_model/dpram_full.v
BB_HW/rtl/mem_model/dpram_rw.v
BB_HW/rtl/backend_wrapper/mem_wrapper.v
BB_HW/rtl/xilinx/xilinx_clk_gate.v     ← clock_gating.v の代替
BB_HW/rtl/xilinx/xilinx_rom_wrapper.v  ← sprom/mem_wrapper.v ROM部の代替
BB_HW/rtl/xilinx/max2771_if.v
BB_HW/rtl/xilinx/gnss_top_axi.v        ← PL設計のトップモジュール
BB_HW/rtl/correlation/*.v
BB_HW/rtl/acquire_engine/*.v
BB_HW/rtl/tracking_engine/*.v
BB_HW/rtl/mem_arbiter/*.v
BB_HW/rtl/gnss_top/*.v
BB_HW/rtl/xilinx/constraints_template.xdc
```

以下のファイルは **追加しない**:

```
BB_HW/rtl/mem_model/sprom.v             ← xilinx_rom_wrapper.v で代替
BB_HW/rtl/backend_wrapper/clock_gating.v  ← xilinx_clk_gate.v で代替
```

### 4.2 IPインテグレータ ブロックデザイン

1. **Zynq7 Processing System** を追加
   - GP0 AXI Masterポートを有効化
   - `FCLK_CLK0` = 100 MHz（推奨）
   - SPI0 を有効化（MIOまたはEMIOで MAX2771 SPI端子へ接続）
2. **AXI Interconnect** を追加し、GP0 → gnss_top_axi を接続
3. **Processor System Reset** を追加し、`peripheral_aresetn` → `S_AXI_ARESETN` に接続
4. **Address Editor** で `gnss_top_axi` のベースアドレスを確認（例: `0x43C0_0000`）
5. `irq` ポート → `IRQ_F2P[0]` に接続
6. `adc_clk`、`max_i`、`max_q`、`pps_pulse*` をブロックデザインの外部ポートとして出力

### 4.3 ROMデータファイルの配置

Vivadoプロジェクトディレクトリ（`.xpr` のあるフォルダ）に以下をコピー:
```
BB_HW/rom_init/b1c_legendre.mem
BB_HW/rom_init/l1c_legendre.mem
BB_HW/rom_init/memory_code.mem
```

---

## 5. 既知の問題・注意事項

### gnss_top.v の軽微なバグ

`gnss_top.v` 268行目に `wire ae_ram_en;` が宣言されているが、実際にAEサンプルバッファに接続されているのは `ae_ram_ena`（`ae_top` の出力ポート、暗黙のワイヤ）。`ae_ram_en` は未接続の死んだ宣言。合成は正常に動作するが、Vivadoが警告を出す。

### MAX2771レジスタ値の検証

`HWCtrl_Zynq.c` の `MAX2771_*_VAL` 定数は16.368 MHzクリスタルを前提とした参考値。ボードの仕様（クリスタル周波数、外付けフィルタ、LNAバイアス等）に応じてMAX2771データシートのTable 2〜4を参照して適宜調整すること。

### sync_data の動作条件

`sync_data.v` は「システムクロックがADCクロックより十分高速」という前提で動作する。`adc_clk`（MAX2771 CLKOUT）が16.368 MHzの場合、システムクロックは最低でも50 MHz以上（推奨100 MHz）が必要。
