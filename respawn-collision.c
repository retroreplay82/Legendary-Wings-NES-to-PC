#include "cyc_mod.h"
#include "vendor/nesrecomp/runner/include/mod_function_hooks.h"
/* Original ground movement bypasses both axes while $5A/$5B is nonzero.
 * Force only the two terrain-query branches to execute for living players.
 * Enemy/bullet damage, the grace countdown, blink and death states are untouched. */
static int terrain_grace(uint16_t address) {
    (void)address;CycModRegs r;cyc_mod_regs(&r);
    if(r.x<2) {uint8_t grace=cyc_mod_peek((uint16_t)(0x5a+r.x));
        if(grace&&grace<0x80){r.p|=2;cyc_mod_set_regs(&r);}}
    return 0;
}
void wings_beta_register_hooks(void) {
    nes_mod_register_function_entry_plugin("wings.terrain-grace-x",0xb299,terrain_grace);
    nes_mod_register_function_entry_plugin("wings.terrain-grace-y",0xb300,terrain_grace);
    nes_mod_set_function_hook_enabled("wings.terrain-grace-x",1);
    nes_mod_set_function_hook_enabled("wings.terrain-grace-y",1);
}
