/*
 * @file wm_hal_sdio_slave.c
 *
 * @brief sdio slave hardware abstraction layer implementation
 *
 */

/*
 *  Copyright 2022-2024 Beijing WinnerMicroelectronics Co.,Ltd.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "wm_hal_irq.h"
#include "wm_hal_sdio_slave.h"

#define WM_HAL_SDIO_SLAVE_DEBUG 0
#if WM_HAL_SDIO_SLAVE_DEBUG
#define LOG_TAG "SDIO_SLAVE HAL"
#include "wm_log.h"
#define WM_HAL_SDIO_SLAVE_LOG_D(fmt, ...)    wm_log_debug("[%s:%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define WM_HAL_SDIO_SLAVE_LOG_I(fmt, ...)    wm_log_info("[%s:%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define WM_HAL_SDIO_SLAVE_LOG_W(fmt, ...)    wm_log_warn("[%s:%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define WM_HAL_SDIO_SLAVE_LOG_E(fmt, ...)    wm_log_error("[%s:%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define WM_HAL_SDIO_SLAVE_LOG_DUMP(fmt, ...) wm_log_dump("[%s:%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define WM_HAL_SDIO_SLAVE_FUNC_EXIT(fn)      WM_HAL_SDIO_SLAVE_LOG_D("%s exit", fn)
#define WM_HAL_SDIO_SLAVE_FUNC_NAME          __func__
#else
#define WM_HAL_SDIO_SLAVE_LOG_D(...)
#define WM_HAL_SDIO_SLAVE_LOG_I(...)
#define WM_HAL_SDIO_SLAVE_LOG_W(...)
#define WM_HAL_SDIO_SLAVE_LOG_E(...)
#define WM_HAL_SDIO_SLAVE_LOG_DUMP(...)
#define WM_HAL_SDIO_SLAVE_FUNC_EXIT(fn)
#endif

static void wm_hal_sdio_slave_irq_handler(wm_irq_no_t irq, void *arg)
{
    wm_hal_sdio_slave_dev_t *dev       = (wm_hal_sdio_slave_dev_t *)arg;
    wm_sdio_wrapper_reg_t *wrapper_reg = NULL;
    uint32_t int_sts                   = 0;

    if (dev == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("dev is NULL");
        return;
    }

    wrapper_reg = dev->wrapper_reg_base;
    if (wrapper_reg == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("wrapper_reg is NULL");
        return;
    }

    int_sts = wm_ll_sdio_wrapper_get_int_sts(wrapper_reg);
    WM_HAL_SDIO_SLAVE_LOG_I("int_sts: 0x%x", int_sts);

    if (int_sts & WM_HAL_SDIO_WRAPPER_INT_STS_UP_FRAME_DONE) {
        if (dev->ctx.cb_info.cb != NULL) {
            if (dev->ctx.curr_txbd != NULL) {
                dev->ctx.cb_info.cb(dev, WM_HAL_SDIO_SLAVE_EVENT_TX, (void *)dev->ctx.curr_txbd->txbuf_addr[0],
                                    dev->ctx.cb_info.priv);
            }
        }
        wm_ll_sdio_wrapper_set_int_sts(wrapper_reg, WM_HAL_SDIO_WRAPPER_INT_STS_UP_FRAME_DONE);
    }
    if (int_sts & WM_HAL_SDIO_WRAPPER_INT_STS_DOWN_FRAME_DONE) {
        wm_hal_sdio_wrapper_rxbd_t *rxbd = dev->ctx.curr_rxbd;
        while (rxbd != NULL && !rxbd->valid) {
            if (dev->ctx.cb_info.cb != NULL) {
                dev->ctx.cb_info.cb(dev, WM_HAL_SDIO_SLAVE_EVENT_RX, (void *)rxbd->rxbuf_addr, dev->ctx.cb_info.priv);
            }
            rxbd->valid = 1;
            wm_ll_sdio_wrapper_set_tx_en(wrapper_reg, WM_HAL_SDIO_WRAPPER_TX_ENABLE);
            if (rxbd->next == NULL) {
                break;
            }
            rxbd               = rxbd->next;
            dev->ctx.curr_rxbd = rxbd;
        }
        wm_ll_sdio_wrapper_set_int_sts(wrapper_reg, WM_HAL_SDIO_WRAPPER_INT_STS_DOWN_FRAME_DONE);
    }
    if (int_sts & WM_HAL_SDIO_WRAPPER_INT_STS_UP_CMD_DONE) {
        if (dev->ctx.cb_info.cb != NULL) {
            uint32_t cmd_buf_addr = wm_ll_sdio_wrapper_get_cmd_buf_addr(wrapper_reg);
            uint32_t cmd_buf_size = wm_ll_sdio_wrapper_get_cmd_buf_size(wrapper_reg);

            dev->ctx.cb_info.cb(dev, WM_HAL_SDIO_SLAVE_EVENT_TX_CMD, (void *)(cmd_buf_addr + cmd_buf_size),
                                dev->ctx.cb_info.priv);
        }
        wm_ll_sdio_wrapper_set_int_sts(wrapper_reg, WM_HAL_SDIO_WRAPPER_INT_STS_UP_CMD_DONE);
    }
    if (int_sts & WM_HAL_SDIO_WRAPPER_INT_STS_DOWN_CMD_DONE) {
        if (dev->ctx.cb_info.cb != NULL) {
            uint32_t cmd_buf_addr = wm_ll_sdio_wrapper_get_cmd_buf_addr(wrapper_reg);

            dev->ctx.cb_info.cb(dev, WM_HAL_SDIO_SLAVE_EVENT_RX_CMD, (void *)cmd_buf_addr, dev->ctx.cb_info.priv);
        }
        wm_ll_sdio_wrapper_set_down_cmd_buf_aval(wrapper_reg, WM_HAL_SDIO_WRAPPER_DOWNLINK_CMD_BUF_READY);
        wm_ll_sdio_wrapper_set_int_sts(wrapper_reg, WM_HAL_SDIO_WRAPPER_INT_STS_DOWN_CMD_DONE);
    }
}

int wm_hal_sdio_slave_register_buf(wm_hal_sdio_slave_dev_t *dev, wm_hal_sdio_slave_buf_desc_t *buf_desc)
{
    int ret                             = WM_ERR_SUCCESS;
    wm_sdio_slave_reg_t *sdio_slave_reg = NULL;
    wm_sdio_wrapper_reg_t *wrapper_reg  = NULL;

    WM_HAL_SDIO_SLAVE_LOG_D("Enter %s", WM_HAL_SDIO_SLAVE_FUNC_NAME);

    /* Check input parameters */
    if (dev == NULL || buf_desc == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid parameters");
        return WM_ERR_INVALID_PARAM;
    }

    sdio_slave_reg = dev->sdio_slave_reg_base;
    wrapper_reg    = dev->wrapper_reg_base;
    if (sdio_slave_reg == NULL || wrapper_reg == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid register base");
        return WM_ERR_INVALID_PARAM;
    }

    /* Validate buffer addresses are within DRAM range */
    if ((uint32_t)buf_desc->f0_cis_addr < CONFIG_DRAM_BASE_ADDR ||
        (uint32_t)buf_desc->f0_cis_addr > CONFIG_DRAM_BASE_ADDR + CONFIG_DRAM_LENGTH ||
        (uint32_t)buf_desc->f1_cis_addr < CONFIG_DRAM_BASE_ADDR ||
        (uint32_t)buf_desc->f1_cis_addr > CONFIG_DRAM_BASE_ADDR + CONFIG_DRAM_LENGTH ||
        (uint32_t)buf_desc->cmd_rxbuf_addr < CONFIG_DRAM_BASE_ADDR ||
        (uint32_t)buf_desc->cmd_rxbuf_addr > CONFIG_DRAM_BASE_ADDR + CONFIG_DRAM_LENGTH || buf_desc->txbd == NULL ||
        (uint32_t)(buf_desc->txbd) < CONFIG_DRAM_BASE_ADDR ||
        (uint32_t)(buf_desc->txbd) > CONFIG_DRAM_BASE_ADDR + CONFIG_DRAM_LENGTH || buf_desc->rxbd == NULL ||
        (uint32_t)(buf_desc->rxbd) < CONFIG_DRAM_BASE_ADDR ||
        (uint32_t)(buf_desc->rxbd) > CONFIG_DRAM_BASE_ADDR + CONFIG_DRAM_LENGTH) {
        WM_HAL_SDIO_SLAVE_LOG_E("Buffer addresses out of range");
        return WM_ERR_INVALID_PARAM;
    }

    /* Validate command buffer size */
    if (buf_desc->cmd_rxbuf_size > WM_HAL_SDIO_SLAVE_CMD_BUF_SIZE_MAX || (buf_desc->cmd_rxbuf_size % 4) != 0) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid command buffer size");
        return WM_ERR_INVALID_PARAM;
    }

    if (buf_desc->tx_buf_len <= 0 || buf_desc->tx_link_num <= 0 || buf_desc->rx_buf_len <= 0 || buf_desc->rx_link_num <= 0) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid buffer length or link number");
        return WM_ERR_INVALID_PARAM;
    }

    /* Configure CIS addresses */
    wm_ll_sdio_slave_set_cisf0_addr(sdio_slave_reg, (uint32_t)buf_desc->f0_cis_addr - WM_HAL_SDIO_SLAVE_CIS_F0_BASE_ADDR);
    wm_ll_sdio_slave_set_cisf1_addr(sdio_slave_reg, (uint32_t)buf_desc->f1_cis_addr - WM_HAL_SDIO_SLAVE_CIS_F1_BASE_ADDR);
    dev->ctx.f0_cis_size = buf_desc->f0_cis_size;
    dev->ctx.f1_cis_size = buf_desc->f1_cis_size;

    /* Configure command buffer */
    wm_ll_sdio_wrapper_set_cmd_buf_addr(wrapper_reg, (uint32_t)buf_desc->cmd_rxbuf_addr);
    wm_ll_sdio_wrapper_set_cmd_buf_size(wrapper_reg, buf_desc->cmd_rxbuf_size);
    wm_ll_sdio_wrapper_set_down_cmd_buf_aval(wrapper_reg, WM_HAL_SDIO_WRAPPER_DOWNLINK_CMD_BUF_READY);

    /* Configure downlink RX buffer descriptors */
    wm_ll_sdio_wrapper_set_tx_link_addr(wrapper_reg, (uint32_t)buf_desc->rxbd);
    dev->ctx.curr_rxbd = buf_desc->rxbd;
    wm_ll_sdio_wrapper_set_tx_link(wrapper_reg, WM_HAL_SDIO_WRAPPER_TX_LINK_ENABLE);
    wm_ll_sdio_wrapper_set_tx_en(wrapper_reg, WM_HAL_SDIO_WRAPPER_TX_ENABLE);

    /* Configure uplink TX buffer descriptors */
    wm_ll_sdio_wrapper_set_rx_link_addr(wrapper_reg, (uint32_t)buf_desc->txbd);
    dev->ctx.curr_txbd = buf_desc->txbd;
    wm_ll_sdio_wrapper_set_rx_link(wrapper_reg, WM_HAL_SDIO_WRAPPER_RX_LINK_ENABLE);

    dev->ctx.tx_buf_len  = buf_desc->tx_buf_len;
    dev->ctx.rx_buf_len  = buf_desc->rx_buf_len;
    dev->ctx.tx_link_num = buf_desc->tx_link_num;
    dev->ctx.rx_link_num = buf_desc->rx_link_num;

    WM_HAL_SDIO_SLAVE_FUNC_EXIT(WM_HAL_SDIO_SLAVE_FUNC_NAME);
    return ret;
}

