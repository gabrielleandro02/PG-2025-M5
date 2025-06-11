#define STB_IMAGE_IMPLEMENTATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "stb_image.h"
#include <iostream>
#include <vector>
#include <map>
#include <string>

// --- SHADER SOURCES ---
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    uniform mat4 model;
    uniform vec2 offsetST;
    uniform vec2 scaleST;
    void main() {
        gl_Position = model * vec4(aPos, 1.0);
        TexCoord = aTexCoord * scaleST + offsetST;
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoord;
    uniform sampler2D texture1;
    uniform bool flipX;
    void main() { 
        vec2 texCoord = TexCoord;
        if(flipX) texCoord.x = 1.0 - texCoord.x;
        vec4 texColor = texture(texture1, texCoord);
        if(texColor.a < 0.1) discard;
        FragColor = texColor; 
    }
)";

// --- WINDOW SIZE CONSTANTS ---
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// --- TEXTURE LOADING FUNCTION ---
unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false); // Keep sprites upright
    unsigned char *data = stbi_load(path, &width, &height, &nrChannels, STBI_rgb_alpha);
    if (data) {
        GLenum format = GL_RGBA;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        std::cout << "Loaded texture: " << path << " (" << width << "x" << height << ")" << std::endl;
    } else {
        std::cout << "Failed to load texture " << path << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

// --- ANIMATION STATE ENUM ---
enum AnimationState {
    IDLE,
    WALK,
    RUN,
    JUMP,
    ATTACK1,
    ATTACK2,
    HURT,
    DEATH,
    CLIMB,
    PUSH,
    THROW
};

// --- DUDE MONSTER CHARACTER CLASS ---
class DudeMonster {
private:
    unsigned int VAO, VBO, EBO;
    std::map<AnimationState, unsigned int> textures;
    std::map<AnimationState, int> frameCount;
    
    float x, y, width, height;
    float speed;
    bool facingRight;
    bool onGround;
    float velocityY;
    const float gravity = -15.0f;
    const float jumpStrength = 8.0f;
    
    AnimationState currentState;
    int currentFrame;
    float frameTimer;
    float frameDuration;
    unsigned int shaderProgram;
    
    // Action timers
    float attackTimer;
    float hurtTimer;
    bool isAttacking;
    bool isHurt;

public:
    DudeMonster(float posX, float posY, float w, float h, float moveSpeed = 3.0f) {
        x = posX;
        y = posY;
        width = w;
        height = h;
        speed = moveSpeed;
        facingRight = true;
        onGround = true;
        velocityY = 0.0f;
        
        currentState = IDLE;
        currentFrame = 0;
        frameTimer = 0.0f;
        frameDuration = 0.1f;
        
        attackTimer = 0.0f;
        hurtTimer = 0.0f;
        isAttacking = false;
        isHurt = false;
        
        loadAnimations();
        setupGeometry();
    }
    
    void loadAnimations() {
        // Load all animation textures based on your sprite pack
        textures[IDLE] = loadTexture("assets/sprites/Dude_Monster_Idle_4.png");
        textures[WALK] = loadTexture("assets/sprites/Dude_Monster_Walk_6.png");
        textures[RUN] = loadTexture("assets/sprites/Dude_Monster_Run_6.png");
        textures[JUMP] = loadTexture("assets/sprites/Dude_Monster_Jump_8.png");
        textures[ATTACK1] = loadTexture("assets/sprites/Dude_Monster_Attack1_4.png");
        textures[ATTACK2] = loadTexture("assets/sprites/Dude_Monster_Attack2_6.png");
        textures[HURT] = loadTexture("assets/sprites/Dude_Monster_Hurt_4.png");
        textures[DEATH] = loadTexture("assets/sprites/Dude_Monster_Death_8.png");
        textures[CLIMB] = loadTexture("assets/sprites/Dude_Monster_Climb_4.png");
        textures[PUSH] = loadTexture("assets/sprites/Dude_Monster_Push_6.png");
        textures[THROW] = loadTexture("assets/sprites/Dude_Monster_Throw_4.png");
        
        // Set frame counts for each animation
        frameCount[IDLE] = 4;
        frameCount[WALK] = 6;
        frameCount[RUN] = 6;
        frameCount[JUMP] = 8;
        frameCount[ATTACK1] = 4;
        frameCount[ATTACK2] = 6;
        frameCount[HURT] = 4;
        frameCount[DEATH] = 8;
        frameCount[CLIMB] = 4;
        frameCount[PUSH] = 6;
        frameCount[THROW] = 4;
    }
    
    void setupGeometry() {
        float vertices[] = {
            // positions              // texture coords
            -width/2,  height/2, 0.0f,   0.0f, 1.0f,    // top left
            -width/2, -height/2, 0.0f,   0.0f, 0.0f,    // bottom left
             width/2,  height/2, 0.0f,   1.0f, 1.0f,    // top right
             width/2, -height/2, 0.0f,   1.0f, 0.0f     // bottom right
        };
        
        unsigned int indices[] = {
            0, 1, 2,   // first triangle
            1, 2, 3    // second triangle
        };
        
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }
    
    void setShaderProgram(unsigned int program) {
        shaderProgram = program;
    }
    
    void handleInput(GLFWwindow* window, float deltaTime) {
        if (isHurt) return; // Can't move while hurt
        
        float deltaX = 0.0f;
        bool isMoving = false;
        bool isRunning = false;
        
        // Check if shift is held for running
        bool shiftPressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        
        // Horizontal movement
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || 
            glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            deltaX -= speed * deltaTime * (shiftPressed ? 1.5f : 1.0f);
            facingRight = false;
            isMoving = true;
            isRunning = shiftPressed;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || 
            glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            deltaX += speed * deltaTime * (shiftPressed ? 1.5f : 1.0f);
            facingRight = true;
            isMoving = true;
            isRunning = shiftPressed;
        }
        
        // Jumping
        if ((glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || 
             glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS ||
             glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) && onGround) {
            velocityY = jumpStrength;
            onGround = false;
        }
        
        // Attacks
        static bool lastAttack1 = false;
        static bool lastAttack2 = false;
        bool attack1Pressed = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
        bool attack2Pressed = glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS;
        
        if (attack1Pressed && !lastAttack1 && !isAttacking) {
            startAttack(ATTACK1);
        }
        if (attack2Pressed && !lastAttack2 && !isAttacking) {
            startAttack(ATTACK2);
        }
        
        lastAttack1 = attack1Pressed;
        lastAttack2 = attack2Pressed;
        
        // Update position
        x += deltaX;
        
        // Keep within screen bounds
        float halfWidth = width / 2.0f;
        if (x - halfWidth < -1.0f) x = -1.0f + halfWidth;
        if (x + halfWidth > 1.0f) x = 1.0f - halfWidth;
        
        // Set animation based on state
        if (!isAttacking && !isHurt) {
            if (!onGround) {
                setState(JUMP);
            } else if (isMoving) {
                setState(isRunning ? RUN : WALK);
            } else {
                setState(IDLE);
            }
        }
    }
    
    void startAttack(AnimationState attackType) {
        isAttacking = true;
        attackTimer = 0.0f;
        currentFrame = 0;
        setState(attackType);
    }
    
    void setState(AnimationState newState) {
        if (currentState != newState) {
            currentState = newState;
            currentFrame = 0;
            frameTimer = 0.0f;
            
            // Adjust frame duration based on animation
            switch (newState) {
                case ATTACK1:
                case ATTACK2:
                    frameDuration = 0.08f; // Faster attacks
                    break;
                case RUN:
                    frameDuration = 0.08f; // Faster running
                    break;
                case HURT:
                    frameDuration = 0.15f; // Slower hurt animation
                    break;
                default:
                    frameDuration = 0.1f;
                    break;
            }
        }
    }
    
    void update(float deltaTime) {
        // Update timers
        if (isAttacking) {
            attackTimer += deltaTime;
            float attackDuration = frameCount[currentState] * frameDuration;
            if (attackTimer >= attackDuration) {
                isAttacking = false;
                attackTimer = 0.0f;
            }
        }
        
        if (isHurt) {
            hurtTimer += deltaTime;
            if (hurtTimer >= 0.5f) { // Hurt state lasts 0.5 seconds
                isHurt = false;
                hurtTimer = 0.0f;
            }
        }
        
        // Apply gravity
        if (!onGround) {
            velocityY += gravity * deltaTime;
            y += velocityY * deltaTime;
            
            // Simple ground collision (y = -0.5)
            if (y <= -0.5f) {
                y = -0.5f;
                velocityY = 0.0f;
                onGround = true;
            }
        }
        
        // Update animation frame
        frameTimer += deltaTime;
        if (frameTimer >= frameDuration) {
            frameTimer = 0.0f;
            if (currentState == DEATH) {
                // Death animation plays once
                if (currentFrame < frameCount[currentState] - 1) {
                    currentFrame++;
                }
            } else {
                currentFrame = (currentFrame + 1) % frameCount[currentState];
            }
        }
    }
    
    void takeDamage() {
        if (!isHurt) {
            isHurt = true;
            hurtTimer = 0.0f;
            currentFrame = 0;
            setState(HURT);
        }
    }
    
    void render() {
        glUseProgram(shaderProgram);
        
        // Calculate texture coordinates for current frame
        float frameWidth = 1.0f / frameCount[currentState];
        float offsetS = currentFrame * frameWidth;
        
        // Set uniforms - CORREÇÃO PRINCIPAL AQUI
        glUniform2f(glGetUniformLocation(shaderProgram, "offsetST"), offsetS, 0.0f);
        glUniform2f(glGetUniformLocation(shaderProgram, "scaleST"), frameWidth, 1.0f); // Escala a largura do frame
        glUniform1i(glGetUniformLocation(shaderProgram, "flipX"), !facingRight);
        
        // Create model matrix
        float model[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            x,    y,    0.0f, 1.0f
        };
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);
        
        // Bind texture and draw
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[currentState]);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
    
    ~DudeMonster() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        for (auto& texture : textures) {
            glDeleteTextures(1, &texture.second);
        }
    }
};

