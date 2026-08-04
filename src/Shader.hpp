#ifndef SHADER_HPP
#define SHADER_HPP

#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <epoxy/gl.h>
#include <GLFW/glfw3.h>
#include "MathUtils.hpp"

class Shader {
public:
    GLuint programID = 0;

    Shader() = default;
    Shader(const char* vertexSrc, const char* fragmentSrc) {
        compile(vertexSrc, fragmentSrc);
    }
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept : programID(other.programID), uniformCache(std::move(other.uniformCache)) {
        other.programID = 0;
    }

    Shader& operator=(Shader&& other) noexcept {
        if (this != &other) {
            if (programID != 0) glDeleteProgram(programID);
            programID = other.programID;
            uniformCache = std::move(other.uniformCache);
            other.programID = 0;
        }
        return *this;
    }

    ~Shader() {
        if (programID) glDeleteProgram(programID);
    }

    bool compile(const char* vShaderCode, const char* fShaderCode) {
        GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        GLuint newProgram = glCreateProgram();
        glAttachShader(newProgram, vertex);
        glAttachShader(newProgram, fragment);
        glLinkProgram(newProgram);
        checkCompileErrors(newProgram, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);

        GLint linked = GL_FALSE;
        glGetProgramiv(newProgram, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            glDeleteProgram(newProgram);
            return false;
        }

        if (programID != 0) glDeleteProgram(programID);
        programID = newProgram;
        uniformCache.clear();
        return true;
    }
    bool compileFromFile(const std::string& vertexPath, const std::string& fragmentPath) {
        std::ifstream vFile(vertexPath), fFile(fragmentPath);
        if (!vFile.is_open() || !fFile.is_open()) {
            std::cerr << "Failed to open shader files: " << vertexPath << ", " << fragmentPath << "\n";
            return false;
        }
        std::stringstream vStream, fStream;
        vStream << vFile.rdbuf();
        fStream << fFile.rdbuf();
        return compile(vStream.str().c_str(), fStream.str().c_str());
    }

    void compileCompute(const char* computeShaderCode) {
        GLuint compute = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(compute, 1, &computeShaderCode, NULL);
        glCompileShader(compute);
        checkCompileErrors(compute, "COMPUTE");

        GLint compiled = GL_FALSE;
        glGetShaderiv(compute, GL_COMPILE_STATUS, &compiled);
        if (compiled != GL_TRUE) {
            glDeleteShader(compute);
            programID = 0;
            return;
        }

        programID = glCreateProgram();
        glAttachShader(programID, compute);
        glLinkProgram(programID);
        checkCompileErrors(programID, "PROGRAM");

        GLint linked = GL_FALSE;
        glGetProgramiv(programID, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            glDeleteProgram(programID);
            programID = 0;
            glDeleteShader(compute);
            return;
        }

        glDeleteShader(compute);
    }

    void use() const {
        if (programID) glUseProgram(programID);
    }

    GLint getUniformLoc(const std::string& name) const {
        auto it = uniformCache.find(name);
        if (it != uniformCache.end()) return it->second;
        GLint loc = glGetUniformLocation(programID, name.c_str());
        uniformCache[name] = loc;
        return loc;
    }

    void setBool(const std::string& name, bool value) const {
        glUniform1i(getUniformLoc(name), (int)value);
    }
    void setInt(const std::string& name, int value) const {
        glUniform1i(getUniformLoc(name), value);
    }
    void setFloat(const std::string& name, float value) const {
        glUniform1f(getUniformLoc(name), value);
    }
    void setVec3(const std::string& name, const Vec3& value) const {
        glUniform3f(getUniformLoc(name), value.x, value.y, value.z);
    }
    void setVec2(const std::string& name, float x, float y) const {
        glUniform2f(getUniformLoc(name), x, y);
    }
    void setMat4(const std::string& name, const Mat4& mat) const {
        glUniformMatrix4fv(getUniformLoc(name), 1, GL_FALSE, mat.m);
    }

private:
    mutable std::unordered_map<std::string, GLint> uniformCache;
private:
    void checkCompileErrors(GLuint shader, std::string type) {
        GLint success;
        GLchar infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n";
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n";
            }
        }
    }
};

#endif // SHADER_HPP
