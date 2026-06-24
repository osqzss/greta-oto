# greta-oto GNSS受信機 — Zynq/MAX2771ポート 設計書

**対象ボード:** Digilent Zybo Z7-20 (XC7Z020-1CLG400C)  
**RFフロントエンド:** MAX2771 (2チップ搭載カスタムPMODボード、チップAのみ使用)  
**ファームウェア環境:** FreeRTOS (Vitis / Xilinx SDK standalone BSP)  
**Vivado要件:** 2019.1 以降 (xpm_memory使用のため)  
**バージョン:** v1.0.1628 (address.v `RELEASE_VERSION`)

---

## 目次

1. [システム全体構成](#1-システム全体構成)
2. [ハードウェア構成・ピン割り当て](#2-ハードウェア構成ピン割り当て)
3. [RFフロントエンド (MAX2771)](#3-rfフロントエンド-max2771)
4. [PL設計 (RTL)](#4-pl設計-rtl)
5. [PS設計 (FreeRTOS ソフトウェア)](#5-ps設計-freertos-ソフトウェア)
6. [Vivadoプロジェクト生成](#6-vivadoプロジェクト生成)
7. [レジスタマップ](#7-レジスタマップ)
8. [タイミング制約](#8-タイミング制約)
9. [既知の問題・注意事項](#9-既知の問題注意事項)
10. [ファイル一覧](#10-ファイル一覧)

---

## 1. システム全体構成

```
┌──────────────────────────────────────────────────────────────────┐
│  Zybo Z7-20 (XC7Z020-1CLG400C)                                   │
│                                                                   │
│  ┌──────────────────────┐   AXI4-Lite    ┌──────────────────┐   │
│  │  Zynq PS (ARM CortexA9)│ ◄──────────► │  gnss_top_axi    │   │
│  │  FreeRTOS              │  GP0 master   │  (PL トップ)     │   │
│  │  HWCtrl_Zynq.c         │               │                  │   │
│  │  PS SPI0 (EMIO) ───────┼─────────────►│  gnss_top        │   │
│  │  IRQ_F2P[0]    ◄───────┼──────────────┤  (コアRTL)       │   │
│  └──────────────────────┘               └──────────────────┘   │
│                                               │  │  │            │
│  JB Pmod                                  adc_clk  I/Q           │
│  ┌────────────────┐                       (JB)  (JC)             │
│  │ SCLK/MOSI/CS_A │◄── PS SPI0 EMIO                              │
│  └────────────────┘                                              │
│  └─── JC Pmod ─── max_i[1:0], max_q[1:0]                        │
│  └─── JB Pin5 ─── adc_clk (CLKOUT, 12 MHz)                      │
│  └─── JD Pmod ─── pps_pulse1/2/3, event_mark                    │
└──────────────────────────────────────────────────────────────────┘
        │ SPI          │ I/Q + CLKOUT
        ▼              ▼
   ┌─────────────────────────────┐
   │  MAX2771 カスタムPMODボード  │
   │  チップA (GPS L1CA用)        │
   │  チップB (予備、未使用)       │
   │  基準クリスタル: 24 MHz      │
   └─────────────────────────────┘
```

### 動作フロー

1. PS起動時に `EnableRF()` を呼び出し、PS SPI0でMAX2771チップAのレジスタを設定
2. MAX2771がGPS L1信号を受信し、12 MHzサンプルクロック (CLKOUT) と 2ビットI/Qデータを出力
3. PL側の `gnss_top_axi` がADCデータをAXIクロックドメインに同期し、捕捉・追尾処理を実行
4. 処理完了時に `irq` をPS側 `IRQ_F2P[0]` へ出力
5. PSのFreeRTOSタスクがISRで起床し、測定データをAXI経由で読み出す

---

## 2. ハードウェア構成・ピン割り当て

### 2.1 PMODボード接続

#### JB Pmod — SPI設定インターフェース + サンプルクロック

| PMODピン | FPGAピン | 信号名 | 方向 | 説明 |
|---|---|---|---|---|
| Pin1 | V8 | SPI SDATA | FPGA→MAX | PS SPI0 MOSI (EMIO) |
| Pin2 | W8 | SPI SCLK | FPGA→MAX | PS SPI0 SCLK (EMIO) |
| Pin3 | U7 | SPI /CS_B | FPGA→MAX | チップB チップセレクト (未使用) |
| Pin4 | V7 | SPI /CS_A | FPGA→MAX | チップA チップセレクト |
| Pin5 | Y7 | CLKOUT | MAX→FPGA | ADCサンプルクロック 12 MHz (MRCC対応ピン) |
| Pin6 | Y6 | — | — | 未接続 |
| Pin7 | V6 | LOCK_B | MAX→FPGA | チップB PLL LOCK (未使用) |
| Pin8 | W6 | LOCK_A | MAX→FPGA | チップA PLL LOCK (未使用) |

#### JC Pmod — I/Qデータ (チップA)

| PMODピン | FPGAピン | 信号名 | 説明 |
|---|---|---|---|
| Pin3 | T11 | Q[1]_A | Qチャネル符号ビット |
| Pin4 | T10 | I[1]_A | Iチャネル符号ビット |
| Pin5 | W14 | Q[0]_B | (チップB、未使用) |
| Pin6 | Y14 | I[0]_B | (チップB、未使用) |
| Pin7 | T12 | Q[0]_A | Qチャネル大きさビット |
| Pin8 | U12 | I[0]_A | Iチャネル大きさビット |

#### JD Pmod — PPS出力 + イベントマーク入力

| PMODピン | FPGAピン | 信号名 | 方向 | 説明 |
|---|---|---|---|---|
| Pin1 | T14 | pps_pulse1 | FPGA→外部 | PPSパルス出力1 |
| Pin2 | T15 | pps_pulse2 | FPGA→外部 | PPSパルス出力2 |
| Pin3 | P14 | pps_pulse3 | FPGA→外部 | PPSパルス出力3 |
| Pin4 | R14 | event_mark | 外部→FPGA | タイミング基準入力 |

### 2.2 XDCファイル

`BB_HW/rtl/xilinx/constraints_zybo_z7.xdc` に全制約を記述。  
`constraints_template.xdc` はこのファイルに置き換える。

---

## 3. RFフロントエンド (MAX2771)

### 3.1 動作パラメータ (チップA、GPS L1CA)

| パラメータ | 値 |
|---|---|
| 基準クリスタル | 24 MHz |
| LO周波数 | 1572.420 MHz |
| IF周波数 | 3.0 MHz (低側注入、GPS L1 − LO) |
| フィルタ帯域幅 | 4.2 MHz |
| ADCサンプルレート (CLKOUT) | 12 MHz |
| 出力フォーマット | 2ビット符号/大きさ (I チャネルのみ) |
| 量子化 | 01=+3, 00=+1, 10=−1, 11=−3 |

> **注:** チップB (L5等向け) はこのプロジェクトでは使用しない。

### 3.2 レジスタ設定値

`pocketgnss.c` (実機動作確認済み) から取得した値を `HWCtrl_Zynq.c` に定数として定義している。

| レジスタ番号 | 名前 | 設定値 |
|---|---|---|
| REG0 | CONF1 | `0xA2241BD7` |
| REG1 | CONF2 | `0x20550288` |
| REG2 | CONF3 | `0x0E8F21D0` |
| REG3 | PLLCONF | `0x49888008` |
| REG4 | DIV | `0x0CCBEC80` |
| REG5 | FDIV | `0x00000070` |
| REG6 | STRM | `0x08000000` |
| REG7 | CLK | `0x00000000` |
| REG8 | TEST1 | `0x01E0F281` |
| REG9 | TEST2 | `0x00000002` |
| REG10 | TEST3 | `0x00000004` |

### 3.3 SPIプロトコル

本ボードのMAX2771は **48ビットトランザクション** を使用する (標準MAX2771の32ビットとは異なる)。

```
[47:36]  12ビット レジスタアドレス (ビット[11:4]=0、ビット[3:0]=アドレス番号)
[35]     R/W ビット (0=書き込み、1=読み出し)
[34:32]  000 (パディング)
[31:0]   32ビット データ (MSBファースト)
```

Cでの実装 (`HWCtrl_Zynq.c:max2771_write_reg`):

```c
u8 tx[6];
tx[0] = 0x00;                            // アドレス上位 (0-10 は常に0)
tx[1] = (u8)((reg_addr & 0x0F) << 4);   // アドレス下位4ビット + R/W=0 + pad
tx[2..5] = data (MSB first);
XSpiPs_PolledTransfer(&spi_inst, tx, NULL, 6);
```

### 3.4 SPI初期化順序

PLL → 分周器 → クロック → アナログ → 出力 の順で書き込む。

```
PLLCONF → DIV → FDIV → CLK → CONF3 → CONF2 → CONF1 → STRM → TEST1 → TEST2 → TEST3
```

---

## 4. PL設計 (RTL)

### 4.1 モジュール階層

```
gnss_top_axi                  (BB_HW/rtl/xilinx/gnss_top_axi.v)
│  AXI4-Liteスレーブラッパー
│
├── max2771_if                 (BB_HW/rtl/xilinx/max2771_if.v)
│      MAX2771 2ビットI/Q → 8ビット S/M変換
│
└── gnss_top                  (BB_HW/rtl/gnss_top/gnss_top.v)
    │  GNSSコアトップ
    │
    ├── sync_data              (BB_HW/rtl/gnss_top/sync_data.v)
    │      ADCクロック→システムクロック ドメイン同期
    │
    ├── te_fifo                (tracking_engine 内)
    │      サンプルFIFO (10240サンプル × 8ビット)
    │
    ├── tracking_engine        (BB_HW/rtl/tracking_engine/tracking_engine.v)
    │      追尾エンジン (TE)
    │      ├── correlator ×N チャネル
    │      ├── down_converter (DDC)
    │      └── coherent_fifo / te_fifo
    │
    ├── ae_top                 (BB_HW/rtl/acquire_engine/ae_top.v)
    │      捕捉エンジン (AE)
    │      └── mf_core (マッチドフィルタ)
    │
    ├── mem_arbiter            (BB_HW/rtl/mem_arbiter/mem_arbiter.v)
    │      BRAM アービタ
    │
    ├── pps / pps_async        (BB_HW/rtl/gnss_top/pps.v, pps_async.v)
    │      PPS生成・タイムスタンプ
    │
    └── (Xilinx固有モジュール置き換え)
        ├── gated_clock_wrapper → xilinx_clk_gate.v (BUFGCE)
        └── ROM wrappers        → xilinx_rom_wrapper.v (xpm_memory_sprom)
```

### 4.2 gnss_top_axi — AXI4-Liteラッパー

**ファイル:** `BB_HW/rtl/xilinx/gnss_top_axi.v`

| ポート | 幅 | 方向 | 説明 |
|---|---|---|---|
| `S_AXI_ACLK` | 1 | in | AXIクロック (FCLK_CLK0, 100 MHz) |
| `S_AXI_ARESETN` | 1 | in | AXIリセット (Active-Low) |
| `S_AXI_AW*` | — | in | 書き込みアドレスチャネル |
| `S_AXI_W*` | — | in | 書き込みデータチャネル |
| `S_AXI_B*` | — | out | 書き込み応答チャネル |
| `S_AXI_AR*` | — | in | 読み出しアドレスチャネル |
| `S_AXI_R*` | — | out | 読み出しデータチャネル |
| `adc_clk` | 1 | in | MAX2771 CLKOUT (12 MHz、非同期) |
| `max_i[1:0]` | 2 | in | Iチャネル [1]=符号 [0]=大きさ |
| `max_q[1:0]` | 2 | in | Qチャネル [1]=符号 [0]=大きさ |
| `event_mark` | 1 | in | 外部タイミング基準 |
| `pps_pulse1/2/3` | 1 | out | PPSパルス出力 |
| `pps_irq` | 1 | out | PPS割り込み (IRQ_F2P[1]に接続可) |
| `irq` | 1 | out | ベースバンド処理完了割り込み |

**アドレス空間:** `C_S_AXI_ADDR_WIDTH = 16` (64 KB)、PSからのバイトアドレス `[15:2]` をDWORDアドレスに変換して `gnss_top` へ渡す。

**読み出しレイテンシ:** `gnss_top` はクロック立ち上がり後にデータを出力するため、AXIラッパーは1サイクルの待機ステート (`RD_WAIT`) を挿入する。

### 4.3 max2771_if — フォーマット変換

**ファイル:** `BB_HW/rtl/xilinx/max2771_if.v`

MAX2771の2ビット符号/大きさ出力を、`gnss_top` が要求する8ビットフォーマットに変換する。

```
adc_data[7:4] = {max_i[1], max_i[0], 2'b00}   // Iチャネル (符号, 大きさ, 0, 0)
adc_data[3:0] = {max_q[1], max_q[0], 2'b00}   // Qチャネル
```

組み合わせ回路のみ、レジスタなし。

### 4.4 sync_data — ADCクロックドメイン同期

**ファイル:** `BB_HW/rtl/gnss_top/sync_data.v`

ADCクロック (`adc_clk`, 12 MHz) はシステムクロック (`clk`, 100 MHz) に対して非同期。  
`sync_data` は2段フリップフロップでADCクロックエッジを検出し、ADCデータをシステムクロックドメインに安全に取り込む。

- `adc_clk` は `BUFG` を通さず直接PLIOピンから使用 (グローバルクロックネット不使用)
- `syn_preserve = 1` 属性で最適化を防止
- システムクロックがADCクロックの十分な整数倍であること (`100/12 ≒ 8.3倍`) を前提とする

### 4.5 Xilinx固有置き換えモジュール

| 汎用モジュール | Xilinx置き換え | ファイル | 備考 |
|---|---|---|---|
| `clock_gating.v` | `xilinx_clk_gate.v` | `BB_HW/rtl/xilinx/` | BUFGCE プリミティブ使用 |
| `mem_wrapper.v` (ROM部) | `xilinx_rom_wrapper.v` | `BB_HW/rtl/xilinx/` | `xpm_memory_sprom` 使用 |

#### ROMサイズ

| ROMモジュール名 | 深さ×幅 | COEファイル | MEMファイル | 用途 |
|---|---|---|---|---|
| `b1c_legendre_rom_640x16_wrapper` | 640×16 bit | `b1c_legendre.coe` | `b1c_legendre.mem` | BDS B1C Legendreコード |
| `l1c_legendre_rom_640x16_wrapper` | 640×16 bit | `l1c_legendre.coe` | `l1c_legendre.mem` | GPS/QZSS L1C Legendreコード |
| `memory_code_rom_12800x32_wrapper` | 12800×32 bit | `memory_code.coe` | `memory_code.mem` | Galileo/BDS メモリコード |

COE/MEMファイルは `tools/gen_rom_init.py` で生成済み。

---

## 5. PS設計 (FreeRTOS ソフトウェア)

### 5.1 ソフトウェア層構成

```
Application (GNSS signal processing tasks)
    │
PlatformCtrl_FreeRTOS.c    FreeRTOSタスク/キュー/セマフォ抽象化
    │
HWCtrl_Zynq.c              Zynq向けハードウェアアクセス層
    │
Xilinx BSP (XSpiPs, XScuGic, Xil_IO)
    │
Zynq PS (ARM Cortex-A9, 100 MHz)
```

**ファイル:** `Firmware/Abstract/HWCtrl_Zynq.c`

### 5.2 主要な関数

#### `EnableRF(void)`

PS SPI0を初期化し、MAX2771チップAの全レジスタを書き込む。  
FreeRTOSスケジューラ起動前のシステム初期化フェーズで呼び出すこと。

```
XSpiPs_CfgInitialize  →  XSpiPs_SetOptions (Master, ForceSelect, Mode0)
→  XSpiPs_SetClkPrescaler (PRESCALE_16 = 6.25 MHz)
→  max2771_init_gps_l1 (REG0〜REG10 を順番に書き込み)
```

#### `AttachBasebandISR(InterruptFunction ISR)`

FreeRTOS環境ではGICはFreeRTOSポートコードが初期化済み。  
`xInterruptController` (externシンボル) に割り込みハンドラを接続するだけでよい。

```c
// bare-metalでのNG例 (FreeRTOS環境では使用禁止)
// XScuGic_CfgInitialize(&gic_inst, ...);        // ← GICを二重初期化してしまう
// Xil_ExceptionRegisterHandler(...);            // ← FreeRTOSの例外ハンドラを上書きしてしまう

// FreeRTOS環境での正しい実装
extern XScuGic xInterruptController;
XScuGic_Connect(&xInterruptController, GNSS_IRQ_ID, (Xil_InterruptHandler)ISR, NULL);
XScuGic_Enable(&xInterruptController, GNSS_IRQ_ID);
```

#### `GetRegValue / SetRegValue`

AXI経由で `gnss_top` レジスタにアクセス。  
アドレスはDWORDオフセット (`address.v` の定義値) で指定する。

```c
#define GNSS_BASE_ADDR  0x43C00000UL
Xil_In32(GNSS_BASE_ADDR + (Address << 2));   // 読み出し
Xil_Out32(GNSS_BASE_ADDR + (Address << 2), Value);  // 書き込み
```

### 5.3 SPI詳細パラメータ

| パラメータ | 値 | 説明 |
|---|---|---|
| `MAX2771_SPI_ID` | `XPAR_XSPIPS_0_DEVICE_ID` | PS SPI0 |
| `MAX2771_SPI_PRESCALER` | `XSPIPS_CLK_PRESCALE_16` | APB 100MHz / 16 = 6.25 MHz SCLK |
| SPI モード | CPOL=0, CPHA=0 (Mode 0) | MAX2771はMode 0対応 |
| トランザクション長 | 6バイト (48ビット) | 16ビットヘッダ + 32ビットデータ |
| CS制御 | `XSPIPS_FORCE_SSELECT_OPTION` | ソフトウェアでアサート/デアサート |

### 5.4 割り込み

| 割り込み | GIC ID | IRQ_F2P ビット | 発生タイミング |
|---|---|---|---|
| GNSS ベースバンド完了 | 61 | [0] | 追尾エンジン1ブロック処理完了時 |

---

## 6. Vivadoプロジェクト生成

### 6.1 スクリプト構成

| ファイル | 役割 |
|---|---|
| `vivado/create_project.tcl` | プロジェクト作成・ソース追加・ラッパー生成のメインスクリプト |
| `vivado/bd_gnss_zynq.tcl` | IP Integratorブロックデザイン (`gnss_zynq.bd`) 生成スクリプト |

### 6.2 プロジェクト生成手順

```bash
cd <リポジトリルート>/vivado
vivado -mode batch -source create_project.tcl
```

またはVivado Tclコンソールから:
```tcl
source <リポジトリルート>/vivado/create_project.tcl
```

生成物: `vivado/gnss_zynq/gnss_zynq.xpr`

### 6.3 ブロックデザイン構成

```
processing_system7_0  (Zynq PS7 v5.5)
│  FCLK_CLK0 = 100 MHz
│  M_AXI_GP0 有効
│  SPI0 = EMIO (JBピンに外部ポートとして引き出し)
│  IRQ_F2P 有効
│
├── proc_sys_reset_0  (v5.0)
│
├── axi_interconnect_0  (v2.1, 1M/1S)
│
└── gnss_top_axi_0  (モジュール参照)

外部ポート:
  adc_clk         (I)  ← JB Pin5 Y7
  max_i[1:0]      (I)  ← JC
  max_q[1:0]      (I)  ← JC
  spi0_sclk_o     (O)  → JB Pin2 W8
  spi0_mosi_o     (O)  → JB Pin1 V8
  spi0_ss_o[2:0]  (O)  → JB (ss_o[0]=CS_A)
  pps_pulse1/2/3  (O)  → JD
  event_mark      (I)  ← JD Pin4
```

**AXIアドレス:** `gnss_top_axi` は `0x43C00000` (64 KB) に割り当て。  
(`GNSS_BASE_ADDR` in `HWCtrl_Zynq.c` と一致させること)

### 6.4 ハードウェアエクスポートとBSP生成

1. Vivadoで合成・実装・ビットストリーム生成
2. **File → Export → Export Hardware** (ビットストリームを含む `.xsa` を出力)
3. Vitis で Platform Project を作成し、FreeRTOS BSP を選択
4. BSPにより `xparameters.h` が生成される (`XPAR_XSPIPS_0_DEVICE_ID` 等)
5. `HWCtrl_Zynq.c` をアプリケーションプロジェクトに追加
6. FreeRTOSポートコードに `xInterruptController` が定義されていることを確認

---

## 7. レジスタマップ

AXI基底アドレス: `0x43C00000`  
アクセス: DWORD単位、アドレスオフセット = `address.v` の定義値 × 4バイト

### 7.1 グローバルレジスタ (ベースオフセット 0x0000)

| オフセット | 名前 | R/W | 主要フィールド |
|---|---|---|---|
| 0x00 | `GLB_BB_ENABLE` | R/W | [8] TE有効 |
| 0x04 | `GLB_BB_RESET` | W | [8]FIFOリセット [1]TEリセット [0]AEリセット |
| 0x08 | `GLB_FIFO_CLEAR` | W | [9]PPSラッチ [8]FIFOクリア [0]FIFOラッチ |
| 0x0C | `GLB_TRACKING_START` | R/W | [0] TE開始トリガ |
| 0x10 | `GLB_MEAS_NUMBER` | R/W | [9:0] 測定ブロック数 |
| 0x14 | `GLB_MEAS_COUNT` | R/W | [9:0] 測定カウンタ |
| 0x18 | `GLB_INTERRUPT_FLAG` | R/W | [11:8] 割り込みフラグ (W=クリア) |
| 0x1C | `GLB_REQUEST_COUNT` | R/W | [9:0] リクエストカウント |
| 0x20 | `GLB_INTERRUPT_MASK` | R/W | [11:8] 割り込みマスク |
| 0x40 | `GLB_BB_VERSION` | R | [31:24]メジャー [23:16]マイナー [15:0]リリース |

### 7.2 捕捉エンジン (AE) レジスタ (ベースオフセット 0x1000)

| オフセット | 名前 | 説明 |
|---|---|---|
| 0x1000 | `AE_CONFIG` | AE設定 |
| 0x1004 | `AE_CONTROL` | AE制御 |
| 0x1008 | `AE_BUFFER_CONTROL` | バッファ制御 |
| 0x100C | `AE_STATUS` | AEステータス |
| 0x1010 | `AE_CARRIER_FREQ` | 搬送波周波数設定 |
| 0x1014 | `AE_CODE_RATIO` | コードレート比 |
| 0x1018 | `AE_THRESHOLD` | 検出閾値 |

### 7.3 TE FIFOレジスタ (ベースオフセット 0x1400)

| オフセット | 名前 | 説明 |
|---|---|---|
| 0x1400 | `TE_FIFO_CONFIG` | FIFO設定 |
| 0x1404 | `TE_FIFO_STATUS` | FIFOステータス |
| 0x1410 | `TE_FIFO_GUARD` | ガードアドレス |
| 0x1414 | `TE_FIFO_READ_ADDR` | 読み出しアドレス |
| 0x1418 | `TE_FIFO_WRITE_ADDR` | 書き込みアドレス |

### 7.4 追尾エンジン (TE) レジスタ (ベースオフセット 0x1800)

| オフセット | 名前 | 説明 |
|---|---|---|
| 0x1800 | `TE_CHANNEL_ENABLE` | チャネル有効ビットマップ |
| 0x1804 | `TE_COH_DATA_READY` | コヒーレントデータ準備完了フラグ |
| 0x1808 | `TE_SEGMENT_NUMBER` | セグメント番号 |
| 0x1818+ | `TE_POLYNOMIAL` | 多項式設定 (チャネル毎) |
| 0x1860 | `TE_NOISE_CONFIG` | ノイズ設定 |

### 7.5 ペリフェラル/PPSレジスタ (ベースオフセット 0x1C00)

| オフセット | 名前 | 説明 |
|---|---|---|
| 0x1C00 | `PPS_CTRL` | PPS制御 |
| 0x1C04 | `PPS_EM_CTRL` | イベントマーク制御 |
| 0x1C08 | `PPS_PULSE_INTERVAL` | PPSパルス間隔 |
| 0x1C0C | `PPS_PULSE_ADJUST` | パルス微調整 |
| 0x1C10/18/20 | `PPS_PULSE_CTRL1/2/3` | 各PPSパルス制御 |
| 0x1C14/1C/24 | `PPS_PULSE_WIDTH1/2/3` | 各PPSパルス幅 |
| 0x1C30/34 | `PPS_CLK/PULSE_COUNT_LATCH_CPU` | CPUラッチタイムスタンプ |
| 0x1C38/3C | `PPS_CLK/PULSE_COUNT_LATCH_EM` | イベントマークラッチタイムスタンプ |

### 7.6 バッファ領域

| アドレス範囲 | 名前 | 説明 |
|---|---|---|
| 0x2000〜0x2FFF | TEバッファ | 追尾エンジン読み出しバッファ |
| 0x3000〜0x3FFF | AEバッファ | 捕捉エンジン読み出しバッファ |

---

## 8. タイミング制約

### 8.1 クロックドメイン

| クロック名 | 源 | 周波数 | 備考 |
|---|---|---|---|
| `clk_fpga_0` | Zynq FCLK_CLK0 | 100 MHz | AXI/システムクロック |
| `adc_clk` | MAX2771 CLKOUT (JB Y7) | 12 MHz | ADCサンプルクロック、非同期 |

### 8.2 `adc_clk` の取り扱い

```xdc
create_clock -period 83.333 -name adc_clk [get_ports adc_clk]

set_clock_groups -asynchronous \
    -group [get_clocks clk_fpga_0] \
    -group [get_clocks adc_clk]
```

- `adc_clk` は `BUFG` を通さない (グローバルクロックネット不使用)
- JB Pin5 (Y7) は `IO_L13P_T2_MRCC_13` (Multi-Region Clock Capable) ピンを使用
- `sync_data.v` がCDCを担当し、2段FFでエッジ検出

### 8.3 I/Qデータ入力タイミング

MAX2771はCLKOUTの立ち上がりエッジ後3〜5 nsでI/Qデータを駆動する。

```xdc
set_input_delay -clock adc_clk -max 5.0 [get_ports {max_i[*] max_q[*]}]
set_input_delay -clock adc_clk -min 1.0 [get_ports {max_i[*] max_q[*]}]
```

---

## 9. 既知の問題・注意事項

### 9.1 `gnss_top.v` のワイヤ宣言の警告

`gnss_top.v` 268行目付近に `wire ae_ram_en;` が宣言されているが、実際に使われるのは `ae_ram_ena` (ae_topの出力)。  
Vivadoは警告を出すが、合成・動作には問題ない。

### 9.2 XPMの Vivadoバージョン依存

`xilinx_rom_wrapper.v` の `xpm_memory_sprom` は **Vivado 2019.1以降** が必要。  
旧バージョンを使う場合はBlock Memory Generator IPに切り替えること (`.coe` ファイルは用意済み)。

### 9.3 PS SPI0 MIOピン競合

Zybo Z7-20のMIOピンはUART/SD/Ethernet/USBで消費されているため、PS SPI0はMIO経由では使用不可。  
ブロックデザインでSPI0を **EMIO経由** でPLに出力し、JBピンに接続する。

### 9.4 FreeRTOS環境でのGIC初期化禁止

`AttachBasebandISR` 内でGICを再初期化したり、`Xil_ExceptionRegisterHandler` / `Xil_ExceptionEnable` を呼んではならない。これらはFreeRTOSポートコードがすでに設定済みであり、上書きするとFreeRTOSのタイマーティックや他の割り込みが破壊される。

### 9.5 MAXSPIトランザクション長

標準MAX2771 (32ビット) と本ボード搭載チップ (48ビット) はSPIプロトコルが異なる。  
既存の32ビット実装コードをそのまま流用すると設定が書き込まれないので注意。  
`HWCtrl_Zynq.c` の `max2771_write_reg` は6バイト転送に修正済み。

### 9.6 `spi0_ss_o[0]` のみ使用

ブロックデザインでは `spi0_ss_o[2:0]` の3ビットバスが外部ポートとして引き出されるが、  
XDCで `spi0_ss_o[0]` (→ JB Pin4 V7, CS_A) のみ制約している。  
`[1]`/`[2]` は未接続 (基板上でCS_Bと電源/GNDに接続されているか確認すること)。

---

## 10. ファイル一覧

### 10.1 新規作成ファイル (今回のZynq/MAX2771ポートで追加)

| ファイルパス | 説明 |
|---|---|
| `BB_HW/rtl/xilinx/gnss_top_axi.v` | AXI4-Liteスレーブラッパー (PLトップ) |
| `BB_HW/rtl/xilinx/max2771_if.v` | MAX2771 I/Q → 8ビット S/Mフォーマット変換 |
| `BB_HW/rtl/xilinx/xilinx_clk_gate.v` | BUFGCEによるクロックゲーティング置き換え |
| `BB_HW/rtl/xilinx/xilinx_rom_wrapper.v` | xpm_memory_spromによるROMラッパー |
| `BB_HW/rtl/xilinx/constraints_zybo_z7.xdc` | **最終XDC** (実ピン番号、タイミング制約) |
| `BB_HW/rom_init/b1c_legendre.coe/.mem` | BDS B1C Legendreコード (640×16 bit) |
| `BB_HW/rom_init/l1c_legendre.coe/.mem` | GPS L1C Legendreコード (640×16 bit) |
| `BB_HW/rom_init/memory_code.coe/.mem` | Galileo/BDS メモリコード (12800×32 bit) |
| `tools/gen_rom_init.py` | ROMデータ生成スクリプト |
| `Firmware/Abstract/HWCtrl_Zynq.c` | Zynq向けHWアクセス層 **(今回更新)** |
| `vivado/create_project.tcl` | Vivadoプロジェクト再現Tclスクリプト |
| `vivado/bd_gnss_zynq.tcl` | IP Integratorブロックデザインスクリプト |

### 10.2 既存コアRTLファイル (変更なし)

| ディレクトリ | 主要ファイル | 説明 |
|---|---|---|
| `BB_HW/rtl/` | `address.v` | レジスタアドレス定義 (全モジュール共通インクルード) |
| `BB_HW/rtl/gnss_top/` | `gnss_top.v`, `sync_data.v`, `pps.v`, `pps_async.v` | GNSSコアトップ・PPS生成 |
| `BB_HW/rtl/tracking_engine/` | `tracking_engine.v` 他 | 追尾エンジン |
| `BB_HW/rtl/acquire_engine/` | `ae_top.v` 他 | 捕捉エンジン |
| `BB_HW/rtl/correlation/` | `correlator.v` 他 | 相関器・DDC |
| `BB_HW/rtl/mem_arbiter/` | `mem_arbiter.v` 他 | メモリアービタ |
| `BB_HW/rtl/mem_model/` | `spram.v`, `sprom.v` 他 | 汎用RAMモデル |
| `BB_HW/rtl/backend_wrapper/` | `clock_gating.v`, `mem_wrapper.v` | Xilinx版で置き換えるモジュール |
| `Firmware/Abstract/` | `PlatformCtrl_FreeRTOS.c` | FreeRTOS OS抽象化層 |

### 10.3 参考資料

| ファイルパス | 説明 |
|---|---|
| `Documents/Zybo-Z7-Master.xdc` | Digilent公式マスターXDC |
| `Documents/Zybo_MAX2771_PMOD.md` | PMODボードのピン接続情報 |
| `Documents/pocketgnss.c` | MAX2771レジスタ動作確認用ビットバンSPIサンプルコード |
| `Documents/Zynq_MAX2771_Port.md` | 前セッションの作業記録 |
