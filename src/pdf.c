/*************************************************
*                  PMW PDF functions             *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: December 2024 */
/* This file last modified: February 2025 */

#include "pmw.h"

/* This file contains code for outputting a PDF. There is quite a lot of code
that is the same as for PostScript. Some of it has been abstracted into
pout_xxx() functions, but there is nevertheless some duplication, because it
was easier that way. */



/************************************************
*           Structure definition                *
************************************************/

/* PDF objects are created in a linked list of these structures. */

typedef struct pdfobject {
  struct pdfobject *next;
  size_t file_offset;    /* Used for remembering offset in file after writing */
  size_t base_size;      /* Size to get for the first added data */
  size_t data_size;      /* Current size of data block */
  size_t data_used;      /* Amount currently used in data block */
  uschar *data;          /* Points to data block */
} pdfobject;



/************************************************
*           MD5 structure and tables            *
************************************************/

/* The code for computing an MD5 hash was obtained from the GitHub repo
https://github.com/Zunawe/md5-c/. It is licenced as "free and unencumbered
software released into the public domain", with no restrictions on its use. */

typedef struct{
    uint64_t size;        // Size of input in bytes
    uint32_t buffer[4];   // Current accumulation of hash
    uint8_t input[64];    // Input to be used in the next step
    uint8_t digest[16];   // Result of algorithm
}MD5Context;

/*
 * Constants defined by the MD5 algorithm
 */
#define A 0x67452301
#define B 0xefcdab89
#define C 0x98badcfe
#define D 0x10325476

static uint32_t S[] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                       5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                       4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                       6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

