# 第六章：Floyd-Warshall 全点对最短路径

本章把 Quinn 书中“Floyd 算法”案例重制为一个可以独立编译、运行、验证和测量的 MPI 学习项目。重点不是把旧版 MPI 函数名机械替换成新函数，而是保留第六章的学习主线：

```text
运行时分配矩阵
    → 按行划分二维矩阵
    → 找到当前 k 行的拥有者
    → 用点对点通信交换 pivot 行
    → 每个 rank 更新自己的行
    → 聚合结果并验证
```

本目录有两个实现：

| 程序 | 通信方式 | 学习重点 |
|---|---|---|
| `floyd_p2p` | `MPI_Send` + `MPI_Recv` | 第六章的点对点通信、消息匹配和避免死锁 |
| `floyd_bcast` | `MPI_Bcast` | 用集合通信表达同一个算法，作为现代对照 |

## 1. 学习目标

完成本章后，应能解释：

- Floyd-Warshall 为什么需要按照 `k = 0, 1, ..., n-1` 的顺序迭代；
- 为什么按行划分后，每轮只需要把第 `k` 行发送给所有 rank；
- `MPI_Scatterv` 和 `MPI_Gatherv` 如何处理 `n` 不能被进程数整除的情况；
- `MPI_Send` 的发送方和 `MPI_Recv` 的接收方如何通过 communicator、source、tag 和 datatype 匹配；
- 为什么“所有 rank 互相先发送，再互相接收”可能死锁；
- 为什么 `MPI_Bcast` 能更直接地描述“一行发给所有进程”；
- 为什么 MPI 进程数增加后，计算量下降不一定能抵消通信和启动开销。

## 2. 算法和数据分解

设 `D` 是一个带权有向图的距离矩阵。初始化时：

- `D[i][i] = 0`；
- `D[i][j]` 是边 `i → j` 的权重；
- 本项目生成的是正权稠密图，因此不会产生负环，且初始矩阵不需要从磁盘读取。

第 `k` 轮的递推式为：

$$
D^{(k)}[i][j] =
\min\left(D^{(k-1)}[i][j],
D^{(k-1)}[i][k] + D^{(k-1)}[k][j]\right).
$$

代码采用原地更新。此时 `D[i][k]` 和 pivot 行 `D[k][j]` 在更新时仍然足够使用，因此不需要保存每一轮完整的矩阵副本。

### 按行划分

rank `r` 负责半开区间：

$$
\text{begin}(r) = \left\lfloor \frac{r n}{p} \right\rfloor,
\qquad
\text{end}(r) = \left\lfloor \frac{(r+1)n}{p} \right\rfloor.
$$

本实现允许 `p > n`，这时部分 rank 没有本地行；这也是使用 `MPI_Scatterv` 而不是简单 `MPI_Scatter` 的原因。

第 `k` 行属于唯一的 owner。owner 从自己的本地矩阵复制 pivot 行，然后：

- `floyd_p2p`：owner 对每个其他 rank 调用一次 `MPI_Send`，其他 rank 调用一次 `MPI_Recv`；
- `floyd_bcast`：所有 rank 对同一个 communicator 调用 `MPI_Bcast`。

得到 pivot 行后，每个 rank 只更新自己拥有的行，不需要再交换完整矩阵。

## 3. 编译

在本目录执行：

```bash
cd /home/radiant/hpc_project/mpi/mpi_omp_quinn/6th
make clean
make -j2
```

依赖：

- C11 编译器；
- `mpicc`；
- `mpirun`；
- Python 3；
- 绘图时需要 `matplotlib`。

检查编译产物：

```bash
ls -lh floyd_p2p floyd_bcast
```

## 4. 运行命令

程序的命令行格式是：

```text
./floyd_p2p N [seed] [output_matrix.txt]
./floyd_bcast N [seed] [output_matrix.txt]
```

例如，用 4 个 MPI 进程计算 `64 × 64` 矩阵：

```bash
mpirun --oversubscribe -np 4 ./floyd_p2p 64 7
mpirun --oversubscribe -np 4 ./floyd_bcast 64 7
```

其中 `7` 是确定性图生成种子。相同的 `N` 和 `seed` 会生成相同矩阵，便于比较两个实现。

输出示例格式：

```text
N 64
PROCESSES 4
DIST_SUM 123456
TIME 0.001234567
CORRECT 1
```

字段含义：

- `DIST_SUM`：最终距离矩阵所有元素之和，用于快速比较运行结果；
- `TIME`：所有 rank 中的最大计算时间，代表并行作业的墙钟时间；
- `CORRECT 1`：rank 0 将并行结果与串行 Floyd-Warshall 参考结果逐元素比较后的结果。

### 输出最终矩阵

小矩阵可以让 rank 0 写出最终矩阵：

```bash
mpirun --oversubscribe -np 4 ./floyd_p2p 8 7 final_p2p.txt
head -10 final_p2p.txt
```

文件第一行是矩阵阶数，后面每行有 `N` 个整数。实际的大规模实验不建议写出矩阵，因为 I/O 会占用大量时间和磁盘空间。程序只由 rank 0 负责写文件，避免多个进程同时覆盖同一个文件。

## 5. 正确性测试

运行完整测试：

```bash
make check
```

测试包含：

- `N = 1, 4, 17, 64, 101`；
- MPI 进程数 `1, 2, 4`；
- `floyd_p2p` 和 `floyd_bcast` 两种实现；
- 进程数不能整除矩阵阶数的情况；
- `N = 1` 且进程数大于矩阵行数的情况。

