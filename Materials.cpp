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

#include <GL/glew.h>

#include "Materials.h"

Materials::Materials() :
    ubo_(0) {
}

Materials::~Materials() {
    if (ubo_)
        glDeleteBuffers(1, &ubo_);
}

void Materials::Add(float dr, float dg, float db, float ar, float ag, float ab,
                    float sr, float sg, float sb, float shininess) {
    Material m;
    m.diffuse[0] = dr;
    m.diffuse[1] = dg;
    m.diffuse[2] = db;
    m.ambient[0] = ar;
    m.ambient[1] = ag;
    m.ambient[2] = ab;
    m.specular[0] = sr;
    m.specular[1] = sg;
    m.specular[2] = sb;
    m.shininess = shininess;
    materials_.push_back(m);
}

void Materials::Reload() {
    if (!ubo_)
        glGenBuffers(1, &ubo_);

    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    auto size = sizeof(Material) * materials_.size();
    auto data = materials_.data();
    glBufferData(GL_UNIFORM_BUFFER, size, data, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

unsigned int Materials::GetId() {
    return ubo_;
}


