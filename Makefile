.PHONY: all run test clean

all: run

run:
	./thornk fixtures/Kbuild.sample thorn.build

test:
	./tests/test_thornk.sh

clean:
	rm -f thorn.build build.ninja Makefile.posix
