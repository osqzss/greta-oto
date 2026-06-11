# Vivadoプロジェクト生成・実機確認 — セッション引継ぎメモ

このファイルを読み込んでから作業を再開してください。

---

## 前回セッションで完了した作業

### 回答済みQ1〜Q5 (Zybo_MAX2771_PMOD.md より)

| 質問 | 回答 |
|---|---|
| Q1 基準クリスタル周波数 | **24 MHz** |
| Q2 I/Q出力ビット幅 | **2ビット符号/大きさ** (01=+3, 00=+1, 10=−1, 11=−3) |
| Q3 使用PMODコネクタ | **JB=SPI+CLKOUT、JC=I/Qデータ、チップA使用** |
| Q4 PSファームウェア環境 | **FreeRTOS** |
| Q5 Master XDCファイル | Documents/Zybo-Z7-Master.xdc として入手済み |

---

### 新規作成・更新ファイル

| ファイルパス | 種別 | 内容 |
|---|---|---|
| `BB_HW/rtl/xilinx/constraints_zybo_z7.xdc` | **新規** | 実ピン番号入り最終XDC |
| `vivado/create_project.tcl` | **新規** | Vivadoプロジェクト再現スクリプト |
| `vivado/bd_gnss_zynq.tcl` | **新規** | IP Integratorブロックデザインスクリプト |
| `Firmware/Abstract/HWCtrl_Zynq.c` | **更新** | 24MHz対応・48ビットSPI・FreeRTOS ISR修正 |
| `Documents/Design_Document.md` | **新規** | 本受信機の設計書 (全体仕様) |

---

### HWCtrl_Zynq.c の主な変更点 (前セッションから)

1. **SPIプロトコル修正**: 32ビット→**48ビット**  
   本ボードのMAX2771は `pocketgnss.c` と同じ 16ビットヘッダ＋32ビットデータのプロトコルを使う。旧実装 (4ビットアドレス+28ビットデータの32ビット) では設定が書き込まれない。

   ```c
   // ヘッダフォーマット: addr[11:0] MSBファースト, R/W=0, 000パッド
   tx[0] = 0x00;
   tx[1] = (u8)((reg_addr & 0x0F) << 4);
   tx[2..5] = data (MSB first);
   XSpiPs_PolledTransfer(&spi_inst, tx, NULL, 6);  // 6バイト = 48ビット
   ```

2. **レジスタ値を24MHz用に変更**: `pocketgnss.c` の実機確認済み値を使用
   - F_LO = 1572.420 MHz → IF = 3.0 MHz
   - F_ADC = 12 MHz (CLKOUT)
   - REG0〜REG10の11レジスタすべて定義

3. **FreeRTOS対応ISR**: GICを再初期化しない (`xInterruptController` externを使用)

---

## 次回セッションでやること

### ステップ1: Vivadoプロジェクト生成

```bash
cd <リポジトリルート>/vivado
vivado -mode batch -source create_project.tcl
```

生成物: `vivado/gnss_zynq/gnss_zynq.xpr`

**注意点:**
- `vivado/gnss_zynq/` ディレクトリはスクリプトが自動作成する
- `.mem` ファイルをプロジェクトディレクトリにコピーする必要があるかもしれない  
  → `BB_HW/rom_init/*.mem` を `vivado/gnss_zynq/` にコピーする

### ステップ2: Vivadoで合成・実装・ビットストリーム生成

1. `vivado/gnss_zynq/gnss_zynq.xpr` を開く
2. **Synthesis → Implementation → Generate Bitstream** を順番に実行
3. タイミングクリア確認: `adc_clk` ↔ `clk_fpga_0` 間は `set_clock_groups -asynchronous` で除外済みなので警告は出てもエラーにはならないはず

### ステップ3: ハードウェアエクスポート

```
Vivado メニュー: File → Export → Export Hardware (Include Bitstream)
→ .xsa ファイルを出力
```

### ステップ4: Vitisでプロジェクト作成

1. Vitis を起動
2. **File → New → Platform Project**  
   - XSAファイル: 上記エクスポートした `.xsa` を選択  
   - OS: **FreeRTOS**
3. **File → New → Application Project**
   - Platform: 上記で作成したプラットフォーム
   - Template: "FreeRTOS Hello World" または空のプロジェクト
4. `Firmware/Abstract/HWCtrl_Zynq.c` をソースに追加
5. `xparameters.h` が自動生成されることを確認  
   (`XPAR_XSPIPS_0_DEVICE_ID`, `XPAR_SCUGIC_0_DEVICE_ID` 等)

### ステップ5: 実機での動作確認項目

優先度順:

#### 5-1. MAX2771 SPI通信確認
- `EnableRF()` 呼び出し後、UARTログを確認
- 可能であれば `pocketgnss.c` のように `read_reg` でレジスタ値を読み返して検証

#### 5-2. CLKOUT確認
- JB Pin5 (Y7) に12 MHzが来ていることをオシロスコープで確認
- Vivado ILA (Integrated Logic Analyzer) を使って `adc_clk` の周期を確認する方法もある