也可以手动对照两个实现：

```bash
for program in floyd_p2p floyd_bcast; do
    mpirun --oversubscribe -np 4 ./$program 32 7
 done
```

两个程序的 `DIST_SUM` 应该一致，并且都应输出：

```text
CORRECT 1
```

## 6. 性能实验、记录和绘图

默认对 `N=512`、1/2/4 个 MPI 进程分别重复 3 次：

```bash
make bench
```

指定规模、进程数和重复次数：

```bash
make bench N=1024 P="1 2 4 8" R=5
```

本项目不把进程数固定为 4。应根据设备的物理核心数逐步测试，例如：

```bash
make bench N=1024 P="1 2 4 8 10" R=5
make bench N=2048 P="1 2 4 8 10 16 20" R=3
```

`N` 增大后，Floyd 算法的计算量大约按 $O(N^3)$ 增长；如果规模太小，MPI 启动和通信时间会淹没计算时间。

`make bench` 会在 `results/` 生成：

```text
benchmark_YYYYMMDD_HHMMSS_nNNN.csv
benchmark_YYYYMMDD_HHMMSS_nNNN.md
```

CSV 保存每一次原始测量，Markdown 保存最小时间、中位数和相对于最小进程数的加速比。

从最新 CSV 生成 PNG 图：

```bash
make plot
```

也可以明确指定文件：

```bash
python3 scripts/plot.py \
    results/benchmark_YYYYMMDD_HHMMSS_n512.csv \
    --output results/floyd_scaling.png
```

建议的实验闭环：

```bash
make clean
make -j2
make check
make bench N=512 P="1 2 4 8" R=5
make plot
ls -lh results/
```

### 读图时要问的问题

1. `floyd_p2p` 与 `floyd_bcast` 的通信成本谁更低？
2. 进程数从 1 增加到 2、4、8 时，时间是否近似减半？
3. `N=128` 和 `N=1024` 的扩展性是否相同？
4. 是否已经超过物理核心数？
5. WSL2、CPU 频率变化、进程绑定和后台任务是否影响了结果？

不要只看一次运行的最小值。应保存 CSV，并同时观察中位数和重复运行的波动。

## 7. 点对点通信与死锁

`floyd_p2p.c` 中，当前 pivot 行的 owner 只发送，其他进程只接收：

```c
if (rank == owner) {
    MPI_Send(pivot, n, MPI_INT, destination, FLOYD_TAG, MPI_COMM_WORLD);
} else {
    MPI_Recv(pivot, n, MPI_INT, owner, FLOYD_TAG,
             MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}
```

这是一种简单且安全的通信角色划分：不会出现两个 rank 都在等待对方接收的环。

不要把它随意改成下面这种模式：

```text
所有进程先 MPI_Send
所有进程再 MPI_Recv
```

当消息超过 MPI 实现的 eager 缓冲区时，发送可能等待接收方，而所有进程又都在发送，于是形成死锁。实际项目中可以使用：

- 明确的发送/接收顺序；
- `MPI_Sendrecv`；
- `MPI_Isend`/`MPI_Irecv` 加 `MPI_Waitall`；
- 合适的集合通信。

`MPI_Send` 与 `MPI_Recv` 是经典 MPI 接口，仍然适合解释消息匹配机制；现代化的重点是检查错误、处理不均匀分块、避免隐含假设，并通过实验比较通信表达方式，而不是为了版本号替换所有基础接口。

## 8. 代码阅读顺序

建议按以下顺序学习：

1. 阅读本 README 的算法和数据分解；
2. 阅读 `floyd_serial`，先理解串行递推；
3. 阅读 `floyd_p2p.c` 的 `MPI_Scatterv`、owner 选择、发送/接收和 `MPI_Gatherv`；
4. 阅读 `floyd_bcast.c`，只比较它与点对点版本的通信差异；
5. 修改 `N`、进程数和 seed，运行 `make check`；
6. 运行 benchmark，观察通信开销；
7. 再到本地 `course-materials-mpi-openmp/mpi-omp/` 中学习 `send_receive.c`、`redistribute.c`、`global_sum.c` 等对应案例。

## 9. 本章暂时不做什么

- 不把完整 Quinn 书籍或课程 PDF 复制到 Git 仓库；
- 不把 `MPI_Bcast` 机械替换成 `MPI_Ibcast`。Floyd 的每一轮都依赖当前 pivot 行，立即等待非阻塞广播通常没有可重叠的独立工作；
- 不把本章的按行算法误称为 SUMMA。SUMMA 属于后续矩阵乘项目，需要二维进程网格和 A/B 面板广播；
- 不把随机生成的稠密图当作真实图应用的唯一输入方式。后续可以增加稀疏图、文件输入和 MPI-IO 案例。

## 10. 完成标准

完成以下项目即可认为掌握本章第一版：

- [ ] 能说明 `k` 循环为什么不能任意重排；
- [ ] 能画出按行分块和 pivot 行通信过程；
- [ ] 能运行 `make check` 并解释 `MPI_Scatterv/Gatherv`；
- [ ] 能解释 `MPI_Send/MPI_Recv` 的 source、tag 和 communicator 匹配；
- [ ] 能指出一种可能死锁的通信写法；
- [ ] 能独立完成一次 benchmark、找到 CSV 和 PNG；
- [ ] 能解释为什么更大的 `N` 往往更适合观察并行加速；
- [ ] 能比较点对点版本与 `MPI_Bcast` 版本的差异。