int wm_hal_sdio_slave_unregister_buf(wm_hal_sdio_slave_dev_t *dev)
{
    int ret                             = WM_ERR_SUCCESS;
    wm_sdio_slave_reg_t *sdio_slave_reg = NULL;
    wm_sdio_wrapper_reg_t *wrapper_reg  = NULL;

    WM_HAL_SDIO_SLAVE_LOG_D("Enter %s", WM_HAL_SDIO_SLAVE_FUNC_NAME);

    /* Check input parameters */
    if (dev == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid parameters");
        return WM_ERR_INVALID_PARAM;
    }

    sdio_slave_reg = dev->sdio_slave_reg_base;
    wrapper_reg    = dev->wrapper_reg_base;
    if (sdio_slave_reg == NULL || wrapper_reg == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid register base");
        return WM_ERR_INVALID_PARAM;
    }

    /* Configure CIS addresses */
    wm_ll_sdio_slave_set_cisf0_addr(sdio_slave_reg, 0);
    wm_ll_sdio_slave_set_cisf1_addr(sdio_slave_reg, 0);
    dev->ctx.f0_cis_size = 0;
    dev->ctx.f1_cis_size = 0;

    /* Configure command buffer */
    wm_ll_sdio_wrapper_set_cmd_buf_addr(wrapper_reg, 0);
    wm_ll_sdio_wrapper_set_cmd_buf_size(wrapper_reg, 0);
    wm_ll_sdio_wrapper_set_down_cmd_buf_aval(wrapper_reg, WM_HAL_SDIO_WRAPPER_DOWNLINK_CMD_BUF_NOT_READY);

    /* Configure TX/RX buffer descriptors */
    wm_ll_sdio_wrapper_set_tx_link_addr(wrapper_reg, 0);
    dev->ctx.curr_rxbd = NULL;
    wm_ll_sdio_wrapper_set_tx_link(wrapper_reg, WM_HAL_SDIO_WRAPPER_TX_LINK_DISABLE);
    wm_ll_sdio_wrapper_set_tx_en(wrapper_reg, WM_HAL_SDIO_WRAPPER_TX_DISABLE);
    wm_ll_sdio_wrapper_set_rx_link_addr(wrapper_reg, 0);
    dev->ctx.curr_txbd = NULL;
    wm_ll_sdio_wrapper_set_rx_link(wrapper_reg, WM_HAL_SDIO_WRAPPER_RX_LINK_DISABLE);

    dev->ctx.tx_buf_len  = 0;
    dev->ctx.rx_buf_len  = 0;
    dev->ctx.tx_link_num = 0;
    dev->ctx.rx_link_num = 0;

    WM_HAL_SDIO_SLAVE_FUNC_EXIT(WM_HAL_SDIO_SLAVE_FUNC_NAME);
    return ret;
}

