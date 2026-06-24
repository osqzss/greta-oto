# Zynq + MAX2771 Vivadoプロジェクト作成 — 引継ぎメモ

このファイルを読み込んでから作業を再開してください。

---

## 現在の状況

greta-oto GNSS受信機（Verilog RTL）を **Digilent Zybo Z7-20**（XC7Z020-1CLG400C）+ **MAX2771** RF フロントエンドICで動作させるためのポート作業を進めている。

### 完了済みの作業

前セッションで以下のファイルをすべて新規作成した。詳細は `docs/Zynq_MAX2771_Port.md` を参照。

| ファイル | 内容 |
|---|---|
| `BB_HW/rtl/xilinx/gnss_top_axi.v` | AXI4-Liteスレーブラッパー（PL設計トップ） |
| `BB_HW/rtl/xilinx/max2771_if.v` | MAX2771 I/Q → 8bit S/M フォーマット変換 |
| `BB_HW/rtl/xilinx/xilinx_clk_gate.v` | BUFGCEによるクロックゲーティング置き換え |
| `BB_HW/rtl/xilinx/xilinx_rom_wrapper.v` | xpm_memory_spromによるROMラッパー |
| `BB_HW/rtl/xilinx/constraints_template.xdc` | タイミング・ピン制約テンプレート（ピン番号は未確定） |
| `BB_HW/rom_init/b1c_legendre.coe/.mem` | BDS B1C Legendreコード（640×16bit） |
| `BB_HW/rom_init/l1c_legendre.coe/.mem` | GPS L1C Legendreコード（640×16bit） |
| `BB_HW/rom_init/memory_code.coe/.mem` | Galileo/BDS メモリコード（12800×32bit） |
| `tools/gen_rom_init.py` | 上記ROMデータ生成スクリプト（実行済み・検証済み） |
| `Firmware/Abstract/HWCtrl_Zynq.c` | Zynq向けHWアクセス層（XSpiPs + XScuGic） |

---

## 次のステップ：Vivadoプロジェクト一式の生成

以下の **5点の回答が揃い次第**、下記の成果物を一括作成する。

### 成果物リスト（作成予定）

1. **`vivado/create_project.tcl`** — Vivadoプロジェクトを再現するTclスクリプト  
2. **`BB_HW/rtl/xilinx/constraints_zybo_z7.xdc`** — 実ピン番号入りの最終XDCファイル（`constraints_template.xdc` を置き換え）  
3. **`vivado/bd_gnss_zynq.tcl`** — IP Integratorブロックデザイン Tcl（PS設定・AXI接続・EMIO SPI含む）  
4. **`Firmware/Abstract/HWCtrl_Zynq.c` の更新** — MAX2771レジスタ値を実クリスタル周波数に合わせて修正  

---

## 5つの未確認項目（次回セッション開始時に回答を提供すること）

### Q1. MAX2771基準クリスタル周波数

`HWCtrl_Zynq.c` のPLL設定レジスタ値（`MAX2771_PLLCONF_VAL`, `MAX2771_DIV_VAL` 等）はこの値に依存する。

- [ ] 16.368 MHz（標準GNSS基準、最も一般的）
- [ ] 24 MHz
- [ ] 26 MHz
- [ ] その他: ___ MHz

### Q2. MAX2771のI/Q出力ビット幅

`max2771_if.v` の配線と必要なPmodピン数が変わる。

- [ ] **2ビット**（符号+大きさ、I/Qそれぞれ2ビット = 合計4ピン、SNR良好）
- [ ] **1ビット**（符号のみ、I/Q各1ビット = 合計2ピン、配線シンプル）

### Q3. MAX2771接続に使用するPmodコネクタ

XDCのPACKAGE_PINを確定するために必要。1つのPmodで全信号を収容できる（下記参照）。

- [ ] JA
- [ ] JB
- [ ] JC（差動対あり → adc_clk に推奨）
- [ ] JD
- [ ] JE（一部PS MIO共有のため不推奨）

