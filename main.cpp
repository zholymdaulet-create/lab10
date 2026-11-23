// week10: толық еркін камера — тінтуірмен бағыт (yaw/pitch), дөңгелекпен зум (fov)
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdio>
#include <iostream>

// ---------------- Камера айнымалылары (жаһандық) ----------------
const glm::vec3 CAM_START_POS = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 camPos   = CAM_START_POS;
glm::vec3 camFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 camUp    = glm::vec3(0.0f, 1.0f, 0.0f);

// 10-апта: тінтуір және зум айнымалылары
const float YAW_START = -90.0f;   // 0 болса камера +X-ке қарайды, куб көрінбейді
const float FOV_START = 45.0f;
const float FOV_MAX   = 90.0f;    // 5-тапсырма: жоғарғы шек 45-тен 90-ға көтерілді
float yaw   = YAW_START;
float pitch = 0.0f;
float lastX = 400.0f;
float lastY = 300.0f;
bool firstMouse = true;
float fov = FOV_START;

// sensitivity const: ол қолданушы баптауы емес, қолмен таңдалған тұрақты (README-ді қара)
const float sensitivity = 0.1f;

// 3-тапсырма: Invert Y
bool invertY = false;
bool iPressedLast = false;
bool pPressedLast = false;

// ---------------- Шейдерлер ----------------
const char* vertexSrc = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 vColor;
void main() {
    vColor = aColor;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* fragmentSrc = R"(#version 330 core
in vec3 vColor;
uniform vec3 tint;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor * tint, 1.0);
}
)";

// Куб: әр төбе x y z r g b. Тәртіп — CCW (culling үшін)
const float s = 0.5f;
float cubeVertices[] = {
    -s,-s, s, 1,0.2f,0.2f,   s,-s, s, 1,0.2f,0.2f,   s, s, s, 1,0.2f,0.2f,
     s, s, s, 1,0.2f,0.2f,  -s, s, s, 1,0.2f,0.2f,  -s,-s, s, 1,0.2f,0.2f,
     s,-s,-s, 0.2f,1,0.2f,  -s,-s,-s, 0.2f,1,0.2f,  -s, s,-s, 0.2f,1,0.2f,
    -s, s,-s, 0.2f,1,0.2f,   s, s,-s, 0.2f,1,0.2f,   s,-s,-s, 0.2f,1,0.2f,
     s,-s, s, 0.2f,0.2f,1,   s,-s,-s, 0.2f,0.2f,1,   s, s,-s, 0.2f,0.2f,1,
     s, s,-s, 0.2f,0.2f,1,   s, s, s, 0.2f,0.2f,1,   s,-s, s, 0.2f,0.2f,1,
    -s,-s,-s, 1,1,0.2f,  -s,-s, s, 1,1,0.2f,  -s, s, s, 1,1,0.2f,
    -s, s, s, 1,1,0.2f,  -s, s,-s, 1,1,0.2f,  -s,-s,-s, 1,1,0.2f,
    -s, s, s, 0.2f,1,1,   s, s, s, 0.2f,1,1,   s, s,-s, 0.2f,1,1,
     s, s,-s, 0.2f,1,1,  -s, s,-s, 0.2f,1,1,  -s, s, s, 0.2f,1,1,
    -s,-s,-s, 1,0.2f,1,   s,-s,-s, 1,0.2f,1,   s,-s, s, 1,0.2f,1,
     s,-s, s, 1,0.2f,1,  -s,-s, s, 1,0.2f,1,  -s,-s,-s, 1,0.2f,1,
};

glm::vec3 cubePositions[] = {
    glm::vec3( 0.0f,  0.0f,  0.0f),
    glm::vec3( 1.8f,  0.6f, -1.0f),
    glm::vec3(-1.8f, -0.6f, -2.0f),
    glm::vec3( 1.5f, -1.0f, -3.0f),
    glm::vec3(-1.2f,  1.0f, -4.0f),
};
glm::vec3 cubeTints[] = {
    glm::vec3(1.0f, 1.0f, 1.0f),
    glm::vec3(0.9f, 0.8f, 0.7f),
    glm::vec3(0.7f, 0.9f, 0.8f),
    glm::vec3(0.8f, 0.7f, 0.9f),
    glm::vec3(1.0f, 0.9f, 0.6f),
};