static uint32_t K[] = {0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
                       0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                       0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
                       0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                       0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
                       0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                       0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
                       0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                       0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
                       0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                       0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
                       0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                       0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
                       0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                       0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
                       0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

/*
 * Padding used to make the size (in bits) of the input congruent to 448 mod 512
 */
static uint8_t PADDING[] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*
 * Bit-manipulation functions defined by the MD5 algorithm
 */
#define F(X, Y, Z) ((X & Y) | (~X & Z))
#define G(X, Y, Z) ((X & Z) | (Y & ~Z))
#define H(X, Y, Z) (X ^ Y ^ Z)
#define I(X, Y, Z) (Y ^ (X | ~Z))



/************************************************
*             Static variables                  *
************************************************/

static pdfobject *obj_anchor = NULL;
static pdfobject *obj_last = NULL;
static pdfobject *obj_conts = NULL;
static int32_t objectcount;

static uint32_t setcaj, savedcaj;
static int32_t setlinewidth, savedlinewidth;
static int32_t saveddashlength, saveddashgaplength;

static int32_t instringtype = -1;   /* Not in a string */
static int32_t setfont = -1;        /* No font set */
static int32_t setsize = -1;        /* No set size */
static BOOL    setX = FALSE;        /* Extended font */
static BOOL    ETpending = FALSE;   /* In a text sequence */
static BOOL music_font_used = FALSE;

static int32_t text_basex;
static int32_t text_basey;

static uschar  *pdf_IdStrings[font_tablen+1];

static const char *font_extensions[] = { ".otf", ".pfb", ".pfa", ".ttf", "" };
enum { fe_otf, fe_pfb, fe_pfa, fe_ttf, fe_none };

static FILE *font_files[20];
static int nextfontfile = 0;



/*************************************************
*        List of PDF standard fonts              *
*************************************************/

static const char *standard_fonts[] = {
  "Times-Roman", "Times-Bold", "Times-Italic", "Times-BoldItalic",
  "Helvetica", "Helvetica-Bold", "Helvetica-Oblique", "Helvetica-BoldOblique",
  "Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique",
  "Symbol", "ZapfDingbats" };



/*************************************************
*   Encoding names for standardly encoded fonts  *
*************************************************/

static const char *lower_names[] = {
  // 00 - 0F
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  // 10 - 1F
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  // 20 - 2F
  "space", "exclam", "quotedbl", "numbersign",
  "dollar", "percent", "ampersand", "quotesingle",
  "parenleft", "parenright", "asterisk", "plus",
  "comma", "hyphen", "period", "slash",
  // 30 - 3F
  "zero", "one", "two", "three",
  "four", "five", "six", "seven",
  "eight", "nine", "colon", "semicolon",
  "less", "equal", "greater", "question",
  // 40 - 4F
  "at", "A", "B", "C", "D", "E", "F", "G",
  "H", "I", "J", "K", "L", "M", "N", "O",
  // 50 - 5F
  "P", "Q", "R", "S", "T", "U", "V", "W",
  "X", "Y", "Z", "bracketleft",
  "backslash", "bracketright", "asciicircum", "underscore",
  // 60 - 6F
  "grave", "a", "b", "c", "d", "e", "f", "g",
  "h", "i", "j", "k", "l", "m", "n", "o",
  // 70 - 7F
  "p", "q", "r", "s", "t", "u", "v",
  "w", "x", "y", "z", "braceleft",
  "bar", "braceright", "asciitilde", "currency",
  // 80 - 8F
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  // 90 - 9F
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  //A0 - AF
  "space", "exclamdown", "cent", "sterling",
  "currency", "yen", "brokenbar", "section",
  "dieresis", "copyright", "ordfeminine", "guillemotleft",
  "logicalnot", "hyphen", "registered", "macron",
  // B0 - BF
  "degree", "plusminus", "twosuperior", "threesuperior",
  "acute", "mu", "paragraph", "bullet",
  "cedilla", "onesuperior", "ordmasculine", "guillemotright",
  "onequarter", "onehalf", "threequarters", "questiondown",
  // C0 - CF
  "Agrave", "Aacute", "Acircumflex", "Atilde",
  "Adieresis", "Aring", "AE", "Ccedilla",
  "Egrave", "Eacute", "Ecircumflex", "Edieresis",
  "Igrave", "Iacute", "Icircumflex", "Idieresis",
  // D0 - DF
  "Eth", "Ntilde", "Ograve", "Oacute",
  "Ocircumflex", "Otilde", "Odieresis", "multiply",
  "Oslash", "Ugrave", "Uacute", "Ucircumflex",
  "Udieresis", "Yacute", "Thorn", "germandbls",
  // E0 - EF
  "agrave", "aacute", "acircumflex", "atilde",
  "adieresis", "aring", "ae", "ccedilla",
  "egrave", "eacute", "ecircumflex", "edieresis",
  "igrave", "iacute", "icircumflex", "idieresis",
  //F0 - FF
  "eth", "ntilde", "ograve", "oacute",
  "ocircumflex", "otilde", "odieresis", "divide",
  "oslash", "ugrave", "uacute", "ucircumflex",
  "udieresis", "yacute", "thorn", "ydieresis" };

/* This is the upper encoding for characters above 255 in standard fonts. Up to
0x17F it is the Unicode encoding. After that come a few other characters that
are in the Adobe fonts. */

static const char *upper_names[] = {
  // 100 - 10F
  "Amacron", "amacron", "Abreve", "abreve",
  "Aogonek", "aogonek", "Cacute", "cacute",
  "Ccircumflex", "ccircumflex", "Cdotaccent", "cdotaccent",
  "Ccaron", "ccaron", "Dcaron", "dcaron",
  // 110 - 11F
  "Dcroat", "dcroat", "Emacron", "emacron",
  "Ebreve", "ebreve", "Edotaccent", "edotaccent",
  "Eogonek", "eogonek", "Ecaron", "ecaron",
  "Gcircumflex", "gcircumflex", "Gbreve", "gbreve",
  // 120 - 12F
  "Gdotaccent", "gdotaccent", "Gcommaaccent", "gcommaaccent",
  "Hcircumflex", "hcircumflex", "Hbar", "hbar",
  "Itilde", "itilde", "Imacron", "imacron",
  "Ibreve", "ibreve", "Iogonek", "iogonek",
  // 130 - 13f
  "Idotaccent", "dotlessi", "IJ", "ij",
  "Jcircumflex", "jcircumflex", "Kcommaaccent", "kcommaaccent",
  "kgreenlandic", "Lacute", "lacute", "Lcommaaccent",
  "lcommaaccent", "Lcaron", "lcaron", "Ldot",
  // 140 - 14F
  "ldot", "Lslash", "lslash", "Nacute",
  "nacute", "Ncommaaccent", "ncommaaccent", "Ncaron",
  "ncaron", "napostrophe", "Eng", "eng",
  "Omacron", "omacron", "Obreve", "obreve",
  // 150 - 15F
  "Ohungarumlaut", "ohungarumlaut", "OE", "oe",
  "Racute", "racute", "Rcommaaccent", "rcommaaccent",
  "Rcaron", "rcaron", "Sacute", "sacute",
  "Scircumflex", "scircumflex", "Scedilla", "scedilla",
  // 160 - 16F
  "Scaron", "scaron", "Tcedilla", "tcedilla",
  "Tcaron", "tcaron", "Tbar", "tbar",
  "Utilde", "utilde", "Umacron", "umacron",
  "Ubreve", "ubreve", "Uring", "uring",
  // 170 - 17F
  "Uhungarumlaut", "uhungarumlaut", "Uogonek", "uogonek",
  "Wcircumflex", "wcircumflex", "Ycircumflex", "ycircumflex",
  "Ydieresis", "Zacute", "zacute", "Zdotaccent",
  "zdotaccent", "Zcaron", "zcaron", "longs",
  // --------------------------------------------------------------------
  // These are the remaining characters in the Adobe standard encoding,
  // in alphabetic order (seems as good as any other).
  // --------------------------------------------------------------------
  // 180 - 18F
  "Delta", "Euro", "Scommaaccent", "Tcommaaccent",
  "breve", "caron", "circumflex", "commaaccent",
  "dagger", "daggerdbl", "dotaccent", "ellipsis",
  "emdash", "endash", "fi", "fl",
  // 190 - 19F
  "florin", "fraction", "greaterequal", "guilsinglleft",
  "guilsinglright", "hungarumlaut", "lessequal", "lozenge",
  "minus", "notequal", "ogonek", "partialdiff",
  "periodcentered", "perthousand", "quotedblbase", "quotedblleft",
  // 1A0 - 1AF
  "quotedblright", "quoteleft", "quoteright", "quotesinglbase",
  "radical", "ring", "scommaaccent", "summation",
  "tcommaaccent", "tilde", "trademark", "infinity",
  // --------------------------------------------------------------------
  // Fill out to the full 256, just in case.
  "currency", "currency", "currency", "currency",
  // 1B0 - 1BF
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  // 1C0 - 1CF
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  // 1D0 - 1DF
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  // 1E0 - 1EF
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  // 1f0 - 1FF
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency",
  "currency", "currency", "currency", "currency" };


/*************************************************
*        Encoding names for the music font       *
*************************************************/

static const char *music_names[] = {
// 00 - 0F
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
// 10 - 1F
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
// 20 - 2F
"PMWspace", "PMWtrebleclef", "PMWbassclef", "PMWaltoclef",
"PMWstar", "PMWsharp", "PMWdoublesharp", "PMWflat",
"PMWnatural", "PMWtfermata", "PMWbrest", "PMWsbrest",
"PMWmrest", "PMWcrest", "PMWqrest", "PMWbfermata",
// 30 - 3F
"PMWlongrest", "PMWbreve", "PMWsemibreve", "PMWuminim",
"PMWdminim", "PMWucrotchet", "PMWdcrotchet", "PMWuquaver",
"PMWdquaver", "PMWusquaver", "PMWdsquaver", "PMWusqtail",
"PMWdsqtail", "PMWledger", "PMWvdot", "PMWhdot",
// 40 - 4F
"PMWbarsingle", "PMWbardouble", "PMWbarthick", "PMWstave1",
"PMWpstave1", "PMWuqtail", "PMWstave10", "PMWpstave10",
"PMWdqtail", "PMWrepeatdots", "PMWustem", "PMWdstem",
"PMWcnh", "PMWmnh", "PMWcomma", "PMWmordent",
// 50 - 5F
"PMWdmordent", "PMWimordent", "PMWdimordent", "PMWturn",
"PMWhbar", "PMWaccent1", "PMWcaesura", "PMWaccent2",
"PMWaccent3", "PMWaccent4", "PMWaccent5", "PMWbardotted",
"PMWcaesura1", "PMWlittle8", "PMWC", "PMWcut",
// 60 - 6F
"PMWtilde", "PMWthumba", "PMWthumbb", "PMWds1",
"PMWds2", "PMWdowna", "PMWdownb", "PMWupa",
"PMWupb", "PMWiturn", "PMWseven", "PMWfour",
"PMWhcnh", "PMWhmnh", "PMWxnh", "PMWxustem",
// 70 - 7F
"PMWxdstem", "PMWfustem", "PMWfdstem", "PMWsix",
"PMWgdot", "PMWgring", "PMWd1", "PMWd4",
"PMWu4", "PMWl1", "PMWr1", "PMWbs",
"PMWds", "PMWfs", "PMWus", "PMWstar",
// 80 - 8F
"PMWtick", "PMWuacc", "PMWdacc", "PMWgrid",
"PMWbarshort", "PMWbreath", "PMWvring", "PMWcross",
"PMWtrill", "PMWscaesura", "PMWlcaesura", "PMWsbra",
"PMWsket", "PMWrbra", "PMWrket", "PMWrep",
// 90 - 9F
"PMWrepdots", "PMWvtilde", "PMWtrem", "PMWcirc",
"PMWcutcirc", "PMWslur1", "PMWslur2", "PMWup",
"PMWdown", "PMWiC", "PMWicut", "PMWunibreve",
"PMWaccent6", "PMWsrbra", "PMWsrket", "PMWangle1",
// A0 - AF
"PMWangle2", "PMWangle3", "PMWangle4", "PMWped",
"PMWuvtilde", "PMWdvtilde", "PMWnail", "PMWangle5",
"PMWangle6", "PMWstave21", "PMWstave31", "PMWstave41",
"PMWstave61", "PMWhclef", "PMWoldbassclef", "PMWoldaltoclef",
// B0 - BF
"PMWbratop", "PMWbrabot", "PMWdirect", "PMWfive",
"PMWmajor", "PMWdimsh", "PMWhdimsh", "PMWgcross",
"PMWledger2", "PMWsrm1", "PMWsrm2", "PMWu12",
"PMWd12", "PMWhalfsharp1", "PMWhalfsharp2", "PMWhalfflat1",
// C0- CF
"PMWhalfflat2", "PMWicomma", "PMWaccent7", "PMWaccent8",
"PMWrturn", "PMWirturn", "PMWthcirc", "PMWbhcirc",
"PMWrcnh", "PMWrmnh",

/* These must always be last in the real characters for silly historical
reasons. See the comment on the adjust_wide_stave_table() function. */

"PMWstave210", "PMWstave310", "PMWstave410", "PMWstave610",

/* Fill out to the full 256 just in case. */

"PMWstar", "PMWstar",
// D0-DF
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
// E0-EF
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
// F0-FF
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar",
"PMWstar", "PMWstar", "PMWstar", "PMWstar" };



/*************************************************
*               MD5 functions                    *
*************************************************/

/* The code for computing an MD5 hash was obtained from the GitHub repo
https://github.com/Zunawe/md5-c/. It is licenced as "free and unencumbered
software released into the public domain", with no restrictions on its use.
These are the functions that implement the algorithm, unmodified except to make
them static. */


/*
 * Rotates a 32-bit word left by n bits
 */
static uint32_t rotateLeft(uint32_t x, uint32_t n){
    return (x << n) | (x >> (32 - n));
}


/*
 * Initialize a context
 */
static void md5Init(MD5Context *ctx){
    ctx->size = (uint64_t)0;

    ctx->buffer[0] = (uint32_t)A;
    ctx->buffer[1] = (uint32_t)B;
    ctx->buffer[2] = (uint32_t)C;
    ctx->buffer[3] = (uint32_t)D;
}


/*
 * Step on 512 bits of input with the main MD5 algorithm.
 */
static void md5Step(uint32_t *buffer, uint32_t *input){
    uint32_t AA = buffer[0];
    uint32_t BB = buffer[1];
    uint32_t CC = buffer[2];
    uint32_t DD = buffer[3];

    uint32_t E;

    unsigned int j;

    for(unsigned int i = 0; i < 64; ++i){
        switch(i / 16){
            case 0:
                E = F(BB, CC, DD);
                j = i;
                break;
            case 1:
                E = G(BB, CC, DD);
                j = ((i * 5) + 1) % 16;
                break;
            case 2:
                E = H(BB, CC, DD);
                j = ((i * 3) + 5) % 16;
                break;
            default:
                E = I(BB, CC, DD);
                j = (i * 7) % 16;
                break;
        }

        uint32_t temp = DD;
        DD = CC;
        CC = BB;
        BB = BB + rotateLeft(AA + E + K[i] + input[j], S[i]);
        AA = temp;
    }

    buffer[0] += AA;
    buffer[1] += BB;
    buffer[2] += CC;
    buffer[3] += DD;
}


/*
 * Add some amount of input to the context
 *
 * If the input fills out a block of 512 bits, apply the algorithm (md5Step)
 * and save the result in the buffer. Also updates the overall size.
 */
static void md5Update(MD5Context *ctx, uint8_t *input_buffer, size_t input_len){
    uint32_t input[16];
    unsigned int offset = ctx->size % 64;
    ctx->size += (uint64_t)input_len;

    // Copy each byte in input_buffer into the next space in our context input
    for(unsigned int i = 0; i < input_len; ++i){
        ctx->input[offset++] = (uint8_t)*(input_buffer + i);

        // If we've filled our context input, copy it into our local array input
        // then reset the offset to 0 and fill in a new buffer.
        // Every time we fill out a chunk, we run it through the algorithm
        // to enable some back and forth between cpu and i/o
        if(offset % 64 == 0){
            for(unsigned int j = 0; j < 16; ++j){
                // Convert to little-endian
                // The local variable `input` our 512-bit chunk separated into 32-bit words
                // we can use in calculations
                input[j] = (uint32_t)(ctx->input[(j * 4) + 3]) << 24 |
                           (uint32_t)(ctx->input[(j * 4) + 2]) << 16 |
                           (uint32_t)(ctx->input[(j * 4) + 1]) <<  8 |
                           (uint32_t)(ctx->input[(j * 4)]);
            }
            md5Step(ctx->buffer, input);
            offset = 0;
        }
    }
}

/*
 * Pad the current input to get to 448 bytes, append the size in bits to the very end,
 * and save the result of the final iteration into digest.
 */
static void md5Finalize(MD5Context *ctx){
    uint32_t input[16];
    unsigned int offset = ctx->size % 64;
    unsigned int padding_length = offset < 56 ? 56 - offset : (56 + 64) - offset;

    // Fill in the padding and undo the changes to size that resulted from the update
    md5Update(ctx, PADDING, padding_length);
    ctx->size -= (uint64_t)padding_length;

    // Do a final update (internal to this function)
    // Last two 32-bit words are the two halves of the size (converted from bytes to bits)
    for(unsigned int j = 0; j < 14; ++j){
        input[j] = (uint32_t)(ctx->input[(j * 4) + 3]) << 24 |
                   (uint32_t)(ctx->input[(j * 4) + 2]) << 16 |
                   (uint32_t)(ctx->input[(j * 4) + 1]) <<  8 |
                   (uint32_t)(ctx->input[(j * 4)]);
    }
    input[14] = (uint32_t)(ctx->size * 8);
    input[15] = (uint32_t)((ctx->size * 8) >> 32);

    md5Step(ctx->buffer, input);

    // Move the result into digest (convert from little-endian)
    for(unsigned int i = 0; i < 4; ++i){
        ctx->digest[(i * 4) + 0] = (uint8_t)((ctx->buffer[i] & 0x000000FF));
        ctx->digest[(i * 4) + 1] = (uint8_t)((ctx->buffer[i] & 0x0000FF00) >>  8);
        ctx->digest[(i * 4) + 2] = (uint8_t)((ctx->buffer[i] & 0x00FF0000) >> 16);
        ctx->digest[(i * 4) + 3] = (uint8_t)((ctx->buffer[i] & 0xFF000000) >> 24);
    }
}



/*************************************************
*     Add new fixed object block to the chain    *
*************************************************/

/* This gets a header block from the memory that will get automatically
freed when the program ends and adds it to the objects chain and then links it
to a given string. Used for fixed objects that are not created bit by bit.

Argument:  the data string
Returns:   pointer to the new object
*/

static pdfobject *
new_fixed_object(const char *s)
{
size_t len = strlen(s);
pdfobject *new = mem_get(sizeof(pdfobject));
obj_last->next = new;
obj_last = new;
new->next = NULL;
new->file_offset = 0;  /* Will be updated later */
new->base_size = 0;    /* Will never be used */
new->data_size = new->data_used = len;
new->data = malloc(len);
if (new->data == NULL)
  error(ERR0, "", "PDF object data", len + 1); /* Hard */
memcpy(new->data, s, len);
objectcount++;
return new;
}



/*************************************************
*  Add new extendable object block to the chain  *
*************************************************/

/* This gets a small header block from the memory that will get automatically
freed when the program ends and adds it to the objects chain. No initial data
block is created.

Argument:  minimum size to use for first data block
Returns:   pointer to the new object
*/

static pdfobject *
new_object(size_t base_size)
{
pdfobject *new = mem_get(sizeof(pdfobject));
obj_last->next = new;
obj_last = new;
new->next = NULL;
new->file_offset = 0;  /* Will be updated later */
new->base_size = base_size;
new->data_size = new->data_used = 0;
new->data = NULL;
objectcount++;
return new;
}



/*************************************************
*             Add text to an object              *
*************************************************/

/* This adds text to an object's data block, (re-)allocating memory as
required. The data block is freed when the object is ultimately written out.
This function is used for relatively short, formatted strings, so a fixed size
buffer can be used.

Arguments:
  p        pointer to the object
  format   printf-style format
  ...      arguments

Returns:   nothing
*/

/* Save typing, because this function is called a lot. */

#define EO extend_object

static void
extend_object(pdfobject *p, const char *format, ...)
{
int bump;
size_t len, newlen;
char buff[256];

va_list ap;
va_start(ap, format);
len = vsprintf(buff+1, format, ap);
va_end(ap);

/* If not in a string, check whether a leading space is needed. */

if (instringtype < 0 && p->data_used > 0 &&
    isalnum((int)p->data[p->data_used - 1]) &&
    (isalnum(buff[1]) || buff[1] == '-'))
  {
  buff[0] = ' ';
  bump = 0;
  len++;
  }
else bump = 1;

newlen = p->data_used + len;
if (newlen > p->data_size)
  {
  size_t newsize = (p->data_used == 0)? p->base_size : 2 * p->data_size;
  if (newsize < len) newsize = 2 * len;
  p->data = realloc(p->data, newsize);
  if (p->data == NULL)
    error(ERR0, (newsize == p->base_size)? "" : "re-", "PDF object data",
      newsize); /* Hard */
  p->data_size = newsize;
  }
memcpy(p->data + p->data_used, buff + bump, len);
p->data_used += len;
}



/*************************************************
*       Detrail the data in an object            *
*************************************************/

/* This is a "tidiness" functionj that removes spaces from the end of an
object. */

static void
detrail_object(pdfobject *p)
{
size_t i = p->data_used;
while (i > 0 && p->data[i-1] == ' ') i--;
p->data_used = i;
}



/*************************************************
*         Check for standard font name           *
*************************************************/

static BOOL
is_standard_font(uschar *name)
{
for (usint i = 0; i < sizeof(standard_fonts)/sizeof(char *); i++)
  if (strcmp((char *)name, standard_fonts[i]) == 0) return TRUE;
return FALSE;
}



/*************************************************
*            Create an encoding object           *
*************************************************/

/* Arguments:
  first       first character to consider
  last        last ditto
  used        bit map of used characters
  names       character name list
  alt         alternate override encoding
Returns:      number of object or zero for no characters used
*/

static int32_t
make_encoding(int first, int last, uschar *used, const char **names,
  uschar **alt)
{
int count, next;
pdfobject *obj;
BOOL empty = TRUE;

/* Check if any characters in the range were used */

for (int c = first/8; c <= last/8; c++)
  {
  if (used[c] != 0)
    {
    empty = FALSE;
    break;
    }
  }
if (empty) return 0;

/* There's something to encode */

count = 0;
next = -1;
obj = new_object(100);

EO(obj, "<</Type/Encoding/BaseEncoding/WinAnsiEncoding/Differences[\n");
for (int c = first; c <= last; c++)
  {
  if ((used[c/8] & (1 << c%8)) != 0)
    {
    int cc = c;
    if (cc > 255) cc -= 256;
    if (cc != next) EO(obj, "%s%d", (count%4 == 0)? "" : " ", cc);
    EO(obj, "/%s", (alt == NULL || alt[c] == NULL)?
      (uschar *)names[cc] : alt[c]);
    count++;
    if (count%4 == 0) EO(obj, "\n");
    next = cc + 1;
    }
  }
detrail_object(obj);  /* Remove final space */
EO(obj, "]>>\n");
return objectcount;
}



/*************************************************
*            Create a font widths object         *
*************************************************/

/* Only output the width if the character was actually used; otherwise just
output 0 to save space. Also output 0 if the width is unset, but that shouldn't
happen.

Arguments:
  fs       points to font structure
  first    first character used
  last     last character used
Returns:   object number
*/

static int32_t
make_widths_object(fontstr *fs, int first, int last)
{
pdfobject *widths = new_object(50);
EO(widths, "[%d", fs->widths[first]);
for (int k = first + 1, kk = 1; k <= last; k++)
  {
  int ww = fs->widths[k];
  if ((fs->used[k/8] & (1 << k%8)) == 0 || ww < 0) ww = 0;
  EO(widths, "%s%d", ((kk++)%8 == 0)? "\n" : " ", ww);
  }
EO(widths, "]\n");
return objectcount;
}



/*************************************************
*               Make a file object               *
*************************************************/

/* This is used for fonts that are not any of the 14 standard PDF fonts,
whether standardly encoded or not. We seek a font file in various formats; only
OpenType fonts are currently supported.

Argument:  pointer to the font structure
Returns:   a value for a placeholder font item (obj << 8 | type)
           a file index for an open Type 3 font file
           or 0 if font file not found
*/

static int
make_fileobject(fontstr *fs)
{
size_t i;
FILE *f;
pdfobject *d;
int yield;
char type = 0;
const char *typename = "";
uschar buffer1[256];
uschar buffer2[256];

for (i = 0; i < sizeof(font_extensions)/sizeof(char *); i++)
  {
  f = font_finddata(fs->name, font_extensions[i], font_data_extra,
    font_data_default, buffer1, FALSE);
  if (f != NULL) break;
  }

if (f == NULL)
  {
  error(ERR184, fs->name);
  return 0;
  }

/* Handle diffent font types */

switch(i)
  {
  case fe_otf:
  typename = "OTF";
  type = '3';
  break;

  case fe_pfb:
  case fe_pfa:
  case fe_ttf:
  error(ERR185, font_extensions[i], fs->name);  /* Hard */
  break;

  /* Check a file with no extension to see if it's a Type 3 font. This just
  gives a slightly clearer error message. */

  case fe_none:
  while (fgets(CS buffer2, 256, f) != NULL)
    {
    if (Ustrcmp(buffer2, "/FontType 3 def\n") == 0)
      error(ERR187, buffer1);  /* Hard: type 3 not supported */
    }
  fclose(f);
  error(ERR186, buffer1);  /* Hard: unrecognized file */
  break;
  }

/* For non-Type3 fonts, save the open file in the next open file slot and
create two placeholder objects, one for the font, and one for the length.
Return the number of the font object left shifted 8, with the ls byte
containing the suffix character for the FontFile setting. */

font_files[nextfontfile] = f;
d = new_object(150);
yield = (objectcount << 8) | type;
EO(d, "*Font%s %d\n", typename, nextfontfile++);
(void) new_object(10);   /* Length placeholder */
return yield;
}



/*************************************************
*            Create a Font object                *
*************************************************/

/* Create a font object and link it to the resources object. When creating an
upper-encoded font, X == "X". An external font descriptor can be provided; if
not, one is created.

Arguments:
  resources      points to the resources object
  fs             points to the font structure
  X              either "" (lower encoding) or "X" (upper encoding)
  ID             the font id (e.g. "rm")
  first          first character used
  last           last character used
  encobjnum      number of the encoding object or 0 if none
  descnum        font descriptor number or zero
  fontfilenuum   included font object or zero (see below)

Returns:         nothing

The included font object is encoded as an object number left-shifted 8, with
the lowest 8 bits containing a character that follows "/FontFile" (e.g. '3').
*/

static void
make_font(pdfobject *resources, fontstr *fs, const char *X, uschar *ID,
  int first, int last, uint32_t encobjnum, int descnum, int fontfilenum)
{
pdfobject *fontobj;
int firstX = first;
int lastX = last;
const char *subtype = "OpenType";

if (X[0] != 0)
  {
  firstX -= 256;
  lastX -= 256;
  }

/* Make a font descriptor from the AFM information if one isn't provided. Try
to get the flags right, but the AFM gives no clue about serifs. The usage for
the "synbolic" (4) and "non-symbolic" (32) bits is complicated. It seems that
one or the other should be set, with "non-symbolic" meaning "only standrd
encoding and standard character names". */

if (descnum == 0)
  {
  int flags = ((fs->flags & ff_stdencoding) != 0)? 32 : 4;
  pdfobject *descobj = new_object(150);
  descnum = objectcount;

  if ((fs->flags & ff_fixedpitch) != 0) flags |= 1;
  if (Ustrncmp(fs->name, "Helvetica", 9) != 0) flags |= 2;  /* Serifs */
  if (fs->italicangle != 0) flags |= 64;

  EO(descobj, "<</Type/FontDescriptor\n/FontName/%s\n", fs->name);
  EO(descobj, "/Flags %d\n", flags);
  EO(descobj, "/Ascent %d\n", (fs->ascent > 0)? fs->ascent : fs->bbox[3]);
  EO(descobj, "/Descent %d\n", (fs->descent < 0)? fs->descent : fs->bbox[1]);
  EO(descobj, "/ItalicAngle %d\n", fs->italicangle);

  if (fs->stemv != 0) EO(descobj, "/StemV %d\n", fs->stemv);
  if (fs->capheight != 0) EO(descobj, "/CapHeight %d\n", fs->capheight);

  if (fontfilenum != 0)
    EO(descobj, "/FontFile%c %d 0 R", fontfilenum & 0xff, fontfilenum >> 8);

  EO(descobj, "/FontBBox[%d %d %d %d]>>\n", fs->bbox[0], fs->bbox[1],
    fs->bbox[2], fs->bbox[3]);
  }

/* Now make the font object and add it to resources. */

fontobj = new_object(100);
EO(resources, "/%s%s %d 0 R\n", ID, X, objectcount);

EO(fontobj, "<</Type/Font/Subtype/%s\n/Name/%s%s", subtype, ID, X);
if (descnum != 0)
  {
  EO(fontobj, "/BaseFont/%s", fs->name);
  EO(fontobj, "/FontDescriptor %d 0 R\n", descnum);
  }

if (encobjnum != 0) EO(fontobj, "/Encoding %d 0 R", encobjnum);
EO(fontobj, "/FirstChar %d/LastChar %d/Widths %d 0 R>>\n",
  firstX, lastX, make_widths_object(fs, first, last));
}



/*************************************************
*        Adjust music font wide stave table      *
*************************************************/

/* For historical reasons, four of the wide stave characters are by default
encoded in the music font at 247-250. Using them at these positions would leave
a large gap in the encoding, which would result in a lot of zeros in the widths
table. To avoid this, the music font's encoding for PDF has these characters
following on after the last "other" character. Although there may never be any
other characters added, we play safe by making this dynamic. This function
scans backwards down the font's widths table and moves the last four
non-negative widths down to just after the last positive width. It also adjusts
the global pout_stavechar10 table appropriately. */

static void
adjust_wide_stave_table(void)
{
int lastwide, lastchar, offset;
int32_t *widths = (&(font_list[font_table[font_mf]]))->widths;

for (lastwide = 255; lastwide > 0; lastwide--)
  {
  if (widths[lastwide] >= 0) break;
  }

for (lastchar = lastwide - 4; lastchar > 0; lastchar--)
  {
  if (widths[lastchar] >= 0) break;
  }

offset = lastwide - lastchar - 4;

for (int i = lastchar + 1; i <= lastchar + 4; i++)
  widths[i] = widths[i + offset];

for (int i = 0; i < 7; i++)
  if (pout_stavechar10[i] > lastchar) pout_stavechar10[i] -= offset;
}



/* ===========================================================
====        Local functions called from those below.      ====
============================================================*/


/*************************************************
*          Close incomplete text object          *
*************************************************/

static void
check_ETpending(void)
{
if (ETpending)
  {
  EO(obj_conts, "ET\n");
  ETpending = FALSE;
  }
}


/*************************************************
*         Set line width if it's changed         *
*************************************************/

static void
check_linewidth(int32_t width)
{
if (width != setlinewidth)
  {
  EO(obj_conts, "%s w", sff(width));
  setlinewidth = width;
  }
}



/*************************************************
*            Manage colour setting               *
*************************************************/

/* At present, colouring facilities are available only via "draw" items. The
variable pout_changecolour is TRUE when there's been a call to ofi_setcolour().
PMW is set up like PostScript, which has only one colour setting, for both
stroking and non-stroking operations. In contrast, PDF has these as separate
settings. As the use of colour is rare, currently we just set both. In the
future, this could be optimized. */

static void
check_colour(void)
{
if (!pout_changecolour) return;

if (pout_wantcolour[0] == pout_wantcolour[1] &&
    pout_wantcolour[1] == pout_wantcolour[2])
  EO(obj_conts, "%s", SFF("%f G %f g ", pout_wantcolour[0],
    pout_wantcolour[0]));

else
  {
  EO(obj_conts, "%s", SFF("%f %f %f RG ", pout_wantcolour[0],
    pout_wantcolour[1], pout_wantcolour[2]));
  EO(obj_conts, "%s", SFF("%f %f %f rg\n", pout_wantcolour[0],
    pout_wantcolour[1], pout_wantcolour[2]));
  }

memcpy(pout_curcolour, pout_wantcolour, 3 * sizeof(int32_t));
pout_changecolour = FALSE;
}



/*************************************************
*               Basic string output code         *
*************************************************/

/* This function outputs a PMW string, all of whose characters have the same
font. The font is passed explicitly rather than taken from the characters so
that font_mu can become font_mf at smaller size, and font_sc can have the small
caps bit removed. All code points were translated earlier if necessary.

Although the entire string is in one font, we have to deal with handling
characters > 255 that must be output using the second binding of the font.

Arguments:
  s            the PMW string
  f            the font
  fdata        points to font instance size etc data
  x            the x coordinate
  y            the y coordinate

Returns:       nothing
*/

static void
pdf_basic_string(uint32_t *s, usint f, fontinststr *fdata, int32_t x, int32_t y)
{
fontstr *fs = &(font_list[font_table[f & ~font_small]]);
kerntablestr *ktable = fs->kerns;
fontinststr tfd = *fdata;
int32_t kernx = 0;
int32_t kerny = 0;
int32_t *matrix = fdata->matrix;
BOOL inkerningstring = FALSE;

(void)kerny;   // Not currently used

/* Adjust the point size in the temporary font instance structure for small
caps and the reduced music font. */

if (f >= font_small)
  {
  f -= font_small;
  if (f == font_mf) tfd.size = (tfd.size * 9) / 10;
    else tfd.size = (tfd.size * curmovt->smallcapsize) / 1000;
  }

/* Note that the font is used. */

fs->flags |= ff_used;
if (f == font_mf) music_font_used = TRUE;

/* When outputting right-to-left, we need to find the length of the string so
that we can output it from the other end, because the fonts still work
left-to-right. By this stage there are no special escape characters left in the
string and we know that it's all in the same font. */

if (main_righttoleft)
  {
  int32_t last_width, last_r2ladjust;
  int32_t swidth = pout_getswidth(s, f, fs, &last_width, &last_r2ladjust);

  /* Adjust the printing position for the string by the length of the string,
  adjusted for the actual bounding box of the final character, and scaled to
  the font size. */

  x += mac_muldiv(swidth - last_width + last_r2ladjust, tfd.size, 1000);
  }

/* Handle transformed strings separately. First, non-transformed strings, which
can be combined into a single text item.*/

if (matrix == NULL)
  {
  int32_t xt, yt;
  int32_t xp = poutx(x);
  int32_t yp = pouty(y);
  BOOL newBT = !ETpending;

  /* Start a new text item if necessary. */

  if (!ETpending)
    {
    EO(obj_conts, "BT\n");
    ETpending = TRUE;
    xt = xp;
    yt = yp;
    }

  /* It we have previously set a text origin, compute a relative move. Note
  that we must always output, even if the new values are (0,0) because the
  current printing position might be elsewhere. The remembered position is the
  page-relative one (xp,yp) rather than just (x,y) because if we have changed
  movements since the last text, print_xmargin might have changed. */

  else
    {
    xt = xp - text_basex;
    yt = yp - text_basey;
    }

  /* In right-to-left mode, if we have just started a new text item, we need to
  set a text transform so that the font is not shown back-to-front. Otherwise,
  as the font itself has no special transformation, we can use the Td operator
  to do a translation, which is all that is needed, but we have to invert the
  x movement. */

  if (main_righttoleft)
    {
    if (newBT) EO(obj_conts, "-1 0 0 1 %s %s Tm\n", sff(xt), sff(yt));
      else EO(obj_conts, "%s %s Td\n", sff(-xt), sff(yt));
    }

  /* In left-to-right mode all we need is Td. */

  else EO(obj_conts, "%s", SFF("%f %f Td\n", xt, yt));

  /* Remember that a text origin has been set. */

  text_basex = xp;
  text_basey = yp;
  }

/* Output transformed text. I originally tried to include this with other text
in a single text item, but even though I wrapped it in q...Q to preserve the
state, different PDF interpreters (gv and evince) behaved differently on the
same input and I couldn't make a file that both displayed the same. So I'm
keeping transformed text strings in their own text items. They are rare, so the
waste (which is small) is not really an issue. */

else
  {
  double fmatrix[4];

  check_ETpending();
  EO(obj_conts, "BT\n");

  /* The Tm operator sets the text modification matrix, which must include the
  translation. PMW's font transformation matrix contains values in Acorn's font
  matrix format, that is 16-bit integer part and 16-bit fraction. Convert to
  floating point. */

  for (int i = 0; i < 4; i++) fmatrix[i] = ((double)matrix[i])/65536.0;
  if (main_righttoleft) fmatrix[0] = -fmatrix[0];
  EO(obj_conts, "%s %s %s Tm", SFD("%f %f %f %f", fmatrix[0], fmatrix[1],
    fmatrix[2], fmatrix[3]), sff(poutx(x)), sff(pouty(y)));
  }

/* Deal with space stretching. This is used only for underlay hyphen strings
and justified headings/footings. It is very unlikely that there will be several
in a row with the same value, so we just reset it at the end. In a PDF the
value is absolute, *not* in the text coordinate system, so if the text itself
has been stretched or compressed, we have to adjust the value. */

if (fdata->spacestretch != 0)
  {
  double stretch = (double)fdata->spacestretch / 1000.0;
  if (fdata->matrix != NULL)
    stretch /= ((double)fdata->matrix[0])/65536.0;
  EO(obj_conts, "%s Tw", sfd(stretch));
  }

/* Generate the text output. Values are always less than FONTWIDTHS_SIZE (512);
those above 255 use the second font encoding. For standardly encoded fonts we
remember which chars were used. */

for (uint32_t *p = s; *p != 0; p++)
  {
  uint32_t c = PCHAR(*p);    /* c is the original code point */
  uint32_t pc = c;           /* pc is the code value to output */

  /* Record which chars are used */

  fs->used[c/8] |= (1 << c%8);

  /* Values of c greater than 255 should occur only for standardly-encoded
  fonts. */

  if (c > 255)
    {
    pc -= 256;
    fs->flags |= ff_usedupper;
    if (c < fs->firstcharU) fs->firstcharU = c;
    if (c > fs->lastcharU) fs->lastcharU = c;
    }
  else
    {
    fs->flags |= ff_usedlower;
    if (c < fs->firstcharL) fs->firstcharL = c;
    if (c > fs->lastcharL) fs->lastcharL = c;
    }

  /* End the "wrong" type of string */

  if ((instringtype == 0 && c > 255) ||
      (instringtype > 0 && c <= 255))
    {
    if (inkerningstring) EO(obj_conts, ")]TJ\n");
      else EO(obj_conts, ")Tj\n");
    instringtype = -1;
    inkerningstring = FALSE;
    }

  /* Not in a string; select font and start string */

  if (instringtype < 0)
    {
    BOOL X = c > 255;
    if ((int)f != setfont || tfd.size != setsize || X != setX)
      {
      EO(obj_conts, "/%s%s %s Tf\n", pdf_IdStrings[f], X? "X" : "",
        sff(tfd.size));
      setfont = f;
      setX = X;
      setsize = tfd.size;
      }

    /* Kernx may be set if kerning from the previous character or if starting a
    new relative string. */

    if (kernx == 0) EO(obj_conts, "("); else
      {
      EO(obj_conts, "[%d(", -kernx);
      inkerningstring = TRUE;
      kernx = 0;
      }

    /* 0 => in lower encoded string; >0 => in upper encoded string. */

    instringtype = X? 1 : 0;
    }

  /* Output the character */

  if (pc == '(' || pc == ')' || pc == '\\')
    EO(obj_conts, "\\%c", pc);
  else if (pc >= 32 && pc <= 126)
    EO(obj_conts, "%c", pc);
  else
    EO(obj_conts, "\\%03o", pc);

  /* If there is another character, scan the kerning table */

  if (main_kerning && fs->kerncount > 0 && p[1] != 0)
    {
    int32_t xadjust = 0, yadjust = 0;
    int bot = 0;
    int top = fs->kerncount;
    uint32_t cc = PCHAR(p[1]);
    uint32_t pair;

    pair = (c << 16) | cc;
    while (bot < top)
      {
      int mid = (bot + top)/2;
      kerntablestr *k = &(ktable[mid]);
      if (pair == k->pair)
        {
        xadjust = k->kwidth;
        break;
        }
      if (pair < k->pair) top = mid; else bot = mid + 1;
      }

    /* If a kern was found, there is no need to scale the adjustment to the
    font size or the sign of kernx (as done for PostScript) because it operates
    in text space. */

    if (xadjust != 0)
      {
      kernx = xadjust;
      kerny = yadjust;

      if (inkerningstring)
        {
        EO(obj_conts, ")%d(", -kernx);
        kernx = 0;
        }
      else
        {
        EO(obj_conts, ")Tj\n");
        instringtype = -1;
        }
      }
    }
  }

/* Terminate the final substring, but leave the text item open in case other
text strings follow. */

if (instringtype >= 0)
  {
  if (inkerningstring) EO(obj_conts, ")]TJ\n");
    else EO(obj_conts, ")Tj\n");
  instringtype = -1;
  }

/* Reset any stretching that was used. Note that this must happen even if we
are going to close the text object because the setting persists. */

if (fdata->spacestretch != 0) EO(obj_conts, "0 Tw\n");

/* If there was a text transformation matrix, terminate the item. ETpending was
set false above. */

if (matrix != NULL) EO(obj_conts, "ET\n");
}



/* ===========================================================
====           Functions called from out_page() etc.      ====
============================================================*/


/*************************************************
*  Output a text string and change current point *
*************************************************/

/* The x and y coordinates are updated if requested - note that y goes
downwards. Change colour if required, then call the common PS/PDF output
function with the PDF basic string output function.

Arguments:
  s             the PMW string
  fdata         points to font instance data
  xu            pointer to the x coordinate
  yu            pointer to the y coordinate
  update        if TRUE, update the x,y positions

Returns:        nothing
*/

void
pdf_string(uint32_t *s, fontinststr *fdata, int32_t *xu, int32_t *yu,
  BOOL update)
{
check_colour();
pout_string(s, fdata, xu, yu, update, pdf_basic_string);
}



/*************************************************
*             Output a bar line                  *
*************************************************/

/* Normally, solid barlines and dashed ones of a single stave's depth are done
with characters from the music font, except when the barline size is greater
than the stave magnification and it's just one stave deep. However, the
bar_use_draw option forces all bar lines to be drawn.

Arguments:
  x       the x coordinate
  ytop    the top of the barline
  ybot    the bottom of the barline
  type    the type of barline
  magn    the appropriate magnification

Returns:     nothing
*/

void
pdf_barline(int32_t x, int32_t ytop, int32_t ybot, int type, int32_t magn)
{
check_colour();

/* Use music font characters if appropriate. */

if (!bar_use_draw &&
    (type != bar_dotted || ytop == ybot) &&
    (magn <= out_stavemagn || ytop != ybot))
  {
  int32_t lastytop = 0;
  uint32_t buff[2];

  buff[0] = type;
  buff[1] = 0;

  pout_mfdata.size = 10 * magn;
  ytop += 16*(magn - out_stavemagn);

  while (ytop <= ybot)
    {
    lastytop = ytop;
    pdf_basic_string(buff, font_mf, &pout_mfdata, x, ytop);
    ytop += 16*magn;
    }

  if (lastytop < ybot)
    pdf_basic_string(buff, font_mf, &pout_mfdata, x, ybot);
  }

/* Long dashed lines have to be drawn, as do other lines if they are shorter
than the character - this happens if barlinesize is greater than the stave
magnification - or if bar_use_draw is set. */

else
  {
  int32_t half_thickness = (type == bar_thick)? magn :
    (type == bar_dotted)? magn/5 : (magn*3)/20;
  int32_t yadjust = out_stavemagn/5;
  int32_t dash = (type == bar_dotted)? 7*half_thickness : 0;

  x += half_thickness;

  check_ETpending();
  check_linewidth(2*half_thickness);
  pdf_setdash(dash, dash);

  for (int i = 0; i < 2; i++)
    {
    EO(obj_conts, "%s", SFF("%f %f m %f %f l S\n", poutx(x),
      pouty(ytop - 16*out_stavemagn - yadjust),
        poutx(x), pouty(ybot - yadjust)));
    if (type != bar_double) break;
    x += 2*magn;
    }
  }
}



/*************************************************
*             Output a brace                     *
*************************************************/

/*
Arguments:
  x          the x coordinate
  ytop       the y coordinate of the top of the brace
  ybot       the y coordinate of the bottom of the brace
  magn       the magnification

Returns:     nothing
*/

void
pdf_brace(int32_t x, int32_t ytop, int32_t ybot, int32_t magn)
{
int32_t scale = ((ybot-ytop+16*magn)*23)/12000;

check_ETpending();
check_colour();

/* Translate to middle of brace and set appropriate scale. */

EO(obj_conts, "q %s", SFF("%f 0 0 %f %f %f cm\n",
  (scale > 110)? 110 : scale/2 + 55, scale,
  poutx(x) + 1500, pouty((ytop-16*magn+ybot)/2)));

/* There is only one alternative brace style at present. */

if (curmovt->bracestyle == 0)
  EO(obj_conts, "0 0 m 100 20 -50 245 60 260 c -50 245 60 20 0 0 c f\n"
                "1 0 0 -1 0 0 cm\n"
                "0 0 m 100 20 -50 245 60 260 c -50 245 60 20 0 0 c f\n");
else
  EO(obj_conts, "0 0 m 95 40 -43 218 37 256 c -59 219 66 34 0 0 c f\n"
                "1 0 0 -1 0 0 cm\n"
                "0 0 m 95 40 -43 218 37 256 c -59 219 66 34 0 0 c f\n");

EO(obj_conts, "Q\n");
}



/*************************************************
*             Output a bracket                   *
*************************************************/

/* The y-coordinates are the positions of the base of the relevant music
characters and may be equal if it's just on one stave. In right-to-left mode,
we don't want to re-convert the music characters so temporarily disable
right-to-left. This means we must start a new text item if one is already in
progress (because it will be transformed) and we must also close this text item
at the end.

Arguments:
  x          the x coordinate
  ytop       the y coordinate of the top of the bracket
  ybot       the y coordinate of the bottom of the bracket
  magn       the magnification of the top stave

Returns:     nothing
*/

void
pdf_bracket(int32_t x, int32_t ytop, int32_t ybot, int32_t magn)
{
uint32_t buff[2];
BOOL save_righttoleft = main_righttoleft;
int32_t stride = ybot - ytop + 16*magn;   /* Total vertical distance */

if (main_righttoleft) check_ETpending();
main_righttoleft = FALSE;
check_colour();

if (stride > 16000) stride = 16000;
pout_mfdata.size = (stride * 10)/16;

buff[0] = 0260;     /* Top character */
buff[1] = 0;

ytop = ytop - 16*magn + stride;  /* Position for top character */
pdf_basic_string(buff, font_mf, &pout_mfdata, x, ytop);

stride -= 1000;   /* Ensure no gap by reducing stride */
ytop += stride;

buff[0] = 'B';      /* Middle character */
while (ytop < ybot)
  {
  pdf_basic_string(buff, font_mf, &pout_mfdata, x, ytop);
  ytop += stride;
  }

buff[0] = 0261;     /* Bottom character */

pdf_basic_string(buff, font_mf, &pout_mfdata, x, ybot);

if (save_righttoleft)
  {
  check_ETpending();
  main_righttoleft = TRUE;
  }
}



/*************************************************
*            Output a stave's lines              *
*************************************************/

/* The stavelines parameter will always be > 0. There is now an option to
draw the stave lines rather than using characters for them (the default). This
helps with screen displays that are using anti-aliasing.

It has been reported that some PostScript interpreters can't handle the
100-point wide characters, so there is an option to use only the 10-point
characters. Assume staves are always at least one character long.

Arguments:
  leftx        the x-coordinate of the stave start
  y            the y-coordinate of the stave start
  rightx       the x-coordinate of the stave end
  stavelines   the number of stave lines

Returns:       nothing
*/

void
pdf_stave(int32_t leftx, int32_t y, int32_t rightx, int stavelines)
{
int32_t save_colour[3];

/* Save whatever colour is set and restore afterwards. Then normally set black,
but use red if running in the special testing mode. */

pout_getcolour(save_colour);
if ((main_testing & mtest_forcered) != 0)
  {
  int32_t red[] = { 1000, 0, 0 };
  pout_setcolour(red);
  }
else pout_setgray(0);
check_colour();

/* Output the stave using drawing primitives. */

if (stave_use_draw > 0)
  {
  int32_t gap;
  int32_t thickness = (stave_use_draw * out_stavemagn)/10;

  switch(stavelines)
    {
    case 1: y -= 4 * out_stavemagn;
    /* Fall through */
    case 2: y -= 4 * out_stavemagn;
    /* Fall through */
    case 3: gap = 8 * out_stavemagn;
    break;

    default: gap = 4 * out_stavemagn;
    break;
    }

  check_linewidth(thickness);
  for (int i = 0; i < stavelines; i++)
    {
    EO(obj_conts, "%s", SFF("%f %f m %f %f l S\n", poutx(leftx), pouty(y),
    poutx(rightx), pouty(y)));
    y -= gap;
    }
  }

/* Output the stave using music font characters */

else
  {
  uschar sbuff[16];
  uschar buff[256];
  int ch, i;
  int32_t chwidth = 0;
  int32_t x = leftx;

  if (stave_use_widechars)
    {
    ch = pout_stavechar10[stavelines];
    i = 100;
    }
  else
    {
    ch = pout_stavechar1[stavelines];
    i = 10;
    }

  /* Select appropriate size of music font */

  pout_mfdata.size = 10 * out_stavemagn;

  /* Build character string of (optionally) 100-point & 10-point chars. */

  buff[0] = 0;

  for (; i >= 10; i /= 10)
    {
    sbuff[0] = ch;
    sbuff[1] = 0;
    chwidth = i * out_stavemagn;
    while (rightx - x >= chwidth)
      {
      Ustrcat(buff, sbuff);
      x += chwidth;
      }
    ch = pout_stavechar1[stavelines];
    }

  pdf_basic_string(string_pmw(buff, font_mf), font_mf, &pout_mfdata, leftx, y);

  /* If there's a fraction of 10 points left, deal with it. */

  if (x < rightx)
    pdf_basic_string(string_pmw(sbuff, font_mf), font_mf, &pout_mfdata,
      rightx - chwidth, y);
  }

pout_setcolour(save_colour);
}



/*************************************************
*       Output one virtual musical character     *
*************************************************/

/* Certain musical characters are given identity numbers in a virtual music
font that may or may not correspond directly to characters in the actual music
font. The table called out_mftable[] defines how they are to be printed.

Arguments:
  x          the x coordinate
  y          the y coordinate
  ch         the character's identity number
  pointsize  the point size

Returns:     nothing
*/

void
pdf_muschar(int32_t x, int32_t y, uint32_t ch, int32_t pointsize)
{
check_colour();
pout_muschar(x, y, ch, pointsize, pdf_basic_string);
}



/*************************************************
*     Output an ASCII string in the music font   *
*************************************************/

/* The strings are always quite short; we have to convert to 32-bits by calling
string_pmw().

Arguments:
  s          the string
  pointsize  the pointsize for the font
  x          the x coordinate
  y          the y coordinate

Returns:     nothing
*/

void
pdf_musstring(uschar *s, int32_t pointsize, int32_t x, int32_t y)
{
pout_mfdata.size = pointsize;
pdf_string(string_pmw(s, font_mf), &pout_mfdata, &x, &y, FALSE);
}



/*************************************************
*            Output a beam line                  *
*************************************************/

/* This function is called several times for a multi-line beam, with the level
number increasing each time. Information about the slope and other features is
in beam_* variables. The initial computation is now in a pout function shared
with PS output.

Arguments:
  x0            starting x coordinate, relative to start of bar
  x1            ending x coordinate, relative to start of bar
  level         level number
  levelchange   set nonzero for accellerando and ritardando beams

Returns:        nothing
*/

void
pdf_beam(int32_t x0, int32_t x1, int level, int levelchange)
{
int32_t y0, y1, depth;

check_ETpending();
check_colour();

pout_beam(&x0, &x1, &y0, &y1, &depth, level, levelchange);
EO(obj_conts, "%s", SFF("%f %f m %f %f l", poutx(x0), pouty(y0),
  poutx(x1), pouty(y1)));
EO(obj_conts, "%s", SFF("%f %f l %f %f l f\n", poutx(x1), pouty(y1-depth),
  poutx(x0), pouty(y0-depth)));
}



/*************************************************
*            Output a slur                       *
*************************************************/

/* This was the original way of drawing all slurs. Additional complication in
slurs has resulted in a function called out_slur() that uses more primitive
output functions, and which could in principle be used for all slurs. However,
we retain a dedicated slur function for complete, non-dashed, curved slurs,
originally (in PostScript output) for compatibility and to keep the size of the
PostScript down in many common cases. Because ps_slur() exists, we have to
provide pdf_slur().

To avoid too many arguments, the rare left/right control point adjustment
parameters are placed in global variables (they are usually zero).

Arguments:
  x0         start x coordinate
  y0         start y coordinate
  x1         end x coordinate
  y1         end y coordinate
  flags      slur flags
  co         "centre out" adjustment

Returns:     nothing
*/

void
pdf_slur(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t flags,
  int32_t co)
{
double fx0, fx1, fy0, fy1, dx, dy;
double fco, width, fc, fs, v, wig;
double fclx, fcly, fcrx, fcry;
int32_t length = x1 - x0;

check_ETpending();
check_colour();

co = ((co + ((length > 20000)? 6000 : (length*6)/20)) * out_stavemagn)/1000;
if ((flags & sflag_b) != 0) co = -co;

/* The following computations implement what is done in PostScript code when
the output format is PostScript. That code works in the page coordinate system,
so we convert to that here first. We need floating point values for sqrt and
trig functions so we do all the following in floating point. */

fx0 = ((double)poutx(x0 + 3*out_stavemagn))/1000.0;
fx1 = ((double)poutx(x1 + 3*out_stavemagn))/1000.0;

fy0 = ((double)pouty(out_ystave - y0))/1000.0;
fy1 = ((double)pouty(out_ystave - y1))/1000.0;

fco = ((double)co)/1000.0;
fclx = ((double)out_slurclx)/1000.0;
fcly = ((double)out_slurcly)/1000.0;
fcrx = ((double)out_slurcrx)/1000.0;
fcry = ((double)out_slurcry)/1000.0;

dx = fx1 - fx0;
dy = fy1 - fy0;

width = sqrt(dy*dy + dx*dx)/2.0;

if (length == 0)   /* x0 == x1; 90 or 270 degree rotation */
  {
  fs = (y1 > y0)? 1.0 : -1.0;
  fc = 0.0;
  }
else
  {
  double angle = atan(dy/dx);
  fs = sin(angle);
  fc = cos(angle);
  }

v = (width > 15.0)? 10.0 : (2.0*width)/3.0;

/* Translate and rotate the coordinate system. In the new system the slur is
horizontal and the end points are close to the x-axis, equidistant from the
y-axis. */

pdf_gsave();
EO(obj_conts, "%s", SFD("%f %f %f %f %f %f cm\n", fc, fs, -fs, fc,
  (fx0+fx1)/2.0, (fy0+fy1)/2.0));

/* Handle an ordinary slur */

if ((flags & sflag_w) == 0)
  {
  wig = 1.0;
  EO(obj_conts, "%s 0.05 m", sfd(-width));
  EO(obj_conts, "%s", SFD("%f %f %f %f %f 0.05 c\n",
    v - width + fclx, fco + fcly, width - v + fcrx, fco + fcry, width));
  EO(obj_conts, "%s 0.05 l\n", sfd(width));
  EO(obj_conts, "%s", SFD("%f %f %f %f %f -0.05 c f\n",
    width - v + fcrx, fco + fcry - 1.0, v - width + fclx, fco + fcly - 1.0,
    -width));
  }

/* Handle a wiggly slur */

else
  {
  wig = -1.0;
  EO(obj_conts, "%s 0.05 m", sfd(-width));
  EO(obj_conts, "%s", SFD("%f %f %f %f %f 0.05 c\n",
    v - width + fclx, fco + fcly, width - v + fcrx, fcry - fco, width));
  EO(obj_conts, "%s -0.05 l\n", sfd(width));
  EO(obj_conts, "%s", SFD("%f %f %f %f %f -0.05 c f\n",
    width - v + fcrx, fcry - fco - 1.0, v - width + fcrx, fco + fcly - 1.0,
    -width));
  }

/* Add an editorial mark to the slur. This is trivial when non-wiggly and the y
control points are equal, but very messy otherwise. */

if ((flags & sflag_e) != 0)
  {
  check_linewidth(400);

  if ((flags & sflag_w) == 0 && out_slurcly == out_slurcry)
    {
    EO(obj_conts, "1 0 0 1 0 %s cm", sfd((fco + fcly) * 0.75));
    EO(obj_conts, "0 2 m 0 -2.8 l S\n");
    }

  /* Either the y control points are unequal or we are dealing with a wiggly
  slur. */

  else
    {
    double a, b, c, t1, t2;

    fx0 = v - width + fclx;
    fx1 = width - v + fcrx;
    fy0 = fco + fcly;
    fy1 = fco*wig + fcry;

    a = 2.0*width + (fx0 - fx1) * 3.0;
    b = (fx1 - 2.0*fx0 - width) * 3.0;
    c = (fx0 + width) * 3.0;

    fx0 = ((0.5*a + b) * 0.5 + c) * 0.5 - width;
    t1 = (1.5*a + 2.0*b) * 0.5 + c;

    a = (fy0 - fy1) * 3.0;
    b = 3.0*fy1 - 6.0*fy0;
    c = 3.0*fy0;

    fy0 = ((0.5*a + b) * 0.5 + c) * 0.5;
    t2 = (1.5*a + 2.0*b) * 0.5 + c;

    if (t1 < 0.00001)   /* 90 or 270 degree rotation */
      {
      fs = (t2 < 0.0)? -1.0 : 1.0;
      fc = 0.0;
      }
    else
      {
      double angle = atan(t2/t1);
      fs = sin(angle);
      fc = cos(angle);
      }

    EO(obj_conts, "%s", SFD("%f %f %f %f %f %f cm\n", fc, fs, -fs, fc, fx0,
      fy0));
    EO(obj_conts, "0 2 m 0 -2.8 l S\n");
    }
  }

pdf_grestore();
}



/*************************************************
*            Output a straight line              *
*************************************************/

/* The origin for y coordinates is in out_ystave, typically the bottom line of
a stave.

Arguments:
  x0          start x coordinate
  y0          start y coordinate
  x1          end x coordinate
  y1          end y coordinate
  thickness   line thickness
  flags       for various kinds of line

Returns:      nothing
*/

void
pdf_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t thickness,
  uint32_t flags)
{
double xx, yy, zz;
int32_t len;
int32_t dashlength = 0;
int32_t gaplength = 0;
int dashcount, spacecount;

check_ETpending();
check_colour();

/* For anything other than a plain unadorned line we need to do some
computation. */

if ((flags & (tief_editorial|tief_dashed|tief_dotted)) != 0)
  {
  xx = (double)((int32_t)(x1 - x0));
  yy = (double)((int32_t)(y1 - y0));
  zz = sqrt(xx*xx + yy*yy);
  len = (int32_t)zz;  /* Don't cast sqrt; it gives a compiler warning */
  }

/* Handle "editorial" lines: won't exist if dashed or dotted */

if ((flags & tief_editorial) != 0)
  {
  double angle = atan2(yy, xx);
  double fs = sin(angle);
  double fc = cos(angle);

  pdf_gsave();
  check_linewidth(400);
  EO(obj_conts, "%s %s %s cm 0 2 m 0 -2 l S",
    SFD("%f %f %f %f",fc, fs, -fs, fc),
    sff(poutx((x0+x1)/2)), sff(pouty(out_ystave - (y0+y1)/2)));
  pdf_grestore();
  }

/* Compute new dash parameters if required */

if ((flags & tief_dashed) != 0)
  {
  dashlength = 3*out_stavemagn;
  dashcount = (len/dashlength) | 1;
  spacecount = dashcount/2;
  if (dashcount != 1)
    {
    gaplength = (len - ((dashcount+1)*dashlength)/2)/spacecount;
    pdf_setdash(dashlength, gaplength);
    }
  }

else if ((flags & tief_dotted) != 0)
  {
  dashlength = 100;
  dashcount = (len + 4*out_stavemagn)/(4*out_stavemagn + dashlength);
  if (dashcount > 1)
    {
    gaplength = (len - dashcount * dashlength)/(dashcount - 1);
    pdf_setcapandjoin(caj_round);
    pdf_setdash(dashlength, gaplength);
    thickness = out_stavemagn;
    }
  }

/* Do the line and reset the dash parameters unless the savedash flag is set.
This is used while drawing jogs on line slurs. */

check_linewidth(thickness);
EO(obj_conts, "%s m %s l S\n",
  SFF("%f %f", poutx(x1), pouty(out_ystave - y1)),
  SFF("%f %f", poutx(x0), pouty(out_ystave - y0)));

if ((flags & tief_savedash) == 0) pdf_setdash(0, 0);
}



