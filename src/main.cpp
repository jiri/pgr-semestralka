#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace std;

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define GLEW_STATIC
#include <GL/glew.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
using namespace glm;

char *slurp_file(const char *path) {
  char *buffer = nullptr;
  uint32_t length;

  FILE *f = fopen(path, "rb");
  if (f) {
    /* Get file length */
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read data */
    buffer = (char *) malloc(length + 1);
    buffer[length] = 0;
    if (buffer) {
      fread (buffer, 1, length, f);
    }
    fclose (f);
  }

  return buffer;
}

GLuint create_shader(GLenum type, const char *path) {
  GLuint id = glCreateShader(type);
  char *src = slurp_file(path);

  glShaderSource(id, 1, &src, nullptr);
  glCompileShader(id);

  GLint success;
  GLchar infoLog[512];
  glGetShaderiv(id, GL_COMPILE_STATUS, &success);

  if (!success) {
    glGetShaderInfoLog(id, 512, nullptr, infoLog);
    fprintf(stderr, "Compilation of shader '%s' failed:\n%s", path, infoLog);
  }

  return id;
}

GLuint create_program(const char *vpath, const char *fpath) {
  GLuint id = glCreateProgram();

  GLuint vsh = create_shader(GL_VERTEX_SHADER, vpath);
  GLuint fsh = create_shader(GL_FRAGMENT_SHADER, fpath);

  glAttachShader(id, vsh);
  glAttachShader(id, fsh);
  glLinkProgram(id);
  
  glDeleteShader(vsh);
  glDeleteShader(fsh);

  GLint success;
  GLchar infoLog[512];
  glGetProgramiv(id, GL_COMPILE_STATUS, &success);

  if (!success) {
    glGetProgramInfoLog(id, 512, nullptr, infoLog);
    fprintf(stderr, "Compilation of program failed:\n%s", infoLog);
  }

  return id;
}

void error_callback(int error, const char *message) {
  fprintf(stderr, "GLFW error: %s\n", message);
  glfwTerminate();
  exit(EXIT_FAILURE);
}

void printf_vec3(const vec3 &v) {
  printf("[%f %f %f]\n", v.x, v.y, v.z);
}

class Camera {
  friend class CameraController;

  private:
    vec3 eye;
    vec3 direction;
    vec3 up;
    vec3 right;

    GLfloat pitch;
    GLfloat yaw;

    void updateViewMatrix() {
      view  = lookAt(eye, eye + direction, up);
    }

  public:
    mat4 projection;
    mat4 view;

    Camera(mat4 p, const vec3 &e, const vec3 &c, const vec3 &u = vec3(0.0f, 1.0f, 0.0f))
      : projection { p }
      , eye { e }
      , direction { normalize(c - e) }
      , up { normalize(u) }
      , right { normalize(cross(direction, up)) }
      , pitch { degrees(asin(direction.y)) }
      , yaw { degrees(atan2(direction.x, direction.z)) }
    {
      updateViewMatrix();
    };

    static Camera orthographicCamera(const vec3 &e, const vec3 &c, GLfloat left, GLfloat right, GLfloat bottom, GLfloat top) {
      mat4 projection = ortho(left, right, bottom, top);
      return Camera(projection, e, c);
    }

    static Camera perspectiveCamera(const vec3 &e, const vec3 &c, GLfloat fov, GLfloat ratio, GLfloat near = 0.01f, GLfloat far = 100.0f) {
      mat4 projection = perspective(radians(fov), ratio, near, far);
      return Camera(projection, e, c);
    }
};

class CameraController {
  private:
    vector<Camera *> cameras;
    GLuint cameraIndex = 0;

    bool keys[6];

    GLfloat speed;

  public:
    CameraController()
      : keys { false }
      , speed { 5.0f }
    { }

    void addCamera(Camera *c) {
      cameras.push_back(c);
    }

    Camera * camera() {
      return cameras[cameraIndex];
    }

    void handleKey(int key, int action) {
      bool mod = false;
      
      switch (action) {
        case GLFW_PRESS:
          mod = true;
          break;

        case GLFW_RELEASE:
          mod = false;
          break;

        default:
          return;
      }

      switch (key) {
        case GLFW_KEY_W:
        case GLFW_KEY_UP:
          keys[0] = mod;
          break;
        case GLFW_KEY_S:
        case GLFW_KEY_DOWN:
          keys[1] = mod;
          break;
        case GLFW_KEY_A:
        case GLFW_KEY_LEFT:
          keys[2] = mod;
          break;
        case GLFW_KEY_D:
        case GLFW_KEY_RIGHT:
          keys[3] = mod;
          break;
        case GLFW_KEY_Q:
          keys[4] = mod;
          break;
        case GLFW_KEY_E:
          keys[5] = mod;
          break;
    
        case GLFW_KEY_TAB:
          if (action == GLFW_PRESS) {
            cameraIndex += 1;
            cameraIndex %= cameras.size();

            printf("Switching to camera %u\n", cameraIndex);
          }
          break;

        default:
          break;
      }
    }

