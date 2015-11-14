// copyright 2014 bitpay inc.
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

//
// to re-create univalue_escapes.h:
// $ g++ -o gen gen.cpp
// $ ./gen > univalue_escapes.h
//

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "univalue.h"

using namespace std;

static bool initescapes;
static const char *escapes[256];

static void initjsonescape()
{
    escapes[(int)'"'] = "\\\"";
    escapes[(int)'\\'] = "\\\\";
    escapes[(int)'/'] = "\\/";
    escapes[(int)'\b'] = "\\b";
    escapes[(int)'\f'] = "\\f";
    escapes[(int)'\n'] = "\\n";
    escapes[(int)'\r'] = "\\r";
    escapes[(int)'\t'] = "\\t";

    initescapes = true;
}

static void outputescape()
{
	printf(	"// automatically generated file. do not modify.\n"
		"#ifndef moorecoin_univalue_univalue_escapes_h\n"
		"#define moorecoin_univalue_univalue_escapes_h\n"
		"static const char *escapes[256] = {\n");

	for (unsigned int i = 0; i < 256; i++) {
		if (!escapes[i]) {
			printf("\tnull,\n");
		} else {
			printf("\t\"");

			unsigned int si;
			for (si = 0; si < strlen(escapes[i]); si++) {
				char ch = escapes[i][si];
				switch (ch) {
				case '"':
					printf("\\\"");
					break;
				case '\\':
					printf("\\\\");
					break;
				default:
					printf("%c", escapes[i][si]);
					break;
				}
			}

			printf("\",\n");
		}
	}

	printf(	"};\n"
		"#endif // moorecoin_univalue_univalue_escapes_h\n");
}

int main (int argc, char *argv[])
{
	initjsonescape();
	outputescape();
	return 0;
}

