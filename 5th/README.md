# 第五章：MPI 埃拉托斯特尼筛法

对应 Quinn 原书第五章。本章学习：**一维块数据分解、局部/全局下标、集合通信、MPI 计时、通信开销和 Cache 局部性**。

## 学习顺序与代码对应

| 顺序 | 代码/文档 | 学习内容 |
|---|---|---|
| 1 | `sieve_book.c` | 书本基线：按 `[2,n]` 分块，rank 0 找素数并用 `MPI_Bcast` 广播 |
| 2 | `5_4_2.md` | 阅读按块分解的边界、局部下标和进程归属公式 |
| 3 | `sieve_odd.c` | 只保存奇数候选数，理解数据压缩对计算和内存的影响 |
| 4 | `sieve_local.c`、`base_primes.h` | 每个 rank 本地生成 `sqrt(n)` 以内的小素数，减少筛选阶段广播 |
| 5 | `sieve_blocked.c` | 在本地小素数版本上进行 Cache 分块，比较循环组织和局部性 |
| 6 | `scripts/benchmark.py`、`scripts/plot.py` | 记录运行时间、加速比并绘图 |

公共实现位于 `sieve_common.h`。

## 相比书本的现代化内容

- 使用现代 C 写法、参数检查和安全的整数计算；
- 使用 `uint64_t`/`MPI_UINT64_T`，减少整数范围假设；
- 使用 `MPI_Wtime`，并以所有 rank 中的最大时间作为并行墙钟时间；
- 支持不能被进程数整除的区间；
- 将公共逻辑抽出，保留四个版本之间的可比性；
- 保留仍然有效的 `MPI_Bcast`、`MPI_Reduce`、`MPI_Barrier`，不为追求“新 API”而机械替换。

## 必要命令

以下命令均在 `5th/` 目录执行：

```bash
cd /home/radiant/hpc_project/mpi/mpi_omp_quinn/5th
```

### 编译

```bash
make clean && make -j2
```

### 运行

```bash
mpirun --oversubscribe -np 4 ./sieve_book 1000000
mpirun --oversubscribe -np 4 ./sieve_odd 1000000
mpirun --oversubscribe -np 4 ./sieve_local 1000000
mpirun --oversubscribe -np 4 ./sieve_blocked 1000000 32
```

`32` 是 Cache 分块大小，单位为 KiB。

如果 Open MPI 拒绝 root 用户运行，在命令中增加：

```text
--allow-run-as-root
```

### 正确性测试

```bash
make check
```

测试四个版本和 `1、2、4` 个进程，检查多个 `n` 下的素数数量。这里的进程数只是快速测试范围，不是性能实验上限。

### 性能记录与绘图

```bash
make bench N=10000000 P="1 2 4 8" R=3
```

其中：

- `N`：输入规模；
- `P`：MPI 进程数列表；
- `R`：每种配置的重复次数。

该命令会在 `results/` 生成 CSV、Markdown 摘要和 PNG 图。也可以重新绘制已有结果：

```bash
python3 scripts/plot.py \
    results/benchmark_YYYYMMDD_HHMMSS_n10000000.csv
```

或让 Makefile 使用最新 CSV：

```bash
make plot
```

建议的完整操作顺序：

```bash
make clean
make -j2
make check
make bench N=10000000 P="1 2 4 8 10" R=3
make plot
```

性能实验应至少记录：`n`、MPI 进程数、重复次数、`blocked` 的块大小、MPI 实现和编译选项。小规模输入容易被 MPI 启动开销影响，不能只根据一次运行判断优化是否有效。

## 本章总结

本章从书本式筛法开始，逐步观察：

```text
连续区间分解
    → 奇数压缩
    → 减少广播
    → Cache 分块
    → 正确性与性能验证
```

重点不是记住某个最快版本，而是理解：**数据如何分给进程、哪些信息必须通信、通信如何限制扩展性，以及局部性优化必须通过实验验证。**
