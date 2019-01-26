#ifndef __HAL_SENSOR_HUB_H__
#define __HAL_SENSOR_HUB_H__

#define SENSOR_HUB_I2C_REG_LEN_MAX (8)

enum HAL_SENSOR_HUB_I2C_DEVICE_ID_T {
    HAL_SENSOR_HUB_I2C_DEVICE_ID_0 = 0,
    HAL_SENSOR_HUB_I2C_DEVICE_ID_1,
    HAL_SENSOR_HUB_I2C_DEVICE_ID_2,
    HAL_SENSOR_HUB_I2C_DEVICE_ID_3,
    HAL_SENSOR_HUB_I2C_DEVICE_ID_QTY,
};

enum HAL_SENSOR_HUB_TRIGGER_TYPE_T {
    HAL_SENSOR_HUB_TRIGGER_TYPE_PERIODIC_TIMER = 0,
    HAL_SENSOR_HUB_TRIGGER_TYPE_EXTERNAL_INPUT,
    HAL_SENSOR_HUB_TRIGGER_TYPE_QTY,
};

enum HAL_SENSOR_HUB_I2C_OPERATOR_MODE_T {
    HAL_SENSOR_HUB_I2C_OPERATOR_MODE_DISABLE = 0,
    HAL_SENSOR_HUB_I2C_OPERATOR_MODE_READ,
    HAL_SENSOR_HUB_I2C_OPERATOR_MODE_WRITE,
    HAL_SENSOR_HUB_I2C_OPERATOR_MODE_QTY,
};

typedef void (*SENSOR_HUB_CAPTURE_CB_T)(uint8_t *, uint32_t);

struct HAL_SENSOR_HUB_I2C_CFG_T {
    enum HAL_I2C_ID_T i2c_id;
    uint32_t speed;
};

struct HAL_SENSOR_HUB_I2C_OPERATOR_T {
    enum HAL_SENSOR_HUB_I2C_OPERATOR_MODE_T mode;
    uint8_t  device_address;
    uint8_t  reg_address[SENSOR_HUB_I2C_REG_LEN_MAX];
    uint32_t reg_len;
    uint32_t value_len;
    uint8_t  restart_after_write;
    uint32_t separation_number;
    uint8_t  *dma_read_buf;
    uint32_t dma_read_buf_len;    
    uint32_t dma_read_prev_pos;
    uint8_t  *result_buf;
    uint32_t  result_buf_len;
    uint8_t  tx_dma_ch;
    uint8_t  rx_dma_ch;
};

struct HAL_SENSOR_HUB_CFG_T {
    enum HAL_SENSOR_HUB_TRIGGER_TYPE_T type;
    struct HAL_SENSOR_HUB_I2C_CFG_T i2c_cfg;
    uint32_t timer_interval;
    uint32_t gpio;
    struct HAL_SENSOR_HUB_I2C_OPERATOR_T i2c_operator[HAL_SENSOR_HUB_I2C_DEVICE_ID_QTY];
    SENSOR_HUB_CAPTURE_CB_T callback;
};

#ifdef __cplusplus
extern "C" {
#endif

int hal_sensor_hub_open(struct HAL_SENSOR_HUB_CFG_T *cfg);
int  hal_sensor_hub_close(void);

#ifdef __cplusplus
}
#endif

#endif


