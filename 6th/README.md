# 第六章：Floyd-Warshall 全点对最短路径

对应 Quinn 原书第六章 Floyd 算法。本章学习：**运行时矩阵分配、按行数据分解、`MPI_Scatterv/Gatherv`、点对点通信、集合通信和死锁**。

## 学习顺序与代码对应

| 顺序 | 代码/文档 | 学习内容 |
|---|---|---|
| 1 | `floyd_common.h` 中的 `floyd_serial` | 先理解 Floyd-Warshall 的串行递推和原地更新 |
| 2 | `floyd_p2p.c` | 按行分块；当前 `k` 行的 owner 用 `MPI_Send/MPI_Recv` 分发 pivot 行 |
| 3 | `floyd_bcast.c` | 用 `MPI_Bcast` 分发同一 pivot 行，与点对点版本比较 |
| 4 | `Makefile` 的 `check` | 验证不同矩阵规模、进程数和不均匀分块 |
| 5 | `scripts/benchmark.py`、`scripts/plot.py` | 记录两种通信方式的时间并绘图 |

矩阵生成、行区间计算、结果验证和输出等公共逻辑位于 `floyd_common.h`。

## 相比书本的现代化内容

- 使用现代 C 写法、参数检查和运行时内存分配；
- 使用 `MPI_Scatterv`/`MPI_Gatherv`，支持矩阵行数不能被进程数整除；
- 支持 `MPI` 进程数大于矩阵阶数的情况；
- 提供 `MPI_Send/MPI_Recv` 与 `MPI_Bcast` 两个可比较版本；
- 使用 `MPI_Wtime` 和最大 rank 时间记录并行墙钟时间；
- 用串行参考程序逐元素验证并行结果；
- 保留 `MPI_Send`、`MPI_Recv` 和 `MPI_Bcast`，因为它们正是本章要学习的有效基础接口。

## 必要命令

以下命令均在 `6th/` 目录执行：

```bash
cd /home/radiant/hpc_project/mpi/mpi_omp_quinn/6th
```

### 编译

```bash
make clean && make -j2
```

### 运行

```bash
mpirun --oversubscribe -np 4 ./floyd_p2p 64 7
mpirun --oversubscribe -np 4 ./floyd_bcast 64 7
```

参数含义：

```text
N       矩阵阶数
seed    可选的确定性图生成种子
```

输出中应包含：

```text
CORRECT 1
```

小矩阵可以输出最终矩阵：

```bash
mpirun --oversubscribe -np 4 \
    ./floyd_p2p 8 7 final_matrix.txt
head final_matrix.txt
```

### 正确性测试

```bash
make check
```

测试两个版本、`1、2、4` 个进程，以及矩阵阶数不能被进程数整除的情况。

### 性能记录与绘图

```bash
make bench N=512 P="1 2 4 8" R=3
make plot
```

也可以使用更大的问题规模：

```bash
make bench N=1024 P="1 2 4 8 10" R=5
```

`make bench` 会在 `results/` 生成：

```text
*.csv    每次运行的原始数据
*.md     最小时间、中位数和加速比
*.png    运行时间随 MPI 进程数变化的图
```

## 本章总结

Floyd-Warshall 的每一轮都依赖当前的 `k` 行，因此并行过程是：

```text
按行分配矩阵
    → 找到 k 行的 owner
    → 分发 pivot 行
    → 各 rank 更新本地行
    → 汇总并验证
```

本章的核心是理解：**点对点消息如何匹配、如何避免死锁、集合通信何时更自然，以及通信成本如何影响并行加速。**
