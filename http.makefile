bin/http-cat: src/http/http-cat.util.o
bin/http-cat: src/http/client.o
bin/http-cat: src/log/log.o
bin/http-cat: src/array/range_atozd.o
bin/http-cat: src/array/range_streq_string.o
bin/http-cat: src/window/alloc.o
bin/http-cat: src/convert/def.o
bin/http-cat: src/convert/fd.o
bin/http-cat: src/array/range.o
bin/http-cat: src/network/network.o
bin/http-cat: src/convert/getline.o
test/run-http-cat: src/http/test/run-http-cat.sh

http-utils: bin/http-cat
utils: http-utils

http-tests: test/run-http-cat
tests: http-tests

C_PROGRAMS += bin/http-cat
SH_PROGRAMS += test/run-http-cat

RUN_TESTS += test/run-http-cat
