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
| §2 Run Implementation | ✅ 完走（エラー解消。§7-10→§7-12 のクロック修正済み） |
| §2.1 タイミング確認 | ✅ **クローズ**（最良 WNS=+0.022 / WHS=+0.030）。マージン薄く run 間で僅かに負になり得る（§7-12, §3-3） |
| §3 ビットストリーム→エクスポート | ⏳ **次回再開ポイント**。軽微違反は許容して `.xsa` 出力（§3-3 の判断）|
| §4 Vitis プラットフォーム＋アプリ | 未 |
| §5 実機動作確認 | 未 |

### 次回セッションの再開手順
1. **このファイル（`Build_Run_Vivado_Vitis_2025.2.md`）を読み込む** ← 最新の作業状態・全トラブル対処が
   ここに集約されている。まずこれ。
2. 補足が必要なら [`Session_Handoff_Vivado.md`](Session_Handoff_Vivado.md)（引継ぎ）と
   [`Design_Document.md`](Design_Document.md)（設計・レジスタマップ）を参照。
3. **タイミングはクローズ済み**（最良 WNS=+0.022 / WHS=+0.030, 構成は §7-12）。ただしマージンが薄く
   build によって WNS が僅かに負（例 -0.030ns / 2 endpoints）になり得る。**この軽微違反は許容して
   §3 ビットストリーム生成 → `.xsa` エクスポートへ進む判断**（根拠と手順は §3-3）。次は §4 Vitis。
   レポートの所在は §2.2。
4. プロジェクトを再生成して再実装する場合:
   ```tcl
   close_project -quiet
   source D:/FPGA/greta-oto/vivado/create_project.tcl
   ```
   → Run Synthesis → **Run Implementation** → §2.1 のタイミング確認。
5. 直近の実装ログは `docs/run_implementation.txt`、合成ログは `docs/run_synthesis.txt`。
   実装が生成する各種レポート（`.rpt`）の所在は §2.1 を参照。

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
  → **今回これに該当（WNS = -1.718ns）。原因分析と対策は §7-11。**
- **adc_clk ↔ clk_fpga_0 のパスが違反** → §2 確認ポイントの通り `set_clock_groups` の async 除外が
  効いているか（クロック名一致）を確認（§7-8(c) / §8.2）。

### 2.2 タイミングレポートの保存場所（どのファイルを見るか）

**Run Implementation を実行すると、Vivado が以下のレポートを自動でディスクに書き出す。**
GUI で開かなくても、これらをテキストで直接参照できる（手動で `report_*` を叩き直す必要はない）。

保存先ディレクトリ:
```
vivado/gnss_zynq/gnss_zynq.runs/impl_1/
```

| ファイル | 内容 | 用途 |
|---|---|---|
| `gnss_zynq_wrapper_timing_summary_routed.rpt` | **タイミングサマリ（最重要）** | WNS/TNS/WHS/THS、クロック別集計、ワースト違反パスの詳細（Timing Details）まで全部入り |
| `gnss_zynq_wrapper_route_status.rpt` | ルーティング状況 | 未配線・コンフリクトの有無 |
| `gnss_zynq_wrapper_methodology_drc_routed.rpt` | メソドロジ DRC | タイミング例外・CDC の妥当性警告 |
| `gnss_zynq_wrapper_drc_routed.rpt` | 配線後 DRC | 一般 DRC |
| `gnss_zynq_wrapper_utilization_placed.rpt` | リソース使用率 | LUT/FF/BRAM/DSP 使用量 |
| `gnss_zynq_wrapper_clock_utilization_routed.rpt` | クロックリソース | BUFG/MMCM 等の使用状況（ゲーテッドクロック確認に有用） |
| `gnss_zynq_wrapper_power_routed.rpt` | 消費電力 | 参考 |
| `gnss_zynq_wrapper_bus_skew_routed.rpt` | バススキュー | — |
| `runme.log` | **実装の全コンソール出力** | `CRITICAL WARNING:` で grep すると Critical Warning を全部抽出できる |

- **Critical Warning の確認:** `runme.log` 内を `CRITICAL WARNING` で検索する。今回はタイミング未達に伴う
  `[Timing 38-282]` が記録されている（タイミング違反の通知であり、独立した別問題ではない）。
