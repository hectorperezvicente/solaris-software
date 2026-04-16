# Solaris Software — CLAUDE.md

## Project Overview

**Solaris** is an ESP32-S3 embedded firmware project for environmental sensing (pressure, IMU, altitude). It is built on top of a custom lightweight protocol stack called **SPP (Solaris Packet Protocol)**. `solaris-v1` is the current active version.

Target: ESP32-S3 via ESP-IDF (managed by the devcontainer toolchain).
Build system: CMake (ESP-IDF component model).
RTOS: FreeRTOS.

---

## Developer Tooling

### Terminal commands (devcontainer)

| Command | Description |
|---------|-------------|
| `help` | Show all available commands |
| `build` / `flash` / `monitor` / `fullflash` | ESP-IDF shortcuts |
| `goto <dest>` | Navigate to key directories (`spp`, `ports`, `services`, `compiler`, `tests`, `docs`) |
| `run_tests <path>` | cmake configure + build + ctest (Cgreen unit tests) |
| `template [type]` | Print Doxygen templates. Types: `h`, `c`, `fn`, `struct`, `enum`, `macro`. No argument = all. |

### Claude Code skills

| Command | Description |
|---------|-------------|
| `/edit <file>` | Apply Doxygen formatting, naming convention checks, and section dividers to a `.c` or `.h` file. Changes are applied directly. |

---

## Repository Layout

```
solaris-software/
├── solaris-v1/                  # Active firmware version
│   ├── main/                    # Application entry point
│   ├── spp/                     # SPP core library (git submodule — do not edit directly)
│   │   ├── include/spp/         # Platform-agnostic public headers
│   │   ├── src/                 # Platform-agnostic implementations
│   │   ├── ports/               # Platform ports (freertos, posix, baremetal, esp32 HAL)
│   │   ├── services/            # Sensor/telemetry services (bmp390, icm20948, datalogger)
│   │   └── tests/unit/          # Cgreen unit tests (run on host via posix port)
│   └── compiler/                # ESP-IDF component wrappers for the spp submodule
│       ├── spp/                 # Wraps spp core + services as ESP-IDF component
│       └── spp_ports/           # Wraps spp ports as ESP-IDF component
├── archive/                     # Legacy firmware (solaris-v0.1, solaris-v0.2 — reference only)
├── docs/                        # Architecture documentation
├── website/                     # Static site (nginx)
└── scripts/                     # Setup scripts (Linux, Windows)
```

---

## SPP (Solaris Packet Protocol) — Core Library

Location: `solaris-v1/spp/`
Reference: modeled after ECSS Space Packet Protocol.

### SPP Architecture Layers

```
Application (main, components)
    ↓
services/ (databank, db_flow, logging)
    ↓
core/ (packet, types, returntypes, macros)
    ↓
osal/ (task, queue, mutex, eventgroups)     ← platform-agnostic interfaces
hal/  (spi, gpio, storage)                  ← platform-agnostic interfaces
```

### core/

**`core/types.h`** — Portable integer types and SPI config:
- Portable types: `spp_uint8_t`, `spp_int8_t`, `spp_uint16_t`, `spp_int16_t`, `spp_uint32_t`, `spp_int32_t`, `spp_uint64_t`, `spp_int64_t`
- SPI config: `SPP_SPI_InitCfg` struct (bus_id, pins, max_hz, mode, duplex, queue_size)
- Enums: `spp_spi_mode_t` (MODE0/1/3), `spp_spi_duplex_t` (FULL/HALF)

**`core/returntypes.h`** — All SPP functions return `retval_t`:
```c
typedef enum {
    SPP_OK,
    SPP_ERROR,
    SPP_NOT_ENOUGH_PACKETS,
    SPP_NULL_PACKET,
    SPP_ERROR_ALREADY_INITIALIZED,
    SPP_ERROR_NULL_POINTER,
    SPP_ERROR_NOT_INITIALIZED,
    SPP_ERROR_INVALID_PARAMETER,
    SPP_ERROR_ON_SPI_TRANSACTION
} retval_t;
```

