#include <GL/glew.h>
#include <iron/full.h>
#include "persist.h"
#include "shader_utils.h"
#include "game.h"

#include <GLFW/glfw3.h>
#include "gui_test.h"
#include "persist_oop.h"
#include "gui.h"
#include "console.h"

vec3 vec3_rand(){
  float rnd(){ return 0.001 * (rand() % 1000); }
  return vec3_new(rnd(), rnd(), rnd());
}

void rectangle_clicked(u64 control, double x, double y){
  UNUSED(x);UNUSED(y);
  logd("WUUT!\n");
  rectangle * rect = get_rectangle(control);
  ASSERT(rect != NULL);
  if(mouse_button_action == 1)
    rect->color = vec3_new(0, 0, 0);
  else{
    rect->color = vec3_scale(vec3_rand(), 0.01);
  }
}

typedef struct{
  vec2 position;
  vec2 size;
} body;

CREATE_TABLE(body, u64, body);
CREATE_MULTI_TABLE(board_elements, u64, u64);

void measure_game_board(u64 id, vec2 * size){
  UNUSED(id);
  *size = vec2_new(0, 0);
}


void render_game_board(u64 id){
  UNUSED(id);
  rect_render(vec3_new(0.3,0.3,0.6), shared_offset, shared_size);
  u64 board_element_cnt = 0;
  u64 bodies[10];
  size_t iter = 0;
  do{
    board_element_cnt = iter_board_elements(id, bodies, array_count(bodies), &iter);
    for(u64 i = 0; i < board_element_cnt; i++){
      body b = get_body(bodies[i]);
      rect_render(vec3_new(1,0,0), vec2_scale(b.position, 10), vec2_scale(b.size, 10));
      set_body(bodies[i], b);
    }
    
  }while(board_element_cnt == array_count(bodies));
}

void load_game_board(u64 id){
  static bool initialized = false;
  static u64 game_board_class;
  if(!initialized){
    initialized = true;
    game_board_class = intern_string("game_board_class");
    define_method(game_board_class, render_control_method, (method)render_game_board);
    define_method(game_board_class, measure_control_method, (method)measure_game_board);
  }
  define_subclass(id, game_board_class);
  
}