- **違反パスの内訳:** `..._timing_summary_routed.rpt` の「Timing Details」セクション（`Slack (VIOLATED)`
  で検索）に、始点・終点・論理段数・データ遅延（logic/route比）・クロックスキューまで載っている。
  これを読めば原因切り分けができる（§7-11 はこの読み解き結果）。
- 合成側の同種レポートは `gnss_zynq.runs/synth_1/`（`*_utilization_synth.rpt` 等）にある。

**追加でワーストパスを詳細出力したい場合**（実装済みデザインを開いた状態で Tcl Console）:
```tcl
open_run impl_1
report_timing -setup -max_paths 50 -nworst 1 -sort_by slack -input_pins \
    -file D:/FPGA/greta-oto/docs/timing_setup_worst.txt
```

---

## 3. ビットストリーム生成 → ハードウェアエクスポート

### 3-1. Generate Bitstream

実装（§2, タイミング達成 or 軽微違反）の後にビットストリームを生成する。

GUI: 左パネル **Generate Bitstream**（合成・実装が未完なら自動で先行実行）。

Tcl:
```tcl
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1
```

### 3-2. Export Hardware（bitstream 込み）

GUI: `File → Export → Export Hardware` → **Include bitstream** を選択 → `gnss_zynq_wrapper.xsa` を出力。

Tcl:
```tcl
write_hw_platform -fixed -include_bit -force \
    D:/FPGA/greta-oto/vivado/gnss_zynq_wrapper.xsa
```

> XSA はプラットフォーム定義（アドレスマップ・IP・BSP 生成情報）。`-include_bit` で PL を
> コンフィグするビットストリームを同梱する（実機ブリングアップ／ソフト開発に必要）。

### 3-3. タイミング軽微違反のまま進める判断（ブリングアップ時）

**`write_bitstream` はタイミング違反では止まらない**（`CRITICAL WARNING [Route 35-39]` / DRC を出すが
**ビットストリームは生成される**）。`[Route 35-39]` は route_design 段階で出るもので、タイミング達成 run
でも一旦出る（その後 post-route phys_opt がクローズする）。**最終判定は §2.2 の
`..._timing_summary_routed.rpt` の "All user specified timing constraints are met" / WNS 値で見る。**

§7-12 のとおり WNS マージンは薄く（+0.02ns 級）、配置の run 間ばらつきで **WNS が僅かに負**
（例: WNS=-0.030ns / 失敗 2 endpoints）になる build があり得る。この程度の軽微違反は
**ブリングアップ・ソフト開発の段階では許容してそのまま XSA 生成・実機投入してよい**。理由:

- **−0.030ns = 周期 10ns の 0.3%**、かつ **slow コーナー（最悪 PVT）** の値。実機の常温・標準シリコンでは
  余裕があり顕在化しにくい。
- 典型的に落ちるのは **AE コアのピーク検出**（`u_noncoh_acc` の `max_amp_r`/`freq_index_r`、論理段数13・
  CARRY4 チェーン）。捕捉は統計的・反復的で、1 回のピーク値が僅かにずれても**次の試行で回復**する
  **リトライ耐性のある内部データパス**。制御線・インタフェースではない。
- GPS L1CA 捕捉（最初のマイルストーン）はこのパスを強くは叩かない。SPI/AXI/FIFO/I-Q 系とは無関係。

> **後回しの TODO（最終運用前）:** 温度・電圧の厳しい環境での最終運用前には、この AE ピーク検出パスを
> **パイプライン化**（`u_noncoh_acc` の `max_amp`/`freq_index` 比較を 2 サイクルに分割し論理段数を半減）して
> 確実なマージンを確保すること。AE 捕捉ロジックのレイテンシが 1 サイクル変わるため機能検証が必要。
> 実機で AE 捕捉が不安定なら、まずこのパスを疑う（可能性は低い）。
>
> **毎ビルドで WNS を確認:** マージンが薄いので、再実装のたびに §2.1 / §2.2 で WNS を確認する。
> 僅かに負なら、同設定で再実行（配置乱数で正に振れることがある）か、§7-12 の実装レシピ強化／上記
> パイプライン化を検討。

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

