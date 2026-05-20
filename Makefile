all: build/miniml

build/miniml: build
	cd build; ninja

build:
	mkdir -p build; cd build; cmake -G Ninja .. -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm@16/bin/clang \
	    -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm@16/bin/clang++ \
	    -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/llvm@16" \
	    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	    -DCMAKE_CXX_STANDARD=20

test:
	cd build; ctest

clean:
	rm -rf out/* *.trieste build/

.PHONY: clean all build/miniml test 