    void handleMouse(int dx, int dy) {
      if (cameras.empty()) {
        return;
      }

      Camera *camera = cameras[cameraIndex];

      camera->pitch -= dy * 0.1f;
      camera->yaw   += dx * 0.1f;

      if (camera->pitch > 89.0f) {
        camera->pitch = 89.0f;
      }
      if (camera->pitch < -89.0f) {
        camera->pitch = -89.0f;
      }

      camera->direction.x = cos(radians(camera->yaw)) * cos(radians(camera->pitch));
      camera->direction.y = sin(radians(camera->pitch));
      camera->direction.z = sin(radians(camera->yaw)) * cos(radians(camera->pitch));

      camera->right = normalize(cross(camera->direction, camera->up));
    }

    void update(GLfloat delta) {
      if (cameras.empty()) {
        return;
      }

      Camera *camera = cameras[cameraIndex];

      if (keys[0]) { camera->eye += delta * speed * camera->direction; }
      if (keys[1]) { camera->eye -= delta * speed * camera->direction; }
      if (keys[2]) { camera->eye -= delta * speed * camera->right;     }
      if (keys[3]) { camera->eye += delta * speed * camera->right;     }
      if (keys[4]) { camera->eye -= delta * speed * camera->up;        }
      if (keys[5]) { camera->eye += delta * speed * camera->up;        }

      camera->updateViewMatrix();
    }
};

CameraController controller;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
  controller.handleKey(key, action);
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
  static double old_xpos = xpos;
  static double old_ypos = ypos;

  controller.handleMouse((int) xpos - old_xpos, (int) ypos - old_ypos);

  old_xpos = xpos;
  old_ypos = ypos;
}

int main() {
  /* Create a window */
  glfwSetErrorCallback(error_callback);

  glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,  3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,  3);
  glfwWindowHint(GLFW_OPENGL_PROFILE,         GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,  GL_TRUE);
  glfwWindowHint(GLFW_RESIZABLE,              GL_FALSE);

  GLFWwindow *window = glfwCreateWindow(800, 600, "", nullptr, nullptr);

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  glfwSetKeyCallback(window, key_callback);
  glfwSetCursorPosCallback(window, mouse_callback);

  /* Initialize OpenGL */
  glfwMakeContextCurrent(window);

  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    printf("Failed to initialize GLEW.\n");
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LINE_SMOOTH);

  /* VSync on */
  glfwSwapInterval(1);

  /* Create data */
                             /*                       */
                             /* y                z    */
  vector<GLfloat> vertices { /* |               /     */
    -0.5f, -0.5f, -0.5f,     /*                       */
    -0.5f, -0.5f,  0.5f,     /*    3---------7        */
    -0.5f,  0.5f, -0.5f,     /*   /|        /|        */
    -0.5f,  0.5f,  0.5f,     /*  / |       / |        */
     0.5f, -0.5f, -0.5f,     /* 2---------6  |        */
     0.5f, -0.5f,  0.5f,     /* |  |      |  |        */
     0.5f,  0.5f, -0.5f,     /* |  1------|--5        */
     0.5f,  0.5f,  0.5f,     /* | /       | /         */
  };                         /* |/        |/          */
                             /* 0---------4     -- x  */
                             /*                       */

  vector<GLuint> indices {
    0, 1, 2,  1, 3, 2,
    0, 2, 4,  4, 2, 6,
    0, 4, 1,  5, 0, 4,
    5, 4, 6,  5, 6, 7,
    6, 2, 3,  6, 3, 7,
    1, 5, 3,  5, 7, 3,
  };

  GLuint vao;
  glGenVertexArrays(1, &vao);

  GLuint vbo, ebo;
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);

  glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLfloat), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(0);
  glBindVertexArray(0);

  /* Load shaders */
  GLuint simple = create_program("shd/simple.vert", "shd/simple.frag");

  /* GLM math */
  Camera c1 = Camera::perspectiveCamera(vec3(5.0f, 5.0f, 5.0f), vec3(0.0f, 0.0f, 0.0f), 75.0f, 800.0f / 600.0f);
  controller.addCamera(&c1);

  Camera c2 = Camera::perspectiveCamera(vec3(5.0f, 8.0f, 5.0f), vec3(0.0f, 0.0f, 0.0f), 75.0f, 800.0f / 600.0f);
  controller.addCamera(&c2);
  
  /* Timing */
  GLfloat delta = 0.0f;
  GLfloat lastFrame = 0.0f; 

  /* Main loop */
  while (!glfwWindowShouldClose(window)) {
    /* Timing */
    GLfloat currentFrame = glfwGetTime();
    delta = currentFrame - lastFrame;
    lastFrame = currentFrame;

    /* Input */
    glfwPollEvents();
    controller.update(delta);

    /* Rendering */
    glClearColor(0.322f, 0.275f, 0.337f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(vao);
    glUseProgram(simple);

    mat4 mvp = controller.camera()->projection * controller.camera()->view;
    GLuint mvp_loc = glGetUniformLocation(simple, "MVP");
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, value_ptr(mvp));

    glLineWidth(20.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr);

    glUseProgram(0);
    glBindVertexArray(0);

    glfwSwapBuffers(window);
  }

  /* TODO: Cleanup */

  return 0;
}
