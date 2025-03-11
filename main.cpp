#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

// Variables globales para las dimensiones de la ventana
const size_t WIDTH = 800;
const size_t HEIGHT = 800;
const int SIMULATION_STEPS_PER_FRAME = 100; // Número de pasos de simulación por frame

// Variables para el zoom y punto central
const float ZOOM = 1.0f;           // 1.0 = sin zoom, mayor = más zoom
const float CENTER_X = 0.0f;       // Rango [-1.0, 1.0], 0.0 = centro
const float CENTER_Y = 0.0f;       // Rango [-1.0, 1.0], 0.0 = centro

// Añadir una nueva variable global para el tiempo de simulación
float simulationTime = 0.0f;

// Función de callback para manejar clics del ratón
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        // Obtener posición del cursor
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        
        // Obtener dimensiones de la ventana
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        
        // Invertir coordenada Y para que origen esté en esquina inferior izquierda
        ypos = height - ypos;
        
        // Convertir a coordenadas normalizadas [-1, 1]
        float normalizedX = (2.0f * xpos / (width - 1.0f) - 1.0f);
        float normalizedY = (2.0f * ypos / (height - 1.0f) - 1.0f);
        
        // Aplicar transformación inversa (zoom y centro)
        float worldX = CENTER_X + normalizedX / ZOOM;
        float worldY = CENTER_Y + normalizedY / ZOOM;
        
        // Convertir a ángulos en radianes (-π a π)
        float angleX = worldX * 3.14159f;
        float angleY = worldY * 3.14159f;
        
        // Imprimir coordenadas
        std::cout << "Clic en coordenadas de pantalla: (" << xpos << ", " << ypos << ")" << std::endl;
        std::cout << "Coordenadas normalizadas con zoom: (" << worldX << ", " << worldY << ")" << std::endl;
        std::cout << "Ángulos correspondientes: (" << angleX << ", " << angleY << ")" << std::endl;
        
        // También imprimir el índice del péndulo para depuración
        size_t pendulumRow = static_cast<size_t>(ypos);
        size_t pendulumCol = static_cast<size_t>(xpos);
        size_t pendulumIndex = pendulumRow * WIDTH + pendulumCol;
        std::cout << "Índice del péndulo: " << pendulumIndex << std::endl;
    }
}

