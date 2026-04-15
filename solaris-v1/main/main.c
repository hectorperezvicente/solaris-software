/**
 * @file main.c
 * @brief Application entry point — boots SPP v2 and starts sensor services.
 *
 * Port registration must happen before SPP_Core_init():
 *   1. SPP_Core_setOsalPort(&g_freertosOsalPort)
 *   2. SPP_Core_setHalPort(&g_esp32HalPort)
 *   3. SPP_Core_init()   — initialises Log, Databank, DbFlow
 */

#include "spp/core/core.h"
#include "spp/core/returntypes.h"
#include "spp/hal/spi.h"
#include "spp/osal/task.h"
#include "spp/osal/port.h"
#include "spp/hal/port.h"
#include "spp/services/log.h"
#include "spp/services/service.h"
#include "spp/services/bmp390.h"

#include "macros_esp32.h"

/* ----------------------------------------------------------------
 * External port objects (defined in spp_port_wrapper)
 * ---------------------------------------------------------------- */

extern const SPP_OsalPort_t g_freertosOsalPort;
extern const SPP_HalPort_t  g_esp32HalPort;

/* ----------------------------------------------------------------
 * Private constants
 * ---------------------------------------------------------------- */

static const char *TAG = "MAIN";

/** @brief GPIO pin used for the BMP390 data-ready interrupt. */
#define K_MAIN_BMP390_INT_PIN  (5U)

/* ----------------------------------------------------------------
 * Service instances (static allocation)
 * ---------------------------------------------------------------- */

static BMP390_ServiceCtx_t s_bmpCtx;

static const BMP390_ServiceCfg_t s_bmpCfg = {
    .spiDevIdx    = K_ESP32_SPI_IDX_BMP,
    .intPin       = K_MAIN_BMP390_INT_PIN,
    .intIntrType  = 1U,   /* rising edge */
    .intPull      = 0U,   /* no pull     */
    .sdCfg = {
        .p_basePath         = "/sdcard",
        .spiHostId          = K_ESP32_SPI_HOST,
        .pinCs              = K_ESP32_PIN_CS_SDC,
        .maxFiles           = 5U,
        .allocationUnitSize = (16U * 1024U),
        .formatIfMountFailed = false,
    },
    .p_logFilePath = "/sdcard/log.txt",
    .logMaxPackets = 10U,
};

/* ----------------------------------------------------------------
 * app_main
 * ---------------------------------------------------------------- */

void app_main(void)
{
    retval_t ret;

    /* --- Port registration ---------------------------------------- */
    ret = SPP_Core_setOsalPort(&g_freertosOsalPort);
    if (ret != SPP_OK)
    {
        /* Cannot log yet — OSAL not ready */
        for (;;) { /* hang */ }
    }

    ret = SPP_Core_setHalPort(&g_esp32HalPort);
    if (ret != SPP_OK)
    {
        for (;;) { /* hang */ }
    }

    /* --- Core init (Log + Databank + DbFlow) ----------------------- */
    ret = SPP_Core_init();
    if (ret != SPP_OK)
    {
        for (;;) { /* hang */ }
    }

    SPP_LOGI(TAG, "Boot");

    /* --- SPI bus init --------------------------------------------- */
    ret = SPP_Hal_spiBusInit();
    if (ret != SPP_OK)
    {
        SPP_LOGE(TAG, "SPI bus init failed");
        for (;;) { SPP_Osal_taskDelayMs(1000U); }
    }

    /* ICM-20948 device — must be added first (index 0) */
    void *p_spiIcm = SPP_Hal_spiGetHandle(K_ESP32_SPI_IDX_ICM);
    ret = SPP_Hal_spiDeviceInit(p_spiIcm);
    if (ret != SPP_OK)
    {
        SPP_LOGE(TAG, "SPI device init ICM failed");
        for (;;) { SPP_Osal_taskDelayMs(1000U); }
    }

    /* BMP-390 device — second handle (index 1) */
    void *p_spiBmp = SPP_Hal_spiGetHandle(K_ESP32_SPI_IDX_BMP);
    ret = SPP_Hal_spiDeviceInit(p_spiBmp);
    if (ret != SPP_OK)
    {
        SPP_LOGE(TAG, "SPI device init BMP failed");
        for (;;) { SPP_Osal_taskDelayMs(1000U); }
    }

    (void)p_spiIcm;
    (void)p_spiBmp;

    /* --- Service registry ----------------------------------------- */
    ret = SPP_Service_register(&g_bmp390ServiceDesc, &s_bmpCtx, &s_bmpCfg);
    if (ret != SPP_OK)
    {
        SPP_LOGE(TAG, "Service register failed ret=%d", (int)ret);
        for (;;) { SPP_Osal_taskDelayMs(1000U); }
    }

    ret = SPP_Service_initAll();
    if (ret != SPP_OK)
    {
        SPP_LOGE(TAG, "Service initAll failed ret=%d", (int)ret);
        for (;;) { SPP_Osal_taskDelayMs(1000U); }
    }

    ret = SPP_Service_startAll();
    if (ret != SPP_OK)
    {
        SPP_LOGE(TAG, "Service startAll failed ret=%d", (int)ret);
        for (;;) { SPP_Osal_taskDelayMs(1000U); }
    }

    SPP_LOGI(TAG, "Main idle");
    for (;;)
    {
        SPP_Osal_taskDelayMs(1000U);
    }
}