int wm_hal_sdio_slave_init(wm_hal_sdio_slave_dev_t *dev)
{
    int ret                                 = WM_ERR_SUCCESS;
    wm_sdio_slave_reg_t *sdio_slave_reg     = NULL;
    wm_sdio_wrapper_reg_t *wrapper_reg      = NULL;
    wm_hal_sdio_slave_cfg_t *sdio_slave_cfg = NULL;

    WM_HAL_SDIO_SLAVE_LOG_D("Enter %s", WM_HAL_SDIO_SLAVE_FUNC_NAME);

    if (dev == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid parameters");
        return WM_ERR_INVALID_PARAM;
    }

    sdio_slave_reg = dev->sdio_slave_reg_base;
    wrapper_reg    = dev->wrapper_reg_base;
    if (sdio_slave_reg == NULL || wrapper_reg == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid register base");
        return WM_ERR_INVALID_PARAM;
    }

    sdio_slave_cfg = &dev->sdio_slave_cfg;
    if (sdio_slave_cfg->ccr_rev >= WM_HAL_SDIO_SLAVE_CCCR_REV_MAX ||
        sdio_slave_cfg->sdio_rev >= WM_HAL_SDIO_SLAVE_SDIO_REV_MAX || sdio_slave_cfg->sd_rev >= WM_HAL_SDIO_SLAVE_SD_REV_MAX) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid configuration");
        return WM_ERR_INVALID_PARAM;
    }
    /** CIA Register */
    wm_ll_sdio_slave_set_ccr_version(sdio_slave_reg, sdio_slave_cfg->ccr_rev);
    wm_ll_sdio_slave_set_sdio_version(sdio_slave_reg, sdio_slave_cfg->sdio_rev);
    wm_ll_sdio_slave_set_sd_version(sdio_slave_reg, sdio_slave_cfg->sd_rev);

    /** Program Register - Function 1 Ready */
    wm_ll_sdio_slave_set_f1_ready(sdio_slave_reg, WM_HAL_SDIO_SLAVE_F1_READY);

    /** Attach interrupt */
    WM_HAL_SDIO_SLAVE_LOG_D("Attach interrupt %d", dev->irq_num);
    wm_hal_irq_attach_sw_vector(dev->irq_num, wm_hal_sdio_slave_irq_handler, dev);
    wm_hal_irq_enable(dev->irq_num);
    wm_hal_irq_clear_pending(dev->irq_num);

    wm_ll_sdio_wrapper_set_int_mask(wrapper_reg, 0x00000000);

    WM_HAL_SDIO_SLAVE_FUNC_EXIT(WM_HAL_SDIO_SLAVE_FUNC_NAME);

    return ret;
}

