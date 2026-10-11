# Greta Oto 設計文書の要約（英語版PDF）

`Documents/` の英語版PDF 3冊を要約したもの。中国語版は対象外。

| 文書 | ページ数 | 日付 |
|---|---|---|
| Greta Oto Design Manual（ベースバンド設計） | 34 | 2021/5/20 |
| Greta Oto Firmware Design | 47 | 2021/7/20 |
| Greta Oto Register and Control Interface | 14 | 2021/6/22 |

---

## 1. 全体像

- **用途**：民生向けの低コスト・低消費電力受信機（IoTなど）。作者 Jun Mo 氏（Globsky Technology）の「Butterfly」アーキテクチャがベースで、OPENGNSS から派生したプロジェクト。
- **対象信号（L1帯）**：GPS L1C/A（SBASを含む）、GPS L1C、Galileo E1、BDS B1C。のちにL5帯のTE拡張（GPS L5、Galileo E5a、BDS B2a）を追加。QZSSは文書中に明記がないが、L1C/A・L1Cと同じ符号生成器で扱える。
- **入力信号**：
  - ゼロ近傍IFの複素信号。サンプリング周波数は4.092 MHzより少し高い値（実際は4.113 MHz）。
  - I/Qそれぞれ4ビットのsign/magnitude形式。MSBが符号、下位3ビットが1, 3, …, 15を表す。
  - RFフロントエンドの片側帯域幅は2 MHzが前提。条件を満たさないフロントエンドには前処理器が必要。
- **ベースバンドの構成**：TE（Tracking Engine）、AE（Acquisition Engine）、TE FIFO、State Buffer、符号ROM（Legendre、メモリ符号）、PPS。64 kBのアドレス空間にマップされている。

---

## 2. Tracking Engine（TE）

### 2.1 時分割処理

- 4本の物理チャネルを時分割で使い、数十本の論理チャネルを処理する。
- 想定は48チャネル（GPS、Galileo、BDSが各16）。1サンプル/クロックで処理するため、100 MHz動作なら処理時間は約50%。
- 処理の流れ（1 msブロック単位）：
  1. TE FIFOに1 ms分のサンプル（データブロック）が溜まるまで待つ。
  2. State Bufferから物理チャネルへチャネル状態を読み込む（fill）。
  3. 相関処理を行う。符号周期の境界で相関結果をFIFOに出力する。
  4. チャネル状態をState Bufferへ書き戻す（dump）。
  5. 全論理チャネルが終わるまで2〜4を繰り返す。
  6. データブロックを破棄し、必要なら割り込みを出してCPUの再開指示を待つ。
- TEはCPUが再開を指示するまで停止している。CPUはその間にISR内でチャネルレジスタ（State Buffer）を読み書きする。

### 2.2 TE FIFO

- read/writeポインタで管理する。readポインタは同じブロックを繰り返し出力し（rewind）、全チャネル終了後に次のブロックへ進む（skip）。
- **オーバーフロー保護**：サンプルカウンタはFIFO容量より広い範囲を持つ。オーバーフローしてもサンプルが壊れるだけで、次のブロックから回復する。閾値超過の表示もあり、ISRを早く切り上げる判断に使う。
- **dummy write**：デューティサイクル動作用。書き込みを止め、ポインタだけ進めてローカル時刻を維持する。
- **writeポインタのラッチ**：ローカル時刻の基準を兼ねる。CPU指令、外部イベントマーク、PPS、AE（AEバッファの書き込み開始）の4つのイベントでラッチされる。値はround番号、ポインタ、sub count（システムクロック単位）の3つに分かれる。
- **ブロックサイズ調整**：−128〜127サンプルの範囲で1回だけ効く（オプション）。

### 2.3 相関チャネル