// yaw мен pitch-тен camFront есептеу (градус -> радиан!)
void updateCameraFront() {
    glm::vec3 dir;
    dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(pitch));
    dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    camFront = glm::normalize(dir);
}

void framebufferSizeCallback(GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); }

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {               // бірінші кадрда камера лақтырылмасын
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }
    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;   // экранның y-і төмен өседі, сондықтан таңба кері
    if (invertY) yoffset = -yoffset;       // 3-тапсырма
    lastX = (float)xpos;
    lastY = (float)ypos;

    yaw   += xoffset * sensitivity;
    pitch += yoffset * sensitivity;

    // Қ4: yaw шексіз өспесін
    if (yaw > 360.0f)  yaw -= 360.0f;
    if (yaw < -360.0f) yaw += 360.0f;

    // 1-тапсырма: бұл екі жолды комментарийге алып қарауға болады (pitch = 90 болғанда сахна бұзылады)
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    updateCameraFront();
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    fov -= (float)yoffset;          // алға айналдырса fov кемиді -> куб үлкейеді
    if (fov < 1.0f) fov = 1.0f;
    if (fov > FOV_MAX) fov = FOV_MAX;
}

void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

    float speed = 2.5f * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) speed *= 2.0f;   // жүгіру (Қ1; Shift — төмен түсу үшін бос емес)

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos += speed * camFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos -= speed * camFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camPos -= glm::normalize(glm::cross(camFront, camUp)) * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camPos += glm::normalize(glm::cross(camFront, camUp)) * speed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)      camPos += speed * camUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camPos -= speed * camUp;

    // 3-тапсырма: I — invert Y (алдыңғы күйді сақтаймыз, әйтпесе әр кадр ауысып кетеді)
    bool iNow = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
    if (iNow && !iPressedLast) invertY = !invertY;
    iPressedLast = iNow;

    // 4-тапсырма: P — камера күйін шығару
    bool pNow = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    if (pNow && !pPressedLast) {
        std::printf("camPos = (%.2f, %.2f, %.2f)\n", camPos.x, camPos.y, camPos.z);
        std::printf("camFront = (%.2f, %.2f, %.2f)\n", camFront.x, camFront.y, camFront.z);
        std::printf("yaw = %.1f pitch = %.1f fov = %.1f\n", yaw, pitch, fov);
        std::printf("length = %f\n", glm::length(camFront));
    }
    pPressedLast = pNow;

    // Қ3: R — бастапқы күйге қайту
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        camPos = CAM_START_POS;
        yaw = YAW_START;
        pitch = 0.0f;
        fov = FOV_START;
        updateCameraFront();
    }

    // камера сахнадан тым алыс кетпесін
    if (glm::length(camPos) > 20.0f) camPos = glm::normalize(camPos) * 20.0f;
}

GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(sh, 512, nullptr, log); std::cerr << "Shader error: " << log << "\n"; }
    return sh;
}

int main() {
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(800, 600, "week10: mouse look", nullptr, nullptr);
    if (!window) { std::cerr << "Window failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "GLAD failed\n"; return -1; }

    // 2-қадам: курсорды ұстау және callback-тарды тіркеу
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLint modelLoc = glGetUniformLocation(prog, "model");
    GLint viewLoc  = glGetUniformLocation(prog, "view");
    GLint projLoc  = glGetUniformLocation(prog, "projection");
    GLint tintLoc  = glGetUniformLocation(prog, "tint");

    updateCameraFront();
    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dt = now - lastTime;
        lastTime = now;

        glfwPollEvents();
        processInput(window, dt);

        // Қ2: терезе тақырыбында бұрыштар
        char buf[128];
        std::snprintf(buf, sizeof(buf), "yaw=%.1f pitch=%.1f fov=%.1f%s", yaw, pitch, fov, invertY ? " [invertY]" : "");
        glfwSetWindowTitle(window, buf);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(prog);

        int gWidth, gHeight; glfwGetFramebufferSize(window, &gWidth, &gHeight);
        float aspect = gHeight > 0 ? (float)gWidth / (float)gHeight : 1.0f;

        // 5-қадам: тұрақты 45 емес, fov айнымалысы
        glm::mat4 projection = glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(vao);
        for (int i = 0; i < 5; i++) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), cubePositions[i]);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform3fv(tintLoc, 1, glm::value_ptr(cubeTints[i]));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    glfwTerminate();
    return 0;
}