int wm_hal_sdio_slave_deinit(wm_hal_sdio_slave_dev_t *dev)
{
    int ret                             = WM_ERR_SUCCESS;
    wm_sdio_slave_reg_t *sdio_slave_reg = NULL;
    wm_sdio_wrapper_reg_t *wrapper_reg  = NULL;

    WM_HAL_SDIO_SLAVE_LOG_D("Enter %s", WM_HAL_SDIO_SLAVE_FUNC_NAME);

    if (dev == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid parameters");
        return WM_ERR_INVALID_PARAM;
    }

    sdio_slave_reg = dev->sdio_slave_reg_base;
    wrapper_reg    = dev->wrapper_reg_base;
    if (sdio_slave_reg == NULL || wrapper_reg == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid register base");
        return WM_ERR_INVALID_PARAM;
    }

    wm_hal_irq_disable(dev->irq_num);
    wm_hal_irq_detach_sw_vector(dev->irq_num);

    /** Program Register - Function 1 Not Ready */
    wm_ll_sdio_slave_set_f1_ready(sdio_slave_reg, WM_HAL_SDIO_SLAVE_F1_NOT_READY);

    WM_HAL_SDIO_SLAVE_FUNC_EXIT(WM_HAL_SDIO_SLAVE_FUNC_NAME);
    return ret;
}