/*************************************************
*         Output a series of lines               *
*************************************************/

/* This is only used for sequences of plain lines (no dashes, etc.)

Arguments:
  x           vector of x coordinates
  y           vector of y coordinates
  count       number of vector elements
  thickness   line thickness

Returns:      nothing
*/

void
pdf_lines(int32_t *x, int32_t *y, int count, int32_t thickness)
{
check_ETpending();
check_linewidth(thickness);
check_colour();
EO(obj_conts, "%s", SFF("%f %f m", poutx(x[0]), pouty(out_ystave - y[0])));
for (int i = 1; i < count; i++)
  EO(obj_conts, "%s", SFF("%f %f l", poutx(x[i]), pouty(out_ystave - y[i])));
EO(obj_conts, "S\n");
}



/*************************************************
*         Stroke or fill a path                  *
*************************************************/

/* Used by pdf_path() and pdf_abspath() below.

Argument: line thickness for stroke; -1 for fill
Returns:  nothing
*/

static void
strokeorfill(int32_t thickness)
{
check_colour();
if (thickness >= 0)
  {
  check_linewidth(thickness);
  EO(obj_conts, "S\n");
  }
else EO(obj_conts, "f\n");
}



/*************************************************
*         Output and stroke or fill a path       *
*************************************************/

