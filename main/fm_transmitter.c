#include "fm_transmitter.h"
#include "driver/i2s.h"
#include "soc/io_mux_reg.h"
#include "soc/soc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_private/rtc_clk.h"
#include "hal/clk_tree_ll.h"
#include "hal/efuse_ll.h"

// 配置
#define TAG "FM_TRANSMITTER"

// 状态
static bool is_enabled = false;
static fm_apll_cfg_t g_apll;

// 获取晶振频率
static inline uint32_t get_xtal_hz(void)
{
    return rtc_clk_xtal_freq_get() * 1000000UL;
}

// 计算APLL配置
static fm_apll_cfg_t fm_calc_apll(uint32_t fout_hz, uint32_t dev_hz)
{
    uint32_t XTAL = get_xtal_hz();
    fm_apll_cfg_t c = {0};
    
    // 1) 选择o_div使VCO ≥350 MHz
    while (c.o_div < 31) {
        if (fout_hz * 2 * (c.o_div + 2) >= 350000000UL) break;
        ++c.o_div;
    }
    
    // 2) 计算分子部分 (4 + sdm2 + frac16/65536)
    double mul = (double)fout_hz * 2 * (c.o_div + 2) / XTAL;
    c.sdm2 = (uint8_t)mul - 4;  // 整数部分
    double frac = mul - (c.sdm2 + 4);  // 0…<1
    uint32_t f16 = lround(frac * 65536.0);  // 0…65535

    if (f16 == 65536) {  // 处理四舍五入溢出
        f16 = 0;
        ++c.sdm2;
    }
    c.base_frac16 = (uint16_t)f16;

    // 保持至少±dev_frac16的余量
    if (c.base_frac16 < c.dev_frac16) {
        c.base_frac16 += c.dev_frac16;
    } else if (c.base_frac16 > 65535 - c.dev_frac16) {
        c.base_frac16 -= c.dev_frac16;
    }

    // 3) 计算在当前o_div下1Hz对应的分数LSB数
    double lsb_hz = XTAL / (2.0 * (c.o_div + 2) * 65536);
    c.dev_frac16 = (uint16_t)lround(dev_hz / lsb_hz);

    // 检查是否为ESP32 rev0芯片
    c.is_rev0 = (efuse_ll_get_chip_ver_rev1() == 0);
    return c;
}

// 设置频率偏差
static inline void fm_set_deviation(int16_t delta_frac16)
{
    int32_t frac32 = (int32_t)g_apll.base_frac16 + delta_frac16;
    int32_t sdm2 = g_apll.sdm2;  // 工作副本

    // 处理分数部分的借位/进位
    if (frac32 < 0) {
        int32_t borrow = (-frac32 + 65535) >> 16;  // 借位次数
        frac32 += borrow * 65536;
        sdm2 -= borrow;
    } else if (frac32 > 65535) {
        int32_t carry = frac32 >> 16;  // 进位次数
        frac32 -= carry * 65536;
        sdm2 += carry;
    }

    // 限制sdm2在有效范围内(0…63)
    if (sdm2 < 0) {
        sdm2 = 0;
        frac32 = 0;
    }
    if (sdm2 > 63) {
        sdm2 = 63;
        frac32 = 65535;
    }

    uint8_t sdm0 = frac32 & 0xFF;
    uint8_t sdm1 = frac32 >> 8;

    // 设置APLL配置
    clk_ll_apll_set_config(g_apll.is_rev0,
                          g_apll.o_div,
                          sdm0,
                          sdm1,
                          (uint8_t)sdm2);
}

// 初始化APLL
static void fm_apll_init(void)
{
    g_apll = fm_calc_apll(FM_FREQUENCY, MAX_DEV_HZ);

    uint8_t sdm0 = g_apll.base_frac16 & 0xFF;
    uint8_t sdm1 = g_apll.base_frac16 >> 8;
    
    // 启用APLL
    rtc_clk_apll_enable(true);
    rtc_clk_apll_coeff_set(g_apll.o_div, sdm0, sdm1, g_apll.sdm2);

    ESP_LOGI(TAG, "APLL初始化成功: o_div=%u, sdm2=%u, frac=0x%04X, dev=%u LSB",
             g_apll.o_div, g_apll.sdm2, g_apll.base_frac16, g_apll.dev_frac16);
}