int wm_hal_sdio_slave_read_cis_reg(wm_hal_sdio_slave_dev_t *dev, wm_hal_sdio_slave_func_t func, uint32_t pos, uint32_t *reg_val)
{
    int ret                             = WM_ERR_SUCCESS;
    wm_sdio_slave_reg_t *sdio_slave_reg = NULL;
    uint32_t addr                       = 0;
    uint32_t addr_offset                = 0;

    WM_HAL_SDIO_SLAVE_LOG_D("Enter %s", WM_HAL_SDIO_SLAVE_FUNC_NAME);

    /* Check input parameters */
    if (dev == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid parameters");
        return WM_ERR_INVALID_PARAM;
    }

    sdio_slave_reg = dev->sdio_slave_reg_base;
    if (sdio_slave_reg == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid register base");
        return WM_ERR_INVALID_PARAM;
    }

    if (reg_val == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid reg_val pointer");
        return WM_ERR_INVALID_PARAM;
    }

    switch (func) {
        case WM_HAL_SDIO_SLAVE_FUNC_0:
            if (pos >= dev->ctx.f0_cis_size - 0x04) {
                WM_HAL_SDIO_SLAVE_LOG_E("Invalid position for FUNC_0");
                return WM_ERR_INVALID_PARAM;
            }
            addr_offset = wm_ll_sdio_slave_get_cisf0_addr(sdio_slave_reg);
            addr        = WM_HAL_SDIO_SLAVE_CIS_F0_BASE_ADDR + addr_offset + pos;
            *reg_val    = *(volatile uint32_t *)addr;
            break;
        case WM_HAL_SDIO_SLAVE_FUNC_1:
            if (pos >= dev->ctx.f1_cis_size - 0x04) {
                WM_HAL_SDIO_SLAVE_LOG_E("Invalid position for FUNC_1");
                return WM_ERR_INVALID_PARAM;
            }
            addr_offset = wm_ll_sdio_slave_get_cisf1_addr(sdio_slave_reg);
            addr        = WM_HAL_SDIO_SLAVE_CIS_F1_BASE_ADDR + addr_offset + pos;
            *reg_val    = *(volatile uint32_t *)addr;
            break;
        default:
            WM_HAL_SDIO_SLAVE_LOG_E("Invalid function");
            return WM_ERR_INVALID_PARAM;
    }

    WM_HAL_SDIO_SLAVE_LOG_D("Read reg[0x%x] = 0x%x", addr, *reg_val);
    WM_HAL_SDIO_SLAVE_FUNC_EXIT(WM_HAL_SDIO_SLAVE_FUNC_NAME);
    return ret;
}

