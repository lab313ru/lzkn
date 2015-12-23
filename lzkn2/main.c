#define ADD_EXPORTS

#include "main.h"
//#include "stdio.h"
#include "stdlib.h"
#include "string.h"

typedef unsigned char byte;
typedef unsigned short ushort;

#define wndsize (1 << 10)
#define wndmask (wndsize - 1)
#define maxreps 0x40

byte read_byte(byte *input, int *readoff)
{
	return (input[(*readoff)++]);
}

void write_byte(byte *output, int *writeoff, byte b)
{
	output[(*writeoff)++] = b;
}

byte read_wnd_byte(byte *window, int *wndoff)
{
	byte b = window[*wndoff];
	*wndoff = (*wndoff + 1) & wndmask;
	return b;
}

void write_to_wnd(byte *window, int *wndoff, byte b)
{
	window[*wndoff] = b;
	*wndoff = (*wndoff + 1) & wndmask;
}

ushort read_word(byte *input, int *readoff)
{
	ushort retn = read_byte(input, readoff) << 8;
	retn |= read_byte(input, readoff);
	return retn;
}

void write_word(byte *output, int *writeoff, ushort w)
{
	write_byte(output, writeoff, w >> 8);
	write_byte(output, writeoff, w & 0xFF);
}

int read_cmd_bit(byte *input, int *readoff, byte *bitscnt, byte *cmd)
{
	(*bitscnt)--;

	if (!*bitscnt)
	{
		*cmd = read_byte(input, readoff);
		*bitscnt = 8;
	}

	int retn = *cmd & 1;
	*cmd >>= 1;
	return retn;
}

void write_cmd_bit(int bit, byte *output, int *writeoff, byte *bitscnt, int *cmdoff)
{
	if (*bitscnt == 8)
	{
		*bitscnt = 0;
		*cmdoff = (*writeoff)++;
		output[*cmdoff] = 0;
	}

	output[*cmdoff] = ((bit & 1) << *bitscnt) | output[*cmdoff];
	bit >>= 1;
	(*bitscnt)++;
}

void init_wnd(byte **window, byte **reserve, int *wndoff)
{
	*window = (byte *)malloc(wndsize);
	*reserve = (byte *)malloc(wndsize);
	memset(*window, 0x20, 0x3C0);
	*wndoff = 0x3C0;
}

void find_matches(byte *input, int readoff, int size, int wndoff, byte *window, byte *reserve, int *reps, int *from)
{
	int wpos = 0, tlen = 0;

	*reps = 1;
	wpos = 0;
	memcpy(reserve, window, wndsize);

	while (wpos < wndsize)
	{
		tlen = 0;
		while ((readoff + tlen < size && tlen < maxreps) &&
			window[(wpos + tlen) & wndmask] == input[readoff + tlen])
		{
			window[(wndoff + tlen) & wndmask] = input[readoff + tlen];
			tlen++;
		}

		if (tlen >= *reps)
		{
			*reps = tlen;
			*from = wpos & wndmask;
		}

		memcpy(window, reserve, wndsize);
		wpos++;
	}
}

int ADDCALL decompress(byte *input, byte *output)
{
	int i = 0, size = 0, bit = 0, readoff = 0, writeoff = 0, wndoff = 0, reps = 0, from = 0;
	byte bitscnt = 0, cmd = 0;
	ushort b = 0;
	byte *window, *reserve;

	init_wnd(&window, &reserve, &wndoff);

	readoff = 0;
	writeoff = 0;
	size = read_word(input, &readoff);

	bitscnt = 1;

	while (readoff - 2 < size)
	{
		bit = read_cmd_bit(input, &readoff, &bitscnt, &cmd);

		if (bit)
		{
			b = read_byte(input, &readoff);
			write_byte(output, &writeoff, (byte)b);
			write_to_wnd(window, &wndoff, (byte)b);
		}
		else
		{
			b = read_word(input, &readoff);
			reps = ((b & 0xFC00) >> 10) + 1;
			from = b & wndmask;

			for (i = 0; i < reps; ++i)
			{
				b = read_wnd_byte(window, &from);
				if (from == wndsize)
					from = 0;
				write_byte(output, &writeoff, (byte)b);
				write_to_wnd(window, &wndoff, (byte)b);
			}
		}
	}

	free(window);
	free(reserve);
	return writeoff;
}

int ADDCALL compress(byte *input, byte *output, int size)
{
	int i = 0, readoff = 0, writeoff = 0, wndoff = 0, cmdoff = 0, reps = 0, from = 0;
	byte bitscnt = 0, b = 0;
	byte *window, *reserve;

	init_wnd(&window, &reserve, &wndoff);

	readoff = 0;
	writeoff = 3;

	cmdoff = 2;
	output[cmdoff] = 0;

	write_cmd_bit(1, output, &writeoff, &bitscnt, &cmdoff);
	b = read_byte(input, &readoff);
	write_byte(output, &writeoff, b);
	write_to_wnd(window, &wndoff, b);

	while (readoff < size)
	{
		find_matches(input, readoff, size, wndoff, window, reserve, &reps, &from);

		if (reps >= 1 && reps <= 2)
		{
			write_cmd_bit(1, output, &writeoff, &bitscnt, &cmdoff);
			b = read_byte(input, &readoff);
			write_byte(output, &writeoff, b);
			write_to_wnd(window, &wndoff, b);
		}
		else
		{
			write_cmd_bit(0, output, &writeoff, &bitscnt, &cmdoff);
			write_word(output, &writeoff, ((reps - 1) << 10) | from);
			readoff += reps;

			for (i = 0; i < reps; ++i)
			{
				b = read_wnd_byte(window, &from);
				write_to_wnd(window, &wndoff, b);
			}
		}
	}

	int retn = writeoff;
	writeoff = 0;
	write_word(output, &writeoff, (retn - 2) & 0xFFFF);
	return (retn & 1) ? retn + 1 : retn;
}

int ADDCALL compressed_size(byte *input)
{
	int size = 0, bit = 0, readoff = 0, wndoff = 0, reps = 0;
	byte bitscnt = 0, cmd = 0;
	byte *window, *reserve;

	init_wnd(&window, &reserve, &wndoff);

	readoff = 0;
	size = read_word(input, &readoff);

	cmd = reps = 0;
	bitscnt = 1;

	while (readoff - 2 < size)
	{
		bit = read_cmd_bit(input, &readoff, &bitscnt, &cmd);

		if (bit)
		{
			read_byte(input, &readoff);
		}
		else
		{
			read_word(input, &readoff);
		}
	}

	free(window);
	free(reserve);
	return readoff;
}

/*
int main(int argc, char *argv[])
{
byte *input, *output;

FILE *inf = fopen(argv[1], "rb");

input = (byte *)malloc(0x10000);
output = (byte *)malloc(0x10000);

char mode = (argv[3][0]);

if (mode == 'd')
{
long offset = strtol(argv[4], NULL, 16);
fseek(inf, offset, SEEK_SET);
}

fread(&input[0], 1, 0x10000, inf);

int dest_size;
if (mode == 'd')
{
dest_size = decompress(input, output);
}
else
{
fseek(inf, 0, SEEK_END);
int dec_size = ftell(inf);

dest_size = compress(input, output, dec_size);
}

if (dest_size != 0)
{
FILE *outf = fopen(argv[2], "wb");
fwrite(&output[0], 1, dest_size, outf);
fclose(outf);
}

fclose(inf);

free(input);
free(output);

return 0;
}
*/
