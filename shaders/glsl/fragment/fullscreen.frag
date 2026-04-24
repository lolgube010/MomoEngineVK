#version 450

// Input from the vertex shader
layout(location = 0) in vec2 inUV;

// Your post-processed image from previous passes
layout(binding = 0) uniform sampler2D screenTexture;

// Output to the swapchain (or next render target)
layout(location = 0) out vec4 outColor;

void main() {
    // Sample the texture
    vec3 color = texture(screenTexture, inUV).rgb;

    // You can do your final tonemapping or gamma correction right here
    // Example: color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}