#include "shader.h"
#include <cassert>
#include "scope_guards.h"


ShaderProgram::ShaderProgram()
    : program_(0)
{
}

ShaderProgram::~ShaderProgram()
{
    glDeleteProgram(program_);
}

bool ShaderProgram::init(const char *vertex_shader_source,
                         const char *fragment_shader_source,
                         std::string &error_log)
{
    assert(program_ == 0);

    int success;
    char info_log[1024];

    // vertex shader
    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    on_scope_exit([&vertex]{ glDeleteShader(vertex); });
    glShaderSource(vertex, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex);
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertex, 1024, NULL, info_log);
        error_log = "compile vertex shader: ";
        error_log += info_log;
        return false;
    }

    // fragment Shader
    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    on_scope_exit([&fragment]{ glDeleteShader(fragment); });
    glShaderSource(fragment, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment);
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragment, 1024, NULL, info_log);
        error_log = "compile fragment shader: ";
        error_log += info_log;
        return false;
    }

    // shader Program
    program_ = glCreateProgram();
    auto program_guard = scope_guards::on_scope_exit_([&]{ glDeleteProgram(program_); program_ = 0; });
    glAttachShader(program_, vertex);
    glAttachShader(program_, fragment);
    glLinkProgram(program_);
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program_, 1024, NULL, info_log);
        error_log = "link: ";
        error_log += info_log;
        return false;
    }
    program_guard.dismiss();

    return true;
}

void ShaderProgram::use(bool use)
{
    assert(program_ != 0);
    GLuint prog = use ? program_ : 0;
    glUseProgram(prog);
}

int ShaderProgram::get_uniform_location(const char* name)
{
    assert(program_ != 0);
    return glGetUniformLocation(program_, name);
}
