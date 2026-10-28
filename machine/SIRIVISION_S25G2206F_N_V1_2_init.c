#include "machine_init_defs.h"

// Tests:
// 1) Block A uncommented only.
// 2) Block B uncommented only.
// 3) A+B
// 4) Block C only.

void machine_custom_init(void) __banked
{
// BLOCK A
	reg_bit_clear(RTL837X_REG_LED_MODE, 7);
  reg_bit_clear(RTL837X_REG_LED_MODE, 9);

// BLOCK B
//  for (char i = 0; i < 27; i++) {
// 		reg_bit_clear(RTL837X_PIN_MUX_0, i);
// 	}

// BLOCK C
  // for (char i = 0; i < 27; i++) {
  //   reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 1);
	// }
}