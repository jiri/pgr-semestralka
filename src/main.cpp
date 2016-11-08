#include <cstdio>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <stdexcept>
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

#include <boost/filesystem.hpp>
using namespace boost::filesystem;

#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>

#include <SOIL.h>

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

    vec3 eye;

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
      mat4 projection = ortho(left, right, bottom, top, -100.0f, 100.0f);
      return Camera(projection, e, c);
    }

    static Camera perspectiveCamera(const vec3 &e, const vec3 &c, GLfloat fov, GLfloat ratio, GLfloat near = 0.01f, GLfloat far = 1000.0f) {
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

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  /* ImGui_ImplGlfwGL3_KeyCallback(window, key, scancode, action, mods); */
  controller.handleKey(key, action);
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
  static double old_xpos = xpos;
  static double old_ypos = ypos;

  controller.handleMouse(xpos - old_xpos, ypos - old_ypos);

  old_xpos = xpos;
  old_ypos = ypos;
}

class Shader {
  friend class Program;

  private:
    GLuint id;

  public:
    Shader(const char *path, GLenum type = 0) {
      if (type == 0) {
        if (extension(path) == ".vert") { type = GL_VERTEX_SHADER;   }
        if (extension(path) == ".frag") { type = GL_FRAGMENT_SHADER; }

        if (type == 0) {
          throw invalid_argument {
            "Unrecognized file extension '" + extension(path) + "'"
          };
        }
      }

      id = glCreateShader(type);

      char *src = slurp_file(path);
      glShaderSource(id, 1, &src, nullptr);

      glCompileShader(id);

      GLint success;
      glGetShaderiv(id, GL_COMPILE_STATUS, &success);

      if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(id, 512, nullptr, infoLog);
        
        cerr << "Compilation of shader '" << path << "' failed:" << endl
             << infoLog;
      }
    }

    ~Shader() {
      glDeleteShader(id);
    }
};

class UniformProxy {
  friend class Program;

  private:
    GLuint program;
    const char *name;

    UniformProxy(GLuint id, const char *name)
      : program { id }
      , name { name }
      , location { glGetUniformLocation(program, name) }
    { }

  public:
    const GLint location;

    template <typename T>
    void set(const T &value) {
      throw domain_error {
        "Uniform assignment not implemented for " + string(typeid(T).name())
      };
    }

    template <typename T>
    T get() {
      throw domain_error {
        "Uniform retrieval not implemented for " + string(typeid(T).name())
      };
    }

    template <typename T>
    void operator=(const T &value) {
      set(value);
    }

    template <typename T>
    operator T() {
      return get<T>();
    }
};

/* Uniform implementations */
template <>
void UniformProxy::set(const vec3 &v) {
  GLint old_id;
  glGetIntegerv(GL_CURRENT_PROGRAM, &old_id);

  glUseProgram(program);
  glUniform3f(location, v.x, v.y, v.z);
  glUseProgram(old_id);
}

template <>
vec3 UniformProxy::get() {
  vec3 v;
  glGetUniformfv(program, location, value_ptr(v));
  return v;
}

template <>
void UniformProxy::set(const mat4 &m) {
  GLint old_id;
  glGetIntegerv(GL_CURRENT_PROGRAM, &old_id);

  glUseProgram(program);
  glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m));
  glUseProgram(old_id);
}

template <>
mat4 UniformProxy::get() {
  mat4 m;
  glGetUniformfv(program, location, value_ptr(m));
  return m;
}

class Program {
  public:
    GLuint id;
    const char *name;

    Program(const char *name, const Shader &vsh, const Shader &fsh)
      : id { glCreateProgram() }
      , name { name }
    {
      glAttachShader(id, vsh.id);
      glAttachShader(id, fsh.id);

      glLinkProgram(id);
      
      GLint success;
      glGetProgramiv(id, GL_LINK_STATUS, &success);

      if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(id, 512, nullptr, infoLog);

        cerr << "Compilation of program '" << name << "' failed:" << endl
             << infoLog;
      }