// --- BACKGROUND CLASS ---
class Background {
private:
    unsigned int VAO, VBO;
    unsigned int texture1, texture2;
    float scrollOffset1, scrollOffset2;
    unsigned int shaderProgram;

public:
    Background() : scrollOffset1(0.0f), scrollOffset2(0.0f) {
        texture1 = loadTexture("assets/sprites/Rock1.png");
        texture2 = loadTexture("assets/sprites/Rock2.png");
        setupGeometry();
    }
    
    void setupGeometry() {
        float vertices[] = {
            -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
             1.0f, -1.0f, 0.0f,  2.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
             1.0f, -1.0f, 0.0f,  2.0f, 0.0f,
             1.0f,  1.0f, 0.0f,  2.0f, 1.0f
        };
        
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }
    
    void setShaderProgram(unsigned int program) {
        shaderProgram = program;
    }
    
    void update(float deltaTime) {
        scrollOffset1 += 0.1f * deltaTime;
        scrollOffset2 += 0.05f * deltaTime;
        if (scrollOffset1 > 1.0f) scrollOffset1 -= 1.0f;
        if (scrollOffset2 > 1.0f) scrollOffset2 -= 1.0f;
    }
    
    void render() {
        glUseProgram(shaderProgram);
        
        float model[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);
        glUniform1i(glGetUniformLocation(shaderProgram, "flipX"), false);
        
        // Para o background, usar escala completa
        glUniform2f(glGetUniformLocation(shaderProgram, "offsetST"), scrollOffset1, 0.0f);
        glUniform2f(glGetUniformLocation(shaderProgram, "scaleST"), 1.0f, 1.0f);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    
    ~Background() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteTextures(1, &texture1);
        glDeleteTextures(1, &texture2);
    }
};

