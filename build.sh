#!/bin/sh

make re EXT_OFLAGS="-fprofile-generate" EXT_LFLAGS="-lgcov"

./stash-bot << EOF
position startpos moves e2e4 e7e5 g1f3 b8c6
go movetime 15000
EOF

rm $(find objects \( -name "*.o" \) )

make EXT_OFLAGS="-fprofile-use -fno-peel-loops -fno-tracer" \
	EXT_LFLAGS="-lgcov"