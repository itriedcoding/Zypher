#version 450 core

// Screen space vertex shader for the editor/debug UI overlay. Positions arrive
// already in pixel space and are converted to clip space here.

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(push_constant) uniform UiPush {
    vec2 viewportSize;
} ui;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;

void main() {
    vec2 clip = (inPosition / ui.viewportSize) * 2.0 - 1.0;
    gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
    vTexCoord = inTexCoord;
    vColor = inColor;
}