**`core/packet.h`** — SPP packet structure:
```c
typedef struct {
    spp_uint8_t  version;       // Protocol version (= 1)
    spp_uint16_t apid;          // Application Process ID
    spp_uint16_t seq;           // Sequence number
    spp_uint16_t payload_len;   // Payload length in bytes
} spp_packet_primary_t;

typedef struct {
    spp_uint32_t timestamp_ms;
    spp_uint8_t  drop_counter;
} spp_packet_secondary_t;

typedef struct {
    spp_packet_primary_t   primary_header;
    spp_packet_secondary_t secondary_header;
    spp_uint8_t            payload[SPP_PKT_PAYLOAD_MAX]; // 48 bytes max
    spp_uint16_t           crc;
} spp_packet_t;
```
Constants: `SPP_PKT_VERSION = 1`, `SPP_PKT_PAYLOAD_MAX = 48`.

**`core/macros.h`** — Compile-time config:
- `DATA_BANK_SIZE = 5` (packet pool size)
- `STATIC` defined → uses static FreeRTOS allocation
- `STACK_SIZE = 8192`

**`core/core.h`** / **`core/core.c`**:
- `retval_t Core_Init(void)` — initializes SPP_LOG and SPP_DATABANK, sets log level to VERBOSE.

### osal/ (OS Abstraction Layer)

Platform-agnostic interfaces. Implementations live in `spp/ports/`.

**`osal/task.h`**:
- `void *SPP_OSAL_GetTaskStorage()` — gets slot from static task pool
- `void *SPP_OSAL_TaskCreate(void *fn, const char *name, uint32_t stack, void *data, spp_uint32_t prio, void *storage)`
- `retval_t SPP_OSAL_TaskDelete(void *task)` — NULL deletes self
- `void SPP_OSAL_TaskDelay(spp_uint32_t ms)`

**`osal/osal.h`** — Full OSAL API (tasks, mutex, queue, semaphore, tick conversion):
```c
typedef enum {
    OSAL_PRIORITY_IDLE = 0,
    OSAL_PRIORITY_LOW = 1,
    OSAL_PRIORITY_NORMAL = 2,
    OSAL_PRIORITY_HIGH = 3,
    OSAL_PRIORITY_CRITICAL = 4
} osal_priority_t;
```

**`osal/queue.h`**:
- `void *SPP_OSAL_QueueCreate(uint32_t length, uint32_t item_size)`
- `void *SPP_OSAL_QueueCreateStatic(uint32_t length, uint32_t item_size, uint8_t *storage, void *buffer)`
- `retval_t SPP_OSAL_QueueSend(void *queue, const void *item, uint32_t timeout_ms)`
- `retval_t SPP_OSAL_QueueReceive(void *queue, void *item, uint32_t timeout_ms)`
- `uint32_t SPP_OSAL_QueueMessagesWaiting(void *queue)`

**`osal/eventgroups.h`**:
- `void *SPP_OSAL_GetEventGroupsBuffer()`
- `void *SPP_OSAL_EventGroupCreate(void *buffer)`
- `retval_t OSAL_EventGroupWaitBits(void *eg, bits, clear_on_exit, wait_all, timeout_ms, *actual)`
- `retval_t OSAL_EventGroupSetBitsFromISR(void *eg, bits, *prev, *higher_task_woken)`

### hal/ (Hardware Abstraction Layer)

**`hal/spi/spi.h`**:
- `retval_t SPP_HAL_SPI_BusInit(void)` — safe to call multiple times
- `void *SPP_HAL_SPI_GetHandler(void)` — index 0 = ICM20948, index 1 = BMP390
- `retval_t SPP_HAL_SPI_DeviceInit(void *handler)`
- `retval_t SPP_HAL_SPI_Transmit(void *handler, spp_uint8_t *data, spp_uint8_t len)`

**`hal/gpio/gpio.h`**:
```c
typedef struct {
    void            *p_event_group;
    osal_eventbits_t bits;
} spp_gpio_isr_ctx_t;
```
- `retval_t SPP_HAL_GPIO_ConfigInterrupt(uint32_t pin, uint32_t intr_type, uint32_t pull)` (pull: 0=none, 1=up, 2=down)
- `retval_t SPP_HAL_GPIO_RegisterISR(uint32_t pin, void *isr_context)`

**`hal/storage/storage.h`**:
```c
typedef struct {
    const char   *p_base_path;
    int           spi_host_id;
    int           pin_cs;
    spp_uint32_t  max_files;
    spp_uint32_t  allocation_unit_size;
    spp_bool_t    format_if_mount_failed;
} SPP_Storage_InitCfg;
```
- `retval_t SPP_HAL_Storage_Mount(void *cfg)`
- `retval_t SPP_HAL_Storage_Unmount(void *cfg)`

### services/

