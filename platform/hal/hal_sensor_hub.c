#include "plat_addr_map.h"
#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "stdbool.h"
#include "hal_trace.h"
#include "hal_dma.h"
#include "hal_i2cip.h"
#include "hal_i2c.h"
#include "reg_timer.h"
#include "hal_timer.h"
#include "hal_location.h"
#include "hal_cache.h"
#include "hal_sensor_hub.h"
#include "cmsis_nvic.h"

#if defined(CHIP_BEST2300)

#define HAL_SENSOR_HUB_BASE_ADDRESS          0x40170000

struct HAL_SENSOR_HUB_REG_T {
    uint32_t SLV0_CONFIG_REG;
    uint32_t SLV1_CONFIG_REG;
    uint32_t SLV2_CONFIG_REG;
    uint32_t SLV3_CONFIG_REG;
    uint32_t GLOBAL_CFG0_REG;
    uint32_t GLOBAL_CFG1_REG;
    uint32_t GLOBAL_CFG2_REG;
    uint32_t GLOBAL_CFG3_REG;
    uint32_t SLV0_INTR_MASK;
    uint32_t SLV1_INTR_MASK;
    uint32_t SLV2_INTR_MASK;
    uint32_t SLV3_INTR_MASK;
    uint32_t SENSOR_INTR_CLR;
    uint32_t SENSOR_INTR_STS;
};

static struct HAL_SENSOR_HUB_REG_T * const sensor_hub[] = {
    (struct HAL_SENSOR_HUB_REG_T *)HAL_SENSOR_HUB_BASE_ADDRESS,
};

static struct DUAL_TIMER_T * const SRAM_DATA_LOC hal_sensorhub_timer = (struct DUAL_TIMER_T *)MCU_TIMER2_BASE;

static SENSOR_HUB_CAPTURE_CB_T sensor_hub_capture_cb = NULL;

static struct HAL_SENSOR_HUB_CFG_T sensorhub_cfg;

void hal_sensor_hub_irq_handler(void)
{

    TRACE("%s, SENSOR_INTR_STS:0x%08x", __func__, sensor_hub[0]->SENSOR_INTR_STS);
    
    sensor_hub[0]->SENSOR_INTR_CLR = 0x00000001;

    uint8_t  *dma_buf_src = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_buf;
    uint32_t dma_buf_len = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_buf_len;
    uint8_t  *dma_buf_active = (uint8_t *)hal_dma_get_cur_dst_addr(sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].rx_dma_ch);
    uint32_t dma_read_prev_pos = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_prev_pos;
    uint8_t  *result_buf = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].result_buf;
    uint32_t result_len = 0;
    uint8_t  *curr_pos;

    curr_pos = dma_buf_src + dma_read_prev_pos;

    TRACE("dma_buf_active:%08x, curr_pos:%08x", dma_buf_active, curr_pos);

    if (dma_buf_active < curr_pos){
        result_len = dma_buf_src+dma_buf_len-curr_pos;
        memcpy(result_buf, curr_pos, result_len);
        dma_read_prev_pos = 0;
        curr_pos = dma_buf_src + dma_read_prev_pos;
    }
     TRACE("result_len:%08x, curr_pos:%08x", result_len, curr_pos);

    memcpy(result_buf+result_len, curr_pos, dma_buf_active-curr_pos);
    result_len += dma_buf_active-curr_pos;
 
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_prev_pos += result_len;
    if (sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_prev_pos >= dma_buf_len){
        sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_prev_pos -= dma_buf_len;
    }
    DUMP8("%02x ", result_buf, result_len);

    if (sensor_hub_capture_cb){
        sensor_hub_capture_cb(result_buf, result_len);
    }
}

void hal_sensor_hub_iic_int_handler(enum HAL_I2C_INT_STATUS_T status, uint32_t errocode)
{
    TRACE("%s, status:%08x, errocode:%08x", __func__, status, errocode);
    if (errocode){
        hal_sensor_hub_close();
    }
}

void hal_sensor_hub_irq_init(void)
{
    NVIC_SetVector(SENSOR_WKUP_IRQn, (uint32_t)hal_sensor_hub_irq_handler);
    NVIC_SetPriority(SENSOR_WKUP_IRQn, IRQ_PRIORITY_HIGH);
    NVIC_ClearPendingIRQ(SENSOR_WKUP_IRQn);
    NVIC_EnableIRQ(SENSOR_WKUP_IRQn);
}