/* The path can contain moves, lines, and curves.

Arguments:
  x            vector of x coordinates
  y            vector of y coordinates
  c            vector of move/line/curve operators
  thickness    thickness of the lines for stroke; negative for fill

Returns:       nothing
*/

void
pdf_path(int32_t *x, int32_t *y, int *c, int32_t thickness)
{
check_ETpending();

while (*c != path_end) switch(*c++)
  {
  case path_move:
  EO(obj_conts, "%s", SFF("%f %f m", poutx(*x++), pouty(out_ystave - *y++)));
  break;

  case path_line:
  EO(obj_conts, "%s", SFF("%f %f l", poutx(*x++), pouty(out_ystave - *y++)));
  break;

  case path_curve:
  EO(obj_conts, "%s", SFF("%f %f %f %f %f %f c",
    poutx(x[0]), pouty(out_ystave - y[0]),
      poutx(x[1]), pouty(out_ystave - y[1]),
        poutx(x[2]), pouty(out_ystave - y[2])));
  x += 3;
  y += 3;
  break;
  }

strokeorfill(thickness);
}



/*************************************************
*   Output and stroke or fill an absolute path   *
*************************************************/

/* This function (similar to the one above) is used for fancy slurs, when the
coordinate system has been rotated and translated so that its origin is at the
centre of the slur with the x axis joining the endpoints. The coordinates must
therefore not use poutx/pouty.

Arguments:
  x            vector of x coordinates
  y            vector of y coordinates
  c            vector of move/line/curve operators
  thickness    thickness of the lines for stroke; negative for fill only

Returns:       nothing
*/