**`services/databank/databank.h`** — Static packet pool (5 packets):
```c
typedef struct {
    spp_packet_t *freePackets[DATA_BANK_SIZE];
    uint32_t      numberOfFreePackets;
} SPP_Databank_t;
```
- `retval_t SPP_DATABANK_init(void)` — safe to call multiple times
- `spp_packet_t *SPP_DATABANK_getPacket(void)` — NULL if exhausted
- `retval_t SPP_DATABANK_returnPacket(spp_packet_t *pkt)` — validates pointer

**`services/db_flow/db_flow.h`** — Circular FIFO (`DB_FLOW_READY_SIZE = 16`) routing packets:
- `retval_t DB_FLOW_Init(void)`
- `retval_t DB_FLOW_PushReady(spp_packet_t *pkt)`
- `retval_t DB_FLOW_PopReady(spp_packet_t **pkt)`
- `uint32_t DB_FLOW_ReadyCount(void)`

**`services/logging/spp_log.h`** — Filtered logging:
```c
typedef enum { SPP_LOG_NONE, SPP_LOG_ERROR, SPP_LOG_WARN,
               SPP_LOG_INFO, SPP_LOG_DEBUG, SPP_LOG_VERBOSE } spp_log_level_t;
```
Macros: `SPP_LOGE`, `SPP_LOGW`, `SPP_LOGI`, `SPP_LOGD`, `SPP_LOGV(tag, fmt, ...)`
- `retval_t SPP_LOG_Init(void)`
- `void SPP_LOG_SetLevel(spp_log_level_t level)`
- `void SPP_LOG_RegisterOutputCallback(spp_log_output_fn_t cb)`

---

## SPP-Ports — Platform Implementations

Location: `solaris-v1/spp/ports/`
Implements the HAL and OSAL interfaces for **ESP32 + FreeRTOS**.

### osal/freertos/

**`task.c`**:
- Static pool: `s_taskPool[50]`, each with 4096-word stack and `StaticTask_t` buffer
- `SPP_OSAL_TaskCreate()` → `xTaskCreateStatic()`
- `SPP_OSAL_TaskDelay()` → `vTaskDelay(pdMS_TO_TICKS(ms))`

**`queue.c`**:
- Dynamic: `xQueueCreate()`, static: `xQueueCreateStatic()`
- ms-to-ticks conversion ensures non-zero timeouts

**`eventgroups.c`**:
- Static pool: `s_eventGroupBuffers[5]`
- `xEventGroupCreateStatic()` or `xEventGroupCreate()` based on `STATIC` macro
- ISR variant: `xEventGroupSetBitsFromISR()` with context switch yield

**`macros_freertos.h`**: `NUM_EVENT_GROUPS = 5`

### hal/esp32/

**`spi_esp32.c`** — SPI bus on SPI2_HOST:
- Pins: MISO=47, MOSI=38, CLK=48
- CS_PIN_ICM=21 @ 1 MHz MODE0, CS_PIN_BMP=18 @ 500 kHz MODE0
- CS_PIN_SDC=8 (SD card)
- DMA: `SPI_DMA_CH_AUTO`, max transfer: unlimited
- Read detected by MSB=1 of first byte

**`gpio.c`** — GPIO interrupts:
- Installs ISR service on first call
- Internal ISR sets event group bits + optional yield

**`storage.c`** — SD card via `esp_vfs_fat_sdspi_mount()`:
- Checks `s_mounted` flag to prevent double-mount

**`macros_esp.h`**: SPI2_HOST, all pin definitions, MAX_DEVICES=4

---

## ESP-IDF Component Wrappers

Location: `solaris-v1/compiler/`

### `compiler/spp/CMakeLists.txt`
- Wraps `spp/` core + opt-in services as an ESP-IDF component
- Flags: `-DSPP_SERVICE_BMP390=ON/OFF`, `-DSPP_SERVICE_ICM20948=ON/OFF`, `-DSPP_SERVICE_DATALOGGER=ON/OFF`
- Exposes SPP headers to dependent components

### `compiler/spp_ports/CMakeLists.txt`
- Wraps `spp/ports/` as an ESP-IDF component
- Selects OSAL port (`SPP_OSAL_FREERTOS` or `SPP_OSAL_BAREMETAL`) and HAL port (`SPP_HAL_ESP32` or `SPP_HAL_ESP32_BM`)
- Depends on: `spp`, `esp_driver_spi`, `esp_driver_gpio`, `esp_driver_sdspi`, `fatfs`, `freertos`

---

## Device Drivers (`components/`)

### `pressureSensorDriver` — BMP390

