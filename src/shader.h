#pragma once

#include <glad/glad.h>
#include <string>

class ShaderProgram
{
public:
    ShaderProgram();
    ~ShaderProgram();

    bool init(const char *vertex_shader_source,
              const char *fragment_shader_source,
              std::string &error_log);

    void use(bool use);

    int get_uniform_location(const char *name);

    unsigned int id() const { return program_; }

private:
    unsigned int program_;
};