void
pdf_abspath(int32_t *x, int32_t *y, int *c, int32_t thickness)
{
check_ETpending();

while (*c != path_end) switch(*c++)
  {
  case path_move:
  EO(obj_conts, "%s", SFF("%f %f m", *x++, *y++));
  break;

  case path_line:
  EO(obj_conts, "%s", SFF("%f %f l", *x++, *y++));
  break;

  case path_curve:
  EO(obj_conts, "%s", SFF("%f %f %f %f %f %f c", x[0], y[0], x[1], y[1],
    x[2], y[2]));
  x += 3;
  y += 3;
  break;
  }

strokeorfill(thickness);
}



/*************************************************
*                   Set dash                     *
*************************************************/

/* The set values are remembered so that repetition is avoided.

Arguments:
  dashlength    the dash length
  gaplength     the gap length

Returns:        nothing
*/

void
pdf_setdash(int32_t dashlength, int32_t gaplength)
{
if (dashlength == out_dashlength && gaplength == out_dashgaplength) return;
if ((dashlength | gaplength) == 0) EO(obj_conts, "[]0 d");
  else EO(obj_conts, "%s", SFF("[%f %f]0 d", dashlength, gaplength));
out_dashlength = dashlength;
out_dashgaplength = gaplength;
}



/*************************************************
*                Set cap and join                *
*************************************************/