> **【後日更新】** ここで入れた `CLOCK_DEDICATED_ROUTE FALSE`（全ゲーテッドクロックの一般配線化）は
> タイミングスキューを悪化させ WNS 違反の主因になった。correlator は **BUFHCE** へ移行して根治し、
> この緩和は AE コアの BUFGCE 1個のためだけに縮小した。詳細は **§7-12**。以下は当時の記録。

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

### 7-11. Run Implementation 後のタイミング違反（Setup, WNS = -1.718ns）— 原因分析

§7-10 の対処で実装はエラーなく完走したが、**配線後タイミングが未達**。
参照レポート: `gnss_zynq.runs/impl_1/gnss_zynq_wrapper_timing_summary_routed.rpt`（保存場所は §2.2）。

**全体サマリ:**

| 指標 | 値 | 判定 |
|---|---|---|
| WNS（Setup） | **-1.718 ns** | ✗ 違反 |
| TNS | -1663.941 ns（3084 / 54261 endpoints 違反） | ✗ |
| WHS（Hold） | +0.037 ns | ✓ |
| THS / WPWS | 0 / +3.750 ns | ✓ |

- 違反は **`clk_fpga_0`（100MHz, 周期10ns）の intra-clock setup に集中**。`adc_clk`(12MHz) 関連や
  inter-clock の違反は無し（§8.2 の async 除外は効いている）。
- **Hold・パルス幅は満足**。§2.1 で警戒した「緩和でスキュー増」の副作用は、Hold ではなく **Setup 側**に出た。

**根本原因は2つが複合している。**

**① ゲーテッドクロックのスキュー（最大要因）— 約 −3 ns**
- ほぼ全ての違反パスで `Clock Path Skew ≈ -2.95 〜 -3.06ns`。違反量(-1.7ns)より大きく、**スキューだけで
  setup 予算を食い潰している**。
- ソースレジスタは `BUFG → BUFGCE`（§7-10 の `gated_clock_u1..u4` カスケード）経由で
  **クロック挿入遅延 ≈ 5.92ns**。一方、行き先（共有 ROM 等、非ゲートの BUFG 直結）は **≈ 2.85ns**。
  差 ≈ 3ns がそのまま負スキューとして setup を削る。
- §7-10 で入れた `CLOCK_DEDICATED_ROUTE FALSE` がゲーテッドクロックを一般ルーティングに乗せ、挿入遅延を
  増大させた結果。**ここを直すのが最も効く。**

**② L1C/B1C Weil-Legendre 符号生成の長い経路（routing 律速）**
- ワースト上位8本がこのパターン:
  - 始点: `correlator_gen[3].../u_prn_code/weil_prn1(2)/code_index*_init_reg`
  - 終点: `l1c_legendre_data` / `b1c_legendre_data` の**共有 Legendre ROM アドレス** (`ADDRARDADDR`)
  - 論理段数 7〜8（LUT6 中心）、**データ遅延の 78〜86% が配線遅延**。
- 経路が `correlator_gen[3]→[1]→[0]` と**複数の相関器チャネルを物理的に横断**している。Legendre ROM を
  全チャネルで共有しているため、全チャネルからアドレス生成ロジックがファンインし、配線が長距離化する。
- 副次クラスタ: `correlator_gen[3]/u_overflow_gen/jump_count_o_reg → u_find_channel/logic_channel_mask2`
  （7段, スキュー -3.06ns）。

**重要な含意（実機ブリングアップとの関係）:**
- 違反パスは **L1C/B1C/Beidou の Weil/Legendre 符号生成**に集中している。§7-9(2) のとおり
  **GPS L1CA は C/A コード（LFSR 生成）を使い、これらの Legendre ROM に依存しない**。
- したがって **L1CA だけの初期動作確認では違反パスが機能的に励起されない可能性が高い**（＝ §5 のブリング
  アップは進められる見込み）。ただしこれは**保証ではなく要検証事項**。タイミング未達のまま実機投入する場合は
  L1CA 経路に違反が無いことを `report_timing -setup -through/-to` 等で個別確認すること。

