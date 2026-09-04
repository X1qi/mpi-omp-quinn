# 第五章：MPI 埃拉托斯特尼筛法

本目录对应 Quinn《MPI与OpenMP并行程序设计：C语言版》第五章的学习项目。
代码保留“逐步优化”的教学主线，同时使用现代 C 写法和仍然有效的 MPI 标准接口。

## 目录

```text
sieve_book.c     书本基线：存储 2..n，rank 0 广播每个筛选素数
sieve_odd.c      优化 1：只存储奇数，仍保留书本的广播流程
sieve_local.c    优化 2：每个 rank 独立生成 sqrt(n) 内的小素数，取消筛选阶段广播
sieve_blocked.c  优化 3：在优化 2 上进行 Cache 分块
sieve_common.h  共享的分块、参数、标记、计时和结果输出
base_primes.h   为优化 2/3 生成本地小素数表
5_4_2.md        5.4.2：一维块分解边界与进程归属公式推导
Makefile         构建、正确性测试和性能实验入口
scripts/benchmark.py 运行实验并生成 CSV、Markdown 摘要、PNG 图
scripts/plot.py      从已有 CSV 重新生成 PNG 图
```

所有编译生成的可执行文件都在本目录根部，运行 `make clean` 可以删除它们。

## 学习顺序

### 1. 先读懂书本基线

```bash
make
mpirun --allow-run-as-root --oversubscribe -np 4 ./sieve_book 1000000
```

书本算法把 `[2, n]` 分成连续区间。每轮由 rank 0 找到下一个素数，再用
`MPI_Bcast` 告诉其他进程。它最适合用来理解：

- block 数据分解；
- 局部数组和全局数值的映射；
- 点对点通信与集合通信的区别；
- 为什么 rank 0 会成为算法瓶颈；
- 为什么进程数过多时书本版本会受到限制。

### 2. 对比三个优化

```bash
mpirun --allow-run-as-root --oversubscribe -np 4 ./sieve_odd 1000000
mpirun --allow-run-as-root --oversubscribe -np 4 ./sieve_local 1000000
mpirun --allow-run-as-root --oversubscribe -np 4 ./sieve_blocked 1000000 32
```

四个版本的关系是：

```text
sieve_book
    ↓ 删除偶数候选数，减少内存和标记工作
sieve_odd
    ↓ 每个进程本地生成小素数，消除筛选阶段的广播
sieve_local
    ↓ 交换循环顺序，逐块处理局部数组，改善 Cache 局部性
sieve_blocked
```

这里的“现代化”主要体现在：

- 使用 `uint64_t` 和 `MPI_UINT64_T`，避免 `int` 的范围限制；
- 使用安全的整数平方根和参数检查；
- 使用 `MPI_Wtime` 和所有 rank 的最大时间；
- 不使用已废弃的 MPI API；
- 把公共逻辑集中在头文件中，减少重复代码。

`MPI_Bcast`、`MPI_Reduce`、`MPI_Barrier` 等基础函数本身并没有过时，
不应为了追求“新 API”而强行替换。MPI-3/4 的非阻塞集合通信、persistent
collective、Sessions 等内容会在更适合的后续案例中学习；本章最重要的是理解
数据分解、通信瓶颈和局部性。

## 命令行操作

以下命令都在本目录执行：

```bash
cd /home/radiant/hpc_project/mpi/mpi_omp_quinn/5th
```

### 编译

```bash
make                 # 编译四个版本
make -j2             # 并行编译
make clean           # 删除编译生成的四个可执行文件
```

也可以手动编译一个程序：

```bash
mpicc -O3 -std=c11 -Wall -Wextra -Wpedantic \
    sieve_book.c -o sieve_book -lm
```

### 运行单个版本

如果当前用户是 root，Open MPI 通常需要
`--allow-run-as-root`；普通用户可以删除该选项。

```bash
mpirun --allow-run-as-root --oversubscribe \
    -np 1 ./sieve_book 1000000

mpirun --allow-run-as-root --oversubscribe \
    -np 4 ./sieve_odd 1000000

mpirun --allow-run-as-root --oversubscribe \
    -np 4 ./sieve_local 1000000

mpirun --allow-run-as-root --oversubscribe \
    -np 4 ./sieve_blocked 1000000 32
```

`blocked` 的第二个参数是 Cache block 大小，单位为 KiB：

```bash
./sieve_blocked <n> [block_KiB]
```

### 检查输出

```bash
mpirun --allow-run-as-root --oversubscribe \
    -np 4 ./sieve_local 1000000
```

输出类似：

```text
VERSION odd_local_primes
N 1000000
PROCESSES 4
PRIMES 78498
TIME_SECONDS 0.000...
```

其中：

