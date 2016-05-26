
typedef struct{
  bool active;
  u64 id;
  char text[64];
}textline;

typedef struct{
  bool active;
  u64 id;
}stackpanel;

typedef struct{
  float left, up, right, down;
}thickness;

typedef struct{
  bool active;
  u64 id;
  vec2 offset;
  vec2 size;
  vec3 color;
}rectangle;


typedef struct{
  bool active;
  u64 parent_id;
  u64 child_id;
}control_pair;


void set_margin(u64 itemid, thickness t);
thickness get_margin(u64 itemid);

control_pair * add_control(u64 itemid, u64 subitem);

void render_gui_window(u64 windowid);
void load_window(u64 id);
GLFWwindow * find_glfw_window(u64 id);
window * get_window_glfw(GLFWwindow * window);

void test_gui();