**`include/bmp390.h`** device context:
```c
typedef struct {
    void            *p_handler_spi;
    void            *p_event_group;
    spp_gpio_isr_ctx_t isr_ctx;
    spp_uint32_t     intPin;
    spp_uint32_t     intIntrType;
    spp_uint32_t     intPull;
} BMP390_Data_t;
```

Key functions:
- `void BMP390_init(void *data)` — init, event group, GPIO interrupt
- `retval_t BMP390_auxConfig(void *spi)` — reset + enable SPI + verify chip ID (0x60)
- `retval_t BMP390_prepareMeasure(void *spi)` — set OSR, ODR=50Hz, IIR, power mode normal
- `retval_t BMP390_waitDrdy(BMP390_Data_t *bmp, uint32_t timeout_ms)` — block on DRDY interrupt
- `retval_t BMP390_calibrate_temp_params(void *spi, BMP390_temp_params_t *out)`
- `retval_t BMP390_calibrate_press_params(void *spi, BMP390_press_params_t *out)`
- `float BMP390_compensate_temperature(uint32_t raw, BMP390_temp_params_t *p)`
- `float BMP390_compensate_pressure(uint32_t raw, float t_lin, BMP390_press_params_t *p)`
- `retval_t BMP390_getAltitude(void *spi, BMP390_Data_t *bmp, float *alt_m, float *press_pa, float *temp_c)`
- `retval_t BMP390_intEnableDrdy(void *spi)`

Registers: chip ID=0x00 (expect 0x60), soft reset=0x7E (cmd 0xB6), power ctrl=0x1B (0x33), OSR=0x1C (0x00), ODR=0x1D (0x02), IIR=0x1F (0x02).

### `icm_driver` — ICM20948

IMU driver (accel + gyro). 715-line header at `include/icm20948.h`.

Key constants:
- Task priorities: CONFIG=5, READ_SENSORS=4
- SPI: SPI2_HOST, CS=GPIO 21, CIPO=47, COPI=38, CLK=48
- Register banks 0–3 selected via register 0x7F
- DMP (Digital Motion Processor) support

### `datalogger_driver` — SD Card Logger

```c
typedef struct {
    void    *p_storage_cfg;
    FILE    *p_file;
    uint8_t  is_initialized;
    uint32_t logged_packets;
} Datalogger_t;
```
- `retval_t DATALOGGER_Init(Datalogger_t *, void *storage_cfg, const char *path)`
- `retval_t DATALOGGER_LogPacket(Datalogger_t *, const spp_packet_t *)`
- `retval_t DATALOGGER_Flush(Datalogger_t *)`
- `retval_t DATALOGGER_Deinit(Datalogger_t *)`

### `general` — Legacy helpers (v0.x)

Contains legacy direct ESP-IDF SPI struct (`data_t`), GPIO interrupt helpers. Not used by spp-based code.

---

## Main Application (`solaris-v1/main/`)

### `main.c` — Initialization sequence:
1. `Core_Init()` — logging + databank
2. `SPP_DATABANK_init()`
3. `DB_FLOW_Init()`
4. `SPP_HAL_SPI_BusInit()`
5. `SPP_HAL_SPI_GetHandler()` + `SPP_HAL_SPI_DeviceInit()` × 2 (ICM first, BMP second)
6. `BMP_ServiceInit(p_spi_bmp)` + `BMP_ServiceStart()`
7. Idle loop: `SPP_OSAL_TaskDelay(1000)` forever

### `bmpService.c` — BMP390 service task:
- APID: `0x0101`
- Task priority: 5, stack: 4096
- Measurement loop (200 ms period):
  1. Wait for DRDY interrupt (5 sec timeout)
  2. `SPP_DATABANK_getPacket()`
  3. `BMP390_getAltitude()` → altitude, pressure, temperature
  4. Fill `spp_packet_t` (version, APID, seq, timestamp, 12-byte payload: 3× float)
  5. `DB_FLOW_PushReady()` → FIFO
  6. `DB_FLOW_PopReady()` → `DATALOGGER_LogPacket()` to `/sdcard/log.txt`
  7. `SPP_DATABANK_returnPacket()`
- Logs max 10 packets then stops logging (continues measuring).
- SD config: base_path=`/sdcard`, pin_cs=8 (CS_PIN_SDC), max_files=5, alloc=16KB.

---

## Hardware Pin Map (ESP32-S3)