int wm_hal_sdio_slave_write_cis_reg(wm_hal_sdio_slave_dev_t *dev, wm_hal_sdio_slave_func_t func, uint32_t pos, uint32_t reg_val)
{
    int ret                             = WM_ERR_SUCCESS;
    wm_sdio_slave_reg_t *sdio_slave_reg = NULL;
    uint32_t addr                       = 0;
    uint32_t addr_offset                = 0;

    WM_HAL_SDIO_SLAVE_LOG_D("Enter %s", WM_HAL_SDIO_SLAVE_FUNC_NAME);

    /* Check input parameters */
    if (dev == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid parameters");
        return WM_ERR_INVALID_PARAM;
    }

    sdio_slave_reg = dev->sdio_slave_reg_base;
    if (sdio_slave_reg == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid register base");
        return WM_ERR_INVALID_PARAM;
    }

    switch (func) {
        case WM_HAL_SDIO_SLAVE_FUNC_0:
            if (pos >= dev->ctx.f0_cis_size - 0x04) {
                WM_HAL_SDIO_SLAVE_LOG_E("Invalid position for FUNC_0");
                return WM_ERR_INVALID_PARAM;
            }
            addr_offset                = wm_ll_sdio_slave_get_cisf0_addr(sdio_slave_reg);
            addr                       = WM_HAL_SDIO_SLAVE_CIS_F0_BASE_ADDR + addr_offset + pos;
            *(volatile uint32_t *)addr = reg_val;
            break;
        case WM_HAL_SDIO_SLAVE_FUNC_1:
            if (pos >= dev->ctx.f1_cis_size - 0x04) {
                WM_HAL_SDIO_SLAVE_LOG_E("Invalid position for FUNC_1");
                return WM_ERR_INVALID_PARAM;
            }
            addr_offset                = wm_ll_sdio_slave_get_cisf1_addr(sdio_slave_reg);
            addr                       = WM_HAL_SDIO_SLAVE_CIS_F1_BASE_ADDR + addr_offset + pos;
            *(volatile uint32_t *)addr = reg_val;
            break;
        default:
            WM_HAL_SDIO_SLAVE_LOG_E("Invalid function");
            return WM_ERR_INVALID_PARAM;
    }

    WM_HAL_SDIO_SLAVE_LOG_D("Write reg[0x%x] = 0x%x", addr, reg_val);
    WM_HAL_SDIO_SLAVE_FUNC_EXIT(WM_HAL_SDIO_SLAVE_FUNC_NAME);
    return ret;
}

int wm_hal_sdio_slave_register_callback(wm_hal_sdio_slave_dev_t *dev, wm_hal_sdio_slave_callback_t cb, void *priv)
{
    int ret = WM_ERR_SUCCESS;

    WM_HAL_SDIO_SLAVE_LOG_D("Enter %s", WM_HAL_SDIO_SLAVE_FUNC_NAME);

    if (dev == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid parameters");
        return WM_ERR_INVALID_PARAM;
    }

    dev->ctx.cb_info.cb   = cb;
    dev->ctx.cb_info.priv = priv;

    WM_HAL_SDIO_SLAVE_FUNC_EXIT(WM_HAL_SDIO_SLAVE_FUNC_NAME);
    return ret;
}

int wm_hal_sdio_slave_unregister_callback(wm_hal_sdio_slave_dev_t *dev)
{
    int ret = WM_ERR_SUCCESS;

    WM_HAL_SDIO_SLAVE_LOG_D("Enter %s", WM_HAL_SDIO_SLAVE_FUNC_NAME);

    if (dev == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid parameters");
        return WM_ERR_INVALID_PARAM;
    }

    dev->ctx.cb_info.cb   = NULL;
    dev->ctx.cb_info.priv = NULL;

    WM_HAL_SDIO_SLAVE_FUNC_EXIT(WM_HAL_SDIO_SLAVE_FUNC_NAME);
    return ret;
}