**2ビットモード時の想定ピン割り当て（8ピン使い切り）:**
```
Pmod Pin 1  → MAX2771 I[1]  (Iチャネル符号)
Pmod Pin 2  → MAX2771 I[0]  (Iチャネル大きさ)
Pmod Pin 3  → MAX2771 Q[1]  (Qチャネル符号)
Pmod Pin 4  → MAX2771 Q[0]  (Qチャネル大きさ)
Pmod Pin 7  → MAX2771 CLKOUT (adc_clk)
Pmod Pin 8  → SPI SCLK
Pmod Pin 9  → SPI SDATA (MOSI)
Pmod Pin 10 → SPI CS_N
```

**1ビットモード時（6ピン、1つのPmodで余裕あり）:**
```
Pmod Pin 1  → MAX2771 I[0]  (Iチャネル符号)
Pmod Pin 2  → MAX2771 Q[0]  (Qチャネル符号)
Pmod Pin 3  → MAX2771 CLKOUT (adc_clk)
Pmod Pin 7  → SPI SCLK
Pmod Pin 8  → SPI SDATA (MOSI)
Pmod Pin 9  → SPI CS_N
```

### Q4. PSファームウェア環境

`HWCtrl_Zynq.c` のISR接続方法と `create_project.tcl` のBSP設定が変わる。

- [ ] **ベアメタル**（Vitis / SDK standalone BSP）— 現在の `HWCtrl_Zynq.c` はこれ向けに実装済み
- [ ] **FreeRTOS**（割り込み接続部の軽微な修正が必要）
- [ ] **Linux / PetaLinux**（ドライバアーキテクチャが大幅に異なる）

### Q5. Digilent Zybo Z7-20 Master XDCファイル

XDCの正確なPCKAGE_PIN値はこのファイルから取得する。  
次回セッション開始時に以下を実行してその出力を貼り付けること:

```bash
! curl -sL "https://raw.githubusercontent.com/Digilent/digilent-xdc/master/Zybo-Z7-Master.xdc"
```

または手動でダウンロードして内容を貼り付ける。  
URL: https://github.com/Digilent/digilent-xdc/blob/master/Zybo-Z7-Master.xdc

---

## 次回セッションの進め方

1. このファイル（`docs/Zynq_Project_Handoff.md`）を読み込む
2. 上記Q1〜Q5の回答を提示する
3. Vivadoプロジェクト一式（Tclスクリプト3本 + 最終XDC + ファームウェア更新）を一括生成する

---

## 技術的な補足メモ（次回作業者向け）

### PS SPI の EMIO ルーティングについて
Zybo Z7-20 のMIOピンはUART/SD/Ethernet/USBで消費されており、PS SPI0/1 はMIO経由では使用不可。  
Vivado IP IntegratorでPS SPI0 を **EMIO経由**（PL IO）にルーティングし、Pmodピンに接続する。  
ブロックデザインでは `SPI0_SCLK_O`, `SPI0_MOSI_O`, `SPI0_SS_O` ポートをトップレベルに引き出す。

### adc_clk のタイミング制約について
`sync_data.v` はシステムクロックのエッジ検出でADCデータを取り込む非同期設計。  
`adc_clk` はFPGAのグローバルクロックネットワークには乗せない（`BUFG` 不使用）。  
XDCで `create_clock` + `set_clock_groups -asynchronous` を設定することで誤ったタイミング解析を防ぐ。  
可能であれば `adc_clk` をSRCC/MRCCピンに接続することを推奨（必須ではない）。

### gnss_top.v の既知バグ（軽微）
268行目に `wire ae_ram_en;` が宣言されているが、実際に使われるのは `ae_ram_ena`（ae_topの出力）。  
Vivadoが警告を出すが合成・動作には問題なし。

### VivadoバージョンについてのXPM依存
`xilinx_rom_wrapper.v` の `xpm_memory_sprom` は **Vivado 2019.1以降**で使用可能。  
古いバージョンを使う場合はBlock Memory Generator IPに切り替えること（.coeファイルは用意済み）。