| Signal   | GPIO |
|----------|------|
| SPI MISO | 47   |
| SPI MOSI | 38   |
| SPI CLK  | 48   |
| CS ICM20948 | 21 |
| CS BMP390   | 18 |
| CS SD card  | 8  |
| IMU INT pin | (configured in icm_driver) |
| BMP INT pin | (configured at BMP390_init) |

---

## Reference: lley-core (Lely Industries) — Architectural Inspiration

Location: `/home/user/Documents/lley-core`
Purpose: Industrial C/C++ library for CANopen and async I/O. Studied as reference architecture for designing `spp/`.

### Key Patterns in lley-core to Apply to SPP

#### 1. OOP-in-C via vtables
```c
typedef const struct io_dev_vtbl *const io_dev_t;
struct io_dev_vtbl {
    io_ctx_t *(*get_ctx)(const io_dev_t *dev);
    ev_exec_t *(*get_exec)(const io_dev_t *dev);
    size_t (*cancel)(io_dev_t *dev, struct ev_task *task);
};
// Dispatch via: (*dev)->get_ctx(dev)
```
SPP equivalent: HAL and OSAL already follow this (function pointer tables per platform).

#### 2. container-of / structof pattern
```c
#define structof(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
```
Allows embedding nodes inside structs and recovering the parent. Avoids separate allocations.

#### 3. Init/Fini allocation split
```c
// Stack or static allocation:
struct __co_dev dev;
__co_dev_init(&dev, id);

// Heap allocation (optional, gated by LELY_NO_MALLOC):
co_dev_t *dev = co_dev_create(id);
co_dev_destroy(dev);
```
SPP already uses static allocation via `STATIC` macro.

#### 4. Red-black trees for ordered lookups
CANopen object dictionary uses `rbtree` (O(log n) lookup by 16-bit index). Useful for SPP if we need indexed registries.

#### 5. Compile-time feature flags
```c
#if !LELY_NO_MALLOC   // heap optional
#if !LELY_NO_CO_OBJ_NAME  // string names optional
```
Pattern already present in SPP via `STATIC` macro. Expand for bare-metal targets.

#### 6. Error codes via thread-local storage
```c
set_errnum(ERRNUM_INVAL);   // set
int errc = get_errc();       // retrieve
```
SPP uses `retval_t` return values. lley-core approach adds TLS-based context for richer errors.

#### 7. Callback registration pattern
```c
typedef co_unsigned32_t co_sub_dn_ind_t(co_sub_t *sub, struct co_sdo_req *req, void *data);
int co_sub_set_dn_ind(co_sub_t *sub, co_sub_dn_ind_t *ind, void *data);
```
SPP logging already has `SPP_LOG_RegisterOutputCallback`. Extend to other services.

#### 8. Module dependency order
```
libc → util → can → co (protocol) → io2 (async I/O) → ev (event loop)
```
SPP equivalent:
```
types/returntypes → core → osal/hal interfaces → services → ports → components → app
```

### lley-core Module Summary

| Module | Purpose |
|--------|---------|
| `libc` | C11/POSIX compatibility |
| `util` | rbtree, dllist, sllist, errnum, membuf |
| `can` | CAN frame structures and network interface |
| `co` | CANopen protocol (NMT, SDO, PDO, SYNC, EMCY) |
| `io2` | Async I/O (vtable-based devices, timers, CAN I/O) |
| `ev` | Event execution loop (poll, dispatch, post, defer) |
| `coapp` | C++ application framework on top of co |

Build system: Autotools (`configure.ac` + `Makefile.am`) with `--disable-malloc`, `--disable-threads`, etc.

---

---

## Coding Conventions (derived from icm20948.h — apply to ALL SPP code)

### Naming rules

| Element | Convention | Example |
|---------|-----------|---------|
| Constants / `#define` | `K_MODULE_NAME` | `K_SPP_PKT_PAYLOAD_MAX` |
| Enum values | `K_MODULE_ENUM_VALUE` | `K_SPP_LOG_VERBOSE`, `K_SPP_SPI_MODE0` |
| Return-code values (`retval_t`) | `SPP_OK`, `SPP_ERROR` | (exception: shared cross-module, no K_) |
| Portable base types | lowercase snake | `spp_uint8_t`, `spp_bool_t` |
| Struct / Union / Enum types | `MODULE_TypeName_t` | `SPP_Packet_t`, `SPP_OsalPort_t` |
| Public functions | `MODULE_functionName()` camelCase | `SPP_Databank_init()`, `ICM20948_configDmp()` |
| Pointer parameters | `p_name` prefix | `p_data`, `p_cfg`, `p_handler` |
| Bitfield members | camelCase | `bankSel`, `dmpEn`, `fifoRst0` |
| Local / private variables | camelCase | `localCount`, `rxBuffer` |
| Static module-level vars | `s_name` prefix | `s_taskPool`, `s_initialized` |

