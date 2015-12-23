#define ADD_EXPORTS

#include "main.h"
//#include "stdio.h"
#include "stdlib.h"
#include "string.h"

typedef unsigned char byte;
typedef unsigned short ushort;

#define wndsize (1 << 10)
#define wndmask (wndsize - 1)
#define maxreps0 0x03
#define maxreps1 0x22
#define maxreps2 0x45

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
	memset(*window, 0x00, wndsize);
	*wndoff = 0x3DF;
}

void find_matches(byte *input, int readoff, int size, int wndoff, byte *window, byte *reserve, int *reps, int *from, int min_pos, int max_pos)
{
	int wpos = 0, tlen = 0;

	*reps = 1;
	wpos = min_pos;
	memcpy(reserve, window, wndsize);

	while (wpos < max_pos)
	{
		tlen = 0;
		while ((readoff + tlen < size && tlen < maxreps1) &&
			window[(wpos + tlen) & wndmask] == input[readoff + tlen])
		{
			window[(wndoff + tlen) & wndmask] = input[readoff + tlen];
			tlen++;
		}

		if (tlen > *reps)
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
	int /*size = 0,*/ bit = 0, readoff = 0, writeoff = 0, wndoff = 0, reps = 0, from = 0;
	byte bitscnt = 0, cmd = 0;
	ushort b = 0;
	byte *window, *reserve;

	init_wnd(&window, &reserve, &wndoff);

	readoff = 0;
	writeoff = 0;
	/*size = */read_word(input, &readoff);

	bitscnt = 1;

	while (1)
	{
		bit = read_cmd_bit(input, &readoff, &bitscnt, &cmd);
		b = read_byte(input, &readoff);

		if (bit == 0) // pack: 1 byte; write: 1 byte
		{
			write_byte(output, &writeoff, (byte)b);
			write_to_wnd(window, &wndoff, (byte)b);
		}
		else
		{
			if (b < 0x80) // pack: 3..34 (wnd: 0..0x3FF); write: 2 bytes
			{
				if (b == 0x1F)
					break;

				// ‭‭01101111 11100011‬
				//    11111          (reps)
				//  11      11111111 (from)
				reps = (b & 0x1F) + 3;
				from = ((b & 0x60) << 3) | read_byte(input, &readoff);
			}
			else if (b >= 0x80 && b <= 0xC0) // pack: 2..5 byte (wnd: wndoff-F..wndoff-0); write: 1 byte
			{
				// ‭1000 0011‬
				//   11      (reps)
				//      1111 (from)
				reps = ((b >> 4) & 3) + 2;
				from = (wndoff - (b & 0xF)) & wndmask;
			}
			else // 0xC1.. // pack: 9..71; write: 10..72 bytes
			{
				reps = (b & 0x3F) + 8; // max == %111111 + 8 == 71 (0x47)

				while (reps > 0)
				{
					b = read_byte(input, &readoff);

					write_byte(output, &writeoff, (byte)b);
					write_to_wnd(window, &wndoff, (byte)b);

					reps--;
				}
			}

			while (reps > 0)
			{
				b = read_wnd_byte(window, &from);

				write_byte(output, &writeoff, (byte)b);
				write_to_wnd(window, &wndoff, (byte)b);

				reps--;
			}
		}
	}

	free(window);
	free(reserve);
	return writeoff;
}

int ADDCALL compress(byte *input, byte *output, int size)
{
	int readoff = 0, writeoff = 0, wndoff = 0, cmdoff = 0, reps = 0,
			from = 0, reps_near = 0, from_near = 0, shift_from = 0, no_reps_count = 0;
	byte bitscnt = 0, b = 0;
	ushort w = 0;
	byte *window, *reserve, *reserve2;

	init_wnd(&window, &reserve, &wndoff);
	reserve2 = (byte *)malloc(wndsize);

	readoff = 0;
	writeoff = 3;

	cmdoff = 2;
	output[cmdoff] = 0;

	while (readoff < size)
	{
		no_reps_count = 0;

		memcpy(reserve2, window, wndsize);
		do
		{
			find_matches(input, readoff, size, wndoff, window, reserve, &reps, &from, 0, wndsize);

			shift_from = (wndoff - from) & wndmask;
			if (
				(reps >= 2) && (reps <= 5) &&
				((shift_from < 0) || (shift_from > 0xF))
				)
			{
				find_matches(input, readoff, size, wndoff, window, reserve, &reps_near, &from_near, wndoff - 0xF, wndoff);

				if (reps_near > 1)
				{
					reps = reps_near;
					from = from_near;
				}
			}

			if (reps == 1)
			{
				no_reps_count++;

				b = read_byte(input, &readoff);
				write_to_wnd(window, &wndoff, b);
			}
			else
			{
				break;
			}
		} while (readoff + no_reps_count < size && no_reps_count <= 71); // minimum for first mode
		memcpy(window, reserve2, wndsize);

		readoff -= no_reps_count;
		wndoff -= no_reps_count;
		wndoff &= wndmask;

		shift_from = (wndoff - from) & wndmask;

		if ((no_reps_count >= 1) && (no_reps_count <= 8))
		{
			while (no_reps_count > 0)
			{
				write_cmd_bit(0, output, &writeoff, &bitscnt, &cmdoff);

				b = read_byte(input, &readoff);
				write_byte(output, &writeoff, b);
				write_to_wnd(window, &wndoff, b);

				no_reps_count--;
			}
		}
		else if (no_reps_count >= 9)
		{
			write_cmd_bit(1, output, &writeoff, &bitscnt, &cmdoff);

			b = (0xC1 + (no_reps_count - 8 - 1));
			write_byte(output, &writeoff, b);

			while (no_reps_count > 0)
			{
				b = read_byte(input, &readoff);
				write_byte(output, &writeoff, b);
				write_to_wnd(window, &wndoff, b);

				no_reps_count--;
			}
		}
		else if (
			(reps == 1) ||
			((reps == 2) && ((shift_from < 0) || (shift_from > 0xF)))
			)
		{
			write_cmd_bit(0, output, &writeoff, &bitscnt, &cmdoff);

			b = read_byte(input, &readoff);
			write_byte(output, &writeoff, b);
			write_to_wnd(window, &wndoff, b);
		}
		else
		{
			write_cmd_bit(1, output, &writeoff, &bitscnt, &cmdoff);

			if (
				(reps >= 2) && (reps <= 5) &&
				(shift_from >= 0) && (shift_from <= 0xF)
				)
			{
				// ‭1000 0011‬
				//   11      (reps)
				//      1111 (from)
				b = 0x80 | ((reps - 2) << 4) | shift_from;
				write_byte(output, &writeoff, b);
			}
			else
			{
				if ((from < 0x100) && (reps == maxreps1))
				{
					reps--;
				}

				// ‭‭‭01100101 11100111‬
				//    11111          (reps)
				//  11      11111111 (from)
				w = ((from & 0x300) << 5) | ((reps - 3) << 8) | (from & 0xFF);
				write_word(output, &writeoff, w);
			}

			while (reps > 0)
			{
				readoff++;
				b = read_wnd_byte(window, &from);
				write_to_wnd(window, &wndoff, b);

				reps--;
			}
		}
	}

	free(window);
	free(reserve);
	free(reserve2);

	write_cmd_bit(1, output, &writeoff, &bitscnt, &cmdoff);
	write_byte(output, &writeoff, 0x1F);

	int retn = writeoff;
	writeoff = 0;
	write_word(output, &writeoff, (readoff & 0xFFFF));
	return (retn & 1) ? retn + 1 : retn;
}

int ADDCALL compressed_size(byte *input) {
	int bit = 0, readoff = 0;
	byte bitscnt = 0, cmd = 0;
	ushort b = 0;

	readoff = 0;
	readoff += 2;

	cmd = 0;
	bitscnt = 1;

	while (1)
	{
		bit = read_cmd_bit(input, &readoff, &bitscnt, &cmd);
		b = read_byte(input, &readoff);

		if (bit == 0)
		{
		}
		else
		{
			if (b < 0x80)
			{
				if (b == 0x1F)
					break;

				readoff++;
			}
			else if (b >= 0x80 && b <= 0xC0)
			{
			}
			else
			{
				readoff += ((b & 0x3F) + 8);
			}
		}
	}

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