// Function to load shader source code from file
std::string loadShaderSource(const char* filepath) {
    std::ifstream file(filepath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Function to compile shaders
GLuint compileShader(GLenum type, const char* filePath) {
    std::string source = loadShaderSource(filePath);
    const char* shaderCode = source.c_str();
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &shaderCode, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader Compilation Error:\n" << infoLog << std::endl;
    }

    return shader;
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Create a GLFW window
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "OpenGL Color Shader", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    GLuint computeShader = compileShader(GL_COMPUTE_SHADER, "./compute_shader.glsl");
    GLuint computeProgram = glCreateProgram();
    glAttachShader(computeProgram, computeShader);
    glLinkProgram(computeProgram);
    {
        int success;
        glGetProgramiv(computeProgram, GL_LINK_STATUS, &success);
        if(!success){
            char infoLog[512];
            glGetProgramInfoLog(computeProgram, 512, nullptr, infoLog);
            std::cerr << "Compute Program Linking Error:\n" << infoLog << std::endl;
        }
    }
    glDeleteShader(computeShader);

    // Añadir ubicaciones para los nuevos uniforms
    GLint widthLocation = glGetUniformLocation(computeProgram, "u_width");
    GLint heightLocation = glGetUniformLocation(computeProgram, "u_height");

    // Crea el SSBO (ejemplo con 256 péndulos, cada uno con 4 floats):
    GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    {
        // Un péndulo por píxel
        size_t width  = WIDTH;
        size_t height = HEIGHT;
        size_t numPendulums = width * height;
        std::vector<float> pendulumData(numPendulums * 5); // 5 floats por péndulo

        // Asignar ángulos iniciales con zoom y punto central
        for (size_t i = 0; i < numPendulums; ++i) {
            size_t row = i / width;
            size_t col = i % width;

            // Usar double para mayor precisión en los cálculos
            double normalizedX = (2.0 * col / (width - 1.0) - 1.0);
            double normalizedY = (2.0 * row / (height - 1.0) - 1.0);
            
            // Aplicar zoom y centrado con double
            double zoomedX = CENTER_X + normalizedX / ZOOM;
            double zoomedY = CENTER_Y + normalizedY / ZOOM;
            
            // Mapear al rango de ángulos [-π, π] y convertir a float al final
            pendulumData[i*5 + 0] = static_cast<float>(zoomedX * 3.14159265358979);
            pendulumData[i*5 + 1] = static_cast<float>(zoomedY * 3.14159265358979);
            pendulumData[i*5 + 2] = 0.f;
            pendulumData[i*5 + 3] = 0.f;
            pendulumData[i*5 + 4] = 0.f;
        }

        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     pendulumData.size() * sizeof(float),
                     pendulumData.data(),
                     GL_DYNAMIC_COPY);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    // Crear y llenar el TBO con los datos del SSBO
    GLuint tbo;
    glGenTextures(1, &tbo);
    glBindTexture(GL_TEXTURE_BUFFER, tbo);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, ssbo);

    // Define the viewport
    glViewport(0, 0, WIDTH, HEIGHT);

    // Define the full-screen quad vertices (in normalized device coordinates)
    float vertices[] = {
        -1.0f, -1.0f,  // Bottom-left
         1.0f, -1.0f,  // Bottom-right
        -1.0f,  1.0f,  // Top-left
         1.0f,  1.0f   // Top-right
    };

    // Define indices for drawing two triangles forming a rectangle
    unsigned int indices[] = {
        0, 1, 2,
        1, 2, 3
    };

    // Create VAO, VBO, and EBO
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Bind and buffer vertex data
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Bind and buffer index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Set up vertex attribute pointers
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Compile shaders from external files
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, "./vertex_shader.glsl");
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, "./fragment_shader.glsl");

    // Create shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check for linking errors
    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader Program Linking Error:\n" << infoLog << std::endl;
    }

    // Clean up shaders (they are now linked into the program)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Crear variables uniform renombradas
    GLint resLocation   = glGetUniformLocation(shaderProgram, "u_resolution");
    GLint timeLocation  = glGetUniformLocation(shaderProgram, "u_time");
    GLint mouseLocation = glGetUniformLocation(shaderProgram, "u_mouse");

    // Obtener la ubicación del uniform para el TBO en el fragment shader
    GLint tboLocation = glGetUniformLocation(shaderProgram, "u_pendulumData");

    // Obtener la ubicación del uniform para el tiempo de simulación
    GLint simTimeLocation = glGetUniformLocation(computeProgram, "u_simulationTime");

    // Registrar el callback para eventos de ratón
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        
        // Invertir coordenada Y
        mouseY = height - mouseY;

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Ejecutar varios pasos de simulación por cada frame
        glUseProgram(computeProgram);
        for (int step = 0; step < SIMULATION_STEPS_PER_FRAME; step++) {
            float dt = 0.01f; // Define el valor de dt
            simulationTime += dt; // Incrementar tiempo de simulación
            glUniform1f(simTimeLocation, simulationTime); // Enviar al shader
            
            glUniform1ui(widthLocation, WIDTH); 
            glUniform1ui(heightLocation, HEIGHT);

            glDispatchCompute(WIDTH/16 + (WIDTH%16 == 0 ? 0 : 1), 
                             HEIGHT/16 + (HEIGHT%16 == 0 ? 0 : 1), 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }
        // Barrera final para asegurar que todos los cambios sean visibles
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        // Actualizar el TBO con los datos del SSBO usando R32F
        glBindTexture(GL_TEXTURE_BUFFER, tbo);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, ssbo);

        glUseProgram(shaderProgram);
        // Asignar la nueva posición del ratón
        glUniform2f(mouseLocation, (float)mouseX, (float)mouseY);

        glViewport(0, 0, width, height);
        glUseProgram(shaderProgram);

        glUniform2f(resLocation, (float)width, (float)height);
        glUniform1f(timeLocation, (float)glfwGetTime());

        glClear(GL_COLOR_BUFFER_BIT);
        
        // Renderizado con el fragment shader
        glUseProgram(shaderProgram);
        glUniform1i(tboLocation, 0); // Asignar el TBO al uniform
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(computeProgram);
    glDeleteBuffers(1, &ssbo);
    glDeleteTextures(1, &tbo);
    
    glfwTerminate();
    return 0;
}
