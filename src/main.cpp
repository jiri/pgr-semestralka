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
#include <glm/gtc/constants.hpp>
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

class Uniform {
  friend class Program;

  private:
    GLuint program;
    const char *name;

    Uniform(GLuint id, const char *name)
      : program { id }
      , name { name }
      , location { glGetUniformLocation(program, name) }
    { }

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

  public:
    const GLint location;

    template <typename T>
    void operator=(const T &value) {
      set<T>(value);
    }

    template <typename T>
    operator T() {
      return get<T>();
    }
};

/* Uniform implementations */
template <>
void Uniform::set(const vec3 &v) {
  GLint old_id;
  glGetIntegerv(GL_CURRENT_PROGRAM, &old_id);

  glUseProgram(program);
  glUniform3f(location, v.x, v.y, v.z);
  glUseProgram(old_id);
}

template <>
vec3 Uniform::get() {
  vec3 v;
  glGetUniformfv(program, location, value_ptr(v));
  return v;
}

template <>
void Uniform::set(const mat4 &m) {
  GLint old_id;
  glGetIntegerv(GL_CURRENT_PROGRAM, &old_id);

  glUseProgram(program);
  glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m));
  glUseProgram(old_id);
}

template <>
mat4 Uniform::get() {
  mat4 m;
  glGetUniformfv(program, location, value_ptr(m));
  return m;
}

template <>
void Uniform::set(const float &f) {
  GLint old_id;
  glGetIntegerv(GL_CURRENT_PROGRAM, &old_id);

  glUseProgram(program);
  glUniform1f(location, f);
  glUseProgram(old_id);
}

template <>
float Uniform::get() {
  float f;
  glGetUniformfv(program, location, &f);
  return f;
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

    Uniform getUniform(const char *name) {
      return Uniform(id, name);
    }

    Uniform operator[](const char *name) {
      return getUniform(name);
    }

    operator GLuint() const { return id; }

    void editor() {
      ImGui::Begin(name);

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

          case GL_FLOAT: {
            float f = getUniform(name);
            ImGui::SliderFloat(name, &f, 0.0f, 4096.0f, "%.0f");
            getUniform(name) = f;
          } break;

          default:
            break;
        }
      }

      ImGui::End();
    }
};

class Texture {
  private:
    GLuint id;
    GLint unit;
    int _w, _h;

  public:
    const int &width;
    const int &height;

    Texture(const char *path)
      : width  { _w }
      , height { _h }
      , unit { -1 }
    {
      uint8_t *image = SOIL_load_image(path, &_w, &_h, nullptr, SOIL_LOAD_RGB);

      if (image == nullptr) {
        throw runtime_error {
          SOIL_last_result()
        };
      }

      glGenTextures(1, &id);  

      glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _w, _h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        glGenerateMipmap(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, 0);

      SOIL_free_image_data(image);
    }

    ~Texture() {
      glDeleteTextures(1, &id);
    }

    operator GLuint() {
      return id;
    }

    operator GLuint() const {
      return id;
    }

    void bind(GLint u) {
      unit = u;
      glActiveTexture(GL_TEXTURE0 + unit);
      glBindTexture(GL_TEXTURE_2D, id);
    }

    void unbind() {
      glActiveTexture(GL_TEXTURE0 + unit);
      glBindTexture(GL_TEXTURE_2D, 0);
      unit = -1;
    }
};

