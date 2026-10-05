# 驗證結果（2026-10-05，Asia/Taipei）

本次只讀取根目錄的 solver 原始碼，新增／更新的工具與產物都在
`tools/`。重現命令見 [README.md](README.md)。

## Host：雙向 BFS 的既有紀錄

保留 2026-10-03 的 [host.log](host.log)：`min.cpp` 的實際搜尋回傳
**3,674,160** 個最佳解，每條路徑均經 `solver.c` replay 解回 solved。
H3 search **605.542 秒**，總計 **607.023 秒**，全域最大距離 **11**。
這是既有紀錄，本次未重跑完整 min H3。

本次整合 smoke 另檢查 24 個 min 狀態並明確標示 PARTIAL。
現有 `min.cpp` 的 consteval builder 呼叫非 constexpr 函式；測試 wrapper
只將轉移表改為啟動時初始化，數值與搜尋程式保持原樣。這項編譯適配
及 oracle 的 static-buffer cleanup 適配都只在 `tools/`。

H1 不適用，因為雙向 BFS 沒有 heuristic。H2 的 **15,120** 個排列與
**2,187** 個方向轉移全部符合 oracle，最大 rank 分別為 5,039／728。
搜尋工作表刻意局部填充，不能宣稱為完整全域距離表。
H4 檢查全部 byte 索引；沒有 packed accessor。

## Host：IDA* 本次檢查

[ida-host.log](ida-host.log) 記錄以 `solver.c` 獨立建圖的檢查：

| Gate | 結果 |
| --- | --- |
| Oracle | 全部 3,674,160 狀態可達；diameter 11。 |
| H1 | 所有狀態的實際 `h=exact depth (≤5), otherwise 6` 均不超過精確距離。 |
| H2 transitions | 全部 17,307 個 quarter-turn transitions 符合 oracle。 |
| H2 sparse | 全部 12,224 個排序 records 的 membership、depth、inverse move 與 suffix replay 正確；完整範圍為 depth ≤ 5。 |
| H4 | 所有 record 欄位解碼／重編碼、全狀態 rank round trips 與 oracle 一致；沒有 packed-nibble distance accessor。 |
| Assembly data | `table.S` 的 17,307 個 transitions、12,224 個 records、257 個 prefix boundaries 全部相符，共 84,024 B。 |
| H3 sample | **PARTIAL**：100,000 個不同狀態涵蓋 depth 0–11；實際 `ida_star` 解皆最佳且 replay 解回 solved。 |
| H3 full | **PASS ALL**：全部 3,674,160 個實際 `ida_star` 解皆最佳且經 oracle replay 解回 solved。 |

100,000 抽樣 run 的全域表格／heuristic／encoding 檢查 **0.654 秒**，
H3 search **10.545 秒**，total **11.205 秒**。全狀態 H3 的結果另存
[ida-full.log](ida-full.log)：search **357.357 秒**、total **357.917 秒**。
完整 run 的解長度分布與 oracle depth 0–11 的分布完全一致。

## Target：搜尋量測與 T5–T7

[target-current.log](target-current.log) 記錄兩個 solver、四個案例、
兩個模型共 **16/16 PASS**。每個結果都檢查 simulated exit、move tokens、
oracle 最短長度及獨立 cubie replay；不要求不同 solver 回傳同一條最短路徑。

| State | 最佳步數 | min ISS instructions | IDA ISS instructions | min 5S instructions | IDA 5S instructions | min 5S cycles | IDA 5S cycles |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `12345671111111` | 0 | 197 | 438 | 196 | 437 | 268 | 589 |
| `62345713133111` | 8 | 62,662 | 25,210 | 62,661 | 25,209 | 75,988 | 33,009 |
| `21345671111111` | 11 | 583,653 | 3,652,883 | 583,652 | 3,652,882 | 720,154 | 4,798,686 |
| `54721631111111` | 11 | 957,112 | 9,831,826 | 957,111 | 9,831,825 | 1,253,670 | 12,901,913 |

T5：所有回傳 moves 均解回 solved。
T6：指定輸入 `21345671111111` 在兩種模型均為最佳 11 步：

```text
min: B' R' D2 R' B R B' R D2 B R'
ida: R B' D2 R' B R' B' R D2 R B
```

T7：兩種模型在所有案例回傳相同路徑與長度；本環境 5S 的 retired
count 比 ISS 少 1。四個樣本皆低於 50M instructions，但尚未逐一量測
全狀態的 target 指令上限；其他合法 grader 輸入可直接傳給 `target.py`。

原始紀錄是 `min-STATE-MODEL.log` 與 `ida-STATE-MODEL.log`；
舊的 `target.log` 及無 solver prefix 的 logs 保持原樣。

## Static data 與 renderer

| Build | .text | .rodata | .data | .bss | 額外 runtime workspace |
| --- | ---: | ---: | ---: | ---: | --- |
| min | 3,220 B | 37,836 B | 0 | 0 | 狀態表 3,674,160 B + 程式要求的 queue 97,792 B |
| IDA、RENDER=0 | 1,440 B | 84,230 B | 0 | 144 B | 無 heap |
| IDA、RENDER=1 | 1,988 B | 84,358 B | 0 | 172 B | 無 heap |

IDA 搜尋版資料為 **84,374 B**，小於 128 KiB；繪圖版為 **84,530 B**。
min 的 ELF section size 沒有計入 `brk` 配置的 **3,771,952 B**，
因此不能以 `.bss=0` 宣稱記憶體符合限制。min 同時依賴 Ripes 的
新記憶體零初值，省略了清表。

[target-render.log](target-render.log) 另記錄 IDA solved／8 步案例：
兩個模型共 **4/4 PASS**。8 步繪圖版為 ISS **225,526 instructions**、
5S **225,525 instructions / 419,538 cycles**。這些結果確認繪圖啟用後
求解路徑仍正確；CLI 沒有驗證 GUI LED 像素。

## Pipeline

[pipeline-results.json](pipeline-results.json) 與 `pipe-*.log` 記錄
RV32_5S 的 1,000 次迴圈：

| Benchmark | Instructions | Cycles |
| --- | ---: | ---: |
| ALU forwarding | 4,005 | 6,009 |
| not-taken branch | 3,005 | 5,009 |
| load + independent instruction + dependent branch | 5,005 | 9,009 |
| load + immediate dependent branch | 4,005 | 9,009 |

這些數字包含 startup／exit；迴圈差異符合 taken branch 的兩-cycle flush
及 load-use 的一-cycle stall。插入獨立指令消除 load-use stall。

本次沒有修改 `min.cpp`、`min.S`、`ida.cpp`、`ida.S`、
`solver.c`、`table.S` 或 Git history。

## Exhaustive IDA search effort

[search-stats.json](search-stats.json) and [ida-profile.log](ida-profile.log)
record all 3,674,160 inputs. Mean search effort is **1,548.102064 node visits**;
the worst is **57,355**, at `54721631111111`. Each node is the first entry into
an IDA* frame, including immediate threshold pruning, roots, table hits and
repeat visits across thresholds. Stored suffix reconstruction adds no nodes.

The profiler checks every optimal length and replays every path with the
independent `solver.c` transitions. Search time was **363.776 s**. The total
was **5,687,974,678** node visits; 64-bit counters preserve the exact sum.
The generated candidate differs from `ida.cpp` only by a counter declaration
and increment, verified by an exact source round trip and a recorded SHA-256.

```sh
python3 tools/profile_ida.py
```