/* The set value is remembered so that repetition is avoided.

Argument: the cap and join flag bits
Returns:  nothing
*/

void
pdf_setcapandjoin(uint32_t caj)
{
if ((caj & (caj_round | caj_square)) != (setcaj & (caj_round | caj_square)))
  EO(obj_conts, "%d J\n", ((caj & caj_round) != 0)? 1 :
    ((caj & caj_square) != 0)? 2 : 0);

if ((caj & (caj_round_join | caj_bevel_join)) !=
    (setcaj & (caj_round_join | caj_bevel_join)))
  EO(obj_conts, "%d j\n", ((caj & caj_round_join) != 0)? 1 :
    ((caj & caj_bevel_join) != 0)? 2 : 0);

setcaj = caj;
}



/*************************************************
*            Gsave and Grestore                  *
*************************************************/

/* These functions are called from setslur.c when the coordinate system is
translated and rotated for the drawing of a fancy slur. They translate directly
into PDF gsave and grestore. We need to preserve and restore our remembered
graphic state parameters. Two of them are out_xxx variables that are referenced
in out_slur().

Arguments:  none
Returns:    nothing
*/

void
pdf_gsave(void)
{
check_ETpending();
savedcaj = setcaj;
savedlinewidth = setlinewidth;
saveddashlength = out_dashlength;
saveddashgaplength = out_dashgaplength;
EO(obj_conts, "q");
}

void
pdf_grestore(void)
{
EO(obj_conts, "Q\n");
setcaj = savedcaj;
setlinewidth = savedlinewidth;
out_dashlength = saveddashlength;
out_dashgaplength = saveddashgaplength;
}



/*************************************************
*                 Rotate                         *
*************************************************/

/* This function rotates the coordinate system.

Argument:   the amount to rotate, in radians
Returns:    nothing
*/

void
pdf_rotate(double r)
{
double s = sin(r);
double c = cos(r);
check_ETpending();
if (r != 0.0) EO(obj_conts, "%s", SFD("%f %f %f %f 0 0 cm\n", c, s, -s, c));
}



/*************************************************
*                  Translate                     *
*************************************************/

/* This function translates the coordinate system.

Arguments:
  x          x coordinate of the new origin
  y          y coordinate of the new origin

Returns:     nothing
*/

void
pdf_translate(int32_t x, int32_t y)
{
check_ETpending();
EO(obj_conts, "1 0 0 1 %s cm\n", SFF("%f %f", poutx(x), pouty(out_ystave - y)));
}



/*************************************************
*       Start a given bar for a given stave      *
*************************************************/

/* When testing, output an identifying comment.

Arguments:
  barnumber    the absolute bar number
  stave        the stave
*/

void
pdf_startbar(int barnumber, int stave)
{
if ((main_testing & mtest_barids) == 0) return;
EO(obj_conts, "\n%%%s/%d\n", sfb(curmovt->barvector[barnumber]), stave);
}


/* =========================================================== */
/* =========================================================== */


/*************************************************
*             Matrix operations                  *
*************************************************/

/* These functions apply scaling and transformation to a transformation matrix.
They are used to generate a single composite matrix at the start of each
physical page. */

static void
matrix_scale(double *mt, double scalex, double scaley)
{
mt[0] *= scalex;
mt[1] *= scalex;
mt[2] *= scaley;
mt[3] *= scaley;
}

static void
matrix_translate(double *mt, double x, double y)
{
mt[4] += x;
mt[5] += y;
}

