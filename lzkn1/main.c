#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "main.h"

#define WNDSIZE 0x3FF

#define BUFSIZE 0x22

int compare_win(const unsigned char *buffer, int size, int readpos, int *offset) {

	int from, match, longest, i, j, k;

	*offset = 0;
	longest = 1;
	from = (readpos - WNDSIZE < 0) ? 0 : (readpos - WNDSIZE);

	for (k = 0; k < WNDSIZE && (from + k) < readpos; k++) {
		i = k;
		j = 0;
		match = 0;

		while (j < BUFSIZE - 1 && (readpos + j) < size) {
			if (buffer[from + i] != buffer[readpos + j])
				break;

			match++;
			i++;
			j++;

		}

		if (match >= longest) {
			*offset = readpos - from - k;
			longest = match;
		}
	}

	return (longest == 1) ? 0 : longest;
}

int ADDCALL decompress(unsigned char *input, unsigned char *output) {
	int BITS, READPOS, WRITEPOS, BIT, COMMAND, SEQ, REPS, FROM, MASK, i;

	READPOS = 2;
	BITS = WRITEPOS = 0;
	while (1) {
		if (!(BITS--)) {
			BITS = 7;
			COMMAND = input[READPOS++];
		}

		BIT = COMMAND & 1;
		COMMAND >>= 1;

		if (!BIT) {
			output[WRITEPOS++] = input[READPOS++];
		} else {
			SEQ = input[READPOS++];
			MASK = (SEQ & 0xC0) >> 6;

			if (SEQ == 0x1F)
				break;

			switch (MASK) {
			case 0: //00,01
					// (REPS >= 2 && REPS <= 0x22)
					// (FROM >= 1 && FROM <= 0x3FF)
			case 1: {
				REPS = (SEQ & 0x1F) + 3;
				FROM = ((SEQ << 3) & 0xFF00) | input[READPOS++];

				for (i = 0; i < REPS; i++) {
					output[WRITEPOS] = output[WRITEPOS - FROM];
					WRITEPOS++;
				}
			}
				break;

			case 2: //10
					// (REPS >= 2 && REPS <= 5)
					// (FROM >= 1 && FROM <= 0xF)
			{
				REPS = (SEQ >> 4) - 6;
				FROM = SEQ & 0x0F;

				for (i = 0; i < REPS; i++) {
					output[WRITEPOS] = output[WRITEPOS - FROM];
					WRITEPOS++;
				}
			}
				break;

			case 3: //11
					// (REPS >= 8 && REPS <= 0x47)
			{
				REPS = SEQ - 0xB8;

				for (i = 0; i < REPS; i++)
					output[WRITEPOS++] = input[READPOS++];
			}
				break;
			}
		}
	}

	return (WRITEPOS & 1) ? WRITEPOS + 1 : WRITEPOS;
}

void write_cmd_bit(unsigned char *output, int bit, int *bits, int *cmdpos,
		int *writepos) {
	if (*bits == 8) {
		*bits = 0;
		*cmdpos = *writepos;
		(*writepos)++;
	}

	output[*cmdpos] = ((bit & 1) << *bits) | output[*cmdpos];
	(*bits)++;
}

int get_mode(int singles, int repeats, int offset) {
	if (!singles && (repeats >= 2 && repeats <= 5) && (offset >= 1 && offset <= 0xF))
		return 3;
	else if (!singles && repeats >= 3 && (offset >= 1 && offset <= WNDSIZE))
		return 2;
	else if (singles > 8 && singles <= 0x47)
		return 0;
	else
		return 1;
}

int ADDCALL compress(unsigned char *input, unsigned char *output, int size) {
	unsigned char towrite;
	int singles, i, offset, length, readoff, writeoff, cmdbits, cmdpos;

	memset(output, 0, size);

	readoff = 0;
	output[0] = (size >> 8);
	output[1] = (size & 0xFF);
	writeoff = 2;
	cmdbits = 8;

	while (readoff < size) {
		singles = 0;

		while (((length = compare_win(input, size, readoff + singles, &offset))
				|| !(length = compare_win(input, size, readoff + singles, &offset)))
				&& (singles < 0x47) && (readoff + singles < size)
				&& (get_mode(0, length, offset) == 1))
			singles++;

		//00,01 (2 bytes) mode 2
		// (REPS >= 3 && REPS <= 0x21)
		// (FROM >= 1 && FROM <= 0x3FF)

		//10 (1 byte) mode 3
		// (REPS >= 2 && REPS <= 5)
		// (FROM >= 1 && FROM <= 0xF)

		//11 (1 byte) mode 0
		// (REPS >= 8 && REPS <= 0x47)

		switch (get_mode(singles, length, offset)) {
		case 0: {
			write_cmd_bit(output, 1, &cmdbits, &cmdpos, &writeoff);

			towrite = singles + 0xB8;
			output[writeoff++] = towrite;

			for (i = 0; i < singles; i++)
				output[writeoff++] = input[readoff++];
		}
			break;

		case 1: {
			write_cmd_bit(output, 0, &cmdbits, &cmdpos, &writeoff);
			output[writeoff++] = input[readoff++];
		}
			break;

		case 2: {
			readoff += length;
			write_cmd_bit(output, 1, &cmdbits, &cmdpos, &writeoff);

			towrite = ((offset & 0x300) >> 3) | (length - 3);
			output[writeoff++] = towrite;
			output[writeoff++] = (offset & 0xFF);
		}
			break;

		case 3: {
			readoff += length;
			write_cmd_bit(output, 1, &cmdbits, &cmdpos, &writeoff);

			towrite = (((length + 6) << 4) & 0xF0) | offset;
			output[writeoff++] = towrite;
		}
			break;
		}
	}

	write_cmd_bit(output, 1, &cmdbits, &cmdpos, &writeoff);
	output[writeoff++] = 0x1F;

	return writeoff;
}

int ADDCALL compressed_size(unsigned char *input) {
	int BITS, READPOS, BIT, COMMAND, SEQ, REPS, MASK, i;

	READPOS = 2;
	BITS = 0;
	while (1) {
		if (!(BITS--)) {
			BITS = 7;
			COMMAND = input[READPOS++];
		}

		BIT = COMMAND & 1;
		COMMAND >>= 1;

		if (!BIT) {
			READPOS++;
		} else {
			SEQ = input[READPOS++];
			MASK = (SEQ & 0xC0) >> 6;

			if (SEQ == 0x1F)
				break;

			switch (MASK) {
			case 0: //00,01
					// (REPS >= 2 && REPS <= 0x22)
					// (FROM >= 1 && FROM <= 0x3FF)
			case 1:
				READPOS++;
				break;

			case 2:
				break;

			case 3: //11
					// (REPS >= 8 && REPS <= 0x47)
			{
				REPS = SEQ - 0xB8;

				for (i = 0; i < REPS; i++)
					READPOS++;
			}
				break;
			}
		}
	}

	return (READPOS & 1) ? READPOS + 1 : READPOS;
}
