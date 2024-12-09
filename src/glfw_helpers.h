#pragma once
static struct State
{
    std::atomic<bool> lorem_ipsums      = true;
    std::atomic<bool> key_input         = true;
    std::atomic<bool> has_input_changed = true;

    std::atomic<float> x_offset = 10, y_offset = 30;

    std::vector<std::u32string> input;
} state;

void toggle(std::atomic_bool& b) { b.exchange(!b); }

void char_callback(GLFWwindow* window, unsigned int codepoint)
{
    std::u32string u32;
    u32.push_back(codepoint);
    if (state.input.empty())
        state.input.push_back(u32);
    else
        state.input.at(0) += u32;
    state.has_input_changed = true;
}

std::function<void(GLFWwindow*)> draw;
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    constexpr double delta_scale = 10.0;
    state.x_offset               = state.x_offset + delta_scale * yoffset;
    //    state.y_offset               = state.y_offset + delta_scale * yoffset;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        if (mods & GLFW_MOD_SUPER || mods & GLFW_MOD_CONTROL)
        {
            // quit
            if (key == GLFW_KEY_Q)
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            // lorem ipsum
            if (key == GLFW_KEY_L)
            {
                toggle(state.lorem_ipsums);
            }

            // lorem ipsum
            if (key == GLFW_KEY_I)
            {
                toggle(state.key_input);
            }

            // Handle paste operation (Ctrl+V)
            if (key == GLFW_KEY_V)
            {
                std::string clipboard_text = glfwGetClipboardString(window);
                state.input.emplace_back(typesetting::convert_for_font_runs(clipboard_text));
                state.has_input_changed = true;
            }
        }
    }
}
