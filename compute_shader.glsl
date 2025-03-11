//// filepath: compute_shader.glsl
#version 430

// Usa grupos de trabajo más grandes (16x16) para mejor rendimiento
layout(local_size_x = 16, local_size_y = 16) in;

// Estructura de datos: 5 floats por cada péndulo
// [ángulo1, ángulo2, velocidad1, velocidad2, tiempo_vuelta]
layout(std430, binding = 0) buffer PendulumBuffer {
    float pendulumData[];
};

// Variable uniforme para el tiempo de simulación
uniform float u_simulationTime;

// Constantes para el péndulo doble
const float g = 9.81;
const float L1 = 1.0;
const float L2 = 1.0;
const float m1 = 1.0;
const float m2 = 1.0;
const float dt = 0.0001; // Ajusta según necesites

// Estructura para almacenar las derivadas
struct Derivatives {
    float da1;
    float da2;
    float dw1;
    float dw2;
};

// Función para calcular las derivadas dado el estado actual
Derivatives calcDerivatives(float a1, float a2, float w1, float w2) {
    Derivatives deriv;
    
    // da1/dt = w1, da2/dt = w2 (por definición)
    deriv.da1 = w1;
    deriv.da2 = w2;
    
    // Calcular aceleraciones (dw1/dt y dw2/dt)
    float sinA1A2 = sin(a1 - a2);
    float cosA1A2 = cos(a1 - a2);
    float denom = 2.0 * m1 + m2 - m2 * cos(2.0 * a1 - 2.0 * a2);
    
    deriv.dw1 = (-g * (2.0 * m1 + m2) * sin(a1)
                - m2 * g * sin(a1 - 2.0 * a2)
                - 2.0 * sinA1A2 * m2
                  * (w2 * w2 * L2 + w1 * w1 * L1 * cosA1A2))
               / (L1 * denom);
    
    deriv.dw2 = (2.0 * sinA1A2
                * (w1 * w1 * L1 * (m1 + m2)
                   + g * (m1 + m2) * cos(a1)
                   + w2 * w2 * L2 * m2 * cosA1A2))
               / (L2 * denom);
               
    return deriv;
}

uniform uint u_width;
uniform uint u_height;

void main() {
    // Obtener coordenadas globales
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;
    
    // Solo procesar dentro de los límites
    if (x >= u_width || y >= u_height) return;
    
    // Calcular el índice correctamente
    uint i = y * u_width + x;
    uint baseIndex = i * 5; // Ahora cada péndulo ocupa 5 floats
    if (baseIndex + 4 >= pendulumData.length()) {
        return;
    }
    
    // Estado actual
    float a1 = pendulumData[baseIndex + 0];
    float a2 = pendulumData[baseIndex + 1];
    float w1 = pendulumData[baseIndex + 2];
    float w2 = pendulumData[baseIndex + 3];
    float tiempoVuelta = pendulumData[baseIndex + 4];
    
    // Guardar valores anteriores para detectar si se da la vuelta
    float oldA1 = a1;
    float oldA2 = a2;
    
    // Implementación de RK4
    // k1 = f(y_n)
    Derivatives k1 = calcDerivatives(a1, a2, w1, w2);
    
    // k2 = f(y_n + dt/2 * k1)
    float a1_k2 = a1 + dt * 0.5 * k1.da1;
    float a2_k2 = a2 + dt * 0.5 * k1.da2;
    float w1_k2 = w1 + dt * 0.5 * k1.dw1;
    float w2_k2 = w2 + dt * 0.5 * k1.dw2;
    Derivatives k2 = calcDerivatives(a1_k2, a2_k2, w1_k2, w2_k2);
    
    // k3 = f(y_n + dt/2 * k2)
    float a1_k3 = a1 + dt * 0.5 * k2.da1;
    float a2_k3 = a2 + dt * 0.5 * k2.da2;
    float w1_k3 = w1 + dt * 0.5 * k2.dw1;
    float w2_k3 = w2 + dt * 0.5 * k2.dw2;
    Derivatives k3 = calcDerivatives(a1_k3, a2_k3, w1_k3, w2_k3);
    
    // k4 = f(y_n + dt * k3)
    float a1_k4 = a1 + dt * k3.da1;
    float a2_k4 = a2 + dt * k3.da2;
    float w1_k4 = w1 + dt * k3.dw1;
    float w2_k4 = w2 + dt * k3.dw2;
    Derivatives k4 = calcDerivatives(a1_k4, a2_k4, w1_k4, w2_k4);
    
    // y_{n+1} = y_n + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
    a1 = a1 + (dt/6.0) * (k1.da1 + 2.0*k2.da1 + 2.0*k3.da1 + k4.da1);
    a2 = a2 + (dt/6.0) * (k1.da2 + 2.0*k2.da2 + 2.0*k3.da2 + k4.da2);
    w1 = w1 + (dt/6.0) * (k1.dw1 + 2.0*k2.dw1 + 2.0*k3.dw1 + k4.dw1);
    w2 = w2 + (dt/6.0) * (k1.dw2 + 2.0*k2.dw2 + 2.0*k3.dw2 + k4.dw2);
    
    // Detectar si alguno de los ángulos da la vuelta completa (supera π)
    // Solo registramos la primera vez que ocurre
    if (tiempoVuelta == 0.0) {
        if (abs(a1) > 3.14159 || abs(a2) > 3.14159) {
            tiempoVuelta = u_simulationTime;
        }
    }
    
    // Guardar resultados
    pendulumData[baseIndex + 0] = a1;
    pendulumData[baseIndex + 1] = a2;
    pendulumData[baseIndex + 2] = w1;
    pendulumData[baseIndex + 3] = w2;
    pendulumData[baseIndex + 4] = tiempoVuelta;
}