      glDetachShader(id, vsh.id);
      glDetachShader(id, fsh.id);
    }

    ~Program() {
      glDeleteProgram(id);
    }

    UniformProxy getUniform(const char *name) {
      return UniformProxy(id, name);
    }

    UniformProxy operator[](const char *name) {
      return getUniform(name);
    }

    operator GLuint() const { return id; }

    void editor() {
      ImGui::Begin(name);

      /* ImGui::Text("Hello"); */

      GLint count;
      glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &count);

      for (GLuint i = 0; i < count; i++) {
        GLint size;
        GLenum type;

        const GLsizei bufSize = 16;
        GLchar name[bufSize];
        GLsizei length;

        glGetActiveUniform(id, i, bufSize, &length, &size, &type, name);
        
        switch (type) {
          case GL_FLOAT_VEC3: {
            vec3 v = getUniform(name);

            glGetUniformfv(id, getUniform(name).location, value_ptr(v));
            ImGui::ColorEdit3(name, value_ptr(v));

            getUniform(name) = v;
          } break;

          default:
            break;
        }
      }

      ImGui::End();
    }
};

int main() {
  /* Create a window */
  glfwSetErrorCallback(error_callback);

  glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,  3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,  3);
  glfwWindowHint(GLFW_OPENGL_PROFILE,         GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,  GL_TRUE);
  glfwWindowHint(GLFW_RESIZABLE,              GL_FALSE);
  /* glfwWindowHint(GLFW_SAMPLES,                4); */

  GLFWwindow *window = glfwCreateWindow(800, 600, "", nullptr, nullptr);

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  /* Initialize ImGui */
  /* ImGui_ImplGlfwGL3_Init(window, false); */

  /* Set up callbacks */
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
  /* glEnable(GL_CULL_FACE); */

  /* VSync on */
  glfwSwapInterval(1);

  /* Create data */
  //                            /*                       */
  //                            /* y                z    */
  // vector<GLfloat> vertices { /* |               /     */
  //   -0.5f, -0.5f, -0.5f,     /*                       */
  //   -0.5f, -0.5f,  0.5f,     /*    3---------7        */
  //   -0.5f,  0.5f, -0.5f,     /*   /|        /|        */
  //   -0.5f,  0.5f,  0.5f,     /*  / |       / |        */
  //    0.5f, -0.5f, -0.5f,     /* 2---------6  |        */
  //    0.5f, -0.5f,  0.5f,     /* |  |      |  |        */
  //    0.5f,  0.5f, -0.5f,     /* |  1------|--5        */
  //    0.5f,  0.5f,  0.5f,     /* | /       | /         */
  // };                         /* |/        |/          */
  //                            /* 0---------4     -- x  */
  //                            /*                       */

  // vector<GLuint> indices {
  //   0, 1, 2,  1, 3, 2,
  //   0, 2, 4,  4, 2, 6,
  //   0, 4, 1,  5, 1, 4,
  //   5, 4, 6,  5, 6, 7,
  //   6, 2, 3,  6, 3, 7,
  //   1, 5, 3,  5, 7, 3,
  // };

  /* Create map */
  const GLuint map_size = 2048;

  vector<GLfloat> vertices;
  vector<GLuint> indices;

  for (GLuint y = 0; y < map_size; y++) {
    for (GLuint x = 0; x < map_size; x++) {
      vertices.insert(vertices.end(), {
          x * 1.0f,
          y * 1.0f,
          0.0f,
      });
    }
  }

  for (GLuint y = 0; y < map_size - 1; y++) {
    for (GLuint x = 0; x < map_size - 1; x++) {
      indices.insert(indices.end(), {
        (y + 0) * map_size + (x + 0),
        (y + 0) * map_size + (x + 1),
        (y + 1) * map_size + (x + 1),
        (y + 0) * map_size + (x + 0),
        (y + 1) * map_size + (x + 1),
        (y + 1) * map_size + (x + 0),
      });
    }
  }

  /* Create buffers */
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

  mat4 model;
  model *= rotate(radians(-90.0f), vec3(1.0f, 0.0f, 0.0f));
  model *= scale(vec3(0.01f, 0.01f, 0.01f));
  /* model *= translate(vec3(map_size * -0.5f, map_size * -0.5f, 0.0f)); */

  /* Load textures */
  GLuint tex; {
    int width, height;
    unsigned char *image = SOIL_load_image(
        "res/spindl.jpg",
        &width, &height,
        nullptr,
        SOIL_LOAD_RGB
    );

    if (image == nullptr) {
      cerr << SOIL_last_result() << endl;
    }

    glGenTextures(1, &tex);  

    glBindTexture(GL_TEXTURE_2D, tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
      glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    SOIL_free_image_data(image);
  }

  GLuint tex2; {
    int width, height;
    unsigned char *image = SOIL_load_image(
        "res/forest.jpg",
        &width, &height,
        nullptr,
        SOIL_LOAD_RGB
    );

    if (image == nullptr) {
      cerr << SOIL_last_result() << endl;
    }

    glGenTextures(1, &tex2);  

    glBindTexture(GL_TEXTURE_2D, tex2);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
      glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    SOIL_free_image_data(image);
  }

  /* Cameras */
  Camera c1 = Camera::perspectiveCamera(
      vec3(0.0f, 5.0f, 0.0f),
      vec3(0.0f, 0.0f, 0.0f),
      60.0f,
      800.0f / 600.0f
  );
  controller.addCamera(&c1);

  /* Camera c2 = Camera::orthographicCamera(vec3(5.0f, 5.0f, 5.0f), vec3(0.0f, 0.0f, 0.0f), -3.0f, 3.0f, -3.0f, 3.0f); */
  /* controller.addCamera(&c2); */

  /* Load shaders */
  mat4 mvp = controller.camera()->projection * controller.camera()->view;

  Program simple  { "Simple",  "shd/simple.vert",  "shd/unicolor.frag" };
  simple["MVP"]   = mvp;
  simple["color"] = vec3(0.655f, 0.773f, 0.741f);

  Program outline { "Outline", "shd/outline.vert", "shd/outline.frag" };
  outline["MVP"]   = mvp;
  /* outline["color"] = vec3(0.898f, 0.867f, 0.796f); */

  Program wires   { "Wires",   "shd/outline.vert", "shd/unicolor.frag" };
  wires["MVP"]   = mvp;
  wires["color"] = vec3(0.922f, 0.482f, 0.349f);

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
    /* ImGui_ImplGlfwGL3_NewFrame(); */

    controller.update(delta);

    /* Shader editors */
    /* simple.editor(); */

    /* Rendering */
    glClearColor(0.322f, 0.275f, 0.337f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(vao);

    /* glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); */
    /* glUseProgram(outline); */
    /*   glCullFace(GL_FRONT); */
    /*   glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr); */
    /* glUseProgram(0); */

    /* glUseProgram(simple); */
    /*   glCullFace(GL_BACK); */
    /*   glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr); */
    /* glUseProgram(0); */

    /* glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex2);

    glUseProgram(outline);
      outline["MVP"] = controller.camera()->projection * controller.camera()->view * model;
      glUniform1i(outline["tex"].location, 0);
      glUniform1i(outline["tex2"].location, 1);

      glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr);
    glUseProgram(0);

    glBindTexture(GL_TEXTURE_2D, 0);

    glBindVertexArray(0);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    /* ImGui::Render(); */

    glfwSwapBuffers(window);
  }

  /* TODO: Cleanup */

  /* ImGui_ImplGlfwGL3_Shutdown(); */
  glfwTerminate();

  return 0;
}
