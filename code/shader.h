#pragma once

#include <GL/glew.h>


#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <SDL2/SDL_log.h>

#include "mymath.h"

#define LOG_SHADER_SOURCE 0

struct Shader
{
    uint32_t programID = {};
    char    *sourcePath {};
};

static void ShaderDestroy(Shader *shader)
{
    free(shader);
}

static size_t strContains(size_t fromOffset, const char *str, size_t strLength, char *src, size_t srcLength)
{
    assert(fromOffset >= 0 && "Negative offset");
    assert(fromOffset <= srcLength && "Starting offset greater than source buffer length");
    assert(fromOffset + strLength <= srcLength && "Starting offset greater than source buffer length");

    size_t i = 0;
    for (; i < (srcLength - fromOffset); i++) {
        if (src[i + fromOffset] == str[0]) {
            if (strncmp(str, src + i + fromOffset, strLength) == 0) {
                break;
            }
        }
    }

    assert(i + fromOffset <= srcLength && "Final position greater than source buffer length");

    return i + fromOffset;
}

static Shader *LoadShader(const char *sourcePath)
{

    FILE *f = fopen(sourcePath, "rb");
    if (!f) {
        SDL_Log("Could not find path to %s : %s", sourcePath, strerror(errno));
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *sourceCode = (char *)malloc(fsize);
    assert(sourceCode);
    fread(sourceCode, fsize, 1, f);
    fclose(f);
    // sourceCode[fsize] = 0;





    /**
     * Parsing the combined vertex/frag shader
     */
#define V_START "#VERT_START"
#define V_END   "#VERT_END"
#define F_START "#FRAG_START"
#define F_END   "#FRAG_END"

    size_t vertStartPos = strContains(0, V_START, strlen(V_START), sourceCode, fsize);
    size_t vertEndPos   = strContains(vertStartPos, V_END, strlen(V_END), sourceCode, fsize);
    size_t fragStartPos = strContains(vertEndPos, F_START, strlen(F_START), sourceCode, fsize);
    size_t fragEndPos   = strContains(fragStartPos, F_END, strlen(F_END), sourceCode, fsize);

    size_t vertSourceSize = vertEndPos - vertStartPos - strlen(V_START);
    size_t fragSourceSize = fragEndPos - fragStartPos - strlen(F_START);

    char *vertSource;
    char *fragSource;
    vertSource = (char *)malloc(sizeof(char) * vertSourceSize + 1);
    fragSource = (char *)malloc(sizeof(char) * fragSourceSize + 1);

    if (!vertSource || !fragSource) {
        SDL_Log("Failed to allocate space for vertex or fragment source");
        return NULL;
    }

    strncpy(vertSource, sourceCode + vertStartPos + strlen(V_START), vertSourceSize);
    strncpy(fragSource, sourceCode + fragStartPos + strlen(F_START), fragSourceSize);
    vertSource[vertSourceSize] = 0;
    fragSource[fragSourceSize] = 0;

    free(sourceCode);





    uint32_t vertID = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertID, 1, &vertSource, NULL);
    glCompileShader(vertID);

    {
        int  success;
        char infoLog[512];
        glGetShaderiv(vertID, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vertID, 512, NULL, infoLog);
            SDL_Log("%s : Vertex shader compilation failed : %s\n", sourcePath, infoLog);
            free(vertSource);
            free(fragSource);
            return NULL;
        }
    }



    uint32_t fragID = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragID, 1, &fragSource, NULL);
    glCompileShader(fragID);

    {
        int  success;
        char infoLog[512];
        glGetShaderiv(fragID, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragID, 512, NULL, infoLog);
            SDL_Log("%s : Fragment shader compilation failed : %s\n", sourcePath, infoLog);
            free(vertSource);
            free(fragSource);
            return NULL;
        }
    }





    Shader *s;
    s = (Shader *)malloc(sizeof(Shader));
    if (!s) {
        SDL_Log("Failed to allocate Shader* : not enough memory");
        return s;
    }

    s->programID = glCreateProgram();
    glAttachShader(s->programID, vertID);
    glAttachShader(s->programID, fragID);
    glLinkProgram(s->programID);

    int  success = 0;
    char infoLog[512];
    glGetProgramiv(s->programID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(s->programID, 512, NULL, infoLog);
        SDL_Log("%s : Failed to link shaders: %s\n", infoLog, sourcePath);
        free(s);
        return NULL;
    }
    SDL_Log("%s shader link done", sourcePath);


    free(vertSource);
    free(fragSource);
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

static GLint GetUniformLocation(const char *name, GLuint programID)
{
    return glGetUniformLocation(programID, name);
}

void ShaderSetMat4ByName(const char *name, glm::mat4 matrix, GLuint programID)
{
    GLint location = GetUniformLocation(name, programID);
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}
void ShaderSetMat4ByName(const char *name, glm::mat4 &matrix, GLsizei count, GLuint programID)
{
    GLint location = GetUniformLocation(name, programID);
    glUniformMatrix4fv(location, count, GL_FALSE, glm::value_ptr(matrix));
}

void ShaderSetUniformMatrix4Name(const char *name, const Matrix4 matrix, GLuint programID)
{
    // we could just hardcode locations to avoid querrying for names each frame..
    GLint location = GetUniformLocation(name, programID);
    glUniformMatrix4fv(location, 1, GL_FALSE, matrix.m);
}

void ShaderSetUniformVec3ByName(const char *name, const glm::vec3 *v, GLuint programID)
{
    GLint location = GetUniformLocation(name, programID);
    glUniform3fv(location, 1, (float *)v);
}

void ShaderSetUniformIntByName(const char *name, int *value, GLuint programID)
{
    GLint location = GetUniformLocation(name, programID);
    glUniform1i(location, *value);
}