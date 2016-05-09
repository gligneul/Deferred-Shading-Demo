# The MIT License (MIT)
# 
# Copyright (c) 2016 Gabriel de Quadros Ligneul
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

target=app
cc=g++
#opt=-O2
opt=-g -O0
iflags=-I./lib
cflags=-Wall -Werror -std=c++11 $(shell pkg-config --cflags glfw3)
lflags=-lGLEW $(shell pkg-config --static --libs glfw3)
src=$(wildcard *.cpp)
obj=$(patsubst %.cpp,%.o,$(src))
libobjs=$(patsubst %.cpp,%.o,$(wildcard lib/*.cpp))

all: $(target)

$(target): $(obj) $(libobjs)
	$(cc) -o $@ $^ $(lflags)

%.o: %.cpp
	$(cc) $(cflags) $(iflags) $(opt) -c -o $@ $<

depend: $(src)
	@$(cc) $(cflags) -MM $^
	
clean:
	rm -rf *.o $(target)

.PHONY: all depend clean libs

# Generated by `make depend`
Texture2D.o: Texture2D.cpp Texture2D.h
Manipulator.o: Manipulator.cpp Manipulator.h
ShaderProgram.o: ShaderProgram.cpp ShaderProgram.h
VertexArray.o: VertexArray.cpp VertexArray.h
Materials.o: Materials.cpp Materials.h
main.o: main.cpp Manipulator.h Materials.h ShaderProgram.h VertexArray.h \
 Texture2D.h