**対策（効果が大きい順）:**
1. **ゲーテッドクロックのスキュー解消（最優先・最大効果）** — クロックゲーティングを
   **クロックイネーブル(CE)方式**へ書き換える、または BUFGCE をソース BUFG に隣接配置して
   `CLOCK_DEDICATED_ROUTE FALSE` を外す。源と行き先が同一クロック挿入を共有すれば ~3ns のスキューが消え、
   単独で違反を解消できる可能性がある（§7-10 代替案と同じ方向）。
2. **Legendre ROM アドレス経路の段数削減** — パイプラインレジスタ挿入、または **ROM をチャネル毎に複製**して
   チャネル横断配線を局所化（BRAM 使用量とのトレードオフ。§2.2 の utilization レポートで余裕を確認）。
3. **マルチサイクル制約** — 符号生成ロジックが実際には毎クロック更新を要しないなら `set_multicycle_path` で
   緩和（要アーキ確認だが効果大。impl 用 XDC に追加）。
4. **実装戦略の強化** — Run 設定で `Performance_ExplorePostRoutePhysOpt` 等のタイミング駆動戦略を選び、
   `phys_opt_design`（place 後・route 後）を有効化、placer/router の effort を上げる。まず①②の構造改善を
   入れてから併用するのが順当。

> まず①（クロック構造）を直して再実装し、WNS の改善幅を見るのが定石。①で大半が解消する見込み。

> **【解決済み】** 上記①②の方針で対処し、タイミングはクローズした（WNS=+0.022 / WHS=+0.030,
> 全制約 met）。実施内容・実測値は **§7-12** を参照。

### 7-12. タイミングクロージャ（WNS 違反の解消・実施記録）

§7-11 の違反（WNS=-1.718ns）を、**実装ディレクティブ強化**と**クロック構造修正**の二段で解消した。

**段階別の WNS 実測（clk_fpga_0 / 100MHz）:**

| # | 構成 | WNS | WHS | 判定 |
|---|---|---|---|---|
| 0 | ベースライン（既定実装） | -1.718 | +0.037 | ✗ |
| 1 | ＋実装ディレクティブ強化のみ（RTL 無改変） | -0.503 | +0.044 | ✗ |
| 2 | 全ゲーテッドクロックを BUFHCE 化 | — | — | ✗ 配置エラー（下記） |
| 3 | **ハイブリッド（correlator=BUFHCE / AE=BUFGCE）＋ディレクティブ** | **+0.022** | **+0.030** | ✅ met |

**(1) 実装ディレクティブ強化（RTL 無改変, -1.718 → -0.503）**
既定フローは `place(既定) → phys_opt(9秒) → route(既定)`、**post-route phys_opt 無し**だった。
データ遅延の 78% が配線でリソースに余裕（LUT40%/BRAM45%）＝配置改善余地大と判断し、以下を採用:
- `place_design -directive ExtraTimingOpt`
- `phys_opt_design -directive AggressiveExplore`（post-place）
- `route_design -directive Explore`
- **`phys_opt_design`（post-route）** ← 既定では無効。これが効いて -1.32 → -0.503 まで改善。
- → `create_project.tcl` に impl_1 の STEPS ディレクティブとして**埋め込み済み**（再生成で自動適用）。
  ただし phys_opt は WNS が -0.5ns より負だと「改善不能」(`[Physopt 32-745]`)と打ち切る。残りは構造修正が必須。

**(2) クロック構造修正（-0.503 → +0.022）— スキューの根治**
§7-11① の -3ns スキューの正体は、§7-10 の `CLOCK_DEDICATED_ROUTE FALSE` がゲーテッドクロックを
**一般配線**に押し出し、BUFG→BUFGCE 間に ~2.8ns の配線遅延を乗せていたこと（バッファ自体ではなく経路）。
- **対処:** `xilinx_clk_gate.v` の `gated_clock_wrapper` を `BUFGCE` → **`BUFHCE`** に変更
  （`parameter USE_BUFH`。CE_TYPE="SYNC" でグリッチフリーのゲーティングは維持）。
  BUFG→BUFHCE は専用クロックスパインを使う**正規トポロジ**（カスケード扱いにならない）ため、
  `CLOCK_DEDICATED_ROUTE FALSE` 不要で配線 penalty が消え、スキューが -2.955ns → **-0.059ns** に激減。
