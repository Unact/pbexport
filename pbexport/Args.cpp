#include "Args.h"

Args::Args(int argc, char* argv[]): full(false), help(false), pbl2src(false), src2pbl(false), sync(false), recompCount(0)
{
	for (int i=1; i<argc; i++) {
		if (!strncmp(argv[i], "--full", 7) ) full = true;		
		if (!strncmp(argv[i], "--help", 7) ) help = true;
		if (!strncmp(argv[i], "/?", 3) ) help = true;
		if (!strncmp(argv[i], "--pbl2src", 10) ) pbl2src = true;
		if (!strncmp(argv[i], "--src2pbl", 10) ) src2pbl = true;
		if (!strncmp(argv[i], "--sync", 6) ) sync = true;
		if (!strncmp(argv[i], "--appl-", 7) ) appl = argv[i]+7;
		if (!strncmp(argv[i], "--pbl-", 6) ) pbl = argv[i]+6;
		if (!strncmp(argv[i], "--src-", 6) ) src = argv[i]+6;
		if (!strncmp(argv[i], "--recomp-", 9) ) recompCount = atoi(argv[i]+9);
		if (!strncmp(argv[i], "--backup-", 9) ) backup = argv[i] + 9;
	};
};