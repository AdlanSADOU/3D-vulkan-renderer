#if !defined(SHADER_H)
#define SHADER_H
#pragma once

#include <GL/glew.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <SDL2/SDL_log.h>
#include "mymath.h"

#define LOG_SHADER_SOURCE 0

struct Shader
{
    uint32_t programID    = {};
    uint32_t vertShaderID = {};
    uint32_t fragShaderID = {};
    char *   vertShaderPath {};
    char *   fragShaderPath {};
};

static void ShaderDestroy(Shader *shader)
{
    free(shader->fragShaderPath);
    free(shader->vertShaderPath);
    free(shader);
}

static uint32_t ShaderSourceLoadAndCompile(const char *sourcePath, GLenum TYPE)
{
    uint32_t ID = -1;

    FILE *f = fopen(sourcePath, "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *sourceCode = (char *)malloc(fsize + 1);
    fread(sourceCode, fsize, 1, f);
    fclose(f);

    sourceCode[fsize] = 0;

#if LOG_SHADER_SOURCE
    SDL_Log(sourceCode);
#endif

    ID = glCreateShader(TYPE);
    glShaderSource(ID, 1, &sourceCode, NULL);
    glCompileShader(ID);

    int  success;
    char infoLog[512];
    glGetShaderiv(ID, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(ID, 512, NULL, infoLog);
        SDL_Log("%s : Failed shader compilation: %s\n", sourcePath, infoLog);
        return ID;
    }

    return ID;
}


static Shader *CreateAndLinkProgram(const char *vertShaderPath, const char *fragShaderPath)
{
    int     success = 0;
    char    infoLog[512];
    Shader *s {};

    uint32_t vertShaderID = ShaderSourceLoadAndCompile(vertShaderPath, GL_VERTEX_SHADER);
    uint32_t fragShaderID = ShaderSourceLoadAndCompile(fragShaderPath, GL_FRAGMENT_SHADER);

    if (vertShaderID && fragShaderID)
        s = (Shader *)malloc(sizeof(Shader));

    s->programID = glCreateProgram();

    glAttachShader(s->programID, vertShaderID);
    glAttachShader(s->programID, fragShaderID);

    glLinkProgram(s->programID);

    glGetProgramiv(s->programID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(s->programID, 512, NULL, infoLog);
        SDL_Log("%s : Failed to link shaders: %s\n", infoLog);
        free(s);
        return NULL;
    }

    glGetProgramiv(s->programID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(s->programID, 512, NULL, infoLog);
        SDL_Log("%s : Failed to link shaders: %s\n", infoLog);
        free(s);
        return NULL;
    }
    SDL_Log("Shader link done");

    return s;
}

void ShaderUse(uint32_t programID)
{
    glUseProgram(programID);
}

void UnuseProgram()
{
    glUseProgram(0);
}

void setBool(const std::string &name, bool value);
void setInt(const std::string &name, int value);
void setFloat(const std::string &name, float value)
{
}

static GLint GetUniformLocation(const char *name, uint32_t programID)
{
    return glGetUniformLocation(programID, name);
}

void ShaderSetMat4ByName(const char *name, glm::mat4 matrix, uint32_t programID)
{
    GLint location = GetUniformLocation(name, programID);
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void SetUniformMatrix4Name(const char *name, const Matrix4 matrix, uint32_t programID)
{
    // we could just hardcode locations to avoid querrying for names each frame..
    GLint location = GetUniformLocation(name, programID);
    glUniformMatrix4fv(location, 1, GL_FALSE, matrix.m);
}


#endif // SHADER_H
