all: build/miniml

build/miniml: build
	cd build; ninja

build:
	mkdir -p build; cd build; cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug

test:
	cd build; ctest

clean:
	rm -rf out/* *.trieste build/

.PHONY: clean all build/miniml test 

