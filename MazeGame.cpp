#include "glad/glad.h"
#ifdef __APPLE__
 #include <SDL2/SDL.h>
 #include <SDL2/SDL_opengl.h>
#else
 #include <SDL.h>
 #include <SDL_opengl.h>
#endif
#include <cstdio>
#include <vector>
#include <fstream>
#include <string>
#include <set>

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

using namespace std;

struct Map {
    int width, height;
    vector<vector<char>> grid;
    glm::vec3 startPos;
    glm::vec3 goalPos;
};

struct Camera {
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    float yaw;
    float pitch;
    
    Camera(glm::vec3 pos) : position(pos), front(0, 0, -1), 
                            up(0, 1, 0), yaw(-90.0f), pitch(0) {}
    
    glm::mat4 getViewMatrix() {
        return glm::lookAt(position, position + front, up);
    }
    
    void updateVectors() {
        glm::vec3 direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(direction);
    }
    
    void rotate(float deltaYaw, float deltaPitch) {
        yaw += deltaYaw;
        pitch += deltaPitch;
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        updateVectors();
    }
};

struct Model {
    GLuint vao;
    int numVertices;
};

int screen_width = 800;
int screen_height = 600;
char window_title[] = "3D Maze Game";
float avg_render_time = 0;


const GLchar* vertexSource =
    "#version 150 core\n"
    "in vec3 position;"
    "in vec3 inColor;"
    "in vec3 inNormal;"
    "out vec3 Color;"
    "out vec3 normal;"
    "out vec3 fragPos;"
    "out vec2 texCoord;"
    "uniform mat4 model;"
    "uniform mat4 view;"
    "uniform mat4 proj;"
    "uniform vec3 objectColor;"
    "void main() {"
    "   fragPos = vec3(model * vec4(position, 1.0));"
    "   Color = objectColor;"
    "   gl_Position = proj * view * model * vec4(position,1.0);"
    "   vec4 norm4 = transpose(inverse(model)) * vec4(inNormal,1.0);"
    "   normal = normalize(norm4.xyz);"
    "   texCoord = position.xy + 0.5;"
    "}";
     
const GLchar* fragmentSource =
    "#version 150 core\n"
    "in vec3 Color;"
    "in vec3 normal;"
    "in vec3 fragPos;"
    "in vec2 texCoord;"
    "out vec4 outColor;"
    "uniform vec3 lightPos;"
    "uniform vec3 viewPos;"
    "uniform float shininess;"
    "uniform bool useTexture;"
    "uniform sampler2D texSampler;"
    "const vec3 lightColor = vec3(1.0, 1.0, 1.0);"
    "const float ambient = 0.25;"
    "void main() {"
    "   vec3 norm = normalize(normal);"
    "   vec3 lightDir = normalize(lightPos - fragPos);"
    "   vec3 ambientLight = ambient * lightColor;"
    "   float diff = max(dot(norm, lightDir), 0.0);"
    "   vec3 diffuse = diff * lightColor;"
    "   vec3 viewDir = normalize(viewPos - fragPos);"
    "   vec3 reflectDir = reflect(-lightDir, norm);"
    "   float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);"
    "   vec3 specular = 0.5 * spec * lightColor;"
    "   vec3 baseColor = Color;"
    "   if (useTexture) baseColor = texture(texSampler, texCoord).rgb * Color;"
    "   vec3 result = (ambientLight + diffuse + specular) * baseColor;"
    "   outColor = vec4(result, 1.0);"
    "}";

GLuint loadBMP(const char* filepath) {
    FILE* file = fopen(filepath, "rb");
    
    unsigned char header[54];
    fread(header, 1, 54, file);
    
    int width = *(int*)&header[18];
    int height = *(int*)&header[22];
    int imageSize = width * height * 3;
    
    unsigned char* data = new unsigned char[imageSize];
    fread(data, 1, imageSize, file);
    fclose(file);
    
    // BGR to RGB for texture wall
    for (int i = 0; i < imageSize; i += 3) {
        unsigned char tmp = data[i];
        data[i] = data[i + 2];
        data[i + 2] = tmp;
    }
    
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    delete[] data;
    return textureID;
}

