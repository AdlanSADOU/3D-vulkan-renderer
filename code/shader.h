#if !defined(SHADER_H)
#define SHADER_H
#pragma once

#include <GL/glew.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <SDL2/SDL_log.h>

#include "mymath.h"

#define LOG_SHADER_SOURCE 0

struct Shader
{
    const char * m_sourcePath = 0;
    unsigned int m_ID         = 0;
    std::string  m_source     = {};

    unsigned int CreateAndCompile(const char *sourcePath, GLenum TYPE)
    {
        m_sourcePath = sourcePath;

        std::ifstream shaderFile;
        shaderFile.open(sourcePath);

        std::stringstream fileStream;

        fileStream << shaderFile.rdbuf();
        shaderFile.close();

        m_source = fileStream.str();
        if (shaderFile.fail()) {
            SDL_Log("%s : Failed to load\n", sourcePath);
            return 0;
        }

        const GLchar *shaderSource = m_source.c_str();
        SDL_Log("Loaded shader: %s\n", sourcePath);

#if LOG_SHADER_SOURCE
        SDL_Log(shaderSource);
#endif

        m_ID = glCreateShader(TYPE);
        glShaderSource(m_ID, 1, &shaderSource, NULL);
        glCompileShader(m_ID);

        int  success;
        char infoLog[512];
        glGetShaderiv(m_ID, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(m_ID, 512, NULL, infoLog);
            SDL_Log("%s : Failed shader compilation: %s\n", m_sourcePath, infoLog);
            return 0;
        }

        return m_ID;
    };
};





struct ShaderProgram
{
    unsigned int          _programId = 0;
    std::vector<Shader *> _shaders   = {};

    void AddShader(Shader *shader)
    {
        _shaders.push_back(shader);
    }

    bool CreateAndLinkProgram()
    {
        int  success = 0;
        char infoLog[512];

        _programId = glCreateProgram();

        for (auto shader : _shaders) {
            glAttachShader(_programId, shader->m_ID);
        }

        glLinkProgram(_programId);

        glGetProgramiv(_programId, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(_programId, 512, NULL, infoLog);
            SDL_Log("%s : Failed to link shaders: %s\n", infoLog);
            return 0;
        }

        glGetProgramiv(_programId, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(_programId, 512, NULL, infoLog);
            SDL_Log("%s : Failed to link shaders: %s\n", infoLog);
            return 0;
        }
        SDL_Log("Shader link done");
        return 1;
    }

    void UseProgram()
    {
        glUseProgram(_programId);
    }

    void UnuseProgram()
    {
        glUseProgram(0);
    }

    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const
    {
    }

    GLint GetUniformLocation(const char *name)
    {
        return glGetUniformLocation(_programId, name);
    }

    // todo(ad): instead of making to calls we could do just 1
    // by caching the location upfront, for vertex attributes, they wont change anyway
    void SetUniformMatrix4Name(const char *name, glm::mat4 matrix)
    {
        GLint location = GetUniformLocation(name);
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }

    void SetUniformMatrix4Name(const char *name, Matrix4 matrix)
    {
        GLint location = GetUniformLocation(name);
        glUniformMatrix4fv(location, 1, GL_TRUE, matrix.m);
    }
};

#endif // SHADER_H