// --- MAIN FUNCTION ---
int main() {
    // GLFW/GLAD/OPENGL INIT
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Dude Monster Game", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // SHADER SETUP
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
    
    // CREATE GAME OBJECTS
    Background* background = new Background();
    DudeMonster* player = new DudeMonster(0.0f, -0.5f, 0.4f, 0.5f, 4.0f);
    
    background->setShaderProgram(shaderProgram);
    player->setShaderProgram(shaderProgram);
    
    float lastTime = glfwGetTime();
    
    std::cout << "=== CONTROLES ===" << std::endl;
    std::cout << "A/D ou Setas: Mover esquerda/direita" << std::endl;
    std::cout << "W/Seta para cima/Espaço: Pular" << std::endl;
    std::cout << "Shift + movimento: Correr" << std::endl;
    std::cout << "Z: Ataque 1" << std::endl;
    std::cout << "X: Ataque 2" << std::endl;
    std::cout << "H: Teste de dano" << std::endl;
    std::cout << "ESC: Sair" << std::endl;
    
    // MAIN GAME LOOP
    while (!glfwWindowShouldClose(window)) {
        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        
        // Input
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
        
        // Test damage (press H)
        static bool lastH = false;
        bool hPressed = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
        if (hPressed && !lastH) {
            player->takeDamage();
        }
        lastH = hPressed;
        
        // Update
        background->update(deltaTime);
        player->handleInput(window, deltaTime);
        player->update(deltaTime);
        
        // Render
        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        background->render();
        player->render();
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // CLEANUP
    delete background;
    delete player;
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}