Model loadModel(const char* filepath, GLuint shaderProgram) {
    Model model;
    
    ifstream file(filepath);
    
    int numFloats;
    file >> numFloats;
    
    float* data = new float[numFloats];
    for (int i = 0; i < numFloats; i++) {
        file >> data[i];
    }
    file.close();
    
    model.numVertices = numFloats / 8;

    glGenVertexArrays(1, &model.vao);
    glBindVertexArray(model.vao);
    
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, numFloats * sizeof(float), data, GL_STATIC_DRAW);
    
    GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), 0);
    glEnableVertexAttribArray(posAttrib);
    
    GLint colAttrib = glGetAttribLocation(shaderProgram, "inColor");
    glVertexAttribPointer(colAttrib, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(colAttrib);
    
    GLint normAttrib = glGetAttribLocation(shaderProgram, "inNormal");
    glVertexAttribPointer(normAttrib, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(5*sizeof(float)));
    glEnableVertexAttribArray(normAttrib);
    
    glBindVertexArray(0);
    delete[] data;
    
    return model;
}

Map loadMap(const string& filename) {
    Map map;
    ifstream file(filename);
    
    file >> map.width >> map.height;
    
    map.grid.resize(map.height);
    string line;
    getline(file, line); // consume newline
    
    for (int y = 0; y < map.height; y++) {
        getline(file, line);
        map.grid[y].resize(map.width);
        for (int x = 0; x < map.width; x++) {
            if (x < line.length()) {
                map.grid[y][x] = line[x];
                
                if (line[x] == 'S') {
                    map.startPos = glm::vec3(x * 2.0f, 1.0f, y * 2.0f);
                    printf("Start found at: (%d, %d)\n", x, y);
                }
                if (line[x] == 'G') {
                    map.goalPos = glm::vec3(x * 2.0f, 1.0f, y * 2.0f);
                    printf("Goal found at: (%d, %d)\n", x, y);
                }
            }
        }
    }
    
    file.close();
    return map;
}

glm::vec3 getKeyColor(char keyLetter) {
    switch(keyLetter) {
        case 'a': case 'A': return glm::vec3(1.0f, 0.0f, 0.0f); // Red
        case 'b': case 'B': return glm::vec3(0.0f, 1.0f, 0.0f); // Green
        case 'c': case 'C': return glm::vec3(0.0f, 0.5f, 1.0f); // Blue
        case 'd': case 'D': return glm::vec3(1.0f, 1.0f, 0.0f); // Yellow
        case 'e': case 'E': return glm::vec3(1.0f, 0.0f, 1.0f); // Magenta
        default: return glm::vec3(1.0f, 1.0f, 1.0f);
    }
}


bool checkCollision(const Map& map, glm::vec3 pos, const set<char>& keys) {
    // Check center position
    int gridX = (int)(pos.x / 2.0f + 0.5f);
    int gridZ = (int)(pos.z / 2.0f + 0.5f);
    
    if (gridX < 0 || gridX >= map.width || gridZ < 0 || gridZ >= map.height)
        return true;
    
    char cell = map.grid[gridZ][gridX];
    
    if (cell == 'W') return true;
    
    if (cell >= 'A' && cell <= 'E') {
        char requiredKey = cell - 'A' + 'a';
        if (keys.find(requiredKey) == keys.end()) {
            return true; // Door is locked
        }
    }
    
    // Check collision in a small radius around player 
    float radius = 0.3f;
    float cellSize = 2.0f;
    
    // Check 4 corners around player
    glm::vec3 offsets[] = {
        glm::vec3(radius, 0, radius),
        glm::vec3(-radius, 0, radius),
        glm::vec3(radius, 0, -radius),
        glm::vec3(-radius, 0, -radius)
    };
    
    for (int i = 0; i < 4; i++) {
        glm::vec3 checkPos = pos + offsets[i];
        int checkX = (int)(checkPos.x / cellSize + 0.5f);
        int checkZ = (int)(checkPos.z / cellSize + 0.5f);
        
        if (checkX < 0 || checkX >= map.width || checkZ < 0 || checkZ >= map.height)
            return true;
        
        char checkCell = map.grid[checkZ][checkX];
        
        if (checkCell == 'W') return true;
        
        if (checkCell >= 'A' && checkCell <= 'E') {
            char requiredKey = checkCell - 'A' + 'a';
            if (keys.find(requiredKey) == keys.end()) {
                return true;
            }
        }
    }
    
    return false;
}

