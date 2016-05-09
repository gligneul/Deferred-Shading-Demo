/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2016 Gabriel de Quadros Ligneul
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <cstring>

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

#include "UniformBuffer.h"

UniforBuffer::UniforBuffer() :
    ubo_(0),
    padding_(0) {
}

UniforBuffer::~UniforBuffer() {
    if (ubo_)
        glDeleteBuffers(1, &ubo_);
}

void UniforBuffer::Init() {
    glGenBuffers(1, &ubo_);
}

template<typename T>
void UniforBuffer::Add(T element) {
    AddToBuffer(&element, sizeof(T));
}

template<typename T>
void UniforBuffer::Add(T *elements, int n) {
    AddToBuffer(elements, n * sizeof(T));
}

void UniforBuffer::Add(glm::vec3 element) {
    AddToBuffer(glm::value_ptr(element), 3 * sizeof(float));
}

void UniforBuffer::Add(glm::vec4 element) {
    AddToBuffer(glm::value_ptr(element), 4 * sizeof(float));
}

void UniforBuffer::Add(glm::mat4 element) {
    AddToBuffer(glm::value_ptr(element), 16 * sizeof(float));
}

void UniforBuffer::FinishChunk() {
    if (padding_ == 0)
        return;
    for (int i = padding_; i < 16; ++i)
        buffer_.push_back(0);
    padding_ = 0;
}

void UniforBuffer::SendToDevice() {
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    glBufferData(GL_UNIFORM_BUFFER, buffer_.size(), buffer_.data(),
            GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

unsigned int UniforBuffer::GetId() {
    return ubo_;
}

void UniforBuffer::CheckChunk(int n) {
    if (padding_ + n > 16)
        FinishChunk();
}

void UniforBuffer::AddToBuffer(void *data, int size) {
    unsigned char bytes[size];
    CheckChunk(size);
    memcpy(bytes, data, size);
    for (int i = 0; i < size; ++i)
        buffer_.push_back(bytes[i]);
    padding_ = (padding_ + size) % 16;
}

template void UniforBuffer::Add(int);
template void UniforBuffer::Add(float);
template void UniforBuffer::Add(float *, int);
