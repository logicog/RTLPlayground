#include "machine_init_defs.h"

void machine_custom_init(void) __banked
{
    /* OEM routine at flash 0x0a2b64, called by the port 5 recovery path:
     * GPIO38 (bit 6 of GPIO32..63) is the active-low external PHY reset.
     * Establish the output level before enabling its driver.
     */
    gpio_output_setup(GPIO38, 0);
    delay(20);
    reg_bit_set(RTL837X_REG_GPIO_32_63_OUTPUT, 6);
    delay(20);

    /* OEM LED routine, flash 0x0a46e8..0x0a47c0. These are masked
     * updates, not complete register values. In particular IO_EN[30]
     * must survive: clearing it left every port LED dark on this board.
     * Restoring it at runtime immediately enabled the port 5 green LED.
     */
    reg_read_m(RTL837X_REG_LED_GLB_ACTIVE);
    sfr_mask_data(0, 0x7f, 0x7f);
    sfr_mask_data(1, 0x97, 0x07);
    sfr_mask_data(2, 0x24, 0x20);
    sfr_mask_data(3, 0x01, 0x01);
    reg_write_m(RTL837X_REG_LED_GLB_ACTIVE);

    reg_read_m(RTL837X_REG_LED_GLB_IO_EN);
    sfr_mask_data(0, 0xff, 0x7f);
    sfr_mask_data(1, 0xff, 0x97);
    sfr_mask_data(2, 0xff, 0x24);
    sfr_mask_data(3, 0x3f, 0x01);
    reg_write_m(RTL837X_REG_LED_GLB_IO_EN);

    reg_read_m(RTL837X_PIN_MUX_0);
    sfr_mask_data(0, 0xff, 0x80);
    sfr_mask_data(1, 0xff, 0x68);
    sfr_mask_data(2, 0xff, 0xdb);
    sfr_mask_data(3, 0x3f, 0x0e);
    reg_write_m(RTL837X_PIN_MUX_0);

    reg_read_m(RTL837X_REG_LED_MODE);
    sfr_mask_data(0, 0xf8, 0x70);
    sfr_mask_data(1, 0x1f, 0x0a);
    reg_write_m(RTL837X_REG_LED_MODE);

    /* OEM LED routing; this does not enable loop detection. */
    reg_read_m(RTL837X_REG_LED_RLDP_1);
    sfr_mask_data(0, 0x03, 0x03);
    reg_write_m(RTL837X_REG_LED_RLDP_1);
    reg_read_m(RTL837X_REG_LED_RLDP_2);
    sfr_mask_data(1, 0xf0, 0x30);
    sfr_mask_data(2, 0xff, 0x33);
    sfr_mask_data(3, 0xff, 0x33);
    reg_write_m(RTL837X_REG_LED_RLDP_2);
    reg_read_m(RTL837X_REG_LED_RLDP_3);
    sfr_mask_data(0, 0x0f, 0x03);
    reg_write_m(RTL837X_REG_LED_RLDP_3);
}