void hal_sensor_hub_reg_timer_stop(void)
{
    hal_sensorhub_timer->timer[1].Control &= ~TIMER_CTRL_EN;
    hal_sensorhub_timer->timer[1].IntClr = 1;
}

void hal_sensor_hub_reg_timer_open(uint32_t tick)
{
    hal_sensorhub_timer->timer[0].Control = TIMER_CTRL_MODE_PERIODIC | TIMER_CTRL_INTEN;
    hal_sensorhub_timer->timer[0].Load = tick;
}

void hal_sensor_hub_reg_timer_start(void)
{
    hal_sensorhub_timer->timer[0].Control |= TIMER_CTRL_EN;
}

int hal_sensor_hub_open(struct HAL_SENSOR_HUB_CFG_T *cfg)
{
    struct HAL_I2C_CONFIG_T i2c_cfg;

    TRACE("%s", __func__);
    memcpy(&sensorhub_cfg, cfg, sizeof(struct HAL_SENSOR_HUB_CFG_T));
    hal_sensor_hub_irq_init();

    if (cfg->i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].mode == HAL_SENSOR_HUB_I2C_OPERATOR_MODE_READ){
        sensor_hub[0]->SLV0_CONFIG_REG = (cfg->i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].device_address << 1) | 1;
        //RX LLI = 1
        sensor_hub[0]->GLOBAL_CFG1_REG &= ~0xf;
        sensor_hub[0]->GLOBAL_CFG1_REG |= 1;
        //TX LLI = 2
        sensor_hub[0]->GLOBAL_CFG3_REG &= ~0xf;
        if (cfg->type == HAL_SENSOR_HUB_TRIGGER_TYPE_PERIODIC_TIMER){
            sensor_hub[0]->GLOBAL_CFG3_REG |= 2;
        }else if (cfg->type == HAL_SENSOR_HUB_TRIGGER_TYPE_EXTERNAL_INPUT){
            sensor_hub[0]->GLOBAL_CFG3_REG |= 1;
            sensor_hub[0]->SLV0_INTR_MASK = ~(1 << cfg->gpio);
        }
    }
    
    if (cfg->i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_1].mode == HAL_SENSOR_HUB_I2C_OPERATOR_MODE_READ){
        sensor_hub[0]->SLV1_CONFIG_REG = (cfg->i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_1].device_address << 1) | 1;
    }
    
    if (cfg->i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_2].mode == HAL_SENSOR_HUB_I2C_OPERATOR_MODE_READ){
        sensor_hub[0]->SLV2_CONFIG_REG = (cfg->i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_2].device_address << 1) | 1;
    }
    
    if (cfg->i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_3].mode == HAL_SENSOR_HUB_I2C_OPERATOR_MODE_READ){
        sensor_hub[0]->SLV3_CONFIG_REG = (cfg->i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_3].device_address << 1) | 1;
    }

    sensor_hub[0]->GLOBAL_CFG0_REG = (cfg->type == HAL_SENSOR_HUB_TRIGGER_TYPE_PERIODIC_TIMER?(0<<0):(1<<0)) |
                                      (cfg->i2c_cfg.i2c_id == HAL_I2C_ID_0?(0<<2):(1<<2))|
                                      (1<<3);

    sensor_hub[0]->GLOBAL_CFG2_REG = hal_i2c_sh_get_i2c_base_addr(cfg->i2c_cfg.i2c_id);

    if (cfg->type == HAL_SENSOR_HUB_TRIGGER_TYPE_PERIODIC_TIMER){
        hal_sensor_hub_reg_timer_open(MS_TO_TICKS(cfg->timer_interval));
    }

    i2c_cfg.mode = HAL_I2C_API_MODE_SENSORHUB;
    i2c_cfg.use_dma  = 1;
    i2c_cfg.use_sync = 1;
    i2c_cfg.speed = cfg->i2c_cfg.speed;
    i2c_cfg.as_master = 1;
    hal_i2c_open(cfg->i2c_cfg.i2c_id, &i2c_cfg);
    hal_i2c_set_interrupt_handler(cfg->i2c_cfg.i2c_id, hal_sensor_hub_iic_int_handler);

    return 0;
}