- `PRIMES` 是不超过 `n` 的素数个数；
- `TIME_SECONDS` 是所有 rank 局部时间中的最大值；
- 只有 rank 0 输出最终结果。

## 正确性测试

```bash
make check
```

测试会使用 1、2、4 个 MPI 进程，并检查：

```text
n = 2          -> 1
n = 3          -> 2
n = 10         -> 4
n = 100        -> 25
n = 10000      -> 1229
n = 1000000    -> 78498
```

测试的原则是：四个版本、多个进程数都必须得到同一个 `PRIMES`。
这里的 1、2、4 只是快速正确性冒烟测试，不是性能实验的进程数上限。
`make check` 使用小规模输入，进程数过多时 MPI 启动开销会淹没算法本身，
因此扩展性应使用下面的较大规模 benchmark 单独测试。
如果你修改了代码，建议先运行 `make check`，再进行性能测试。

## 性能实验应该测试多少个 MPI 进程

只测试到 4 个进程对于性能分析通常偏少，最多只能观察并行化的初步效果。
在本机上可以先查看 CPU 拓扑：

```bash
nproc
lscpu | egrep 'Model name|CPU\(s\)|Core\(s\) per socket|Thread\(s\) per core'
```

本机当前可见 10 个物理核心、20 个逻辑 CPU。因此建议分两组实验：

```bash
# 先测物理核心范围，优先观察真正的计算扩展性
make bench N=10000000 P="1 2 4 8 10" R=5

# 再测超线程/逻辑 CPU，观察超过物理核心后的收益
make bench N=10000000 P="1 2 4 8 10 16 20" R=5
```

建议同时记录 1、2、4、8，以及物理核心数 10；如果要观察超线程，再加入
16 和 20。不要只使用 `1 2 4 8 16`，因为 10 是本机物理核心数这个重要拐点。

单机 MPI 实验最好显式绑定进程，减少进程漂移造成的噪声：

```bash
# 1 到 10 个进程：每个 MPI rank 绑定一个物理核心
MPIEXEC_FLAGS="--oversubscribe --bind-to core --map-by core" \
make bench N=10000000 P="1 2 4 8 10" R=5

# 16 到 20 个进程：允许使用逻辑 CPU
MPIEXEC_FLAGS="--oversubscribe --bind-to hwthread --map-by hwthread" \
make bench N=10000000 P="1 2 4 8 10 16 20" R=5
```

输入规模太小时，测到的主要是 `mpirun` 启动和调度开销。建议先使用
`n = 10^7`，若运行时间仍然很短，再增加到 `10^8`；每组至少重复 5 次，
并使用中位数进行比较。超过物理核心数后速度不一定继续提升，这是正常现象。

## 性能记录与可视化

### 一条命令完成实验

```bash
make bench N=10000000 P="1 2 4" R=3
```

该命令会：

1. 编译四个版本；
2. 对每个版本、每个 MPI 进程数重复运行 3 次；
3. 将每次运行记录到 CSV；
4. 生成中位数性能摘要 Markdown；
5. 生成 PNG 可视化图。

结果保存在：

```text
results/
├── benchmark_YYYYMMDD_HHMMSS_n10000000.csv  # 每次运行的原始记录
├── benchmark_YYYYMMDD_HHMMSS_n10000000.md   # 中位数和加速比摘要
└── benchmark_YYYYMMDD_HHMMSS_n10000000.png  # 性能图
```

`results/` 是运行时生成的，不放入源代码目录的固定清单中。

### 调整实验规模、进程数、重复次数和块大小

```bash
make bench \
    N=100000000 \
    P="1 2 4 8" \
    R=5 \
    BLOCK_KIB=32
```

参数含义：

```text
N          筛选上限，默认 10000000
P          MPI 进程数列表，默认 "1 2 4"
R          每个配置的重复次数，默认 3
BLOCK_KIB  sieve_blocked 的块大小，默认 32
```

建议先做小规模测试：

```bash
make bench N=1000000 P="1 2 4" R=3
```

确认流程无误后再增加规模：

```bash
make bench N=100000000 P="1 2 4 8" R=5
```

### 只运行某个版本并手工记录

```bash
for p in 1 2 4; do
    mpirun --allow-run-as-root --oversubscribe \
        -np "$p" ./sieve_local 10000000
 done
```

### 从已有 CSV 重新绘图

```bash
python3 scripts/plot.py \
    results/benchmark_YYYYMMDD_HHMMSS_n10000000.csv
```

指定输出文件：

```bash
python3 scripts/plot.py \
    results/benchmark_YYYYMMDD_HHMMSS_n10000000.csv \
    --output results/sieve_performance.png
```

也可以让 Makefile 自动寻找最近的一份 CSV：

```bash
make plot
```

