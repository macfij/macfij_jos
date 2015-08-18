// Simple implementation of cprintf console output for the kernel,
// based on printfmt() and the kernel console's cputchar().

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>

static void
putch_color(int font_color, int bg_color, int ch, int *cnt)
{
	cputchar_color(font_color, bg_color, ch);
	*cnt++;
}

int
vcprintf(int font_color, int bg_color, const char *fmt, va_list ap)
{
	int cnt = 0;

	vprintfmt((void*)putch_color, font_color, bg_color, &cnt, fmt, ap);
	return cnt;
}

int
cprintf_color(int font_color, int bg_color, const char *fmt, ...)
{
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vcprintf(font_color, bg_color, fmt, ap);
	va_end(ap);

	return cnt;
}

int cprintf(const char *fmt, ...)
{
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vcprintf(WHITE, BLACK, fmt, ap);
	va_end(ap);

	return cnt;
}