- 1物理チャネルあたり8相関器（Cor0〜7）。間隔は1/2チップ。
- carrier NCOは32ビットで、上位6ビットでsin/cosテーブル（振幅12）を引く。code NCOは符号レートの2倍でオーバーフローする。
- 積算結果はI/Q各16ビット。
- **code jump**：8ビットの符号付き値で、相関ピークを相関器単位で前後に移動させる。
- **狭相関**：code NCOの上位ビットを使って、1/4チップ、1/8チップ間隔の符号を作る。BOCには1/8チップを推奨。
- **BOC(1,1)**：オーバーフロー2回に1回、反転した符号を入れてサブキャリアを生成する。片側ローブで追尾する場合は、1.023 MHzずらしたBPSKとして扱える。
- **パイロットとデータ**：相関器1〜7はパイロット、相関器0は第2符号生成器でデータチャネルに割り当てる（2符号遅延させて相関器4に揃える）。
- **NH符号の除去**：25ビットのNHレジスタを持つ。E1Cの25ビットはハードウェアで除去する。L1C/B1Cの1800ビットは、ソフトウェアが20ビット単位（実際は21ビット以上、1ビット以上重複）のセグメントに分け、90セグメントを順に書き換える。
- **上書き保護**：コードドップラーが正の場合などに、1ブロック内で同じ相関器が2回結果を出すことがある。2回目の結果は専用レジスタ（1組のみ）に退避される。
- **データのハード復号**：パイロットのコヒーレント時間（20 ms以上）はデータのビット長（E1は4 ms、L1C/B1Cは10 ms）より長い。そのため、相関器0の結果をハードウェアで軟判定する（Viterbi用に4ビット、LDPC用に8ビット）。

### 2.4 符号生成器

| 種類 | 対象 | 方式 |
|---|---|---|
| 汎用LFSR | L1C/A、SBAS、L5、E5a、B2a | 14ビットLFSRを2本使うGold符号生成器（28ビットのシーケンシャルモードもあり） |
| Weil符号 | L1C、B1C | Legendre ROM（GPS用とBDS用が各1つ）を16ビットポートで読み出し、2段のアービタで共有する |
| メモリ符号 | Galileo E1B/E1C | 36 kBのROM。1セグメント32 DWORD（1023ビット）で、1衛星あたりE1BとE1Cに各4セグメント |

### 2.5 State Buffer とノイズフロア

- **State Buffer**：1論理チャネルあたり32 DWORD。
  - 0〜15：設定・状態（CARRIER_FREQ、CODE_FREQ、CORR_CONFIG、NH_CONFIG、COH_CONFIG、PRN_CONFIG/STATE/COUNT、CARRIER_PHASE/COUNT、CODE_PHASE、PRN_CODE、CORR_STATE など）
  - 16〜23：部分積分
  - 24〜31：コヒーレント和
- **ノイズフロア**：最初の論理チャネルのデータを1023長のm系列と相関させ、1 msの振幅をJPL近似で求める。平滑化係数は1/256〜1/16384。Rayleigh分布の期待値 σ√(π/2) からσを逆算し、C/N0の計算に使う。

---

## 3. Acquisition Engine（AE）

- **rate adaptor**：
  1. 公称IFを除去する。BOC信号の場合はさらに1.023 MHzずらし、片側ローブで捕捉する。
  2. 6タップFIR（係数 −5, 8, 20, 20, 8, −5）を通し、24ビットNCOで2.046 MHzにリサンプリングする。
  3. 閾値1つで2ビットsign/magnitudeに再量子化する。
- **AEバッファ**：64〜256 kB。1 msあたり1 kB。書き込み開始時にTE FIFOのアドレスをラッチし、AEからTEへの引き渡し（aiding）に使う。
- **マッチトフィルタ**：
  - サンプル682段（I/Q各6ビット）、符号は341段×2組（交互に使用）。1回に1/3 ms分を処理する。
  - 回路を小さくする工夫：隣接サンプルの事前加算で入力を半減、XORによる符号乗算と+1補正の一括化、1ビット加算木の再利用。