bool starts_with(const char *pre, const char *str)
{
  size_t lenpre = strlen(pre);
  size_t lenstr = strlen(str);
  return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

#define UID ({u64 get_number(){static u64 number = 0; if(number == 0) number = get_unique_number2(); return number;}; get_number;})

typedef enum{
  COMMAND_ARG_POSITION,
  COMMAND_ARG_ENTITY,
  COMMAND_ARG_ITEM,

} command_arg_type;

typedef struct{
  command_arg_type type;
  union{
    struct{
      int x;
      int y;
    };
    u64 id;
  };

}command_arg;

CREATE_MULTI_TABLE(faction_visible_items, u64, u64);
u64 get_visible_items(u64 id, u64 * items, u64 items_cnt){
  UNUSED(id);
  if(items == NULL){
    return get_faction_visible_items(1, NULL, 10000);
  }
  return get_faction_visible_items(1, items, items_cnt);
}

CREATE_TABLE(command_invocation, u64, u64);
CREATE_MULTI_TABLE(command_queue, u64, u64);
CREATE_MULTI_TABLE(command_args, u64, command_arg);
CREATE_MULTI_TABLE(available_commands, u64, u64);
CREATE_STRING_TABLE(command_name, u64);
CREATE_TABLE(target, u64, vec2);
CREATE_TABLE(is_paused, u64, bool);
typedef enum{
  CMD_DONE,
  CMD_NOT_DONE
}command_state;

u64 invoke_command_method = 0;
u64 command_class = 0;

void update_game_board(u64 id){

  void handle_commands(u64 entity){
    u64 command = 1, it = 0;
    while(command != 0){
      int c = iter_command_queue(entity, &command, 1,  &it);
      if(c == 0) break;
      if(command != 0){
	u64 cmd = get_command_invocation(command);
	command_state (* cmd2)(u64 id, u64 id2) =(void *) get_method(cmd, invoke_command_method);
	bool pop = false;
	if(cmd2 == NULL){
	  pop = true;
	}else{
	  if(cmd2(command, entity) == CMD_DONE)
	    pop = true;
	}
	if(pop)
	  clear_at_command_queue(it - 1);
	else break;
      }
    }
  }
  handle_commands(id);
  if(get_is_paused(id))
    return;
  u64 board_element_cnt = 0;
  u64 bodies[10];
  size_t iter = 0;
  do{
    board_element_cnt = iter_board_elements(id, bodies, array_count(bodies), &iter);
    for(u64 i = 0; i < board_element_cnt; i++){
      handle_commands(bodies[i]);

      body b = get_body(bodies[i]);
      vec2 target = get_target(bodies[i]);
      vec2 d = vec2_sub(target, b.position);
      float dl = vec2_len(d);
      if(dl > 0){
	if(dl > 1.0)
	  d = vec2_scale(d, 1.0 / dl);
	b.position = vec2_add(vec2_scale(d, 0.1), b.position);
	set_body(bodies[i], b);
	
      }
    }    
  }while(board_element_cnt == array_count(bodies));
}

u64 parse_command(u64 id, const char * str, command_arg * out_args, u64 * in_out_cnt){
  u64 visible_item_cnt = get_visible_items(id, NULL, 0);
  logd("VISIBLE ITEM CNT: %i\n", visible_item_cnt);
  u64 visible_items[visible_item_cnt];
  get_visible_items(id, visible_items, visible_item_cnt);
  *in_out_cnt = 0;
  u64 avail_commands[32];
  u64 idx = 0;
  u64 cnt = 0;
  while(0 < (cnt = iter_available_commands(id, avail_commands, array_count(avail_commands), &idx))){
    for(u64 i = 0; i < cnt; i++){
      named_item nm = unintern_string(avail_commands[i]);
      if(false && starts_with(str, nm.name)){

      }else{
	char buf[33];
	int offset = 0;
	if(strcmp(str,nm.name) == 0){
	  offset = sprintf(buf, "%s", nm.name);
	}else{
	  offset = sprintf(buf, "%s ", nm.name);
	}
	
	if(starts_with(buf, str)){
	  char * args_start = ((char *)str) + offset;
	  while(*args_start != 0){
	    while(*args_start == ' ')
	      args_start += 1;
	    if(*args_start == 0) break;
	    int x, y;
	    int c = sscanf(args_start, "%i ", &x);

	    if(c == 1){
	      while(*args_start != ' ' && *args_start != 0)
		args_start += 1;
	      
	      while(*args_start == ' ')
		args_start += 1;
	      c += sscanf(args_start, "%i", &y);
	      while(*args_start != ' ' && *args_start != 0)
		args_start += 1;
	    }
	    if(c == 2){ 
	      out_args->type = COMMAND_ARG_POSITION;
	      out_args->x = x;
	      out_args->y = y;
	      *in_out_cnt += 1;
	      out_args += 1;
	      continue;
	    }else if(c == 1)
	      return 0;
	    
	    c = sscanf(args_start, "%s ", buf );
	    
	    if(c == 1 && strlen(buf) > 0){
	      u64 prevcnt = *in_out_cnt;
	      for(u64 i = 0; i < visible_item_cnt; i++){
		char buf2[33];
		u64 c2 = get_name(visible_items[i], buf2, 33);
	        if(c2 > 0 && strcmp(buf2, buf) == 0){
		  
		  args_start += strlen(buf2);
		  out_args->type = COMMAND_ARG_ENTITY;
		  out_args->id = visible_items[i];
		  *in_out_cnt += 1;
		  break;
		}
	      }
	      if(prevcnt == *in_out_cnt){
		return 0;
	      }
	    }else return 0;
	    
	  }
	  
	  return avail_commands[i];
	}
      }
    }
  }
  return 0;
}

void test_gui(){

  init_gui();
  window * win = make_window(4);
  sprintf(win->title, "%s", "Test Window");
  logd("window opened: %p\n", win);

  u64 game_board = intern_string("game board");
  load_game_board(game_board);
  add_control(win->id, game_board);

  u64 ball = intern_string("Ball");
  set_name(ball, "Ball");
  add_faction_visible_items(1, ball); 
  set_body(ball, (body){vec2_new(20,10), vec2_new(1,1)});
  add_board_elements(game_board, ball);
  
  u64 player = intern_string("Player");
  set_name(player, "Player");
  add_faction_visible_items(1, player);
  set_body(player, (body){ vec2_new(1,1), vec2_new(2,2) });
  add_board_elements(game_board, player);

  clear_available_commands();
  command_class = intern_string("command class");
  invoke_command_method = intern_string("invoke");

  command_state invoke_command(u64 cmd, u64 id){
    logd("No handler defined for %i: %i\n", cmd, id);
    return CMD_DONE;
  }

  define_method(command_class, invoke_command_method, (method)invoke_command);
  
  u64 move_cmd = intern_string("move");
  define_subclass(move_cmd, command_class);
  add_available_commands(player, move_cmd);

  command_state do_move(u64 cmd, u64 id){
    command_arg arg;
    if(get_command_args(cmd, &arg, 1) == 0) return CMD_DONE;
    int x = 0, y = 0;
    if(arg.type == COMMAND_ARG_POSITION){
      x = arg.x;
      y = arg.y;
    }else{
      body b = get_body(arg.id);
      x = b.position.x;
      y = b.position.y;
    }
    body b = get_body(id);
    float d = vec2_len(vec2_sub(b.position, vec2_new(x, y)));
    set_target(id, vec2_new(x, y));
    logd("Distance: %f\n", d);
    if(d > 0.1)
      return CMD_NOT_DONE;
    return CMD_DONE;
  }
  define_method(move_cmd, invoke_command_method, (method) do_move);
  
  u64 stop_cmd = intern_string("stop");
  define_subclass(stop_cmd, command_class);
  add_available_commands(player, stop_cmd);

  u64 pause_cmd = intern_string("pause");
  command_state _pause_board(u64 cmd, u64 id){
    UNUSED(cmd);
    logd("Pause! %i\n", id);
    set_is_paused(id, !get_is_paused(id));
    return CMD_DONE;
  }
  define_method(pause_cmd, invoke_command_method, (method)_pause_board);
  define_subclass(pause_cmd, command_class);
  add_available_commands(game_board, pause_cmd);
  
  //return;
  
  stackpanel * panel = get_stackpanel(intern_string("stackpanel1"));
  set_horizontal_alignment(panel->id, HALIGN_CENTER);
  panel->orientation = ORIENTATION_VERTICAL;
  set_margin(panel->id, (thickness){30,30,20,20});
  ASSERT(panel->id != win->id && panel->id != 0);
  add_control(win->id, panel->id);
  rectangle * rect = get_rectangle(intern_string("rect1"));
  define_method(rect->id, mouse_down_method, (method) rectangle_clicked);
  if(once(rect->id)){
    rect->size = vec2_new(30, 30);
    rect->color = vec3_new(1, 0, 0);
    set_margin(rect->id, (thickness){1,1,3,3});
    set_corner_roundness(rect->id, (thickness) {1, 1, 1, 1});
  }
  add_control(panel->id, rect->id);
  ASSERT(rect->id != panel->id);

  rect = get_rectangle(intern_string("rect2"));
  if(once(rect->id)){
    rect->size = vec2_new(30, 30);
    rect->color = vec3_new(1, 1, 0);
    set_margin(rect->id, (thickness){1,1,3,3});
    thickness readback = get_margin(rect->id);
    logd("%i: %f %f %f %f\n", rect->id, readback.left, readback.up, readback.right, readback.down);
    ASSERT(get_margin(rect->id).up == 1);
    
    add_control(panel->id, rect->id);
  }
  set_horizontal_alignment(rect->id, HALIGN_RIGHT);
  rect->color = vec3_new(1,0,0);
  rect = get_rectangle(intern_string("rect3"));
  if(once(rect->id)){
    set_margin(rect->id, (thickness){1,1,1,3});
    rect->size = vec2_new(30, 30);
    rect->color = vec3_new(1, 1, 1);
    add_control(panel->id, rect->id);
  }
  
  u64 txtid = get_textline(intern_string("txt1"));
  if(once(txtid)){
    set_text(txtid, "Click me!");
    set_margin(txtid, (thickness){40,3,40,3});
  }
  
  stackpanel * panel2 = get_stackpanel(intern_string("stackpanel_nested"));
  add_control(panel->id, panel2->id);
  add_control(panel2->id, rect->id);

  u64 btn_test = intern_string("button_test");
  if(once(btn_test)){
    set_margin(btn_test, (thickness) {10, 1, 10, 1});
    define_subclass(btn_test, ui_element_class);
  }

  void rectangle_clicked(u64 control, double x, double y){
    rectangle * r = get_rectangle(control);
    vec2 size = r->size;
    if(x > size.x || y > size.y || x < 0 || y < 0) return;
    
    r->color = vec3_rand();
  }
  define_method(rectangle_class, mouse_down_method, (method) rectangle_clicked);
  
  int color_idx = 0;
  vec3 colors[] = {vec3_new(1,0,0), vec3_new(0,1,0), vec3_new(0, 0, 1)};
  
  void button_clicked2(u64 control, double x, double y){
    UNUSED(x);UNUSED(y);UNUSED(control);
    logd("Clicked!\n");
    rectangle * r = get_rectangle(intern_string("rect4"));
    r->color = colors[color_idx];
    color_idx = (color_idx + 1) % array_count(colors);
  }

  void button_render(u64 control){
    u64 index = 0;
    control_pair * child_control = NULL;
    vec2 _size = vec2_zero;
    while((child_control = get_control_pair_parent(control, &index))){
       if(child_control == NULL)
	break;
       rectangle * rect = find_rectangle(child_control->child_id, false);
       if(rect != NULL)
	 continue;
       vec2 size = measure_sub(child_control->child_id);
       _size = vec2_max(_size, size);
    }
    index = 0;
    while((child_control = get_control_pair_parent(control, &index))){
      if(child_control == NULL)
	break;
      rectangle * rect = find_rectangle(child_control->child_id, false);
      if(rect == NULL) continue;
      rect->size = _size;
    }
    index = 0;
    u64 base = get_baseclass(control, &index);
    method base_method = get_method(base, render_control_method);
    ASSERT(base_method != NULL);
    base_method(control);      
  }
  
  define_method(btn_test, mouse_down_method, (method) button_clicked2);
  define_method(btn_test, render_control_method, (method) button_render);
  
  rect = get_rectangle(intern_string("rect4"));
  if(once(rect->id)){
    set_margin(rect->id, (thickness){0,0,0,0});
    rect->size = vec2_new(40, 40);
    rect->color = vec3_new(0.5, 0.5, 0.5);
  }
  add_control(btn_test, rect->id);
  add_control(btn_test, txtid);
  add_control(panel2->id, btn_test);

  rect = get_rectangle(intern_string("rect5"));
  if(once(rect->id)){
    set_margin(rect->id, (thickness){3,3,3,3});
    rect->size = vec2_new(100, 30);
    rect->color = vec3_new(1, 1, 1);
  }
  add_control(panel->id, rect->id);

  rect = get_rectangle(intern_string("rect6"));
  if(once(rect->id)){
    set_margin(rect->id, (thickness){1,1,1,3});
    rect->size = vec2_new(30, 50);
    rect->color = vec3_new(1, 1, 1);
  }
  add_control(panel->id, rect->id);
  
  rect = get_rectangle(intern_string("rect7"));
  if(once(rect->id)){
    set_margin(rect->id, (thickness){6,6,6,6});
    rect->size = vec2_new(50, 30);
    rect->color = vec3_new(1, 0, 1);
  }
  add_control(panel2->id, rect->id);

  u64 console = intern_string("console");
  set_focused_element(win->id, console);
  set_console_height(console, 300);
  create_console(console);
  add_control(win->id, console);
  set_margin(console, (thickness){5,5,5,5});
  set_vertical_alignment(console, VALIGN_BOTTOM);
  set_console_history_cnt(console, 100);
  void command_entered(u64 id, char * cmd){
    UNUSED(id);
    bool parse_cmd(u64 id){
      command_arg args[10];
      u64 arg_cnt = 10;
      u64 found_cmd = parse_command(id, cmd, args, &arg_cnt);
      if(found_cmd != 0){
	u64 cmd_inv = get_unique_number();
	set_command_invocation(cmd_inv, found_cmd);
	for(u64 i = 0; i < arg_cnt; i++){
	  add_command_args(cmd_inv, args[i]);
	}
	add_command_queue(id, cmd_inv);
	return true;
      }
      return false;
    }
    if(parse_cmd(player)){
      u64 queue_length = get_command_queue(player, NULL, 1000);
      logd("Player Queue length: %i\n", queue_length);
    }else{
      logd("Test game board\n");
      if( parse_cmd(game_board)){
      u64 queue_length = get_command_queue(game_board, NULL, 1000);
      logd("Board Queue length: %i\n", queue_length);
      }
    }
  }
  
  define_method(console, console_command_entered_method, (method)command_entered);

  void print_console(const char * fmt, va_list lst){
    int l = strlen(fmt);
    if(l == 0) return;
    char buf[200];
    vsprintf(buf, fmt, lst);
    push_console_history(console, buf);
  }
  iron_log_printer = print_console;
  
  while(true){

    update_game_board(game_board);
    window * w = persist_alloc("win_state", sizeof(window));
    int cnt = (int)(persist_size(w)  / sizeof(window));
    bool any_active = false;
    for(int j = 0; j < cnt; j++){
      if(!w[j].id != 0) continue;
      any_active = true;
      auto method = get_method(w[j].id, render_control_method);
      if(method!= NULL) method(w[j].id);
    }
    if(!any_active){
      logd("Ending program\n");
      break;
    }

    controller_state * controller = persist_alloc("controller", sizeof(controller_state));
    controller->btn1_clicked = false;
    glfwPollEvents();
    if(controller->btn1_clicked){
      //sprintf(buffer, "window%i", get_unique_number());
      //get_window2(buffer);
    }
  }
}
