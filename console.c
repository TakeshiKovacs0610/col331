// Console input and output.
// Input is from the serial port.
// Output is written to the serial port.

#include "types.h"
#include "defs.h"
// #include "param.h"
#include "x86.h"
// #include "traps.h"

static void consputc(int);
static int panicked = 0;


// Converts an integer to text and prints it one character at a time. () 
// If signed and negative, it flips sign and remembers to print minus.
// Repeatedly takes remainder by base (10 or 16) to collect digits in reverse.
// Then emits digits backward through consputc so output appears in correct order.
static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}

// Print to the console. only understands %d, %x, %p, %s.
// Minimal formatted printer for kernel messages.
// Walks format string character by character.
// Normal chars go directly to consputc.
// Handles only percent d, percent x/percent p, percent s, and percent percent.
// Uses printint for numbers and raw loop for strings.
void
cprintf(char *fmt, ...)
{
  int i, c;
  uint *argp;
  char *s;

  if (fmt == 0)
    // panic("null fmt");
    return;

  argp = (uint*)(void*)(&fmt + 1);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }
}


// halt(): Powers off QEMU and never returns.
// Prints farewell message.
// Writes shutdown value to QEMU power ports 0x602 and 0xB002.
// Spins forever afterward.
void
halt(void)
{
  cprintf("Bye COL%d!\n\0", 331);
  outw(0x602, 0x2000);
  // For older versions of QEMU, 
  outw(0xB002, 0x2000);
  for(;;);
}


// panic(s): Fatal error handler.
// Disables interrupts with cli.
// Prints panic header and message.
// Captures and prints a small call stack via getcallerpcs.
// Sets panicked flag and calls halt.
void
panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  halt();
}

#define BACKSPACE 0x100


// Lowest-level console character output helper.
// For BACKSPACE, sends backspace-space-backspace to visually erase one char.
// Otherwise forwards character to uartputc (serial output).
void
consputc(int c)
{
  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else
    uartputc(c);
}

#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

// consoleintr(getc): Core input interrupt consumer.
// Repeatedly calls getc until no more chars (negative return).
// Ctrl+U: deletes current edited line back to newline or write boundary.
// Backspace or DEL: deletes one edited char if possible.
// Normal char: normalizes carriage return to newline, stores in circular buffer, echoes to output.
// Marks input as committed (moves w to e) on newline, Ctrl+D, or full buffer.
void
consoleintr(int (*getc)(void))
{
  int c;

  while((c = getc()) >= 0){
    switch(c){
    case C('U'):  // Kill line.
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(input.e != input.w){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        input.buf[input.e++ % INPUT_BUF] = c;
        consputc(c);
        if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
          input.w = input.e;
        }
      }
      break;
    }
  }
}
