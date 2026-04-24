# 手写 RPC 框架 (C++17)

基于 Reactor 模式实现的高性能 RPC 框架，对标简历所述技术栈。

## 架构概览

```
┌────────────────────────────────────────────────────────────┐
│                        Consumer                            │
│  ServiceProxy<T>  →  RpcChannel  →  [wire protocol]       │
└────────────────────────────────────────────────────────────┘
                              │
               TCP (自定义帧: Magic+Length+Body)
                              │
┌────────────────────────────────────────────────────────────┐
│                        Provider                            │
│  TcpServer → Codec → RpcServer → ThreadPool → ServiceImpl │
└────────────────────────────────────────────────────────────┘
                              │
                     ServiceRegistry
                     (ZooKeeper / stub)
```

## 核心组件

| 模块 | 说明 |
|------|------|
| `net/EventLoop` | 基于 `epoll(ET)` 的 Reactor，one-loop-per-thread |
| `net/Channel` | fd 事件绑定；通过 `tie()` 防止 TcpConnection 提前析构 |
| `net/TcpServer` | accept 新连接，分发到 IO 线程 |
| `net/ThreadPool` | `std::thread` + 无界任务队列，处理业务逻辑 |
| `net/Buffer` | 读写双 idx 缓冲区，`readv` 减少系统调用 |
| `codec/Codec` | `[Magic:4][BodyLen:4][Body:N]` 帧格式，解决拆包/粘包 |
| `rpc/RpcServer` | 注册 `protobuf::Service`，反射调度方法 |
| `rpc/RpcChannel` | 实现 `protobuf::RpcChannel`；`request_id` 多路复用 |
| `rpc/ServiceProxy<T>` | RAII 代理对象，模板 + 虚函数封装调用细节 |
| `registry/ZooKeeperClient` | 封装 ZK C 客户端；无 ZK 环境时自动降级为内存 stub |
| `registry/ServiceRegistry` | 服务注册/发现 + 节点变更 Watcher |
| `registry/LoadBalancer` | 随机 / 轮询 / 权重随机 / 一致性哈希 四种策略 |

## 传输协议

```
 0       4       8       8+N
 +-------+-------+---------+
 | Magic | Len   |  Body   |
 | 4 B   | 4 B   |  N B    |
 +-------+-------+---------+
Magic = 0xCAFEBABE (大端)
Len   = Body 字节数 (大端)
Body  = 序列化后的 RpcRequest / RpcResponse protobuf
```

## 快速开始

```bash
# 依赖 (Ubuntu/Debian)
sudo apt install -y libprotobuf-dev protobuf-compiler cmake build-essential

# 编译
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 运行 Hello 示例
./hello_server &
./hello_client
# SayHello → Hello, World!
# SayBye   → Goodbye, World!

# 运行 Calculator 示例
./calc_server &
./calc_client
# Add(10.00, 3.00) = 13.00
# Sub(10.00, 3.00) = 7.00
# Mul(10.00, 3.00) = 30.00
# Div(10.00, 3.00) = 3.33
# Div(10.00, 0.00) = ERROR: division by zero
```

## 启用 ZooKeeper

```bash
sudo apt install -y libzookeeper-mt-dev
cmake .. -DWITH_ZOOKEEPER=ON
```

## 目录结构

```
RPC/
├── include/
│   ├── net/          # EventLoop / Channel / EpollPoller / TcpServer / ThreadPool / Buffer
│   ├── codec/        # Protocol 常量 / Codec 编解码器
│   ├── rpc/          # RpcServer / RpcChannel / RpcController / ServiceProxy<T>
│   └── registry/     # ZooKeeperClient / ServiceRegistry / LoadBalancer
├── src/              # 对应实现
├── proto/            # rpc.proto (RpcRequest / RpcResponse)
└── example/
    ├── hello/        # HelloService: SayHello / SayBye
    └── calculator/   # CalcService: Add / Sub / Mul / Div
```

## 设计亮点

- **RAII 贯穿全局**：`TcpConnection`、`ServiceProxy`、`ZooKeeperClient` 均自动管理资源
- **模板动态代理**：`ServiceProxy<T>` 编译期绑定 Stub，运行时通过虚函数分发，零运行时类型判断开销
- **无锁 wakeup**：EventLoop 用 `eventfd` + 原子标志实现跨线程唤醒，避免 `write/read` 竞争
- **零拷贝读**：`Buffer::read_fd` 使用 `readv` 搭配栈上 64KB 临时缓冲区，单次系统调用读满内核缓冲区
- **一致性哈希**：150 个虚拟节点 + FNV-1a，支持设置 affinity key 实现会话粘性
