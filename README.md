# simple-modbus

## Overview

**simple-modbus** is a minimal, platform-agnostic Modbus RTU server implementation in C/C++.  
It is designed for embedded and bare-metal applications, providing a lightweight, single-instance Modbus RTU server core and frame handler.  
The module is split into two main components:

- **Modbus RTU Frame Handler (`simple_modbus_rtu.h`)**:  
  Implements the Modbus RTU frame detection state machine, including 3.5 character timeouts, and provides a simple interface for integrating with UART drivers and timer interrupts. It is responsible for detecting, buffering, and emitting Modbus RTU frames, but does not implement Modbus function code handling.

- **Modbus Server Core (`simple_modbus.h`)**:  
  Implements the Modbus protocol logic for reading and writing registers. It is platform-agnostic and relies on user-provided callbacks for transport (frame I/O) and register access. The server core supports basic Modbus function codes and can be used with any transport layer, including the RTU handler above.

**Integration**:  
You can use the RTU frame handler to connect your UART and timer logic, and then pass complete frames to the Modbus server core for protocol processing. This separation allows for flexible adaptation to different hardware and application requirements.

**Key Concepts:**
- Platform independence: All hardware-specific logic (UART, timer, register access) is provided by the user via callback interfaces.
- No dynamic memory allocation: All buffers are statically allocated for deterministic behavior.
- Single-instance: Only one Modbus server/RTU handler is supported per application.
- MIT licensed for use in commercial and open-source projects.

**Typical Usage Flow:**
1. Implement the required callback interfaces for your platform:
   - For RTU: `smb_rtu_if_t` (UART write, timer start, frame received callback)
   - For server: `smb_server_if_t` (register access)
2. Set up the server to use the RTU handler for frame read and write operations.
3. Configure the RTU handler with your server address, baud rate, and interface.
4. Use `smb_server_poll()` periodically to process requests and send responses.

**For more details, see the documentation in `simple_modbus.h` and `simple_modbus_rtu.h`.**


## Limitations

- Only one server/RTU instance per application
- User must implement UART, timer, and register access callbacks
- Not thread-safe nor interrupt-safe; must be called from a single thread or context
	- No re-entrancy
    - This can be achieved by disabling interrupts or using a mutex/semaphore in combination with thread flags.
- No built-in support for advanced Modbus features (e.g., multi-drop, advanced diagnostics)
- No Modbus ASCII or TCP support.

## How to Run the Tests

1. Open a terminal and navigate to the project root directory.
2. Create a build directory and run CMake to generate the build files:    
```bash
mkdir test/build
cmake -S test/ -B test/build
cmake --build test/build
   ```
3. Then run the tests (location depends on platform)
Example Windows:
```bash
./test/build/Debug/tests.exe
```


## Usage Example

See examples directory:

| Project | Hardware | Description |
|-------- | ---------| ----------- |
| [smb-stm32-superloop](examples/stm32/smb-stm32-superloop) | STM32C0 | Bare-metal example with only a timer interrupt |
| [smb-stm32-rtos](examples/stm32/smb-stm32-rtos)   | STM32C0 | RTOS example with thread flags, tasks, and locking mechanisms |


## License  
This project is licensed under the MIT License. See the LICENSE file for details.
