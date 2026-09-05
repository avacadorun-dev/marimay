#ifndef MMAY_CLI_BYTECODE_REPORT_H
#define MMAY_CLI_BYTECODE_REPORT_H

#include "mmay/bytecode/codec.h"

/* Static, allocation-free reasons shared by run and test diagnostics. */
const char *mmay_cli_bytecode_codec_message(MarimayBytecodeCodecStatus status);

#endif