### Migration map (old SPP names → new)

| Old | New |
|-----|-----|
| `spp_packet_t` | `SPP_Packet_t` |
| `spp_packet_primary_t` | `SPP_PacketPrimary_t` |
| `spp_packet_secondary_t` | `SPP_PacketSecondary_t` |
| `spp_spi_mode_t` | `SPP_SpiMode_t` |
| `spp_spi_duplex_t` | `SPP_SpiDuplex_t` |
| `SPP_SPI_InitCfg` | `SPP_SpiInitCfg_t` |
| `spp_gpio_isr_ctx_t` | `SPP_GpioIsrCtx_t` |
| `SPP_Storage_InitCfg` | `SPP_StorageInitCfg_t` |
| `spp_log_level_t` | `SPP_LogLevel_t` |
| `SPP_LOG_NONE/ERROR/...` | `K_SPP_LOG_NONE/ERROR/...` |
| `spp_spi_mode_t` values | `K_SPP_SPI_MODE0/1/3` |
| `SPP_DATABANK_init()` | `SPP_Databank_init()` |
| `SPP_DATABANK_getPacket()` | `SPP_Databank_getPacket()` |
| `SPP_DATABANK_returnPacket()` | `SPP_Databank_returnPacket()` |
| `DB_FLOW_Init()` | `SPP_DbFlow_init()` |
| `DB_FLOW_PushReady()` | `SPP_DbFlow_pushReady()` |
| `DB_FLOW_PopReady()` | `SPP_DbFlow_popReady()` |
| `DB_FLOW_ReadyCount()` | `SPP_DbFlow_readyCount()` |
| `SPP_LOG_Init()` | `SPP_Log_init()` |
| `SPP_LOG_SetLevel()` | `SPP_Log_setLevel()` |
| `Core_Init()` | `SPP_Core_init()` |
| `DATA_BANK_SIZE` | `K_SPP_DATABANK_SIZE` |
| `DB_FLOW_READY_SIZE` | `K_SPP_DBFLOW_READY_SIZE` |
| `SPP_PKT_PAYLOAD_MAX` | `K_SPP_PKT_PAYLOAD_MAX` |
| `SPP_PKT_VERSION` | `K_SPP_PKT_VERSION` |
| `STACK_SIZE` | `K_SPP_STACK_SIZE` |
| `SPP_OSAL_TaskCreate()` | `SPP_Osal_taskCreate()` |
| `SPP_OSAL_TaskDelay()` | `SPP_Osal_taskDelayMs()` |
| `SPP_OSAL_QueueCreate()` | `SPP_Osal_queueCreate()` |
| `SPP_OSAL_QueueSend()` | `SPP_Osal_queueSend()` |
| `SPP_OSAL_QueueReceive()` | `SPP_Osal_queueReceive()` |
| `OSAL_EventGroupWaitBits()` | `SPP_Osal_eventWait()` |
| `OSAL_EventGroupSetBitsFromISR()` | `SPP_Osal_eventSetFromIsr()` |
| `SPP_HAL_SPI_BusInit()` | `SPP_Hal_spiBusInit()` |
| `SPP_HAL_SPI_GetHandler()` | `SPP_Hal_spiGetHandle()` |
| `SPP_HAL_SPI_Transmit()` | `SPP_Hal_spiTransmit()` |
| `SPP_HAL_GPIO_ConfigInterrupt()` | `SPP_Hal_gpioConfigInterrupt()` |
| `SPP_HAL_GPIO_RegisterISR()` | `SPP_Hal_gpioRegisterIsr()` |
| `SPP_HAL_Storage_Mount()` | `SPP_Hal_storageMount()` |

### Doxygen style (mandatory on all public symbols)

```c
/**
 * @file module.h
 * @brief One-line description.
 *
 * Extended description if needed.
 *
 * Naming conventions used in this file:
 * - Global constants/macros: K_SPP_MODULE_*
 * - Types: SPP_Module*_t
 * - Public functions: SPP_Module_functionName()
 * - Pointer variables/parameters: p_pointerName
 */

/**
 * @brief Short description of the symbol.
 *
 * @param[in]     p_cfg  Pointer to configuration struct.
 * @param[in,out] p_ctx  Pointer to context (modified on init).
 *
 * @return K_SPP_OK on success, or an error code otherwise.
 */
```