- **DFT**：4組の共役回転因子で8周波数ビン（±1, 3, 5, 7 ×Δf）を並列に計算する。回転テーブルは256エントリで10ビット。コヒーレントRAMはexp10形式（I/Q各10ビット＋指数4ビット）で682×192ビット。
- **非コヒーレント和**：振幅を加算する。8ビットのブロック浮動小数点で、682×64ビット。最終ラウンドで参照ノイズフロアを計算する。
- **ピークソータ**：上位3ピーク（振幅、コード位相、ドップラー）を保持する。隣接するピークとの統合を7通りのケースで処理する。1位と3位のピーク比が閾値を超えたら探索を打ち切る機能もある。
- **ドップラー探索**：0, +1, −1, +2, −2, … × stride の順に探索する。DFTビン間隔はstrideの1/8。
- **探索パラメータ**：1チャネルあたり8 DWORD（設定4、結果4）。最大32チャネルで1 kB。CPUが書き込んで起動すると、AEは7重ループを自律的に実行し、終了時に割り込みを出す。

| 信号 | 探索コード位相数 | code span |
|---|---|---|
| L1C/A | 2046 | 3 |
| Galileo E1 | 8184 | 12 |
| L1C / B1C | 20460 | 30 |

- **結果の解釈**：ドップラーは9ビット（stride 6ビット＋DFT 3ビット）。周波数 = (値 − 3.5) × DFT間隔。

---

## 4. PPS

- 独立したクロックドメインで動作する。1つのマスタカウンタ（31ビット）から3系統のパルスを出し、それぞれ遅延・極性・幅を設定できる。
- CPUからの書き込みは、toggleと2段ラッチでクロックドメインをまたぐ（PPSクロックがシステムクロックより速いことが前提）。
- エッジ合わせの手順：
  1. 測位後にPPSを有効にする。
  2. PPSイベントでラッチされたTE FIFOアドレスを読む。
  3. PVTのクロック誤差から補正量を計算し、`PPS_PULSE_ADJUST` に書き込む。
  4. クロックドリフトから `PPS_PULSE_INTERVAL` を設定する。
  5. 必要なら2〜3を繰り返して精度を上げる。
  6. パルス出力を有効にする。

---

## 5. ファームウェア

### 5.1 構成

| ディレクトリ | 内容 |
|---|---|
| Abstract | `HWCtrl_Model.cpp`（HWアクセス）、`PlatformCtrl_Model.cpp`（OS/BSP） |
| Baseband | `AEManager.c`、`ChannelManager.c`、`TEManager.c`、`TrackingLoop.c`、`TrackingStage.c`、`TaskManager.c`、`TaskQueue.c`、`FirmwarePortal.c` など |
| PVT/frontend | `GpsFrame.c`、`BdsFrame.c`、`MsrProc.c` |
| PVT/backend | `PvtLsq.c`、`PvtKF.c`、`SatCoord.c`、`SatManage.c`、`Matrix.c`、`Convert.c`、`PvtProc.c` |

- **ハードウェア抽象化層**（`HWCtrl.h`）：`AttachBasebandISR`、`GetRegValue`/`SetRegValue`、`LoadMemory`/`SaveMemory`、`SetInputFile`、`EnableRF`。
- **OS/BSP抽象化層**（`PlatformCtrl.h`）：`EnableInt`/`DisableInt`、`CreateThread`（8スレッド以上）、`ENTER_CRITICAL`/`EXIT_CRITICAL`、`EventCreate`/`EventSet`/`EventWait`。
- 同じファームウェアを次の3つの環境で動かせる。Zynqに移植する場合は、この2つのヘッダの実装を差し替える。
  - 実機
  - C model：IFファイルを入力
  - 振る舞いモデル：SignalSimのシナリオXMLを入力し、実機より数十倍速く動作

### 5.2 実行の仕組み