static void wm_hal_sdio_slave_get_us(wm_hal_sdio_slave_dev_t *dev, long *total_us)
{
    static uint32_t last_tick = 0;
    uint32_t now_tick         = 0;
    uint32_t tick_us          = 0;
    uint32_t cpu_clock        = 0;
    now_tick                  = csi_coret_get_value();
    cpu_clock                 = dev->sysclk;

    if (*total_us == 0) {
        last_tick = 0;
    }

    if (now_tick > last_tick) {
        tick_us = (now_tick - last_tick) / cpu_clock;
    }

    last_tick = now_tick;
    *total_us += tick_us;
}

int wm_hal_sdio_slave_tx_dma(wm_hal_sdio_slave_dev_t *dev, void *addr, int len)
{
    int ret                             = WM_ERR_SUCCESS;
    wm_sdio_slave_reg_t *sdio_slave_reg = NULL;
    wm_sdio_wrapper_reg_t *wrapper_reg  = NULL;
    wm_hal_sdio_wrapper_txbd_t *txbd    = NULL;

    WM_HAL_SDIO_SLAVE_LOG_D("Enter %s", WM_HAL_SDIO_SLAVE_FUNC_NAME);

    /* Check input parameters */
    if (dev == NULL || addr == NULL || len <= 0 || len > dev->ctx.tx_buf_len * dev->ctx.tx_link_num) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid parameters");
        return WM_ERR_INVALID_PARAM;
    }

    if ((uint32_t)addr < CONFIG_DRAM_BASE_ADDR || (uint32_t)addr + len >= CONFIG_DRAM_BASE_ADDR + CONFIG_DRAM_LENGTH) {
        WM_HAL_SDIO_SLAVE_LOG_E("Buffer address out of range");
        return WM_ERR_INVALID_PARAM;
    }

    sdio_slave_reg = dev->sdio_slave_reg_base;
    wrapper_reg    = dev->wrapper_reg_base;
    if (sdio_slave_reg == NULL || wrapper_reg == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid register base");
        return WM_ERR_INVALID_PARAM;
    }

    txbd = dev->ctx.curr_txbd;
    if (txbd == NULL) {
        WM_HAL_SDIO_SLAVE_LOG_E("Invalid txbd");
        return WM_ERR_INVALID_PARAM;
    }

    for (int remain_len = len; remain_len > 0;) {
        long tick_us   = 0;
        int loop_count = 0;
        while (txbd->valid) {
            txbd = txbd->next;
            if (txbd == NULL) {
                WM_HAL_SDIO_SLAVE_LOG_E("No available txbd");
                return WM_ERR_INVALID_PARAM;
            }
            loop_count++;
            if (loop_count > dev->ctx.tx_link_num) {
                wm_hal_sdio_slave_get_us(dev, &tick_us);
                if (tick_us > WM_HAL_SDIO_SLAVE_TIMEOUT * 1000) {
                    WM_HAL_SDIO_SLAVE_LOG_E("Wait txbd timeout:%ld", tick_us);
                    return WM_ERR_TIMEOUT;
                }
            }
        }
        txbd->txbuf_addr[0] = (uint32_t)addr + (len - remain_len);
        txbd->frm_len       = (remain_len > dev->ctx.tx_buf_len) ? dev->ctx.tx_buf_len : remain_len;
        remain_len -= txbd->frm_len;
        txbd->valid = 1;
        wm_ll_sdio_wrapper_set_rx_en(wrapper_reg, WM_HAL_SDIO_WRAPPER_RX_ENABLE);
        txbd               = txbd->next;
        dev->ctx.curr_txbd = txbd;
        WM_HAL_SDIO_SLAVE_LOG_D("txbd frm_len: %d, remain_len: %d", txbd->frm_len, remain_len);
        if (remain_len <= 0) {
            break;
        }
    }

    WM_HAL_SDIO_SLAVE_FUNC_EXIT(WM_HAL_SDIO_SLAVE_FUNC_NAME);

    return ret;
}