### Section dividers (mandatory in headers)

```c
/* ----------------------------------------------------------------
 * Section Name
 * ---------------------------------------------------------------- */
```

---

## SPP v2 Architecture

### Final directory structure

```
spp/                              ← single repo, self-contained
├── CMakeLists.txt                ← standalone CMake (no ESP-IDF dependency)
├── include/spp/
│   ├── core/
│   │   ├── packet.h              ← SPP_Packet_t, SPP_PacketPrimary_t, SPP_PacketSecondary_t
│   │   ├── types.h               ← spp_uint8_t, spp_bool_t, SPP_SpiInitCfg_t, SPP_SpiMode_t
│   │   ├── returntypes.h         ← retval_t  { SPP_OK, SPP_ERROR, ... }
│   │   └── version.h             ← K_SPP_VERSION_MAJOR/MINOR/PATCH
│   ├── osal/
│   │   ├── port.h                ← SPP_OsalPort_t  ← THE CONTRACT
│   │   ├── task.h                ← SPP_Osal_taskCreate() etc (dispatch to port)
│   │   ├── queue.h               ← SPP_Osal_queueCreate() etc
│   │   ├── mutex.h               ← SPP_Osal_mutexCreate() etc
│   │   └── event.h               ← SPP_Osal_eventCreate() etc
│   ├── hal/
│   │   ├── port.h                ← SPP_HalPort_t   ← THE CONTRACT
│   │   ├── spi.h                 ← SPP_Hal_spiBusInit() etc
│   │   ├── gpio.h                ← SPP_Hal_gpioConfigInterrupt() etc
│   │   └── storage.h             ← SPP_Hal_storageMount() etc
│   ├── services/
│   │   ├── service.h             ← SPP_ServiceDesc_t + SPP_Service_register()
│   │   ├── databank.h            ← SPP_Databank_init/getPacket/returnPacket
│   │   ├── db_flow.h             ← SPP_DbFlow_init/pushReady/popReady
│   │   └── log.h                 ← SPP_Log_init/setLevel + SPP_LOG* macros
│   └── util/
│       ├── macros.h              ← K_SPP_NO_RTOS, K_SPP_NO_MALLOC, K_SPP_STACK_SIZE
│       ├── crc.h                 ← SPP_Util_crc16()
│       └── structof.h            ← SPP_STRUCTOF() macro (lley-core pattern)
├── src/
│   ├── core/core.c               ← SPP_Core_init()
│   ├── osal/osal_dispatch.c      ← calls through registered SPP_OsalPort_t
│   ├── hal/hal_dispatch.c        ← calls through registered SPP_HalPort_t
│   ├── services/
│   │   ├── databank/databank.c
│   │   ├── db_flow/db_flow.c
│   │   └── log/log.c
│   └── util/crc.c
├── services/                     ← optional sensor/telemetry services
│   ├── bmp390/
│   │   ├── include/spp/services/bmp390.h
│   │   ├── src/bmp390_service.c
│   │   └── CMakeLists.txt
│   ├── icm20948/
│   │   ├── include/spp/services/icm20948_service.h
│   │   ├── src/icm20948_service.c
│   │   └── CMakeLists.txt
│   └── datalogger/
│       ├── include/spp/services/datalogger.h
│       ├── src/datalogger_service.c
│       └── CMakeLists.txt
├── ports/
│   ├── freertos/
│   │   ├── osal/osal_freertos.c
│   │   └── CMakeLists.txt
│   ├── posix/                    ← NEW: for Cgreen tests on host
│   │   ├── osal/osal_posix.c
│   │   ├── hal/hal_stub.c
│   │   └── CMakeLists.txt
│   └── baremetal/                ← NEW: cooperative scheduler, no OS
│       ├── osal/osal_baremetal.c
│       └── CMakeLists.txt
└── tests/
    ├── CMakeLists.txt
    └── unit/
        ├── test_packet.c         ← Cgreen BDD
        ├── test_databank.c
        ├── test_db_flow.c
        └── test_log.c
```

### Port contract (what a porter implements — the whole porting effort)

