.PHONY: all build run test clean

all: build

build:
	@mkdir -p bin
	pith build src/kconfig.pi src/composite.pi src/walker.pi src/main.pi -o bin/thornk

run: build
	./thornk test/fixtures/Kbuild.sample thorn.build

test: build
	./test/test_thornk.sh

clean:
	rm -rf bin out thorn.build build.ninja Makefile.posix thornk_bin .ninja_deps .ninja_log test/out
