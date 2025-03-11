#version 430 core

uniform vec2 u_resolution;
uniform float u_time;
uniform vec2 u_mouse;
uniform samplerBuffer u_pendulumData;

out vec4 FragColor;

void main() {
    // Calcula el índice basado en la posición del píxel
    int x = int(gl_FragCoord.x);
    int y = int(gl_FragCoord.y);
    
    int width = int(u_resolution.x);
    int pendulumIndex = y * width + x;
    
    // Con GL_R32F, cada texel es un solo float, así que accedemos directamente
    int baseIndex = pendulumIndex * 5;
    float a1 = texelFetch(u_pendulumData, baseIndex + 0).r;
    float a2 = texelFetch(u_pendulumData, baseIndex + 1).r;
    float tiempoVuelta = texelFetch(u_pendulumData, baseIndex + 4).r;
    
    // Normaliza los ángulos para colores
    // Aplicar antialiasing suavizando los bordes para evitar patrones de Moiré
    float r = 0.5 * (sin(a1 * 0.95) + 1.0); // Escala ligeramente los ángulos para reducir
    float g = 0.5 * (sin(a2 * 0.95) + 1.0); // patrones de alta frecuencia
    
    // Usar una función de mapeo más suave para el tercer canal
    float b = (tiempoVuelta > 0.0) ? 
              smoothstep(0.0, 0.2, tiempoVuelta / 10.0) : 
              0.3 + 0.2 * sin(a1 * 0.7 + a2 * 0.7);
    
    FragColor = vec4(r, g, b, 1.0);
}