void checkKeyPickup(Map& map, glm::vec3 pos, set<char>& keys) {
    int gridX = (int)(pos.x / 2.0f + 0.5f);
    int gridZ = (int)(pos.z / 2.0f + 0.5f);
    
    if (gridX < 0 || gridX >= map.width || gridZ < 0 || gridZ >= map.height)
        return;
    
    char cell = map.grid[gridZ][gridX];
    
    if (cell >= 'a' && cell <= 'e') {
        keys.insert(cell);
        map.grid[gridZ][gridX] = '0';
        printf("Picked up key: %c\n", cell);
    }
}

bool checkWin(const Map& map, glm::vec3 pos) {
    int gridX = (int)(pos.x / 2.0f + 0.5f);
    int gridZ = (int)(pos.z / 2.0f + 0.5f);
    
    if (gridX < 0 || gridX >= map.width || gridZ < 0 || gridZ >= map.height)
        return false;
    
    return map.grid[gridZ][gridX] == 'G';
}

void renderDoor(GLuint shaderProgram, const Model& cubeModel, glm::mat4 baseModel, glm::vec3 color) {
    GLint uniModel = glGetUniformLocation(shaderProgram, "model");
    GLint uniColor = glGetUniformLocation(shaderProgram, "objectColor");
    GLint uniUseTexture = glGetUniformLocation(shaderProgram, "useTexture");
    
    glUniform1i(uniUseTexture, 0);
    glBindVertexArray(cubeModel.vao);
    
    // Main door panel
    glm::mat4 panel = baseModel;
    panel = glm::scale(panel, glm::vec3(0.95f, 1.85f, 0.12f));
    glUniform3fv(uniColor, 1, glm::value_ptr(color));
    glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(panel));
    glDrawArrays(GL_TRIANGLES, 0, cubeModel.numVertices);
    
    // Door frame 
    glm::vec3 frameColor = color * 0.5f;
    glUniform3fv(uniColor, 1, glm::value_ptr(frameColor));
    
    // Left frame
    glm::mat4 leftFrame = baseModel;
    leftFrame = glm::translate(leftFrame, glm::vec3(-0.55f, 0, 0));
    leftFrame = glm::scale(leftFrame, glm::vec3(0.1f, 2.0f, 0.18f));
    glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(leftFrame));
    glDrawArrays(GL_TRIANGLES, 0, cubeModel.numVertices);
    
    // Right frame
    glm::mat4 rightFrame = baseModel;
    rightFrame = glm::translate(rightFrame, glm::vec3(0.55f, 0, 0));
    rightFrame = glm::scale(rightFrame, glm::vec3(0.1f, 2.0f, 0.18f));
    glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(rightFrame));
    glDrawArrays(GL_TRIANGLES, 0, cubeModel.numVertices);
    
    // Top frame
    glm::mat4 topFrame = baseModel;
    topFrame = glm::translate(topFrame, glm::vec3(0, 1.0f, 0));
    topFrame = glm::scale(topFrame, glm::vec3(1.2f, 0.1f, 0.18f));
    glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(topFrame));
    glDrawArrays(GL_TRIANGLES, 0, cubeModel.numVertices);
    
    // Door handle (brass/gold)
    glUniform3f(uniColor, 0.8f, 0.6f, 0.2f);
    glm::mat4 handle = baseModel;
    handle = glm::translate(handle, glm::vec3(0.4f, 0, 0.12f));
    handle = glm::scale(handle, glm::vec3(0.15f, 0.05f, 0.08f));
    glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(handle));
    glDrawArrays(GL_TRIANGLES, 0, cubeModel.numVertices);
}

