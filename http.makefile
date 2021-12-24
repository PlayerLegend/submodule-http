bin/http-cat: src/http/http-cat.util.o
bin/http-cat: src/http/client.o
bin/http-cat: src/log/log.o
bin/http-cat: src/range/range_atozd.o
bin/http-cat: src/range/strchr.o
bin/http-cat: src/range/streq.o
bin/http-cat: src/convert/getline.o
bin/http-cat: src/range/strstr.o
bin/http-cat: src/range/range_streq_string.o
bin/http-cat: src/window/alloc.o
bin/http-cat: src/convert/source.o
bin/http-cat: src/convert/fd/source.o
bin/http-cat: src/convert/sink.o
bin/http-cat: src/convert/fd/sink.o
bin/http-cat: src/convert/duplex.o
bin/http-cat: src/network/network.o
bin/http-cat: src/convert/getline.o
test/run-http-cat: src/http/test/run-http-cat.sh

http-utils: bin/http-cat

http-tests: test/run-http-cat
tests: http-tests

C_PROGRAMS += bin/http-cat
SH_PROGRAMS += test/run-http-cat

RUN_TESTS += test/run-http-cat