// 将FM信号路由到GPIO
static void fm_route_to_pin(void)
{
    // 将GPIO0配置为CLK_OUT1功能
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
    // 设置CLK_OUT1源为I2S0 MCLK
    REG_SET_FIELD(PIN_CTRL, CLK_OUT1, 0);
    // 设置GPIO0为输出
    gpio_set_direction(FM_FM_PIN, GPIO_MODE_OUTPUT);
    
    ESP_LOGI(TAG, "FM信号已路由到GPIO%d", FM_FM_PIN);
}

// 初始化I2S
static void fm_i2s_init(void)
{
    const i2s_config_t cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = WAV_SR_HZ,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_PCM_SHORT,
        .use_apll = true,
        .fixed_mclk = FM_FREQUENCY,
        .dma_buf_count = 4,
        .dma_buf_len = 64,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
    };
    
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL));
    ESP_ERROR_CHECK(i2s_start(I2S_NUM_0));
    
    ESP_LOGI(TAG, "I2S初始化成功，MCLK频率: %u Hz", FM_FREQUENCY);
}

// 初始化FM发射器
esp_err_t fm_transmitter_init(void)
{
    ESP_LOGI(TAG, "初始化FM发射器");
    
    // 初始化APLL
    fm_apll_init();
    
    // 路由到GPIO
    fm_route_to_pin();
    
    // 初始化I2S
    fm_i2s_init();
    
    is_enabled = false;
    ESP_LOGI(TAG, "FM发射器初始化完成");
    return ESP_OK;
}

// 设置FM频率
esp_err_t fm_transmitter_set_frequency(uint32_t frequency)
{
    ESP_LOGI(TAG, "设置FM频率: %lu Hz", frequency);
    
    // 重新计算APLL配置
    g_apll = fm_calc_apll(frequency, MAX_DEV_HZ);
    
    uint8_t sdm0 = g_apll.base_frac16 & 0xFF;
    uint8_t sdm1 = g_apll.base_frac16 >> 8;
    
    // 更新APLL配置
    rtc_clk_apll_enable(true);
    rtc_clk_apll_coeff_set(g_apll.o_div, sdm0, sdm1, g_apll.sdm2);
    
    // 更新I2S配置
    i2s_stop(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_0);
    
    const i2s_config_t cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = WAV_SR_HZ,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_PCM_SHORT,
        .use_apll = true,
        .fixed_mclk = frequency,
        .dma_buf_count = 4,
        .dma_buf_len = 64,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
    };
    
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL));
    ESP_ERROR_CHECK(i2s_start(I2S_NUM_0));
    
    ESP_LOGI(TAG, "FM发射器频率已设置为: %lu Hz", frequency);
    return ESP_OK;
}

// 发送音频信号到FM发射器
esp_err_t fm_transmitter_send_sample(uint8_t audio_sample)
{
    if (!is_enabled) {
        return ESP_OK;
    }
    
    // 将0-255范围转换为-128到127
    int16_t audio = (int16_t)audio_sample - 128;
    
    // 计算频率偏移（-dev_frac16到+dev_frac16）
    int16_t delta = (audio * g_apll.dev_frac16) / 128;
    
    // 设置频率偏移
    fm_set_deviation(delta);
    
    return ESP_OK;
}

// 启用FM发射器
esp_err_t fm_transmitter_enable(void)
{
    ESP_LOGI(TAG, "启用FM发射器");
    is_enabled = true;
    return ESP_OK;
}

// 禁用FM发射器
esp_err_t fm_transmitter_disable(void)
{
    ESP_LOGI(TAG, "禁用FM发射器");
    is_enabled = false;
    return ESP_OK;
}

// 检查FM发射器是否已启用
bool fm_transmitter_is_enabled(void)
{
    return is_enabled;
}