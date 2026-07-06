# D3D11Processing Future Work

この文書は、D3D11Helper の Processing Layer が一通り安定した段階で、将来実装候補として残す画像処理を整理するためのメモです。

## 現在の安定版に含める想定の処理

現時点の Processing Layer には、以下の処理が入っている。

- FormatConvert
- Resize
- Remap
- Composite
- Fused Convert + Resize
- Blur
- RegionEffect
- RegionBlur
- ColorAdjust
- KernelFilter
- MaskProcessor
- Threshold / Visualization
- PyramidProcessor
- PyramidBlur
- PyramidRegionBlur

ここまでで、RGBA-like texture を中心とした基本的な画像処理、領域処理、マスク処理、可視化処理、大半径 blur の高速化が揃っている。

## 将来実装候補

### 1. D3D11MorphologyProcessor

優先度: 高

MaskProcessor と ThresholdProcessor の次に自然につながる処理。

想定機能:

- Erode
- Dilate
- Open
- Close
- MinFilter
- MaxFilter

主な用途:

- segmentation mask の穴埋め
- 小さいノイズ除去
- mask 領域の膨張・収縮
- 輪郭の調整
- AOI / gaze 領域の補正

実装方針:

- 最初は R8G8B8A8_UNORM / R16G16B16A16_FLOAT など RGBA-like で実装する。
- mask channel を選択できるようにする。
- radius 1 の 3x3 morphology から始め、必要に応じて radius N に拡張する。
- Open / Close は Erode / Dilate の組み合わせとして、scratch texture を明示的に渡す API にする。

### 2. D3D11LutProcessor / D3D11ToneMapper

優先度: 中

カメラ映像、HDR、RGBA16F を本格的に扱う場合に有用。

想定機能:

- 1D LUT
- 3D LUT
- Linear -> sRGB
- sRGB -> Linear
- HDR -> SDR tone mapping
- False color
- Exposure / filmic tone mapping

主な用途:

- カメラ映像の色補正
- HDR texture の表示
- depth / confidence / heatmap の false color 表示
- デバッグ用可視化

実装方針:

- 1D LUT は Texture1D または small Texture2D で表現する。
- 3D LUT は Texture3D を使う。
- 最初は RGBA-like input / RGBA-like output に限定する。
- RGBA16F を優先対応 format に含める。

### 3. YUV processing extensions

優先度: 中

現状、多くの Processing は RGBA-like 前提である。カメラ入力や動画処理では NV12 / P010 を多用するため、必要になった処理から YUV 直処理を増やす。

想定機能:

- NV12 / P010 の Y plane のみ darken
- NV12 / P010 の Y plane のみ blur
- NV12 / P010 の Y plane sharpen
- NV12 / P010 の crop / resize
- YUV -> RGB -> processing -> RGB を単一 HLSL に融合する path
- YUV -> RGB -> processing -> YUV の fused path

主な用途:

- カメラ入力の高速前処理
- 動画エンコード前処理
- luminance のみの低コスト効果
- NV12 のまま処理して memory bandwidth を削減する

実装方針:

- まず Y plane のみの処理を検討する。
- UV plane を変更しない処理から始める。
- UV 解像度が 1/2 であることに注意して、blur / resize では Y と UV の処理を分ける。
- 必要がなければ YUV 直接処理は増やしすぎない。

### 4. D3D11ProcessingGraph / D3D11FusedEffectProcessor

優先度: 中から高

Processor の種類が増えると、dispatch 数と中間 texture 数が増える。性能が必要になった段階で、複数処理を fused shader にまとめる仕組みを検討する。

想定機能:

- 処理 node の連結
- 中間 texture の自動割当
- dispatch 順序の管理
- shader fusion の補助
- HLSL include library を使った custom fused shader の作成支援

主な用途:

- NV12 -> RGBA -> resize -> color adjust -> region effect の fused 化
- region blur / mask overlay / composite の dispatch 削減
- real-time camera pipeline の低 latency 化

実装方針:

- まずは完全自動 fusion ではなく、手動 fused shader を書きやすくする補助 API から始める。
- ProcessingCommon.hlsli などを整理して、HLSL function library として再利用しやすくする。
- Graph は resource lifetime と validation を担当する。
- 自動最適化は後回しにする。

### 5. D3D11ProcessingBenchmark

優先度: 高

機能追加が一段落したため、次は性能測定用 sample / utility が必要になる。

想定機能:

- GPU timestamp query による dispatch 時間測定
- 1920x1080 / 3840x2160 の固定ベンチ
- warmup
- average / min / max
- CSV 出力
- processor ごとの比較

測定対象候補:

- Blur
- PyramidBlur
- RegionBlur
- PyramidRegionBlur
- Resize
- FusedConvertResize
- ColorAdjust
- KernelFilter
- MaskProcessor
- ThresholdProcessor

実装方針:

- sample/ProcessingBenchmark として追加する。
- ベンチ対象をコマンドライン引数で指定できるようにする。
- GPU timing と CPU wall time の両方を出す。
- 実験用途で比較しやすいよう CSV 出力を入れる。

### 6. Documentation update

優先度: 高

Processing Layer の機能が増えたため、README と doc を整理する必要がある。

更新候補:

- doc/D3D11Processing.md
- README.md
- sample 一覧
- 各 processor の対応 format 表
- 必要 bind flags
- in-place 可否
- scratch / workspace の必要性
- dispatch 数
- 推奨 pipeline 例

特に書くべき表:

- Processor ごとの input format / output format
- SRV / UAV / scratch の要否
- rect 対応の有無
- RGBA-like / YUV420 / mask texture 対応
- sample 名
- test suite 名

## 将来の実装順の推奨

次に再開する場合は、以下の順番を推奨する。

1. D3D11MorphologyProcessor
2. D3D11ProcessingBenchmark
3. doc/D3D11Processing.md 更新
4. D3D11LutProcessor / D3D11ToneMapper
5. YUV processing extensions
6. D3D11ProcessingGraph / D3D11FusedEffectProcessor

## 安定版タグの考え方

現在の Processing Layer は大きく機能追加されたため、既存の `0.1.0` から `0.2.0` へ上げるのが自然である。

推奨タグ:

- `v0.2.0`

推奨 release title:

- `D3D11Helper v0.2.0 - Processing stable`

推奨説明:

- Processing Layer に blur、region effects、mask、threshold visualization、pyramid blur などを追加した安定版。