int main(int argc, char *argv[]){
    // map file is the 2nd argument
    string mapFile = argv[1]; 

    SDL_Init(SDL_INIT_VIDEO);
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    
    SDL_Window* window = SDL_CreateWindow(window_title, 100, 100, 
                                          screen_width, screen_height, SDL_WINDOW_OPENGL);
    float aspect = screen_width/(float)screen_height;
    
    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    
    if (!gladLoadGLLoader(SDL_GL_GetProcAddress)){
        return -1;
    }
    
    // Compile shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glBindFragDataLocation(shaderProgram, 0, "outColor");
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);
    
    // Load models
    Model cubeModel = loadModel("models/cube.txt", shaderProgram);
    Model teapotModel = loadModel("models/teapot.txt", shaderProgram);
    Model knotModel = loadModel("models/knot.txt", shaderProgram);
    
    // Load texture
    GLuint wallTexture = loadBMP("text.bmp");
    
    // Load map from argument or default
    Map map = loadMap(mapFile);
    Camera camera(map.startPos);
    set<char> collectedKeys;
    
    glEnable(GL_DEPTH_TEST);
    
    SDL_Event windowEvent;
    bool quit = false;
    float lastTime = SDL_GetTicks() / 1000.0f;
    
    printf("WASD: Move\n");
    printf("Mouse: Look around\n");
    printf("ESC: Exit\n");
    
    while (!quit){
        float t_start = SDL_GetTicks();
        float currentTime = SDL_GetTicks() / 1000.0f;
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        
        while (SDL_PollEvent(&windowEvent)){
            if (windowEvent.type == SDL_QUIT) quit = true;
            if (windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_ESCAPE) 
                quit = true;
            
            if (windowEvent.type == SDL_MOUSEMOTION) {
                float sensitivity = 0.1f;
                camera.rotate(windowEvent.motion.xrel * sensitivity, 
                            -windowEvent.motion.yrel * sensitivity);
            }
        }
        
        const Uint8* keyState = SDL_GetKeyboardState(NULL);
        float moveSpeed = 3.0f * deltaTime;
        
        glm::vec3 forward = glm::normalize(glm::vec3(camera.front.x, 0, camera.front.z));
        glm::vec3 right = glm::normalize(glm::cross(forward, camera.up));
        
        glm::vec3 newPos = camera.position;
        
        if (keyState[SDL_SCANCODE_W]) newPos += forward * moveSpeed;
        if (keyState[SDL_SCANCODE_S]) newPos -= forward * moveSpeed;
        if (keyState[SDL_SCANCODE_A]) newPos -= right * moveSpeed;
        if (keyState[SDL_SCANCODE_D]) newPos += right * moveSpeed;
        
        if (!checkCollision(map, newPos, collectedKeys)) {
            camera.position = newPos;
            checkKeyPickup(map, camera.position, collectedKeys);
            
            if (checkWin(map, camera.position)) {
                printf("\n YOU WIN! \n");
                quit = true;
            }
        }
        
        glClearColor(.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glm::mat4 view = camera.getViewMatrix();
        GLint uniView = glGetUniformLocation(shaderProgram, "view");
        glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));
        
        glm::mat4 proj = glm::perspective(3.14f/4, aspect, 0.1f, 100.0f);
        GLint uniProj = glGetUniformLocation(shaderProgram, "proj");
        glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));
        
        GLint uniModel = glGetUniformLocation(shaderProgram, "model");
        GLint uniColor = glGetUniformLocation(shaderProgram, "objectColor");
        GLint uniUseTexture = glGetUniformLocation(shaderProgram, "useTexture");
        
        // Set lighting uniforms
        glm::vec3 lightPos(map.width, 8.0f, map.height);
        GLint lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
        glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightPos));

        GLint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
        glUniform3fv(viewPosLoc, 1, glm::value_ptr(camera.position));

        GLint shininessLoc = glGetUniformLocation(shaderProgram, "shininess");
        
        // Render the map
        for (int z = 0; z < map.height; z++) {
            for (int x = 0; x < map.width; x++) {
                char cell = map.grid[z][x];
                glm::vec3 pos(x * 2.0f, 0.0f, z * 2.0f);
                
                // Draw floor
                glUniform1f(shininessLoc, 8.0f);
                glUniform1i(uniUseTexture, 0);
                glBindVertexArray(cubeModel.vao);
                
                glm::mat4 floorModel = glm::translate(glm::mat4(1), pos);
                floorModel = glm::scale(floorModel, glm::vec3(2.0f, 0.1f, 2.0f));
                glUniform3f(uniColor, 0.3f, 0.3f, 0.3f);
                glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(floorModel));
                glDrawArrays(GL_TRIANGLES, 0, cubeModel.numVertices);
                
                // Draw walls with texture
                if (cell == 'W') {
                    glUniform1f(shininessLoc, 16.0f);
                    glUniform1i(uniUseTexture, 1);
                    glBindTexture(GL_TEXTURE_2D, wallTexture);
                    glBindVertexArray(cubeModel.vao);
                    
                    glm::mat4 wallModel = glm::translate(glm::mat4(1), pos + glm::vec3(0, 1.0f, 0));
                    wallModel = glm::scale(wallModel, glm::vec3(1.0f, 2.0f, 1.0f));
                    glUniform3f(uniColor, 1.0f, 1.0f, 1.0f);
                    glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(wallModel));
                    glDrawArrays(GL_TRIANGLES, 0, cubeModel.numVertices);
                }
                
                // Draw keys (teapots)
                if (cell >= 'a' && cell <= 'e') {
                    float time = SDL_GetTicks() / 1000.0f;
                    glUniform1f(shininessLoc, 128.0f);
                    glUniform1i(uniUseTexture, 0);
                    glBindVertexArray(teapotModel.vao);
                    
                    glm::mat4 keyModel = glm::translate(glm::mat4(1), pos + glm::vec3(0, 0.8f + sin(time * 2) * 0.2f, 0));
                    keyModel = glm::rotate(keyModel, time, glm::vec3(0, 1, 0));
                    keyModel = glm::scale(keyModel, glm::vec3(0.3f, 0.3f, 0.3f));
                    glUniform3fv(uniColor, 1, glm::value_ptr(getKeyColor(cell)));
                    glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(keyModel));
                    glDrawArrays(GL_TRIANGLES, 0, teapotModel.numVertices);
                }
                
                // Draw doors
                if (cell >= 'A' && cell <= 'E') {
                    glUniform1f(shininessLoc, 32.0f);
                    glm::mat4 doorModel = glm::translate(glm::mat4(1), pos + glm::vec3(0, 1.0f, 0));
                    renderDoor(shaderProgram, cubeModel, doorModel, getKeyColor(cell));
                }

                // Draw goal (knot model)
                if (cell == 'G') {
                    float time = SDL_GetTicks() / 1000.0f;
                    glUniform1f(shininessLoc, 128.0f);
                    glUniform1i(uniUseTexture, 0);
                    glBindVertexArray(knotModel.vao);
                    
                    glm::mat4 goalModel = glm::translate(glm::mat4(1), pos + glm::vec3(0, 1.0f + sin(time * 1.5f) * 0.15f, 0));
                    goalModel = glm::rotate(goalModel, time * 0.5f, glm::vec3(0, 1, 0));
                    goalModel = glm::scale(goalModel, glm::vec3(0.4f, 0.4f, 0.4f));
                    glUniform3f(uniColor, 1.0f, 0.8f, 0.0f);
                    glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(goalModel));
                    glDrawArrays(GL_TRIANGLES, 0, knotModel.numVertices);
                }
            }
        }
        
        // Render held key (teapot) in player's hand
        if (!collectedKeys.empty()) {
            float time = SDL_GetTicks() / 1000.0f;
            char lastKey = *collectedKeys.rbegin();
            
            glUniform1f(shininessLoc, 128.0f);
            glUniform1i(uniUseTexture, 0);
            glBindVertexArray(teapotModel.vao);
            
            glm::vec3 keyPos = camera.position + 
                             camera.front * 0.8f +
                             glm::normalize(glm::cross(camera.front, camera.up)) * 0.4f -
                             camera.up * 0.3f;
            
            glm::mat4 heldKeyModel = glm::translate(glm::mat4(1), keyPos);
            heldKeyModel = glm::rotate(heldKeyModel, time * 2.0f, glm::vec3(0, 1, 0));
            heldKeyModel = glm::scale(heldKeyModel, glm::vec3(0.2f, 0.2f, 0.2f));
            
            glUniform3fv(uniColor, 1, glm::value_ptr(getKeyColor(lastKey)));
            glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(heldKeyModel));
            glDrawArrays(GL_TRIANGLES, 0, teapotModel.numVertices);
        }
        
        SDL_GL_SwapWindow(window);
        
        float t_end = SDL_GetTicks();
        char update_title[100];
        float time_per_frame = t_end-t_start;
        avg_render_time = .98*avg_render_time + .02*time_per_frame;
        snprintf(update_title, sizeof(update_title), "%s [%3.0f ms] Keys: %lu", 
                 window_title, avg_render_time, collectedKeys.size());
        SDL_SetWindowTitle(window, update_title);
    }
    
    glDeleteProgram(shaderProgram);
    glDeleteShader(fragmentShader);
    glDeleteShader(vertexShader);
    
    SDL_GL_DeleteContext(context);
    SDL_Quit();
    return 0;
}