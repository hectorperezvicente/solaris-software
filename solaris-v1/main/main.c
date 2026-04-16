/**
 * @file main.c
 * @brief ICM20948 data readout test — prints accel, gyro, mag and quaternion
 *        to the serial monitor via SPP_LOGI.
 *
 * Boot sequence:
 *   1. Register baremetal OSAL + ESP32 polling HAL ports
 *   2. SPP_Core_init()
 *   3. SPI bus + ICM20948 device init
 *   4. Register and start ICM20948 service (runs DMP init)
 *   5. Poll ICM20948_checkFifoData() in a loop — data is printed inside
 */

#include "spp/core/core.h"
#include "spp/core/returntypes.h"
#include "spp/hal/spi.h"
#include "spp/osal/task.h"
#include "spp/osal/port.h"
#include "spp/hal/port.h"
#include "spp/services/log/log.h"
#include "spp/services/service.h"
#include "spp/services/icm20948/icm20948.h"

#include "macros_esp32.h"

/* ----------------------------------------------------------------
 * External port objects (defined in spp_port_wrapper)
 * ---------------------------------------------------------------- */

extern const SPP_OsalPort_t g_baremetalOsalPort;
extern const SPP_HalPort_t  g_esp32BaremetalHalPort;

/* ----------------------------------------------------------------
 * Private constants
 * ---------------------------------------------------------------- */

static const char *TAG = "MAIN";

/** @brief Poll interval for ICM20948 FIFO readout (milliseconds). */
#define K_MAIN_POLL_INTERVAL_MS (50U)

/* ----------------------------------------------------------------
 * Service instances (static allocation)
 * ---------------------------------------------------------------- */

static ICM20948_ServiceCtx_t s_icmCtx;

static const ICM20948_ServiceCfg_t s_icmCfg =
{
    .spiDevIdx = K_ESP32_SPI_IDX_ICM,
};

/* ----------------------------------------------------------------
 * app_main
 * ---------------------------------------------------------------- */

void app_main(void)
{
    SPP_RetVal_t ret;

    /* --- Port registration ---------------------------------------- */
    ret = SPP_Core_setOsalPort(&g_baremetalOsalPort);
    if (ret != K_SPP_OK) { for (;;) {} }

    ret = SPP_Core_setHalPort(&g_esp32BaremetalHalPort);
    if (ret != K_SPP_OK) { for (;;) {} }

    /* --- Core init ------------------------------------------------- */
    ret = SPP_Core_init();
    if (ret != K_SPP_OK) { for (;;) {} }

    SPP_LOGI(TAG, "ICM20948 test — boot");

    /* --- SPI bus + ICM20948 device --------------------------------- */
    ret = SPP_HAL_spiBusInit();
    if (ret != K_SPP_OK)
    {
        SPP_LOGE(TAG, "SPI bus init failed");
        for (;;) {}
    }

    void *p_spiIcm = SPP_HAL_spiGetHandle(K_ESP32_SPI_IDX_ICM);
    ret = SPP_HAL_spiDeviceInit(p_spiIcm);
    if (ret != K_SPP_OK)
    {
        SPP_LOGE(TAG, "SPI ICM20948 device init failed");
        for (;;) {}
    }

    /* --- Service registry ------------------------------------------ */
    ret = SPP_Service_register(&g_icm20948ServiceDesc, &s_icmCtx, &s_icmCfg);
    if (ret != K_SPP_OK)
    {
        SPP_LOGE(TAG, "ICM20948 service register failed ret=%d", (int)ret);
        for (;;) {}
    }

    ret = SPP_Service_initAll();
    if (ret != K_SPP_OK)
    {
        SPP_LOGE(TAG, "Service initAll failed ret=%d", (int)ret);
        for (;;) {}
    }

    ret = SPP_Service_startAll();
    if (ret != K_SPP_OK)
    {
        SPP_LOGE(TAG, "Service startAll failed ret=%d", (int)ret);
        for (;;) {}
    }

    SPP_LOGI(TAG, "DMP ready — polling FIFO");

    /* --- Poll loop ------------------------------------------------- */
    for (;;)
    {
        ICM20948_checkFifoData(s_icmCtx.p_spi);
        SPP_OSAL_taskDelayMs(K_MAIN_POLL_INTERVAL_MS);
    }
}