int hal_sensor_hub_close(void)
{
    hal_i2c_sh_recv_stop(sensorhub_cfg.i2c_cfg.i2c_id);
    hal_i2c_close(sensorhub_cfg.i2c_cfg.i2c_id);
    sensor_hub[0]->GLOBAL_CFG0_REG = 0;
    return 0;
}

#if 0
int hal_sensor_hub_periodic_timer_test(void)
{
    static uint8_t dma_buf[20];
    static uint8_t out_buf[20];
    struct HAL_I2C_SENSOR_HUB_READ_CONFIG_T i2c_rd_cfg;
    struct HAL_I2C_SENSOR_HUB_DMA_CONFIG_T i2c_dma_cfg;

    memset(&sensorhub_cfg, 0, sizeof(struct HAL_SENSOR_HUB_CFG_T));
    sensorhub_cfg.type = HAL_SENSOR_HUB_TRIGGER_TYPE_PERIODIC_TIMER;
    sensorhub_cfg.i2c_cfg.i2c_id = HAL_I2C_ID_0;
    sensorhub_cfg.i2c_cfg.speed = 100000;

    sensorhub_cfg.timer_interval = 20;

    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].mode = HAL_SENSOR_HUB_I2C_OPERATOR_MODE_READ;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].device_address = 0x16;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].reg_address[0] = 0x80;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].reg_len = 1;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].value_len = 2;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].restart_after_write = 1;

    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].separation_number = 5;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_buf = dma_buf;

    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_buf_len = 
            sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].value_len * 
            sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].separation_number;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_prev_pos = 0;

    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].result_buf = out_buf;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].result_buf_len = sizeof(out_buf);

    hal_sensor_hub_open(&sensorhub_cfg);

    i2c_rd_cfg.trigger_type = HAL_I2C_SENSOR_HUB_READ_TRIGGER_TYPE_PERIODIC_TIMER;
    i2c_rd_cfg.intr_clr_reg_address = (uint32_t)&(sensor_hub[0]->SENSOR_INTR_CLR);
    i2c_rd_cfg.target_addr = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].device_address;
    i2c_rd_cfg.write_buf = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].reg_address;
    i2c_rd_cfg.write_len = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].reg_len;
    i2c_rd_cfg.read_buf = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_buf;
    i2c_rd_cfg.read_buf_burst_len = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].value_len;
    i2c_rd_cfg.read_buf_total_len = sizeof(dma_buf);
    i2c_rd_cfg.restart_after_write = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].restart_after_write;
    i2c_rd_cfg.separation_number = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].separation_number; //byte

    hal_i2c_sh_recv_config(sensorhub_cfg.i2c_cfg.i2c_id, &i2c_rd_cfg);
    hal_i2c_sh_get_dma_config(sensorhub_cfg.i2c_cfg.i2c_id, &i2c_dma_cfg);
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].tx_dma_ch = i2c_dma_cfg.tx_dma_ch;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].rx_dma_ch = i2c_dma_cfg.rx_dma_ch;
    
    hal_i2c_sh_recv_start(sensorhub_cfg.i2c_cfg.i2c_id);
    
    hal_sensor_hub_reg_timer_start();
    return 0;
}

#include "hal_gpio.h"

static void hal_sensor_hub_external_input_pin_irqhandler(enum HAL_GPIO_PIN_T pin)
{
    bool gpio_lvl = 0;
    gpio_lvl = hal_gpio_pin_get_val(pin);
    TRACE("hal_sensor_hub_external_irqhandler io=%d val=%d", pin, gpio_lvl);
}

