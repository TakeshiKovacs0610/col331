#include "types.h"
#include "defs.h"
#include "x86.h"

extern char end[]; // first address after kernel loaded from ELF file

// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{
  mpinit();        // detect other processors
  lapicinit();     // interrupt controller
  picinit();       // disable pic
  ioapicinit();    // another interrupt controller
  uartinit();      // serial port
  cprintf("xv6 OS booting...\n");
  cprintf("Enabling interrupts NOW!\n");
  tvinit();        // trap vectors
  idtinit();       // load idt register
  sti();
  //for(int i = 0;i < 5;i++)
  //  wfi();
  for(;;)
    wfi();
}
