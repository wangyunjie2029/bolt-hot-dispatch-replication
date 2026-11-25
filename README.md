# 课程项目（一）：BOLT 热分派链复制（Hot Dispatch Replication）

基于 LLVM 主线 BOLT 的新优化：把热分派链复制到高扇入分派块的各个热前驱之后，使原本的
`jmp dispatch` 变成 fall-through，减少 taken 分支。默认关闭，由 `-replicate-hot-dispatch` 开启。

## 目录

```
docs/设计文档.md              设计文档（问题/设计/实现/实验/局限）
patch/hot-dispatch-replication.patch   LLVM 主线段代码 patch（仅 bolt/ 路径）
docker/Dockerfile             两阶段镜像：编 llvm-bolt + 负载 + 脚本
bench/                        负载源码
  vm_dispatch.c               字节码 VM 微基准（分派密集）
  flat_kernel.c               直线型内核（对照，pass 不应触发）
  sqlite3/                    SQLite 3.54 amalgamation
  sqlite_create.sql           建库（30 万行）
  sqlite_query.sql            通用查询混合
  scan.sql                    VDBE 主导的扫描+表达式负载
scripts/
  run_all.sh                  端到端复现（构建→插桩→优化→测量）
  compare.py                  交错+随机顺序的 A/B 测量
  measure.py                  单命令重复计时
results/                      已跑出的测量数据（json）
work/bolt-src/                BOLT 源码副本（便于检索，可删）
```

## 快速开始

```bash
docker build -f docker/Dockerfile -t bolt-hdr .
docker run --rm -it -v "$PWD/work:/work" bolt-hdr
# 容器内
bash /opt/scripts/run_all.sh /work
```

`run_all.sh` 会重建负载、采集 instrumentation profile、生成 `orig / bolt / bolt+hdr` 三个版本，
并用交错随机顺序跑 15 轮，结果写到 `/work/*.json`。

## 编译负载的必备参数

```bash
clang -O2 -g -Wl,-q -fno-pie -no-pie -fcf-protection=none ...
```

- `-Wl,-q`：保留重定位，否则 BOLT 无法改写（报 `instrumentation runtime libraries require relocations`）
- `-fno-pie -no-pie`：PIE 下跳转表基址会被提出循环，BOLT 无法回溯 → 函数被判 non-simple，
  整个函数不优化（本项目的核心负载都会中招）
- `-fcf-protection=none`：去掉 `notrack` 前缀，避免干扰间接跳转识别

## 结果速览

| 负载 | orig | bolt | bolt+hdr | HDR 相对 BOLT |
| --- | --- | --- | --- | --- |
| vm_dispatch | 0.5475 s | 0.4741 s | **0.4646 s** | **+2.0%** |
| SQLite 扫描（40 副本） | 2.0625 s | 1.9595 s | **1.9375 s** | **+1.1%** |
| flat_kernel（对照） | 0.0289 s | 0.0295 s | 0.0286 s | 0 复制，噪声内 |

## 状态

- [x] pass 实现、选项、流水线接入
- [x] patch（仅 `bolt/`）
- [x] Dockerfile + 复现脚本
- [x] 微基准 / SQLite / 对照三组实验
- [ ] DuckDB 负载（构建耗时较长，可选）
- [ ] 设计文档导出 PDF

## 许可证说明

- `patch/` 下的代码基于 LLVM，遵循 *Apache License v2.0 with LLVM Exceptions*；
- `bench/sqlite3/` 为 SQLite 官方 amalgamation（public domain）；
- 其余脚本、文档与实验数据由作者编写。