- 割り込み駆動。TEの割り込み（Data Ready、Measurement、Request）とAEの割り込みがある。
- タスクキューは優先度の高い順に Request（ISR内で実行）→ Baseband → PVT → I/O の4つ。タスクは連結リストで管理し、パラメータは循環バッファにコピーする。
- State Bufferへのアクセスは、TE停止中、つまりISR内でしかできない。そのためスレッドからの要求はRequestキューに積む。
  - すぐ実行するタスクは `REQUEST_COUNT = 1` で起動する。
  - 条件を待つタスク（AEバッファが溜まるのを待つなど）は静的に登録し、`REQUEST_COUNT` で定期的に条件を確認する。
- State Bufferの内容はRAM上にミラーを持ち、更新箇所に応じたdirtyフラグで部分的に同期する。

| フラグ | 対象DWORD | 用途 |
|---|---|---|
| ALL_DIRTY | 0〜15 | チャネル初期化 |
| FREQ_DIRTY | 0/1 | 追尾ループの更新 |
| CONFIG_DIRTY | 2/3/4 | 追尾ステージの切り替え |
| CODE_DIRTY | 7/10/11/12 | コード位相の設定 |

- ISRとベースバンド処理は固定小数点が原則（RTOSがFPUレジスタを退避しない場合への対策）。

### 5.3 部分コヒーレント和の処理

文書で最も難しい部分。

- 積分区間は相関器ごとの符号周期に揃うため、ブロック境界で8相関器の結果が揃わないことがある。
- コードドップラーの正負とコヒーレント数が1かどうかで、4つのケースに分けている。
- `CurrentCor`（次に出力される相関器）、`PendingCor`（保留中の数）、`CoherentCount`、上書き保護レジスタの組み合わせで判定する。
  - **保留**：`CurrentCor != 0` かつ `CoherentCount == N−1` のとき、先頭の `CurrentCor` 個を保存する。
  - **補完**：`PendingCor != 0` のとき、残りの 8 − `PendingCor` 個で補う。
  - **上書き保護**：コヒーレント数が1でなければ、退避された値をコヒーレント和の該当位置にコピーする。1なら新しい結果として扱う。
- `DumpCount`/`PrnCount` はCor0に揃っていて、`CoherentCount` はCor7に揃っている。この違いに注意が必要。

### 5.4 追尾

- **ステージ遷移**：Pull-in → Bit-sync（L1C/Aのみ）→ Track0 → Track1（強信号）/ Track2（弱信号）→ Track hold → Release。E1、B1C、L1Cはビット同期の代わりにセカンダリ符号の同期を行う。
- **シフト設定**：4ビット入力のとき、1 msのノイズ振幅は約625。C/N0が50 dB-Hzでも信号振幅は約8838。20 msのコヒーレント和でpost-shift 3なら約22000で、16ビットに収まる。
- **ビット同期**：20 ms分の相関結果をBasebandタスクに渡して処理する。
- **データ復号**：シンボルはL1C/Aが1ビット、E1が4ビット、L1C/B1Cが8ビット。
- **弁別器**：
  - 位相：CORDICによるarctan（14回反復、±π = ±32768）。L1C/Aは2象限、それ以外は4象限。
  - 周波数：cross-dot、または8点FFTと補間 (L−R)/(2P−L−R)。
  - 遅延：(E−L)/(2P−E−L)。
- **ループフィルタ**：FLL補助付き3次PLL、2次FLL、2次DLL（または搬送波補助付き1次DLL）。
  - 係数はBnTだけで決まる形にし、固定小数点でスケーリングする（K1 = 2¹³k1、K2/K3 = 2¹⁵k）。
  - 例：fs = 4.113 MHz、Tc = 20 ms、Bn = 7.5 Hz のとき K1/K2/K3 = 1734/975/56。
- **C/N0**：ノイズフロアと非コヒーレント電力 P = (S₀·nc² + N₀·nc)·nn / 2^(2B+F) から求める。log10は固定小数点で近似する。
- **ロック指標**：PLL/FLL/DLLそれぞれ0〜100。弁別器の出力から増減させる。

### 5.5 観測量とフレーム同期