template <>
void Uniform::set(const Texture &t) {
  GLint old_id;
  glGetIntegerv(GL_CURRENT_PROGRAM, &old_id);

  glUseProgram(program);
  glUniform1ui(location, t);
  glUseProgram(old_id);
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
  glfwWindowHint(GLFW_FOCUSED,                GL_TRUE);
  /* glfwWindowHint(GLFW_SAMPLES,                4); */

  GLFWwindow *window = glfwCreateWindow(800, 600, "", nullptr, nullptr);

  /* glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); */

  /* Initialize ImGui */
  ImGui_ImplGlfwGL3_Init(window, true);

  /* Set up callbacks */
  /* glfwSetKeyCallback(window, key_callback); */
  /* glfwSetCursorPosCallback(window, mouse_callback); */

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
  /* model *= rotate(radians(90.0f), vec3(1.0f, 0.0f, 0.0f)); */
  /* model *= rotate(radians(-90.0f), vec3(0.0f, 1.0f, 0.0f)); */
  /* model *= scale(vec3(1.0f / map_size)); */
  /* model *= translate(vec3(-0.5f * map_size, -0.5f * map_size, 0.0f)); */
  /* model *= translate(vec3(map_size * -0.5f, map_size * -0.5f, 0.0f)); */

  /* Load textures */
  Texture heightmap { "res/spindl.jpg" };

  /* Cameras */
  mat4 projection = perspective(radians(60.0f), 4.0f / 3.0f, 0.01f, 100.0f);
  mat4 view = lookAt(
      vec3(0.0f, 5.0f, 0.0f),
      vec3(1.0f, 0.0f, 1.0f),
      vec3(0.0f, 1.0f, 0.0f)
  );

  /* Load shaders */
  Program outline { "Outline", "shd/outline.vert", "shd/outline.frag" };

  outline["MVP"]      = projection * view * model;
  outline["map_size"] = (GLfloat) map_size - 1;
  outline["height"]   = map_size / 4.0f;

  /* Timing */
  GLfloat delta = 0.0f;
  GLfloat lastFrame = 0.0f; 

  /* Params */
  vec3 position {  0.0f,  2.0f,  2.0f };
  vec3 target   {  0.0f,  0.0f,  0.0f };

  float rot[3] { -90.0f, 0.0f, 0.0f };

  /* Main loop */
  while (!glfwWindowShouldClose(window)) {
    /* Timing */
    GLfloat currentFrame = glfwGetTime();
    delta = currentFrame - lastFrame;
    lastFrame = currentFrame;

    /* Input */
    glfwPollEvents();
    ImGui_ImplGlfwGL3_NewFrame();

    /* controller.update(delta); */

    /* Shader editors */
    ImGui::Begin("Camera & model");
      ImGui::SliderFloat3("Camera position", value_ptr(position), -30.0f, 30.0f);
      ImGui::SliderFloat3("Camera target",   value_ptr(target),   -30.0f, 30.0f);

      ImGui::SliderFloat3("Rotation", rot, 0.0f, 360.0f);

      ImGui::Image((GLvoid*)(GLuint)heightmap, ImVec2(100, 100), ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255,255), ImColor(255,255,255,128));
    ImGui::End();

    mat4 projection = perspective(radians(60.0f), 4.0f / 3.0f, 0.01f, 100.0f);
    mat4 view = lookAt(position, target, vec3(0.0f, 1.0f, 0.0f));

    mat4 model;
    model *= rotate(radians(rot[0]), vec3(1.0f, 0.0f, 0.0f));
    model *= rotate(radians(rot[1]), vec3(0.0f, 1.0f, 0.0f));
    model *= rotate(radians(rot[2]), vec3(0.0f, 0.0f, 1.0f));
    model *= translate(vec3(-0.5f, -0.5f,  0.0f));
    model *= scale(vec3(1.0f / map_size));

    outline.editor();

    /* Rendering */
    glClearColor(0.322f, 0.275f, 0.337f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(vao);

    heightmap.bind(0);

    glUseProgram(outline);
      outline["MVP"] = projection * view * model;
      outline["heightmap"] = heightmap;

      glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr);
    glUseProgram(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    ImGui::Render();

    glfwSwapBuffers(window);
  }

  /* TODO: Cleanup */

  ImGui_ImplGlfwGL3_Shutdown();
  glfwTerminate();

  return 0;
}