int hal_sensor_hub_external_input_test(void)
{
    static uint8_t dma_buf[20];
    static uint8_t out_buf[20];
    struct HAL_I2C_SENSOR_HUB_READ_CONFIG_T i2c_rd_cfg;
    struct HAL_I2C_SENSOR_HUB_DMA_CONFIG_T i2c_dma_cfg;

    struct HAL_GPIO_IRQ_CFG_T gpiocfg;
    const struct HAL_IOMUX_PIN_FUNCTION_MAP ext_pin[] = {
        {HAL_IOMUX_PIN_P0_0, HAL_IOMUX_FUNC_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
    };
            

    memset(&sensorhub_cfg, 0, sizeof(struct HAL_SENSOR_HUB_CFG_T));
    sensorhub_cfg.type = HAL_SENSOR_HUB_TRIGGER_TYPE_EXTERNAL_INPUT;
    sensorhub_cfg.i2c_cfg.i2c_id = HAL_I2C_ID_0;
    sensorhub_cfg.i2c_cfg.speed = 100000;

    sensorhub_cfg.gpio = HAL_GPIO_PIN_P0_0;

    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].mode = HAL_SENSOR_HUB_I2C_OPERATOR_MODE_READ;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].device_address = 0x16;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].reg_address[0] = 0x80;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].reg_len = 1;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].value_len = 2;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].restart_after_write = 1;

    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].separation_number = 5;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_buf = dma_buf;

    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_buf_len = 
            sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].value_len * 
            sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].separation_number;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_prev_pos = 0;

    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].result_buf = out_buf;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].result_buf_len = sizeof(out_buf);

    hal_sensor_hub_open(&sensorhub_cfg);

    i2c_rd_cfg.trigger_type = HAL_I2C_SENSOR_HUB_READ_TRIGGER_TYPE_EXTERNAL_INPUT;
    i2c_rd_cfg.intr_clr_reg_address = (uint32_t)&(sensor_hub[0]->SENSOR_INTR_CLR);
    i2c_rd_cfg.target_addr = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].device_address;
    i2c_rd_cfg.write_buf = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].reg_address;
    i2c_rd_cfg.write_len = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].reg_len;
    i2c_rd_cfg.read_buf = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_buf;
    i2c_rd_cfg.read_buf_burst_len = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].value_len;
    i2c_rd_cfg.read_buf_total_len = sizeof(dma_buf);
    i2c_rd_cfg.restart_after_write = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].restart_after_write;
    i2c_rd_cfg.separation_number = sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].separation_number; //byte

    hal_i2c_sh_recv_config(sensorhub_cfg.i2c_cfg.i2c_id, &i2c_rd_cfg);
    hal_i2c_sh_get_dma_config(sensorhub_cfg.i2c_cfg.i2c_id, &i2c_dma_cfg);
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].tx_dma_ch = i2c_dma_cfg.tx_dma_ch;
    sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].rx_dma_ch = i2c_dma_cfg.rx_dma_ch;
    
    hal_i2c_sh_recv_start(sensorhub_cfg.i2c_cfg.i2c_id);
    
    hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)ext_pin, sizeof(ext_pin)/sizeof(struct HAL_IOMUX_PIN_FUNCTION_MAP));
    hal_gpio_pin_set_dir(HAL_GPIO_PIN_P0_0, HAL_GPIO_DIR_IN, 1);
    
    gpiocfg.irq_enable = true;
    gpiocfg.irq_debounce = false;
    gpiocfg.irq_polarity = HAL_GPIO_IRQ_POLARITY_HIGH_RISING;
    gpiocfg.irq_handler = hal_sensor_hub_external_input_pin_irqhandler;
    gpiocfg.irq_type = HAL_GPIO_IRQ_TYPE_EDGE_SENSITIVE;
    
    hal_gpio_setup_irq(HAL_GPIO_PIN_P0_0, &gpiocfg);

    TRACE("SLV0:%08x CFG1:0x%08x CFG3:0x%08x CFG0:0x%08x INTR_MASK:0x%08x", 
        sensor_hub[0]->SLV0_CONFIG_REG,
        sensor_hub[0]->GLOBAL_CFG1_REG, 
        sensor_hub[0]->GLOBAL_CFG3_REG, 
        sensor_hub[0]->GLOBAL_CFG0_REG, 
        sensor_hub[0]->SLV0_INTR_MASK);
    return 0;
}

int hal_sensor_hub_test_loop(void)
{
    DUMP8("%02x ", sensorhub_cfg.i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_0].dma_read_buf, 12);
    TRACE("SENSOR_INTR_STS:0x%08x", sensor_hub[0]->SENSOR_INTR_STS);
//    DUMP8("%02x ", &dma_buf[0], 8);
//memset(&dma_buf[1], 0, sizeof(dma_buf)-1);

    return 0;
}

#endif

#endif