- **ただし全数 BUFHCE 化は失敗（試行2）:** BUFHCE は**1クロックリージョン**しか駆動できない。
  **AE コア**(`u_ae_top/u_ae_core/u_gated_clock`)のゲート域は大きく 1 リージョンに収まらず
  `[Place 30-487]`（730 スライス要求に対し 495 しか空き無し）で配置失敗。
- **最終構成＝ハイブリッド（試行3, 達成）:**
  - **correlator 4個**（`tracking_engine` の `gated_clock_u1..u4`）= **BUFHCE**。
    WNS クリティカルな Weil/Legendre パスはここ。小さく各リージョン(BUFHCE_X0Y0..3)に収まる。
  - **AE コア 1個**（`ae_core`）= **BUFGCE**（`gated_clock_wrapper #(.USE_BUFH(0))`）。
    大きいので従来どおり。**AE は setup クリティカルでない**ためスキューが乗っても無害。
    単一 BUFGCE は FCLK BUFG(X0Y16)に隣接(X0Y17)配置でき、`CLOCK_DEDICATED_ROUTE FALSE` は
    **この AE の BUFGCE のためだけに残す**（`constraints_zybo_z7_impl.xdc`）。

**最終結果（§2.2 の `impl_1/..._timing_summary_routed.rpt`）:**
- **All user specified timing constraints are met.** WNS=+0.022 / TNS=0 / Setup 失敗 0/54261
  （修正前 3084）, WHS=+0.030 / Hold 失敗 0, WPWS=+3.750。
- 新ワーストパスは AE コア `u_noncoh_acc` 内の**論理律速**パス（データ遅延9.56ns/論理45%, スキュー-0.059ns）。
  スキュー律速から健全な論理律速に移行した。

**注意 — マージンは薄い（最良 WNS=+0.022ns）:**
- met だが余裕は僅少。**配置の run 間ばらつきで負に振れ得る**（別マシンでの再実装で
  **WNS=-0.030ns / 失敗 2 endpoints**（AE `u_noncoh_acc` のピーク検出, 論理段数13/CARRY4）を実観測）。
- さらに詰めるなら §7-11② の段数削減（AE 非コヒーレント積算/`max_amp`・`freq_index` 比較ロジックの
  パイプライン化）や Legendre ROM 複製が候補。
- **この程度の軽微違反はブリングアップ段階では許容して `.xsa` 生成・実機投入してよい**（判断根拠・
  ビットストリーム生成手順は **§3-3**）。機能確認（§5）は L1CA 中心で進められるが、再実装で WNS を
  毎回確認すること（§2.1 / §2.2）。

**関連ファイル（この修正で変更したもの）:**
- `BB_HW/rtl/xilinx/xilinx_clk_gate.v` … `USE_BUFH` パラメータ化（BUFHCE/BUFGCE 切替）
- `BB_HW/rtl/acquire_engine/ae_core.v` … AE の `gated_clock_wrapper` に `#(.USE_BUFH(0))`
- `BB_HW/rtl/xilinx/constraints_zybo_z7_impl.xdc` … `CLOCK_DEDICATED_ROUTE FALSE` は AE 用に限定
- `vivado/create_project.tcl` … impl_1 の STEPS ディレクティブを埋め込み

---

## 参照ドキュメント

| ファイル | 内容 |
|---|---|
| [`Session_Handoff_Vivado.md`](Session_Handoff_Vivado.md) | セッション引継ぎ・トラブル対処集 |
| [`Design_Document.md`](Design_Document.md) | 受信機の完全な設計書（レジスタマップ・タイミング制約含む） |
| [`Zybo_MAX2771_PMOD.md`](Zybo_MAX2771_PMOD.md) | PMODボードのピン接続・Q1〜Q5の回答 |
| [`pocketgnss.c`](pocketgnss.c) | MAX2771レジスタ動作確認用ビットバンSPIサンプルコード |
| [`Zybo-Z7-Master.xdc`](Zybo-Z7-Master.xdc) | Digilent公式マスターXDC |