```c
// SPP_OsalPort_t — OS abstraction
typedef struct {
    void    *(*taskCreate)(void (*p_fn)(void *), const char *p_name,
                           spp_uint32_t stackWords, void *p_arg, spp_uint32_t prio);
    void     (*taskDelete)(void *p_handle);
    void     (*taskDelayMs)(spp_uint32_t ms);
    spp_uint32_t (*getTickMs)(void);
    void    *(*queueCreate)(spp_uint32_t len, spp_uint32_t itemSize);
    retval_t (*queueSend)(void *p_q, const void *p_item, spp_uint32_t timeoutMs);
    retval_t (*queueRecv)(void *p_q, void *p_item, spp_uint32_t timeoutMs);
    spp_uint32_t (*queueCount)(void *p_q);
    void    *(*mutexCreate)(void);
    retval_t (*mutexLock)(void *p_m, spp_uint32_t timeoutMs);
    retval_t (*mutexUnlock)(void *p_m);
    void    *(*eventCreate)(void);
    retval_t (*eventWait)(void *p_e, spp_uint32_t bits, spp_bool_t clear,
                          spp_uint32_t timeoutMs, spp_uint32_t *p_actualBits);
    retval_t (*eventSetFromIsr)(void *p_e, spp_uint32_t bits,
                                spp_uint32_t *p_prev, spp_bool_t *p_yield);
} SPP_OsalPort_t;

// SPP_HalPort_t — hardware abstraction
typedef struct {
    retval_t (*spiBusInit)(void);
    void    *(*spiGetHandle)(spp_uint8_t deviceIdx);
    retval_t (*spiDeviceInit)(void *p_handle);
    retval_t (*spiTransmit)(void *p_handle, spp_uint8_t *p_data, spp_uint8_t len);
    retval_t (*gpioConfigInterrupt)(spp_uint32_t pin, spp_uint32_t intrType, spp_uint32_t pull);
    retval_t (*gpioRegisterIsr)(spp_uint32_t pin, void *p_isrCtx);
    retval_t (*storageMount)(void *p_cfg);
    retval_t (*storageUnmount)(void *p_cfg);
    spp_uint32_t (*getTimeMs)(void);
} SPP_HalPort_t;
```

One call at startup registers both:
```c
SPP_Core_setOsalPort(&g_freertosOsalPort);
SPP_Core_setHalPort(&g_esp32HalPort);
SPP_Core_init();
```

### Service descriptor (how to add a new service)

```c
typedef struct {
    const char    *p_name;
    spp_uint16_t   apid;
    size_t         ctxSize;
    retval_t (*init)(void *p_ctx, const void *p_cfg);
    retval_t (*start)(void *p_ctx);
    retval_t (*stop)(void *p_ctx);
    retval_t (*deinit)(void *p_ctx);
} SPP_ServiceDesc_t;

// Register:
static BMP390_ServiceCtx_t  s_bmpCtx;
static BMP390_ServiceCfg_t  s_bmpCfg = { .csPin = 18U, .intPin = 5U };
SPP_Service_register(&g_bmp390ServiceDesc, &s_bmpCtx, &s_bmpCfg);
SPP_Service_startAll();
```

### Porting in 2-3 days
- Day 1: implement `SPP_OsalPort_t` for your OS → run `tests/unit/` on host
- Day 2: implement `SPP_HalPort_t` for your MCU (SPI, GPIO, timers)
- Day 3: register services, flash, verify

### Key differences vs lley-core
- **Port = global struct registered once** (lley-core: per-object vtable). Simpler for single-MCU targets.
- **Task-based bare-metal** via cooperative scheduler (lley-core: pure event loop). More familiar for FreeRTOS teams.
- **Service registry** (lley-core: explicit init). More extensible for plug-in services.
- Same: single repo, static-first, compile flags, layering, structof macro.

---

## Design Goals for `spp/` (Future Work)

Key objectives for the SPP library (architecture already implemented in v2):

1. **Platform-agnostic core** — no ESP-IDF or FreeRTOS in `spp/` at all (achieved)
2. **Port abstraction** — `spp/ports/` provides concrete implementations (achieved)
3. **Packet pool** — static databank of fixed-size packets (achieved, `K_SPP_DATABANK_SIZE=5`)
4. **Service registry** — producers fill packets, consumers read via db_flow FIFO (partial)
5. **Potential improvements inspired by lley-core**:
   - vtable-based service registration (like `io_dev_vtbl`)
   - Richer error context (TLS or thread-local error codes alongside `retval_t`)
   - `structof` pattern for zero-overhead embedded nodes
   - Feature flag system for bare-metal vs. RTOS builds
   - Separate alloc/init functions for static vs. heap deployment
