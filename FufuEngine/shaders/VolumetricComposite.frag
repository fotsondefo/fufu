#version 430 core

// Composite additif : blend le scatter/transmittance (demi-résolution, GL_LINEAR)
// sur le buffer HDR pleine résolution.
//   final = hdr * transmittance + inscatter

in  vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_HDR;         // HDR pleine résolution
uniform sampler2D u_Scatter;     // RGBA16F demi-résolution (GL_LINEAR → upsampling implicite)

void main()
{
    vec3 hdr     = texture(u_HDR,     v_UV).rgb;
    vec4 scatter = texture(u_Scatter, v_UV);   // rgb = inscatter, a = transmittance

    fragColor = vec4(hdr * scatter.a + scatter.rgb, 1.0);
}
