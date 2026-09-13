# ba7lya.serial

跨平台串口通信库，使用现代 C++（C++23）编写。

Cross-platform serial port library written in modern C++.

## 特性 (Features)

- 单一门面类 `ba7lya::serial::Serial`，pimpl 隔离平台实现
- 平台后端仅两处：Windows（Win32 comm API）与 Linux（termios + pselect）
- `std::span` / `std::string_view` 零拷贝读写接口
- 全接口线程安全（内部互斥）
- 异常体系派生自 `std::runtime_error` / `std::system_error`
- 串口枚举 `list_ports()`

## 快速上手 (Quick start)

```cpp
#include "ba7lya/serial/serial.hxx"

using namespace ba7lya::serial;

int main() {
    Serial port("COM1", 115200, Timeout::simple_timeout(1000));
    if (!port.is_open()) { return 1; }

    port.write("Hello, serial!\n");
    const std::string line = port.readline(256, "\r\n");
    port.close();
}
```

枚举串口：

```cpp
for (const auto& info : list_ports()) {
    // info.name / info.description / info.hardware_id
}
```

## 构建 (Build)

依赖 [vcpkg](https://github.com/microsoft/vcpkg)（仓库内 submodule：`thirdparty/vcpkg`）与 CMake Presets：

```bash
# Windows (MSVC + Ninja)
cmake --preset MSVC-2022-x64-Debug
cmake --build --preset MSVC-2022-x64-Debug

# Linux
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -Dba7lya.serial_BUILD_TEST=ON
cmake --build build
ctest --test-dir build
```

选项：

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `ba7lya.serial_BUILD_EXAMPLE` | ON | 构建 examples/ |
| `ba7lya.serial_BUILD_TEST` | OFF | 构建 tests/（需要 GTest） |

## 目录结构 (Layout)

```
include/ba7lya/serial/   公开头文件 (serial.hxx, exception.hxx)
src/serial.cxx           平台无关门面
src/win/                 Windows 后端 + 串口枚举
src/linux/               Linux 后端 + 串口枚举
examples/                示例程序
tests/                   GoogleTest 单元测试
thirdparty/vcpkg         vcpkg submodule
```

## References

https://en.wikipedia.org/wiki/Serial_port

https://en.wikipedia.org/wiki/RS-423

https://en.wikipedia.org/wiki/RS-449

https://www.ti.com/lit/an/slla037a/slla037a.pdf?ts=1733055324883&ref_url=https%253A%252F%252Fwww.google.com%252F

https://en.wikipedia.org/wiki/Serial_communication

https://en.wikipedia.org/wiki/Synchronous_serial_communication

https://en.wikipedia.org/wiki/Asynchronous_serial_communication

https://github.com/wjwwood/serial