static void
matrix_rotate(double *mt, double fs, double fc)
{
double r0 = fc * mt[0] + fs * mt[2];
double r1 = fc * mt[1] + fs * mt[3];
double r2 = fc * mt[2] - fs * mt[0];
double r3 = fc * mt[3] - fs * mt[1];

mt[0] = r0;
mt[1] = r1;
mt[2] = r2;
mt[3] = r3;
}



/*************************************************
*       Write out a font as an encoded stream    *
*************************************************/

/* OTF fonts need an additional entry in the stream's dictionary, provided by
the "subtype" argument. The font is encoded using the ASCII85 encoding, which
encodes each 4 bytes as a 32-bit number to be encoded in base 85 using ASCII
printable characters. There is special treatment of zero and for any trailing
bytes.

Arguments:
  f              the font file
  subtype        a subtype to be defined, or NULL
  length         the indirect object in which to put the length
  length_number  the number of the length object

Returns:   the number of characters written
*/

static uint32_t
write_font_stream(FILE *f, const char *subtype, pdfobject *length,
  uint32_t length_number)
{
char coded[6];
int c, n;
uint32_t acc;
uint32_t lenstart;
uint32_t filecount = 0;
uint32_t count = 0;

filecount += fprintf(out_file, "<</Filter/ASCII85Decode\n");
if (subtype != NULL) filecount += fprintf(out_file, "/Subtype/%s\n", subtype);
filecount += fprintf(out_file, "/Length %d 0 R>>\nstream\n", length_number);

acc = 0;
n = 0;
coded[5] = 0;
lenstart = filecount;

while ((c = fgetc(f)) != EOF)
  {
  acc = (acc << 8) | c;
  if (++n == 4)
    {
    if (acc == 0)
      {
      fputc('z', out_file);
      filecount++;
      count++;
      }
    else
      {
      for (int i = 4; i > 0; i--)
        {
        coded[i] = (acc % 85) + '!';
        acc /= 85;
        }
      coded[0] = acc + '!';
      filecount += fprintf(out_file, "%s", coded);

      if ((count += 5) >= 75)
        {
        filecount += fprintf(out_file, "\n");
        count = 0;
        }
      }
    n = 0;
    }
  }

/* Deal with final fragment */

if (n != 0)
  {
  acc <<= (4 - n) * 8;
  for (int i = 4; i > 0; i--)
    {
    coded[i] = (acc % 85) + '!';
    acc /= 85;
    }
  coded[0] = acc + '!';
  filecount += fprintf(out_file, "%.*s", n + 1, coded);
  }

/* Finally, the EOD sequence. We then have the length of the stream item and
can update the length object, which always follows the font object. Then end
the stream. */

filecount += fprintf(out_file, "~>");
EO(length, "%d\n", filecount - lenstart);  /* Update the length object */
filecount += fprintf(out_file, "\nendstream\n");
fclose(f);
return filecount;
}



/*************************************************
*           Free expandable data blocks          *
*************************************************/

/* This is called from the exit function in main.c so that it is run however
PMW exits. */

void
pdf_free_data(void)
{
for (pdfobject *p = obj_anchor; p != NULL; p = p->next)
  if (p->data != NULL) free(p->data);
}



/*************************************************
*                Produce PDF output              *
*************************************************/

/* This is the controlling function for generating PDF output.

Arguments: none
Returns:   nothing
*/