- **送信時刻** = TOW + フレーム内のビット数 + 半チップ数（2×PrnCount + CodeSubPhase + コヒーレント数）+ code NCO。Cor0基準から4半チップ引いてCor4（prompt）に合わせる。
- **ローカル時刻**：`MEAS_NUMBER` を800〜1000の範囲で調整して正秒に合わせる。クロック誤差はms単位で `MEAS_NUMBER` に加えて補正する（このとき擬似距離が跳ぶ）。
- **ADR（積算ドップラー）**：搬送波のサイクル数とNCO値から公称IF分を引いて計算する。符号はドップラーの逆。
- **パイロット信号のフレーム同期**：B1C/L1Cは、1800ビットのセカンダリ符号の中で16ビットを照合して位置を決め、次の16ビットで確認する。その後サブフレーム1を復号し、サブフレーム2/3をデインタリーブする（LDPCはハードウェアを想定）。
- **GPS LNAV**：32ビットDWORD（前のWORDのD29/D30を含む）でパリティを検査 → 10ビットの同期語を検出 → HOWからTOWを取得 → 362ビットでTOWの連続性を見て確定。暦との整合チェックで相互相関による誤りを防ぐ。

### 5.6 PVT

- **処理手順**：観測量の選別 → 衛星位置・仰角の計算 → 仰角・C/N0によるフィルタ → 電離層・対流圏の補正 → 測位方式の決定 → 解の計算。
- **LSQ**：HᵀHをCholesky分解して解く。速度は同じH行列で計算する。
- **KF**：等速モデル。状態は X = [v, clock drift, pos, δt_GPS, δt_BDS, δt_Galileo]。逐次更新なので逆行列計算は不要。

---

## 6. レジスタ仕様

| アドレス | モジュール |
|---|---|
| 0x0000 | Global（BB_ENABLE、BB_RESET、FIFO_CLEAR、TRACKING_START、MEASUREMENT_NUMBER/COUNT、INTERRUPT_FLAG、REQUEST_COUNT、INT_MASK、BB_VERSION） |
| 0x4000 | AE（AE_CONTROL、AE_BUFFER_CONTROL、AE_STATUS、AE_CARRIER_FREQ、AE_CODE_RATIO、AE_THRESHOLD） |
| 0x5000 | TE FIFO（CONFIG、STATUS、GUARD、READ/WRITE_ADDR、BLOCK_SIZE/ADJUST、LWADDR_CPU/EM/PPS/AE） |
| 0x6000 | TE（CHANNEL_ENABLE、COH_DATA_READY、OVERWRITE_PROTECT_*、POLYNOMIAL、CODE_LENGTH、NOISE_CONFIG/FLOOR） |
| 0x7000 | PPS |
| 0x8000 | TE State Buffer |
| 0xC000 | AE Config Buffer |

- 割り込みは4種類：AE、Request、Measurement、Data Ready。
- アクセスはすべて32ビット単位で、バイトイネーブルはない。

---

## 7. 注意点と、Zynq/MAX2771検討との関係

- **文書と現在のコードのずれ**：
  - 文書は2021年版。ファームウェアの構成一覧には `GpsFrame.c` と `BdsFrame.c` しかなく、直近で追加されたE1 FEC2 RSやNMEA出力は反映されていない。
  - Firmware Design 2章の捕捉手順は「今後記載」となっていて、中身がない。捕捉手順は `AEManager.c` を読む必要がある。
- **2ビット入力の影響**：文書の前提は4ビットI/Q・片側帯域2 MHz。MAX2771の2ビット入力（±1/±3）では、次の値を再調整する必要がある（`zynq_max2771_implementation_study.md` の指摘と同じ）。
  - ノイズ振幅（625）を前提にしたシフト量の設計値
  - ノイズフロアの初期値（784）
  - 軟判定のビット選択
  - `AE_THRESHOLD`（初期値37）
- **MBOC成分**：TEのBOCはBOC(1,1)のみで、狭相関（1/8チップ）を推奨している。CBOC/TMBOC/QMBOCのBOC(6,1)成分は扱わない。これは検討書の信号対応表の記述と一致する。
