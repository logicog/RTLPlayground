#include "machine_init_defs.h"

void machine_custom_init(void) __banked
{
    /* Set SYS_LED (pin 28) to active-high so Power LED stays ON after boot */
    reg_read_m(RTL837X_REG_LED_GLB_ACTIVE);
    sfr_mask_data(3, 0x10, 0);
    reg_write_m(RTL837X_REG_LED_GLB_ACTIVE);
}