void
pdf_go(void)
{
int32_t w = 0, d = 0;
int32_t bboxx, bboxy;
int32_t filecount = 0;
int32_t pagecount = 0;
int32_t resources_number;
int32_t pages_number;
int32_t upperencoding_number = 0;
int32_t lowerencoding_number = 0;
int32_t musicdescriptor_number = 0;
int32_t musicbinary_number = 0;
int32_t scaled_main_sheetwidth =
  mac_muldiv(main_sheetwidth, print_magnification, 1000);
uschar standard_encoding_used[FONTWIDTHS_SIZE/8];
pdfobject *pages, *resources, *info;
BOOL shared_standard_encoding = FALSE;
MD5Context ctx;
char md5ID[33];

/* See comment on this function as to what it does. */

adjust_wide_stave_table();

/* Initialize the indirect function pointers for PostScript output. */

ofi_abspath = pdf_abspath;
ofi_barline = pdf_barline;
ofi_beam = pdf_beam;
ofi_brace = pdf_brace;
ofi_bracket = pdf_bracket;
ofi_getcolour = pout_getcolour;
ofi_grestore = pdf_grestore;
ofi_gsave = pdf_gsave;
ofi_line = pdf_line;
ofi_lines = pdf_lines;
ofi_muschar = pdf_muschar;
ofi_musstring = pdf_musstring;
ofi_path = pdf_path;
ofi_rotate = pdf_rotate;
ofi_setcapandjoin = pdf_setcapandjoin;
ofi_setcolour = pout_setcolour;
ofi_setdash = pdf_setdash;
ofi_setgray = pout_setgray;
ofi_slur = pdf_slur;
ofi_startbar = pdf_startbar;
ofi_stave = pdf_stave;
ofi_string = pdf_string;
ofi_translate = pdf_translate;

/* Initialize the current page number and page list data */

pout_setup_pagelist(print_reverse);
pout_set_ymax_etc(&w, &d);

/* Get values for MediaBox. These have to be the dimensions of the actual paper
when unrotated rather than the rotated and/or diminished values that PMW has
traditionally used. */

if (print_imposition == pc_a5ona4 || print_imposition == pc_a4ona3)
  {
  bboxx = main_sheetdepth;
  bboxy = 2 * main_sheetwidth;
  }

else if (main_landscape)
  {
  bboxx = main_sheetdepth;
  bboxy = main_sheetwidth;
  }
else
  {
  bboxx = main_sheetwidth;
  bboxy = main_sheetdepth;
  }

/* Adjust paper size to the magnification */

print_sheetwidth = mac_muldiv(main_sheetwidth, 1000, main_magnification);
pout_ymax = mac_muldiv(pout_ymax, 1000, main_magnification);

/* Scan the fonts, set the pdf id. This ensures that the unset "extra" fonts
use the "rm" name if used without being defined. */

for (int i = 0; i < font_tablen; i++)
  {
  int j;
  for (j = 0; j < i; j++) if (font_table[i] == font_table[j]) break;
  pdf_IdStrings[i] = font_IdStrings[j];
  }

/* Initializing stuff at the start of the PDF file. */

filecount += fprintf(out_file, "%%PDF-2.0\n%c%c%c%c%c\n", 0x25, 0xb5, 0xb5,
  0xb5, 0xb5);

/* Create the first PDF object block in memory by hand. This means that
new_object() can always assume that obj_last is not NULL. */

objectcount = 1;
obj_anchor = obj_last = mem_get(sizeof(pdfobject));
obj_last->next = NULL;
obj_last->file_offset = 0;  /* Will be updated later */
obj_last->base_size = 10;   /* Can be small, as only one string added */
obj_last->data_size = obj_last->data_used = 0;
obj_last->data = NULL;

/* Create an information dictionary. This is deprecated in PDF 2.0, but the
alternative is horribly messy and long XML metadata, which seems very much
overkill. Putting this at the top of the file makes it easily findable by
humans. */

info = new_object(150);

if ((main_testing & mtest_version) != 0)
  {
  EO(info, "<</Creator(PMW)>>\n");
  }
else
  {
  int len;
  char buff[100];
  time_t now = time(NULL);

  buff[0] = 'D';
  buff[1] = ':';

  len = 2 + strftime(buff + 2, sizeof(buff) - 2, "%Y%m%d%H%M%S%z",
    localtime(&now));

  /* Insert quote between time zone hours and minutes, as required by PDF. */

  buff[len+1] = 0;
  buff[len] = buff[len-1];
  buff[len-1] = buff[len-2];
  buff[len - 2] = '\'';

  EO(info, "<</Creator(PMW %s)\n/CreationDate(%s)>>\n", PMW_VERSION, buff);
  }

/* The anchor object is the catalog. Create its entry for the pages, which will
be the next object. */

EO(obj_anchor, "<</Type/Catalog\n/Pages %d 0 R>>\n", objectcount + 1);

/* Set up the Pages object */

pages = new_object(64);
pages_number = objectcount;
EO(pages, "<</Type/Pages/Kids[");

/* Set up the Resources object */

resources = new_object(100);
resources_number = objectcount;
EO(resources, "<</ProcSet[/PDF/Text]/Font<<\n");

/* Now the requested pages. We do this first so that a record can be kept of
which fonts and which characters are actually used. The pout_get_pages()
function returns one or two pages. When printing 2-up either one of them may be
null. Start with curmovt set to NULL so that a "change of movement" happens at
the start. */

curmovt = NULL;

for (;;)
  {
  pagestr *p_1stpage, *p_2ndpage;
  pdfobject *pagebase;
  double mt[6];
  int32_t scaled;
  BOOL recto;

  if (!pout_get_pages(&p_1stpage, &p_2ndpage)) break;  /* No more to output */

  /* Set up for a new page */

  scaled = 1000;
  recto = FALSE;
  mt[0] = mt[3] = 1.0;
  mt[1] = mt[2] = mt[4] = mt[5] = 0.0;

  setlinewidth = -1;   /* Unset */
  setcaj = 0;
  setfont = -1;

  /* Create a new page object and add it to the pages list. */

  pagebase = new_object(100);
  EO(pagebase, "<</Type/Page/Parent %d 0 R\n", pages_number);
  EO(pagebase, "/MediaBox[0 0 %s %s]\n", sff(bboxx), sff(bboxy));
  if (main_landscape) EO(pagebase, "/Rotate 270\n");

  EO(pages, "\n%d 0 R", objectcount);

  /* Create the contents object */

  obj_conts = new_object(1000);
  EO(obj_conts, "stream\n");

  /* Output a comment when testing; check for recto. */

  if (p_1stpage != NULL && p_2ndpage != NULL)
    {
    if ((main_testing & mtest_barids) != 0)
      EO(obj_conts, "%% ------ Pages %d & %d ------\n", p_1stpage->number,
        p_2ndpage->number);
    }
  else if (p_1stpage != NULL)
    {
    if ((main_testing & mtest_barids) != 0)
      EO(obj_conts, "%% ------ Page %d ------\n", p_1stpage->number);
    recto = (p_1stpage->number & 1) != 0;
    }
  else
    {
    if ((main_testing & mtest_barids) != 0)
      EO(obj_conts, "%% ------ Page %d ------\n", p_2ndpage->number);
    recto = (p_2ndpage->number & 1) != 0;
    }

  /* Save the graphics state. */

  EO(obj_conts, "q\n");

  /* Connect contents and resources to the page. */

  EO(pagebase, "/Contents %d 0 R\n", objectcount);
  EO(pagebase, "/Resources %d 0 R>>\n", resources_number);

  /* Swap pages for righttoleft. */

  if (main_righttoleft)
    {
    pagestr *temp = p_1stpage;
    p_1stpage = p_2ndpage;
    p_2ndpage = temp;
    }

  /* Move the origin to the desired position. A number of configurations that
  are available for PostScript printers do not make sense for PDFs. They should
  not get here, having been trapped on input. */

  switch (print_pageorigin)
    {
    case 0: /* A4 Sideways, 1-up, portrait */
    case 2: /* A4 Sideways, 2-up, portrait */
    case 4: /* A4 Sideways, 1-up, landscape */
    case 6: /* A4 Sideways, 2-up, landscape */
    error(ERR183);  /* Hard */
    break;

    case 1: /* Upright, 1-up, portrait */
    if (print_gutter != 0) matrix_translate(mt,
      (recto? +1.0 : -1.0) * (double)print_gutter/1000.0, 0.0);
    break;

    case 3: /* Upright, 2-up, portrait */
    matrix_translate(mt, 0.0, (double)(d - (d/2 - scaled_main_sheetwidth)/
      (print_pamphlet? 1:2))/1000.0);
    matrix_rotate(mt, -1.0, 0.0);
    break;

    case 5: /* Upright, 1-up, landscape; page size defined by sheetsize */
            /* Sheetwidth is original sheet height */
    matrix_translate(mt, 0.0, (double)scaled_main_sheetwidth/1000.0);
    matrix_rotate(mt, -1.0, 0.0);
    break;

    case 7: /* Upright, 2-up, landscape */
    matrix_translate(mt, 0.0, (double)(d/2)/1000.0);
    break;
    }

  if (print_image_xadjust != 0 || print_image_yadjust != 0)
    matrix_translate(mt, ((double)print_image_xadjust)/1000.0,
      ((double)print_image_yadjust)/1000.0);

  if (main_righttoleft)
    {
    matrix_translate(mt, ((double)scaled_main_sheetwidth)/1000.0, 0.0);
    matrix_scale(mt, -1.0, 1.0);
    }

  /* Adjust overall scaling. */

  if (main_magnification != 1000 || print_magnification != 1000)
    {
    scaled = mac_muldiv(main_magnification, print_magnification, 1000);
    matrix_scale(mt, ((double)scaled)/1000.0, ((double)scaled)/1000.0);
    }

  /* Output the scaling and positioning matrix transformation. We increase the
  precision for this because another decimal place in the magnification does
  make a noticeable different in the output. */

  string_double_precision = 3;
  EO(obj_conts, "%s", SFD("%f %f %f %f %f %f cm\n", mt[0], mt[1], mt[2], mt[3],
    mt[4], mt[5]));
  string_double_precision = 2;

  /* When printing 2-up, we may get one or both pages; when not printing 2-up,
  we may get either page given, but not both. The colour always reverts to
  black at the start of a PDF page. */

  if (p_1stpage != NULL)
    {
    pout_curcolour[0] = pout_curcolour[1] = pout_curcolour[2] = 0;
    curpage = p_1stpage;
    out_page();
    check_ETpending();       /* Close incomplete text */
    }

  if (p_2ndpage != NULL)
    {
    if (print_imposition == pc_a5ona4 || print_imposition == pc_a4ona3)
      {
      int sign = main_righttoleft? -1 : +1;
      int32_t dd = mac_muldiv(d, 500, scaled);
      if (main_landscape)
        {
        EO(obj_conts, "1 0 0 1 0 %s cm\n", sff(-dd));
        }
      else
        {
        EO(obj_conts, "1 0 0 1 %s 0 cm\n", sff(sign * (print_pamphlet?
          mac_muldiv(main_sheetwidth, 1000, main_magnification) : dd)));
        }
      }
    curpage = p_2ndpage;
    pout_curcolour[0] = pout_curcolour[1] = pout_curcolour[2] = 0;
    out_page();
    check_ETpending();       /* Close incomplete text */
    }

  EO(obj_conts, "Q\n");    /* Pops the graphics state */
  pagecount++;
  }

/* Add closing text to Pages object */

EO(pages, "]\n/Count %d>>\n", pagecount);

/* If the music font was used, create a placeholder object that represents it,
along with a second placeholder for its length. At the output stage these will
be filled in. Then create a font descriptor for the music font. The "8" flag is
"glyphs resemble cursive handwriting" and "4" is "symbolic". */

if (music_font_used)
  {
  (void)new_fixed_object("*Font PMW-Music\n");
  musicbinary_number = objectcount;
  (void) new_object(10);   /* Length placeholder */

  EO(new_object(150),
     "<</Type/FontDescriptor\n"
     "/FontName/PMW-Music\n"
     "/Flags 12\n"
     "/FontBBox[-70 -656 1176 2219]\n"
     "/Ascent 2219\n"
     "/Descent -656\n"
     "/CapHeight 2219\n"
     "/ItalicAngle 0\n"
     "/StemV 176\n"
     "/FontFile3 %d 0 R\n>>\n", musicbinary_number);

  musicdescriptor_number = objectcount;
  }

/* See if any standardly encoded fonts were used without recoding via a .utr
file (this is likely to be common). If so, amalgamate all the used bit maps and
create a unified encoding that they all can used. */

for (int i = 0; i < font_tablen; i++)
  {
  int j;
  fontstr *fs;

  for (j = 0; j < i; j++) if (font_table[i] == font_table[j]) break;
  if (j != i) continue;   /* Seen this one already */

  fs = font_list + font_table[i];             /* Font structure */
  if ((fs->flags & ff_used) == 0) continue;   /* Omit if not actually used */

  if ((fs->flags & ff_stdencoding) != 0 && fs->encoding == NULL)
    {
    if (!shared_standard_encoding)
      memset(standard_encoding_used, 0, FONTWIDTHS_SIZE/8);
    for (int k = 0; k < FONTWIDTHS_SIZE/8; k++)
      standard_encoding_used[k] |= fs->used[k];
    shared_standard_encoding = TRUE;
    }
  }

/* Set up shared standard font encoding objects if needed. */

if (shared_standard_encoding)
  {
  lowerencoding_number = make_encoding(0, 255, standard_encoding_used,
    lower_names, NULL);
  upperencoding_number = make_encoding(256, 511, standard_encoding_used,
    upper_names, NULL);
  }

/* Set up the font objects that are needed. */

for (int i = 0; i < font_tablen; i++)
  {
  int j;
  fontstr *fs;
  uschar *ID;

  for (j = 0; j < i; j++) if (font_table[i] == font_table[j]) break;
  if (j != i) continue;   /* Seen this one already */

  fs = font_list + font_table[i];             /* Font structure */
  if ((fs->flags & ff_used) == 0) continue;   /* Omit if not actually used */

  ID = pdf_IdStrings[j];   /* e.g. "rm" */

  /* A standardly encoded font is encoded as Unicode and extended using a
  second binding with different encoding when wide characters are used.
  However, if a .utr file created custom encoding we have to create a unique
  encoding for this font. If this is not one of the 14 standard fonts, a font
  item will be needed. */

  if ((fs->flags & ff_stdencoding) != 0)
    {
    int ffnum = is_standard_font(fs->name)? 0 : make_fileobject(fs);

    if ((fs->flags & ff_usedlower) != 0)
      make_font(resources, fs, "", ID, fs->firstcharL, fs->lastcharL,
        (fs->encoding == NULL)? lowerencoding_number :
        make_encoding(0, 255, fs->used, lower_names, fs->encoding), 0, ffnum);

    if ((fs->flags & ff_usedupper) != 0)
      make_font(resources, fs, "X", ID, fs->firstcharU, fs->lastcharU,
        (fs->encoding == NULL)? upperencoding_number :
        make_encoding(256, 511, fs->used, upper_names, fs->encoding), 0, ffnum);
    }

  /* The music font uses only low-valued codes. */

  else if (Ustrcmp(fs->name, "PMW-Music") == 0 &&
           (fs->flags & ff_usedlower) != 0)
    {
    make_font(resources, fs, "", ID, fs->firstcharL, fs->lastcharL,
      make_encoding(0, sizeof(music_names)/sizeof(char *), fs->used,
        music_names, NULL), musicdescriptor_number,
        (musicbinary_number << 8) | '3');
    }

  /* Neither music nor standardly encoded. Characters in the upper half may be
  specified by .utr translation. There may be no existing widths or encodings,
  so duplicate the lower half for unset values because this mimics what
  the PostScript output code does. Again, if this it not one of the 14 standard
  fonts, a font item will be needed. */

  else
    {
    int ffnum = is_standard_font(fs->name)? 0 : make_fileobject(fs);

    if ((fs->flags & ff_usedlower) != 0)
      make_font(resources, fs, "", ID, fs->firstcharL, fs->lastcharL,
        (fs->encoding == NULL)? 0 : make_encoding(0, 255, fs->used,
          (const char **)fs->encoding, NULL),
        0, ffnum);

    if ((fs->flags & ff_usedupper) != 0)
      {
      for (int k = 0; k < 256; k++)
        {
        if (fs->widths[k+256] < 0) fs->widths[k+256] = fs->widths[k];
        }

      if (fs->encoding != NULL)
        {
        for (int k = 0; k < 256; k++)
          {
          if (fs->encoding[k+256] == NULL)
            fs->encoding[k+256] = fs->encoding[k];
          }
        }

      make_font(resources, fs, "X", ID, fs->firstcharU, fs->lastcharU,
        (fs->encoding == NULL)? 0 : make_encoding(256, 511, fs->used,
          (const char **)(fs->encoding + 256), NULL),
        0, ffnum);
      }
    }
  }

/* Add closing text to Resources object. */

EO(resources, ">> >>\n");

/* Compute an MD5 hash to act as an ID for the PDF, as required by ISO 32000-2
for PDF 2.0. */

md5Init(&ctx);
for (pdfobject *p = obj_anchor; p != NULL; p = p->next)
  {
  if (p->data_used > 0) md5Update(&ctx, (uint8_t *)(p->data), p->data_used);
  }
md5Finalize(&ctx);
for (int i = 0; i < 16; i++) sprintf(md5ID + 2*i, "%02x", ctx.digest[i]);
md5ID[32] = 0;

/* Write out the objects, setting the sizes of stream objects, and remembering
the offsets of all objects. Free each object data memory after writing. */

objectcount = 1;
for (pdfobject *p = obj_anchor; p != NULL; p = p->next)
  {
  p->file_offset = filecount;                    /* Save for index */
  filecount += fprintf(out_file, "%d 0 obj\n", objectcount++);

  /* If the object starts with the text "*Font PMW-Music" it is a placeholder
  for inserting the music font. There's a testing option for omitting this,
  because it's pointless having a copy in every test file output. */

  if (p->data_used >= 15 && Ustrncmp(p->data, "*Font PMW-Music", 15) == 0 &&
        (main_testing & mtest_omitfont) == 0)
    {
    uschar buffer[256];
    FILE *f = font_finddata(US "PMW-Music", ".otf", font_music_extra,
      font_music_default, buffer, TRUE);
    filecount += write_font_stream(f, "OpenType", p->next, objectcount);
    }

  /* If the object starts with "*FontOTF" it is a placeholder for inserting an
  OTF font, whose open file is indexed by the number that follows. */

  else if (p->data_used >= 8 && Ustrncmp(p->data, "*FontOTF", 8) == 0 &&
             (main_testing & mtest_omitfont) == 0)
    {
    FILE *f = font_files[atoi((char *)(p->data + 8))];
    filecount += write_font_stream(f, "OpenType", p->next, objectcount);
    }

  /* For other objects, the data is in memory. In the case of an object that
  starts with "stream" we must add the length. */

  else if (p->data_used >=7 && Ustrncmp(p->data, "stream\n", 7) == 0)
    {
    filecount += fprintf(out_file, "<</Length %lu>>\n", p->data_used - 7);
    filecount += fwrite(p->data, 1, p->data_used, out_file);
    filecount += fprintf(out_file, "endstream\n");
    }

  /* Not a stream object. */

  else filecount += fwrite(p->data, 1, p->data_used, out_file);

  /* Terminate the object */

  filecount += fprintf(out_file, "endobj\n");
  }

/* At this point, objectcount is one more than the number of objects, which is
exactly the value we need for the xref and trailer items. The current value of
filecount is the offset of the xref object. There is no need to increase it
further. First write the xref table. */

(void)fprintf(out_file, "xref\n0 %d\n", objectcount);
(void)fprintf(out_file, "0000000000 65535 f\r\n");

for (pdfobject *p = obj_anchor; p != NULL; p = p->next)
  fprintf(out_file, "%010lu 00000 n\r\n", p->file_offset);

/* Now write the trailer, which contains (inter alia) the number of objects and
where to start. */

fprintf(out_file, "trailer\n<</Size %d/Root 1 0 R/Info 2 0 R\n", objectcount);
fprintf(out_file, "/ID[<%s><%s>]>>\n", md5ID, md5ID);

/* Finally, the offset to the start of the xref table. */

fprintf(out_file, "startxref\n%u\n%%%%EOF\n", filecount);
}

/* End of pdf.c */
