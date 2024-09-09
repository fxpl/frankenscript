all: build/miniml

build/miniml: build
	cd build; ninja

build:
	mkdir -p build; cd build; cmake -G Ninja ..

buildf: 
	mkdir -p out; touch out/$f.trieste; ./build/miniml build ./programs/$f.miniml -o ./out/$f.trieste 
	
buildp: 
	touch out/$f.trieste; ./build/miniml build programs/$f.miniml -o out/$f.trieste -p $p  

test:
	cd build; ctest

clean:
	rm -rf out/* *.trieste build/

.PHONY: clean all build/miniml test 

