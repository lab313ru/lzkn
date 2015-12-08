#ifndef SIM__HEADER
#define SIM__HEADER

#ifdef _WIN32

  /* You should define ADD_EXPORTS *only* when building the DLL. */
  #ifdef ADD_EXPORTS
    #define ADDAPI __declspec(dllexport)
  #else
    #define ADDAPI __declspec(dllimport)
  #endif

  /* Define calling convention in one place, for convenience. */
  #define ADDCALL __cdecl

#else /* _WIN32 not defined. */

  /* Define with no value on non-Windows OSes. */
  #define ADDAPI
  #define ADDCALL

#endif

typedef unsigned char byte;

ADDAPI int ADDCALL decompress(byte *input, byte *output);
ADDAPI int ADDCALL compress(byte *input, byte *output, int size);
ADDAPI int ADDCALL compressed_size(byte *input);

#endif /* SIM__HEADER */
