# Greta-Oto Zynq/MAX2771 — Vivado/Vitis 2025.2 ビルド・実機動作確認手順

**対象:** Windows 11 + Vivado 2025.2 / Vitis 2025.2 (Unified IDE)
**ボード:** Digilent Zybo Z7-20 (`xc7z020clg400-1`)
**RFフロントエンド:** MAX2771 カスタムPMODボード (チップAのみ使用)

このファイルは [`Session_Handoff_Vivado.md`](Session_Handoff_Vivado.md) と
[`Design_Document.md`](Design_Document.md) を基にした実作業手順をまとめたものです。
背景・設計の詳細は両ドキュメントを参照してください。

---

## 現在の進捗（2026-06-24 時点）

| フェーズ | 状態 |
|---|---|
| §1 Vivado プロジェクト生成 | ✅ 完了（BD・SmartConnect・TTC0・アドレス 0x43C00000） |
| §2 Run Synthesis | ✅ 完了（警告のみ。§7-9 で分類・対処済み） |
| §2 Run Implementation | ⏳ **次回再開ポイント**。BUFG カスケード対処（§7-10）を入れて再実装する |
| §2.1 タイミング確認 | ⏳ 実装後に WNS/WHS を確認（特に Hold） |
| §3 ハードウェアエクスポート | 未 |
| §4 Vitis プラットフォーム＋アプリ | 未 |
| §5 実機動作確認 | 未 |

### 次回セッションの再開手順
1. **このファイル（`Build_Run_Vivado_Vitis_2025.2.md`）を読み込む** ← 最新の作業状態・全トラブル対処が
   ここに集約されている。まずこれ。
2. 補足が必要なら [`Session_Handoff_Vivado.md`](Session_Handoff_Vivado.md)（引継ぎ）と
   [`Design_Document.md`](Design_Document.md)（設計・レジスタマップ）を参照。
3. プロジェクトを再生成して再実装:
   ```tcl
   close_project -quiet
   source D:/FPGA/greta-oto/vivado/create_project.tcl
   ```
   → Run Synthesis → **Run Implementation** → §2.1 のタイミング確認。
4. 直近の実装ログは `docs/run_implementation.txt`、合成ログは `docs/run_synthesis.txt`。

---

## 0. 前提・事前確認

| 項目 | 内容 |
|---|---|
| ボード | Digilent Zybo Z7-20 (`xc7z020clg400-1`) |
| ツール | Vivado 2025.2 / Vitis 2025.2 (Unified IDE) |
| ボードファイル | `digilentinc.com:zybo-z7-20:part0:1.2` が必要 |

**最初にボードファイルを確認してください。** `vivado/create_project.tcl` の21行目が
`set_property board_part digilentinc.com:zybo-z7-20:part0:1.2` を要求します。識別子が一致しないと
`ERROR: [Board 49-71] The board_part ... cannot be found` でここで失敗します。

