#pragma once

#include <glad/glad.h>
#include <iostream>

inline GLuint compile_shader(GLenum type, const char* source)
{
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    GLint success = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE)
    {
        char log[512];
        glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error: " << log << "\n";
    }
    return id;
}

inline GLuint create_program(const char* vs_src, const char* fs_src)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}