#### 5-3. I/Qデータ確認
- JC に信号が来ていることを確認 (アンテナ接続後)
- gnss_top の TE FIFO にサンプルが蓄積されるか確認  
  (`GLB_BB_ENABLE` → `GLB_TRACKING_START` → TE FIFO ステータス読み出し)

#### 5-4. 捕捉エンジン (AE) 動作確認
- GPS L1CA信号の捕捉を試みる
- `AE_STATUS` レジスタで捕捉完了フラグを確認

---

## 想定されるトラブルと対処法

### トラブル1: `bd_gnss_zynq.tcl` でモジュール参照が失敗する

**症状:** `create_bd_cell -type module -reference gnss_top_axi` がエラー  
**原因:** ソースファイルが fileset に登録される前にブロックデザインを生成しようとしている  
**対処:** `create_project.tcl` の `source bd_gnss_zynq.tcl` の前に  
`update_compile_order -fileset sources_1` が実行されているか確認

### トラブル2: `.mem` ファイルが見つからない

**症状:** 合成時に ROM 初期化ファイルが見つからないエラー  
**対処:** 
```bash
cp BB_HW/rom_init/*.mem vivado/gnss_zynq/
```
または `xilinx_rom_wrapper.v` の `MEMORY_INIT_FILE` パラメータに絶対パスを設定

### トラブル3: タイミングエラー (adc_clk 関連)

**症状:** `adc_clk` から `clk_fpga_0` へのパスでタイミング違反  
**確認:** `constraints_zybo_z7.xdc` の `set_clock_groups -asynchronous` が有効になっているか確認  
`clk_fpga_0` という名前がVivadoレポートの実際のクロック名と一致しているか確認  
→ Vivadoのタイミングレポートでシステムクロック名を確認し、必要に応じてXDCを修正する

### トラブル4: MAX2771 CLKOUT が出ない / 周波数が違う

**症状:** F_ADC = 12 MHz が出ていない  
**確認:** `pocketgnss.c` のビットバン実装で直接レジスタを書き込んで確認  
→ `pocketgnss.c` はビットバンGPIOを使っているので、MAX2771 SPIが確実に動くかを  
　 まずビットバンで確認してから PS SPI0 に移行するアプローチも有効

### トラブル5: FreeRTOS で割り込みが来ない

**症状:** ISRが呼ばれない  
**確認1:** `GNSS_IRQ_ID = 61` が正しいか → Vivadoブロックデザインで `IRQ_F2P[0]` のGIC IDを確認  
**確認2:** `AttachBasebandISR` が FreeRTOSスケジューラ起動後に呼ばれているか  
**確認3:** `xInterruptController` が FreeRTOS BSP の `xparameters.h` で公開されているか

---

## 技術メモ (再確認用)

### ピン割り当てクイックリファレンス

```
JB Pin5 Y7  → adc_clk      (MRCC, IO_L13P_T2_MRCC_13)
JB Pin1 V8  → spi0_mosi_o
JB Pin2 W8  → spi0_sclk_o
JB Pin4 V7  → spi0_ss_o[0] (CS_A)

JC Pin4 T10 → max_i[1]     (I符号)
JC Pin8 U12 → max_i[0]     (I大きさ)
JC Pin3 T11 → max_q[1]     (Q符号)
JC Pin7 T12 → max_q[0]     (Q大きさ)

JD Pin1 T14 → pps_pulse1
JD Pin2 T15 → pps_pulse2
JD Pin3 P14 → pps_pulse3
JD Pin4 R14 ← event_mark
```

### AXIアドレス

```
GNSS_BASE_ADDR = 0x43C00000  (64KB, HWCtrl_Zynq.c と bd_gnss_zynq.tcl で一致)
```

### MAX2771 SPIパラメータ

```
プロトコル: 48ビット (16ビットヘッダ + 32ビットデータ)
SCLK: 6.25 MHz  (APB 100MHz / PRESCALE_16)
モード: CPOL=0, CPHA=0 (Mode 0)
CS: アクティブLow, 手動制御 (FORCE_SSELECT)
```

### FreeRTOS GIC注意

```c
// 正しい実装 (GICはFreeRTOSが初期化済み)
extern XScuGic xInterruptController;
XScuGic_Connect(&xInterruptController, GNSS_IRQ_ID, ISR, NULL);
XScuGic_Enable(&xInterruptController, GNSS_IRQ_ID);

// NG (FreeRTOS環境では絶対にやってはいけない)
// XScuGic_CfgInitialize(...)
// Xil_ExceptionRegisterHandler(...)
// Xil_ExceptionEnable()
```

---

## 参照ドキュメント

| ファイル | 内容 |
|---|---|
| `Documents/Design_Document.md` | 本受信機の完全な設計書 |
| `Documents/Zybo_MAX2771_PMOD.md` | PMODボードのピン接続・Q1〜Q5の回答 |
| `Documents/pocketgnss.c` | MAX2771レジスタ動作確認用サンプルコード (ビットバンSPI) |
| `Documents/Zybo-Z7-Master.xdc` | Digilent公式マスターXDC |
| `Documents/Zynq_MAX2771_Port.md` | 前々セッションの作業記録 (RTLファイル新規作成) |