このボードファイルは **Vivado Store からインストール**できます
（[XilinxBoardStore: Digilent/zybo-z7-20/1.2](https://github.com/Xilinx/XilinxBoardStore/tree/2025.2/boards/Digilent/zybo-z7-20/1.2)）。
Vivado GUI の `Tools → Vivado Store → Boards` から `Zybo Z7-20` を追加してください。

インストール済みの識別子は Tcl Console / Tcl Shell で確認できます:

```tcl
get_board_parts *zybo-z7-20*
# → digilentinc.com:zybo-z7-20:part0:1.2  などが返ればOK
```

> **識別子のフォーマット:** `ベンダ:ボード名:part0:バージョン`。
> Vivado Store 版は `digilentinc.com:zybo-z7-20:part0:1.2`（旧 Digilent 配布の
> `zybo-z7:...:1.0` とは**ボード名・バージョンが異なる**）。`get_board_parts` の出力と
> `create_project.tcl` の指定が一致している必要があります。

> 注: ドキュメントは「Vivado 2019.1以降」を要件としています（`xpm_memory_sprom` 使用、Design_Document §9.2）。
> 2025.2 は問題なく上回ります。

---

## 1. Vivado プロジェクト生成

`create_project.tcl` を実行する方法は3通りあります。**どれを使っても結果は同じ**です
（GUI / Tcl Shell / Tcl Console はいずれも同一の Tcl インタプリタ）。お好みの方法を1つ選んでください。

### 方法A: PowerShell / コマンドプロンプトから（バッチ実行）

`vivado` が PATH にある前提（無ければ後述の方法B/Cを使用）:

```powershell
cd D:\FPGA\greta-oto\vivado
vivado -mode batch -source create_project.tcl
```

> `-mode batch` は GUI を開かずに実行して終了します。

### 方法B: Vivado 2025.2 Tcl Shell から

スタートメニューの「Vivado 2025.2 Tcl Shell」を起動すると `Vivado%` プロンプトが出ます。
ここはすでに Tcl インタプリタの中なので、`vivado ...` ではなく **Tcl コマンドを直接**入力します:

```tcl
source D:/FPGA/greta-oto/vivado/create_project.tcl
```

### 方法C: Vivado GUI の Tcl Console から

Vivado を起動し、画面下部の **Tcl Console** に入力します:

```tcl
source D:/FPGA/greta-oto/vivado/create_project.tcl
```

生成後、続けて GUI でプロジェクトを開く場合は `start_gui`（Tcl Shell の場合）。
GUI の Tcl Console から実行した場合は自動的に現在のセッションにプロジェクトが開きます。

> **Tcl のパス表記に注意（方法B/C共通）**
> Tcl ではバックスラッシュ `\` がエスケープ文字です。絶対パスは次のいずれかで指定してください:
> - スラッシュ: `source D:/FPGA/greta-oto/vivado/create_project.tcl`
> - 波括弧で囲む: `source {D:\FPGA\greta-oto\vivado\create_project.tcl}`
>
> `source D:\FPGA\...` のようにバックスラッシュを裸で書くと、`\v`（vivado の先頭）が
> 垂直タブと解釈されパスが破損します。`cd` は不要です（スクリプトが自身の場所を基準にパスを解決するため）。

### 共通事項

- `create_project.tcl` はリポジトリルート相対でパスを解決するため（`script_dir/..`）、
  ドキュメントの `docs` への移動の影響は受けません。
- 生成物: `vivado/gnss_zynq/gnss_zynq.xpr`
- スクリプトは RTL 一括追加 → `.coe` 追加 → XDC 追加 → `bd_gnss_zynq.tcl` でブロックデザイン生成
  → ラッパー生成 → top を `gnss_zynq_wrapper` に設定、まで自動実行します。
- 完了すると `Project created: .../gnss_zynq.xpr` と表示されれば成功です。

> **再生成（上書き）について**
> `create_project.tcl` は `create_project -force` を使用しているため、既存の `vivado/gnss_zynq/`
> プロジェクトがあっても**自動で削除して作り直します**。手動でのディレクトリ削除は不要です。
> ただし **GUI でそのプロジェクトを開いたままだとファイルがロックされて失敗**します。
> 再 `source` する前に `close_project`（または Vivado を閉じる）を実行してください:
> ```tcl
> close_project -quiet
> source D:/FPGA/greta-oto/vivado/create_project.tcl
> ```
> なお生成物 `vivado/gnss_zynq/` は中間ファイルのため、Git では `.gitignore` 対象にしておくのが無難です。

### `.mem` について

`.coe` はプロジェクトに登録されますが、ROM が `.mem`（`xpm_memory_sprom` の `MEMORY_INIT_FILE`）を
参照する場合は合成ディレクトリから見つからないことがあります。合成でエラーが出たら:

```powershell
copy D:\FPGA\greta-oto\BB_HW\rom_init\*.mem D:\FPGA\greta-oto\vivado\gnss_zynq\
```

---

## 2. 合成 → 実装 → ビットストリーム

GUI で開く:

```powershell
cd D:\FPGA\greta-oto\vivado
vivado gnss_zynq\gnss_zynq.xpr
```

1. **Run Synthesis**
2. **Run Implementation**
3. **Generate Bitstream**

### 確認ポイント

- `adc_clk`(12MHz) ↔ `clk_fpga_0`(100MHz) は `set_clock_groups -asynchronous` で除外済み（Design_Document §8.2）。
  CDC 警告は出てもエラーにはなりません。
- タイミング違反が出たら、レポート上の実クロック名が `clk_fpga_0` と一致しているか確認
  （不一致なら `constraints_zybo_z7.xdc` を実名に合わせる。Handoff トラブル3）。
- `gnss_top.v` の `wire ae_ram_en;` 未使用警告は無害（§9.1）。

### 2.1 実装後のタイミング確認手順（重要）

§7-10 の `CLOCK_DEDICATED_ROUTE FALSE` 緩和でクロックスキューが増えるため、**実装後に必ずタイミングを
確認**する。エクスポート（§3）に進む前のゲート。

**GUI で確認:**
1. 左パネル **Implementation → Open Implemented Design** を開く。
2. **Reports → Timing → Report Timing Summary**（または `Window → Timing Summary`）。
3. **Design Timing Summary** の以下が**すべて正（または "met"）**であること:
   - **WNS**（Worst Negative Slack, セットアップ）≥ 0
   - **WHS**（Worst Hold Slack, ホールド）≥ 0 ← 今回の緩和で特に注意
   - **TNS / THS**（合計負スラック）= 0
   - **WPWS**（Pulse Width Slack）≥ 0
4. 上部に **"All user specified timing constraints are met"** と出れば合格。

**Tcl で確認（コンソール）:**
```tcl
open_run impl_1
report_timing_summary -delay_type min_max -report_unconstrained \
    -max_paths 10 -file impl_timing_summary.rpt
# 主要数値だけ素早く見る:
puts "WNS=[get_property SLACK [get_timing_paths -setup -max_paths 1]]"
puts "WHS=[get_property SLACK [get_timing_paths -hold  -max_paths 1]]"
```

**判定と対処:**
- すべて met → §3 ハードウェアエクスポートへ進む。
- **Hold(WHS) 違反** → クロック緩和の副作用の可能性大。§7-10 の代替案（`BUFGCE→BUFHCE`、または
  クロックイネーブル方式）を検討。まず違反パスが gated clock 由来か `report_timing -hold` で確認。
- **Setup(WNS) 違反** → 100MHz に対する論理段数の問題。違反パスを `report_timing -setup` で特定。
- **adc_clk ↔ clk_fpga_0 のパスが違反** → §2 確認ポイントの通り `set_clock_groups` の async 除外が
  効いているか（クロック名一致）を確認（§7-8(c) / §8.2）。

---

## 3. ハードウェアエクスポート

`File → Export → Export Hardware` → **Include bitstream** を選択 → `gnss_zynq_wrapper.xsa` を出力。

---

## 4. Vitis 2025.2 でプラットフォーム＋アプリ作成

> 2025.2 は Unified Vitis IDE（Component ベース）です。旧 Classic とメニュー名が異なります。

### 4-1. Platform Component

- `File → New Component → Platform`
- XSA: ステップ3の `gnss_zynq_wrapper.xsa`
- OS: **FreeRTOS** / CPU: `ps7_cortexa9_0`
- ビルドすると BSP が `xparameters.h`（`XPAR_XSPIPS_0_DEVICE_ID`, `XPAR_SCUGIC_0_DEVICE_ID` 等）を生成。
- FreeRTOS ポートコードに `xInterruptController` が公開されているか確認（Design_Document §5.2 / Handoff トラブル5）。

> **TTC0 が必要（OSティックタイマー）**
> Vitis の Cortex-A9 FreeRTOS ポートは **TTC（Triple Timer Counter）を OS ティックタイマー**として使うため、
> ハードウェアで TTC0 が有効になっている必要があります（無効だと BSP の `xparameters.h` に
> `XPAR_XTTCPS_*` が出ず、ティックが動かない）。
> `bd_gnss_zynq.tcl` で `CONFIG.PCW_TTC0_PERIPHERAL_ENABLE {1}` を設定済みなので**手動操作は不要**。
> 外部ピンは不要なため EMIO routing（`PCW_TTC0_TTC0_IO`）は設定していない（内部利用のみ）。
> BD の PS7 で確認する場合は `IRQ & Reset` または `Application Processor Unit` の TTC0 にチェックが入っている。

### 4-2. Application Component

- `File → New Component → Application`、上記プラットフォームを選択、FreeRTOS テンプレート（または空）。
- ソースを追加:
  - `Firmware/Abstract/HWCtrl_Zynq.c`（Zynq HW 層）
  - `Firmware/Abstract/PlatformCtrl_FreeRTOS.c`（OS 抽象化）
  - `Firmware/Baseband/src/*.c`（ベースバンド処理本体）
  - `Firmware/PVT/`, `Firmware/common/` の必要ファイル
- インクルードパス追加: `Firmware/Abstract`, `Firmware/Baseband/inc`, `Firmware/common` 等。

### 動作確認のエントリポイント

- `Firmware/Baseband/src/FirmwarePortal.c` の `FirmwareInitialize()`
  （`AttachBasebandISR(InterruptService)` を呼ぶ）が処理の起点。
- ISR は同ファイル `InterruptService()`。
- FreeRTOS の `main()` で次の順に呼ぶ構成にします
  （`EnableRF` は初期化フェーズ、`AttachBasebandISR` はスケジューラ起動後、Handoff トラブル5-確認2）:
  1. `EnableRF()`
  2. FreeRTOS スケジューラ起動
  3. タスク内で `FirmwareInitialize()`

---

## 5. 実機動作確認（優先度順）

| # | 確認項目 | 方法 |
|---|---|---|
| 5-1 | **MAX2771 SPI** | `EnableRF()` 後に UART ログ確認。`docs/pocketgnss.c` のビットバン実装で read_reg 読み返し検証も有効 |
| 5-2 | **CLKOUT 12MHz** | JB Pin5 (Y7) をオシロ確認、または Vivado ILA で `adc_clk` 周期確認 |
| 5-3 | **I/Q データ** | JC に信号到達確認 → `GLB_BB_ENABLE` → `GLB_TRACKING_START` → `TE_FIFO_STATUS`(0x1404) でサンプル蓄積確認 |
| 5-4 | **捕捉エンジン** | GPS L1CA 捕捉 → `AE_STATUS`(0x100C) で完了フラグ確認 |

確認の足がかりとして、`Xil_In32(GNSS_BASE_ADDR + (GLB_BB_VERSION << 2))`（`0x43C00000 + 0x100`）で
バージョンレジスタ（§7.1）を読み、AXI 経路自体が生きているかをまず確認するのが確実です。

---

## 6. つまずきやすい点（要点）

1. **board_part 未インストール** → ステップ1で即失敗。最初に確認。
2. **MAX2771 SPI は 48ビット**（標準32ビットではない）。`HWCtrl_Zynq.c` は修正済みだが、流用コードに注意（§9.5）。
3. **PS SPI0 は EMIO 経由**（MIO はZyboで埋まっている、§9.3）。BD で JB に引き出し済み。
4. **FreeRTOS で GIC 再初期化禁止** — `XScuGic_CfgInitialize` / `Xil_ExceptionRegisterHandler` を呼ばない（§9.4）。
5. **GNSS_IRQ_ID = 61** が `IRQ_F2P[0]` と一致するか BD で確認（トラブル5）。

---

## 7. プロジェクト生成時に遭遇した問題と対処（解決済み）

初回の `source create_project.tcl` 実行ログ（`docs/tcl_console.txt`）で発生した問題と、スクリプト側の修正内容。
**現在のスクリプトには修正反映済み**だが、同種の症状が出たときの参考に残す。

### 7-1. `CRITICAL WARNING [HDL 9-3952] use of undefined macro ...`（多数）

- **症状:** `AE_CONTROL` / `GLB_BASE_ADDR` / `PPS_CTRL` など `address.v` 定義のマクロが
  `ae_top.v` / `gnss_top.v` / `te_fifo.v` / `pps.v` / `tracking_engine.v` で「undefined macro」と
  大量に報告され、続いて `[HDL 9-1206] Syntax error near ...` が出る。
- **原因:** コアRTLは個別に `` `include "address.v" `` していない。元設計は「`address.v` を先頭で
  1回コンパイルしマクロをグローバル共有」する流儀だが、Vivado は各ファイルを個別パースするため
  マクロが未定義になる（合成時に必ず失敗する）。
- **対処:** `create_project.tcl` で `address.v` を**グローバルインクルード**に設定:
  ```tcl
  set_property file_type        "Verilog Header" [get_files .../address.v]
  set_property is_global_include true             [get_files .../address.v]
  ```
  併せて、二重定義警告を避けるため `gnss_top_axi.v` 内の明示的 `` `include "address.v" `` は
  コメントアウト済み。

### 7-2. `ERROR [Common 17-107] Cannot change read-only property 'offset'`

- **症状:** `bd_gnss_zynq.tcl` のアドレス割り当てで停止。`set_property offset 0x43C00000 ...` がエラー。
- **原因:** 新しい Vivado ではアドレスセグメントの `offset` / `range` は読み取り専用プロパティで、
  `set_property` では変更できない。
  さらに経験則として、**デフォルトのレンジが大きいままだとオフセットを変更できない**
  （大きいレンジが衝突・アラインメント不整合を起こす）。`set_property` 方式では
  「先にレンジを小さくしてからオフセットを変更」する順序が必要だった。
- **対処:** `assign_bd_address` に `-offset` と `-range` を**同時に**渡す方式へ変更。両者がアトミックに
  設定されるため、過大なデフォルトレンジが先に居座ってオフセット変更を阻む問題自体を回避できる
  （`-force` で自動割り当て値を上書き）:
  ```tcl
  assign_bd_address -offset 0x43C00000 -range 64K \
      -target_address_space [get_bd_addr_spaces processing_system7_0/Data] \
      [get_bd_addr_segs gnss_top_axi_0/S_AXI/reg0] -force
  ```
  （本設計では `gnss_top_axi` が `C_S_AXI_ADDR_WIDTH=16` のためスレーブレンジは元々 64K だが、
  `-range 64K` を明示することでデフォルト値によらず確実に割り当たる。）

### 7-3. PS7 で SPI 0 しか有効にならない（Zybo プリセット未適用）

- **症状:** BD を開くと PS7 の Peripherals が **SPI 0 しかアクティブでない**。
  ログに `INFO [Ipptcl 7-1463] No Compatible Board Interface found. Board Tab not created` が出る。
- **原因:** `bd_gnss_zynq.tcl` の `apply_bd_automation` が `FIXED_IO`/`DDR` の外部化のみで、
  **ボードプリセットを適用していなかった**。そのため PS7 は IP デフォルト（ほぼ全ペリフェラル無効）
  のままで、明示設定した SPI0/GP0/IRQ_F2P だけが乗る。ボードファイルを読み込んでいても、
  プリセットは自動では適用されない。
- **影響:** 見た目だけの問題ではない。**DDR3 がデフォルト設定のままだと ARM が DDR から動作できず、
  FreeRTOS アプリが起動しない**。UART も無効のままだと §5 の UART ログ確認ができない。
- **対処:** `apply_bd_automation` の `-config` に `apply_board_preset "1"` を追加。Zybo Z7-20 の
  プリセット（DDR3 タイミング＋標準 MIO ペリフェラル: UART1 / SD0 / ENET0 / USB0 / QSPI …）が
  読み込まれる。その後に `set_property -dict`（FCLK 100MHz / GP0 / EMIO SPI0 / IRQ_F2P）が上書き適用される:
  ```tcl
  apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 \
      -config { apply_board_preset "1" make_external "FIXED_IO, DDR" \
                Master "Disable" Slave "Disable" } \
      [get_bd_cells processing_system7_0]
  ```

### 7-4. `When using EMIO pins for SPI_0 tie SSIN High`（SPI0 SSIN 未固定）

- **症状:** `validate_bd_design` で
  `WARNING [#UNDEF] When using EMIO pins for SPI_0 tie SSIN High in the PL bitstream`。
- **原因:** EMIO で SPI0 をマスター動作させる場合、`SPI0_SS_I`（SSIN, スレーブセレクト入力）を
  PL 内で High に固定しないと、コントローラが mode-fault を検出して SS/SCLK の駆動を止めることがある。
- **影響:** MAX2771 へのレジスタ書き込みが不安定／無反応になり得る。
- **対処:** `bd_gnss_zynq.tcl` で `xlconstant`(=1) を `SPI0_SS_I` に接続して High 固定:
  ```tcl
  set_property -dict [list CONFIG.CONST_VAL {1} CONFIG.CONST_WIDTH {1}] \
      [create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_ssin]
  connect_bd_net [get_bd_pins xlconstant_ssin/dout] \
      [get_bd_pins processing_system7_0/SPI0_SS_I]
  ```
  （MISO は read 不要のため別の `xlconstant`(=0) で `SPI0_MISO_I` に固定済み。）
- **重要 — この警告は Tie-High 後も消えない:** `[#UNDEF] ... tie SSIN High` は PS7 IP が
  「SPI0 を EMIO にしたら**無条件に**」出すリマインダーで、実際の PL 配線を検査していない。
  上記で `SPI0_SS_I` を High 固定すれば**機能要件は満たされており、警告が残っても無視してよい**。
  結線確認は: `get_bd_nets -of_objects [get_bd_pins processing_system7_0/SPI0_SS_I]`。

### 7-5. DDR DQS_TO_CLK_DELAY が負値（Zybo プリセットの既知 CRITICAL WARNING）

- **症状:** `validate_bd_design` で
  `CRITICAL WARNING [PSU-1..4] PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_x has negative value ...`。
- **原因:** Zybo Z7-20 のボードプリセットが持つ DDR3 スキュー較正値がもともと負値（Digilent 公式設定どおり）。
- **対処不要:** 実機で正常動作する既知警告。プリセットを使う限り消せない。**無視してよい。**

### 7-6. 無害な警告（対処不要）

- `WARNING [IP_Flow 19-587] ... 'host_cs'/'host_rd' has a dependency on ... 'RD_EXEC'`
  — `gnss_top_axi.v` のローカルパラメータ依存に関する情報的警告。動作に影響なし。
- `WARNING [IP_Flow 19-11770] Clock interface 'adc_clk'/'S_AXI_ACLK' has no FREQ_HZ parameter`
  — BD のクロック周波数メタデータ未設定の警告。`adc_clk` は外部 12MHz、`S_AXI_ACLK` は
  PS の 100MHz が実体なので機能上問題なし（気になる場合は BD で FREQ_HZ を明示可）。

> 注: `INFO [Ipptcl 7-1463] No Compatible Board Interface found` は §7-3 のプリセット適用前に出ていた
> もの。`apply_board_preset "1"` 適用後は解消する。

### 7-7. AXI Interconnect → SmartConnect へ置き換え（Discontinued 対応）

- **背景:** BD で `AXI Interconnect`(v2.1) と `Constant`(xlconstant) が **Discontinued** 表示になる。
- **AXI Interconnect:** 単なる非推奨ではなく**将来の Vivado リリースで削除予定**。AMD 推奨の
  **AXI SmartConnect** に置き換え済み（`bd_gnss_zynq.tcl`、セル名 `axi_smc_0`）。SmartConnect は
  PS7 GP0 の AXI3 マスターと AXI4-Lite スレーブの両方をサポート。単一クロックドメインのため
  `aclk` / `aresetn`（Active-Low）の2本だけ接続し、旧 Interconnect の per-port
  `S00_ACLK`/`M00_ACLK`/`*_ARESETN` 結線は不要になった。
- **Constant (xlconstant):** Discontinued 表示は出るが**引き続き利用可能・サポート対象**で削除予定の
  告知はない。SSIN/MISO の Tie-off 用途はこのまま `xlconstant` を使用（置き換え不要）。AMD は
  サンプルでインライン HDL 定数へ移行しつつあるが、BD のドロップイン代替IPではなく RTL 直書きの方式。

### 7-8. 合成（Run Synthesis）の CRITICAL WARNING 3種

初回 Run Synthesis で出た3種。いずれもスクリプト／RTL 側で対処済み。

**(a) モジュール二重定義（合成 `[Synth 8-9873]` / プロジェクト `[HDL 9-3756]` `[filemgmt 20-1318]`）**
- **原因:** 汎用版 `backend_wrapper/mem_wrapper.v`・`clock_gating.v` と、Xilinx版
  `xilinx/xilinx_rom_wrapper.v`・`xilinx_clk_gate.v` が**同名モジュールを二重定義**。両ディレクトリを
  glob していたため。compile 順で Xilinx 版が `(Active)` になるが非決定的。
- **`ifdef` ガードでは不十分:** 当初 `` `ifndef XILINX_BACKEND `` ＋ `verilog_define XILINX_BACKEND` で
  対処したが、これは**合成**にしか効かない。プロジェクトのソース階層解析（`filemgmt`）は `ifdef` を
  評価せずモジュール宣言を構文的に拾うため、`[HDL 9-3756]` / `[filemgmt 20-1318]` が**プロジェクト
  作成時点で**残ってしまう。
- **対処（最終）— 物理的に重複を排除:** 汎用版をプロジェクトに含めない構成へ変更（ファイル自体は
  ディスク上に温存）。
  - `clock_gating.v`: Vivado プロジェクトに追加しない（`xilinx_clk_gate.v` が `gated_clock_wrapper` を提供）。
  - `mem_wrapper.v`: 3つの汎用 ROM ラッパーを別ファイル `backend_wrapper/rom_wrapper.v` へ**移設**し、
    `mem_wrapper.v` には RAM ラッパーのみ残す。`rom_wrapper.v` はプロジェクトに追加しない
    （`xilinx_rom_wrapper.v` が xpm 版を提供）。
  - `create_project.tcl`: `backend_wrapper` を一括 glob から外し、`mem_wrapper.v` だけ明示 `add_files`。
    `verilog_define XILINX_BACKEND` は不要になり削除。
  - これで合成・プロジェクト階層の双方で重複が消える。汎用版（`clock_gating.v` / `rom_wrapper.v`）は
    非Xilinx／RTLシミュレーション用にディスク上へ残置。

**(b) `[Synth 8-4445] could not open $readmem data file '*.mem'`**
- **原因:** `create_project.tcl` が `.coe` のみ追加し、**`.mem` を未追加**だった。xpm_memory_sprom の
  `MEMORY_INIT_FILE` は `.mem`（相対名）を参照するため、探索パスに無く読めなかった。
- **対処:** `create_project.tcl` に `b1c_legendre.mem` / `l1c_legendre.mem` / `memory_code.mem` を
  `add_files` で追加し、`rom_init/` を探索パスに乗せた。なお GPS L1CA は C/A コード（LFSR生成）を使い
  これらの ROM（L1C/B1C/Galileo/BDS用）に依存しないため、L1CA 確認だけなら未ロードでも基本動作は可能。
- **補足:** それでも解決しない場合は §1 の通り `.mem` を `vivado/gnss_zynq/` へコピー、または
  `xilinx_rom_wrapper.v` の `MEMORY_INIT_FILE` を絶対パスにする。

**(c) `[Vivado 12-4739] set_clock_groups: No valid object(s) found for ... clk_fpga_0`**
- **原因:** 合成時は PS7 がブラックボックスで `clk_fpga_0`(FCLK_CLK0) が未定義。`get_clocks` が空を返し
  `set_clock_groups` が空振り。実装時には存在するので、合成時のみの警告。
- **対処（最終）:** この async clock-group 制約を**実装専用の別ファイル
  `constraints_zybo_z7_impl.xdc`** に分離し、`create_project.tcl` で `used_in_synthesis false` を設定。
  合成では処理されず、`clk_fpga_0` が存在する実装時のみ適用される。
  - **※ Tcl の `if` ガードは XDC では使えない**（`[Designutils 20-1307] Command 'if' is not
    supported in the xdc constraint file`）。当初 `if {[llength ...]}` でガードしたがこのエラーになり、
    最終的にファイル分離方式へ変更した。
  - `adc_clk` の `create_clock` は本体 `constraints_zybo_z7.xdc`（合成＋実装）側に残す。
- **要確認:** 実装後のタイミングレポートで `clk_fpga_0` が実際に存在し、async 除外が効いているか確認
  （クロック名が異なる場合は impl 用 XDC の名前を実名に合わせる。§2 確認ポイント / Handoff トラブル3）。

> 修正後は `close_project -quiet` してから再度 `source create_project.tcl` を実行する（§1 の再生成メモ参照）。

### 7-9. Run Synthesis の警告（合格・分類）

合成は **警告のみで完走**。内容を3分類で整理。

**(1) 移植ファイル側 — 修正済み**
- `[Synth 8-6901] identifier 'host_d4rd' is used before its declaration`（`gnss_top_axi.v`）
  → `host_d4rd` の `wire [31:0]` 宣言を read FSM の前へ移動。機能影響なし（元々32bitで正しく解決
  されていたが、宣言順を直し警告解消）。
- `[Netlist 29-345] SIM_DEVICE on BUFGCE ... 'ULTRASCALE' ... changed to '7SERIES'`（`xilinx_clk_gate.v`）
  → BUFGCE に `#(.SIM_DEVICE("7SERIES"))` を明示。合成は元々自動補正されていたが、機能シミュレーションを
  ハードと一致させるため明示。

**(2) 意図した設計／IP内部 — 対処不要**
- `port 'pps_irq' ... unconnected` / `gnss_top_axi_0 ... 30 connections declared, but only 29 given`
  → `pps_irq`（PPS割り込み）は未接続のままにしている。`irq`→`IRQ_F2P[0]` のみ使用。PPS割り込みを
  使うなら将来 `IRQ_F2P[1]` 等へ接続（Design_Document §4.2）。現状は無害。
- `Port spi0_ss_o[2] ... unconnected or has no load` → `spi0_ss_o[2:0]` のうち `[0]=CS_A` のみ使用
  （Design_Document §9.6）。想定どおり。
- `xpm_memory_sprom ... 'injectsbiterra' unconnected` / `11 connections declared, but only 7 given`
  → xpm のオプション ECC ポート未接続。xpm の通常動作。
- SmartConnect 内部（`psr_aclk ... mb_reset unconnected`、`aresetn_out ... no driver`、
  `FDRE_inst is unused and will be removed`）→ 自動生成IPの内部。無害。
- `Auto Incremental Compile: No reference checkpoint`（初回実行）、
  `Unused sequential element sample_in_r2_reg was removed`（最適化）→ 情報。

**(3) コア RTL の既存警告（今回の移植由来ではない・要監視）**
これらは上流 greta-oto のコア RTL（未変更）に元からあるもの。参照設計で動作実績がある前提では
基本的に無害だが、実機検証時に挙動を確認する。
- `[Synth 8-3848] Net te_rd_buffer ... does not have driver`（`tracking_engine.v:37`）
  → 駆動なしネット。TE バッファ読み出し（§5-3 / レジスタ 0x2000〜）の挙動を実機で確認。
- `[Synth 8-327] inferring latch for 'FSM_sequential_next_state_reg'`（`coherent_sum.v:60`）
  → FSM 次状態ロジックのラッチ推論。コヒーレント積算の動作を実機/シミュレーションで確認。
- `[Synth 8-7137] te_noise_config_reg has both Set and reset with same priority`
  （`tracking_engine.v:102`）→ シミュレーション不一致の可能性の注意喚起。HW では通常無害。

> いずれもエラーなし。(1) を修正のうえ再合成し、Run Implementation → Generate Bitstream に進んで問題ない。

### 7-10. Run Implementation の配置エラー（BUFG→BUFGCE カスケード）

- **症状:** `[Place 30-120] ... rule_cascaded_bufg ... FAILED`（`Cascaded bufg (bufg->bufg) must be
  adjacent and cyclic`）→ `[Place 30-99] IO Clock Placer failed` → `[Common 17-69] Placer could not
  place all instances` で実装が停止。
- **原因:** PS7 `FCLK_CLK0` が **BUFG** でバッファされ、その出力が ae_top / tracking_engine の
  **5つの BUFGCE**（クロックゲーティング `xilinx_clk_gate.v`）入力に入る → **BUFG→BUFGCE カスケード**。
  7-series は「カスケードした BUFG は隣接配置必須」で、1つの FCLK BUFG に 5つの BUFGCE を隣接させられず失敗。
  ASIC 流クロックゲーティングを FPGA に移植する際の典型的摩擦。
- **対処:** `constraints_zybo_z7_impl.xdc` に下記を追加し、BUFGCE のクロック入力を専用ルート要件から
  外して一般ルーティングを許可（実装専用。ネット名は placer メッセージが提示する階層名）:
  ```tcl
  set_property CLOCK_DEDICATED_ROUTE FALSE \
      [get_nets gnss_zynq_i/processing_system7_0/inst/FCLK_CLK0]
  ```
  全ゲーテッドクロックは同一 100MHz FCLK 由来のイネーブル的用途のため、この緩和は妥当。
- **要確認:** 緩和は一般ルーティングを使うためクロックスキューが増える。**実装後のタイミングレポートで
  WNS/WHS（特に保持時間）が満たされているか必ず確認**すること。
- **代替案（タイミング/スキューが問題化した場合）:**
  - `BUFGCE` を **`BUFHCE`（リージョナルクロックバッファ＋CE）** に変更（BUFG→BUFH は正規トポロジで
    カスケード扱いにならない）。ただしゲーテッド論理が単一クロックリージョンに収まる必要あり。
  - クロックゲーティングを廃し**クロックイネーブル（CE）方式**へ書き換え（FPGA 本来の手法。コア RTL の
    広範な改修が必要）。

---

## 参照ドキュメント

| ファイル | 内容 |
|---|---|
| [`Session_Handoff_Vivado.md`](Session_Handoff_Vivado.md) | セッション引継ぎ・トラブル対処集 |
| [`Design_Document.md`](Design_Document.md) | 受信機の完全な設計書（レジスタマップ・タイミング制約含む） |
| [`Zybo_MAX2771_PMOD.md`](Zybo_MAX2771_PMOD.md) | PMODボードのピン接続・Q1〜Q5の回答 |
| [`pocketgnss.c`](pocketgnss.c) | MAX2771レジスタ動作確認用ビットバンSPIサンプルコード |
| [`Zybo-Z7-Master.xdc`](Zybo-Z7-Master.xdc) | Digilent公式マスターXDC |