### CSV 中记录的字段

```text
timestamp       实验开始时间
host            主机名
n               输入规模
version         教学版本名
executable      可执行文件
processes       MPI 进程数
repeat          第几次重复
repeats         总重复次数
block_kib       blocked 版本的块大小
primes          素数数量
time_seconds    本次运行时间
```

### 图像如何阅读

PNG 图包含两个子图：

1. **运行时间—MPI 进程数**：越低越好；
2. **相对 p=1 的加速比**：越高越好，虚线表示理想线性加速。

绘图使用每个配置的中位数，而不是某一次运行值。这样可以减少操作系统
调度、CPU 频率变化和后台负载带来的偶然波动。

性能实验时必须记录：

```text
输入规模 n
MPI 进程数
重复次数
blocked 块大小
MPI 实现及版本
编译选项
机器和 CPU 信息
```

不要把不同机器、不同 MPI 实现或不同编译选项的结果直接放在一张图中比较。

### 当前实现的实验限制

`sieve_book` 和 `sieve_odd` 保留了书本算法的 rank 0 限制。对于过小的 `n`
和过多 MPI 进程，它们会主动退出；这是教学上的算法限制，不是测试脚本故障。
正式 benchmark 建议使用：

```bash
N=10000000 P="1 2 4"
```

或者更大的 `N`。如果只想测试可扩展性，可以重点观察：

```text
sieve_local
sieve_blocked
```

## 现代化边界

本章的现代化不是把所有阻塞通信机械替换成最新 API：

- `MPI_Bcast`、`MPI_Reduce`、`MPI_Barrier` 仍然是有效的 MPI 标准接口；
- 本项目使用 `uint64_t`、`MPI_UINT64_T` 和现代 C 的参数检查；
- 已废弃 API 的替换会在派生类型等真正相关的案例中学习；
- `MPI_Ibcast` 在本筛法中没有明显的计算重叠机会，因此不作为默认版本；
- MPI-4 persistent collective、Sessions 等内容留到矩阵乘、stencil 和 SUMMA。

## 5.4.2 公式推导

阅读代码前，建议先看 [5.4.2 公式推导](5_4_2.md)，重点掌握：

- 不均匀块分解中 $s_i$、$e_i$ 和 `owner(j)` 的关系；
- 闭区间与半开区间的换算；
- 为什么实际 C 实现应优先使用分段公式，避免负数整除的语言差异。

## 代码阅读重点

### 全局区间到局部区间

`block_begin()` 和 `block_end()` 表示半开区间 `[begin, end)`，并且可以处理
不能被进程数整除的候选数：

```text
全局候选数 = 10，进程数 = 3
rank 0: [0, 4)
rank 1: [4, 7)
rank 2: [7, 10)
```

### 奇数压缩

奇数候选数用下面的映射表示：

```text
odd_index 0 -> 3
odd_index 1 -> 5
odd_index 2 -> 7
```

因此一个字节对应一个奇数候选数，偶数 2 单独计入结果。

### 标记起点

对于素数 `p`，只需从下面两者中较大的那个开始标记：

```text
p * p
low 以上第一个 p 的倍数
```

`p*p` 以前的倍数已经会被更小的素数处理，不需要重复标记。

### 为什么分块可能更快

`sieve_local` 的循环顺序是：

```text
素数 -> 整个局部数组
```

`sieve_blocked` 的循环顺序是：

```text
小块 -> 所有相关素数
```

后者让工作集更容易留在 Cache 中，但块大小不是越大越好，需要通过测试选择。

## 这一章暂时不做什么

- 不把 `MPI_Bcast` 机械替换为 `MPI_Ibcast`；本算法在广播后才能继续，
  非阻塞写法未必有可重叠的计算。
- 不在本章引入 OpenMP；混合 MPI+OpenMP 会在后续矩阵乘和 stencil 中学习。
- 不引入 MPI-4 的 Sessions 或 persistent collective；它们不适合成为这个
  入门案例的第一重点。
- 不保留历史 benchmark、归档二进制和多个脚本；需要新结果时直接运行 `make bench`。

## 完成标准

读完本目录后，你应该能够解释：

1. 每个 rank 负责全局候选区间的哪一部分；
2. rank 0 为什么在书本版本中必须拥有 `sqrt(n)` 以内的候选数；
3. 删除偶数为什么能够减少内存和计算；
4. 本地生成小素数为什么可以消除筛选阶段广播；
5. `MPI_Reduce(..., MPI_SUM, ...)` 和 `MPI_Reduce(..., MPI_MAX, ...)` 各自汇总什么；
6. 为什么并行计时要取所有 rank 时间的最大值；
7. Cache 分块改变了什么，以及为什么必须用 benchmark 验证收益。
