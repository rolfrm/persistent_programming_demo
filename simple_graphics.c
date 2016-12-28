#include <GL/glew.h>
#include <iron/full.h>
#include "persist.h"
#include "sortable.h"
#include "persist_oop.h"
#include "shader_utils.h"
#include "game.h"

#include <GLFW/glfw3.h>

#include "gui.h"
#include "gui_test.h"
#include "index_table.h"
#include "game_board.h"
#include "console.h"
typedef struct{
  vec3 position;
  u32 model;
}entity_data;

typedef enum{
  KIND_MODEL,
  KIND_POLYGON
}model_kind;

typedef struct{
  index_table_sequence polygons;
}model_data;

typedef struct{
  index_table_sequence vertexes;
  u32 material;
}polygon_data;

typedef struct{
  u32 color;
  vec2 position;
}vertex_data;


typedef struct{
  u32 entity;
  u32 model;
  u32 polygon_offset;
  u32 vertex_offset;
}entity_local_data;

typedef struct{
  entity_local_data entity1;
  entity_local_data entity2;
}collision_data;


void print_model_data(u32 index, index_table * models, index_table * polygon, index_table * vertex){
  model_data * m2 = index_table_lookup(models, index);
  for(u32 i = 0; i < m2->polygons.count; i++){
    u32 index = i + m2->polygons.index;
    logd("POLYGON:\n");
    polygon_data * pd = index_table_lookup(polygon, index);
    vertex_data * vd = index_table_lookup_sequence(vertex, pd->vertexes);
    logd("  Vertex: %p ", vd->color);
    vec2_print(vd->position);
    logd("\n"); 
  }
}

typedef struct{
  u32 gl_ref;
  u32 count;
}loaded_polygon_data;

CREATE_TABLE_DECL2(active_entities, u32, bool);
CREATE_TABLE_NP(active_entities, u32, bool);
CREATE_TABLE_DECL2(loaded_polygon, u32, loaded_polygon_data);
CREATE_TABLE_NP(loaded_polygon, u32, loaded_polygon_data);
CREATE_TABLE_DECL2(polygon_color, u32, vec4);
CREATE_TABLE2(polygon_color, u32, vec4);


typedef struct{
  index_table * entities;
  index_table * models;
  index_table * polygon;
  index_table * vertex;
  loaded_polygon_table * gpu_poly;
  index_table * polygons_to_delete;
  polygon_color_table * poly_color;
  active_entities_table * active_entities;
  vec2 render_size;
  vec2 pointer_position;
  u32 pointer_index;
  index_table * collision_table;

}graphics_context;

typedef u32 polygon_id;
polygon_id polygon_create(graphics_context * ctx);
void polygon_add_vertex2f(graphics_context * ctx, polygon_id polygon, vec2 offset);

void graphics_context_load(graphics_context * ctx){
  memset(ctx, 0, sizeof(*ctx));
  ctx->models = index_table_create("simple/models", sizeof(model_data));
  ctx->polygon = index_table_create("simple/polygon", sizeof(polygon_data));

  ctx->vertex = index_table_create("simple/vertex", sizeof(vertex_data));
  ctx->entities = index_table_create("simple/entities", sizeof(entity_data));
  ctx->polygons_to_delete = index_table_create(NULL, sizeof(u32));
  ctx->gpu_poly = loaded_polygon_table_create(NULL);
  ctx->poly_color = polygon_color_table_create("simple/polygon_color");
  ctx->active_entities = active_entities_table_create("simple/active_entities");
  polygon_id p1 = polygon_create(ctx);
  ctx->pointer_index = p1;
  ctx->collision_table = index_table_create(NULL, sizeof(collision_data));
  polygon_add_vertex2f(ctx, p1, vec2_new(0, 0));
}


polygon_id polygon_create(graphics_context * ctx){
  return index_table_alloc(ctx->polygon);
}

void polygon_add_vertex2f(graphics_context * ctx, polygon_id polygon, vec2 offset){

  polygon_data * pd = index_table_lookup(ctx->polygon, polygon);
  u32 pcnt = pd->vertexes.count;
  index_table_resize_sequence(ctx->vertex, &(pd->vertexes), pcnt + 1);
  
  vertex_data * v = index_table_lookup_sequence(ctx->vertex, pd->vertexes);
  v[pcnt].position = offset;
}

u32 vertex_get_polygon(graphics_context ctx, u32 vertex){
  u64 polycnt = 0;
  polygon_data * pd = index_table_all(ctx.polygon, &polycnt);
  for(u64 i = 0; i < polycnt; i++){
    u32 offset = pd[i].vertexes.index;
    u32 count = pd[i].vertexes.count;
    if(offset <= vertex && vertex < offset + count){
      return i + 1;
    }
  }
  return 0;
}


void simple_graphics_load_test(graphics_context * ctx){
  u32 i1 = index_table_alloc(ctx->models);
  
  model_data * m1 = index_table_lookup(ctx->models, i1);
  {
    polygon_id p1 = polygon_create(ctx);
    polygon_add_vertex2f(ctx, p1, vec2_new(0,0));
    m1->polygons = (index_table_sequence){.index = p1, .count = 1};
  }
  u32 player = index_table_alloc(ctx->entities);
  entity_data * ed = index_table_lookup(ctx->entities, player);
  ed->position = vec3_new(0,0,0);
  ed->model = i1;

  active_entities_set(ctx->active_entities, player, true);
}


CREATE_TABLE_DECL2(graphics_context, u64, graphics_context);
CREATE_TABLE_NP(graphics_context, u64, graphics_context);
u32 simple_grid_polygon_load(vec2 * vertexes, u32 count){
  u32 vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertexes[0]) * count, vertexes, GL_STATIC_DRAW);
  logd("Loaded %i bytes to VBO\n", sizeof(vertexes[0]) * count);
  return vbo;
}

typedef struct{
  u32 shader;
  u32 camera_loc;
  u32 color_loc;
  u32 vertex_loc;

}simple_grid_shader;

simple_grid_shader simple_grid_shader_get(){
  static bool initialized = false;
  static u32 shader = 0;
  static u32 camera_loc, color_loc, vertex_loc;
  
  if(!initialized){
    initialized = true;
    char * vs = read_file_to_string("simple_shader.vs");
    char * fs = read_file_to_string("simple_shader.fs");
    shader = load_simple_shader(vs, strlen(vs), fs, strlen(fs));
    dealloc(vs);
    dealloc(fs);
    
    camera_loc = glGetUniformLocation(shader, "camera");
    color_loc = glGetUniformLocation(shader, "color");
    vertex_loc = glGetAttribLocation(shader, "vertex");
    logd("Loading shader..\n");
  }
  simple_grid_shader _shader = {
    .shader = shader,
    .camera_loc = camera_loc,
    .color_loc = color_loc,
    .vertex_loc = vertex_loc
  };
  return _shader;
}

vec2 graphics_context_pixel_to_screen(const graphics_context ctx, vec2 pixel_coords){
  ASSERT(fabs(ctx.render_size.x) >=0.001);
  vec2 v = vec2_sub(vec2_scale(vec2_div(pixel_coords, ctx.render_size), 2.0), vec2_one);
  v.y = -v.y;

  
  return v;
}

void simple_grid_render_gl(const graphics_context ctx, u32 polygon_id, mat4 camera, bool draw_points){
  loaded_polygon_data loaded;
  ASSERT(polygon_id != 0);
  polygon_data * pd = index_table_lookup(ctx.polygon, polygon_id);
  if(false == loaded_polygon_try_get(ctx.gpu_poly, polygon_id, &loaded)) {
    u32 count = pd->vertexes.count;
    if(count == 0)
      return;
    vec2 positions[count];
    

    vertex_data * vd = index_table_lookup_sequence(ctx.vertex, pd->vertexes);
    for(u32 i = 0; i < count; i++)
      positions[i] = vd[i].position;
    
    ASSERT(count > 0);
    loaded.gl_ref = simple_grid_polygon_load(positions, count);
    ASSERT(loaded.gl_ref > 0);
    loaded.count = count;
    loaded_polygon_set(ctx.gpu_poly, polygon_id, loaded);
  }

  glBindBuffer(GL_ARRAY_BUFFER, loaded.gl_ref);  
  simple_grid_shader shader = simple_grid_shader_get();
  glFrontFace(GL_CW);
  glCullFace(GL_BACK);
  glEnable(GL_CULL_FACE);
  vec4 color = vec4_zero;
  polygon_color_try_get(ctx.poly_color, pd->material, &color);
  glUseProgram(shader.shader);
  glUniformMatrix4fv(shader.camera_loc,1,false, &(camera.data[0][0]));
  glUniform4f(shader.color_loc, color.x, color.y, color.z, color.w);
  glEnableVertexAttribArray(shader.vertex_loc);
  glVertexAttribPointer(shader.vertex_loc, 2, GL_FLOAT, false, 0, 0);
  
  glDrawArrays(GL_TRIANGLE_STRIP, 0, loaded.count);
  if(draw_points){
    glPointSize(3);
    glDrawArrays(GL_POINTS, 0, loaded.count);
  }
  glDisableVertexAttribArray(shader.vertex_loc);
  glDisable(GL_CULL_FACE);
}


void simple_grid_mouse_over_func(u64 grid_id, double x, double y, u64 method){
  graphics_context ctx = get_graphics_context(grid_id);
  ctx.pointer_position = vec2_new(x, y);
  set_graphics_context(grid_id, ctx);
  if(method != 0){
    auto on_mouse_over = get_method(grid_id, method);
    on_mouse_over(grid_id, x, y, 0);
    return;
  }
}

typedef enum{
  SELECTED_ENTITY,
  SELECTED_MODEL,
  SELECTED_POLYGON,
  SELECTED_VERTEX,
  SELECTED_NONE
}selected_kind;

typedef struct{
  selected_kind selection_kind;
  u32 selected_index;
  float zoom;
  u32 focused_item;
  selected_kind focused_item_kind;
}editor_context;


CREATE_TABLE_DECL2(simple_graphics_editor_context, u64, editor_context);
CREATE_TABLE2(simple_graphics_editor_context, u64, editor_context);


void simple_grid_mouse_down_func(u64 grid_id, double x, double y, u64 method){
  UNUSED(method);
  if(mouse_button_action != 1) return;
  graphics_context ctx = get_graphics_context(grid_id);
  editor_context editor = get_simple_graphics_editor_context(grid_id);
  vec2 p = graphics_context_pixel_to_screen(ctx, vec2_new(x, y));
  p = vec2_scale(p, 1.0 / editor.zoom);
  if(editor.selection_kind == SELECTED_VERTEX){
    vertex_data * vd = index_table_lookup(ctx.vertex, editor.selected_index);
    vd->position = p;
    u32 polygon = vertex_get_polygon(ctx, editor.selected_index);
    
    loaded_polygon_data loaded;
    if(loaded_polygon_try_get(ctx.gpu_poly, polygon, &loaded)){
      u32 idx = index_table_alloc(ctx.polygons_to_delete);
      u32 * ptr = index_table_lookup(ctx.polygons_to_delete, idx);
      ptr[0] = polygon;
    }
    
  }else if(editor.selection_kind == SELECTED_POLYGON){
    polygon_add_vertex2f(&ctx, editor.selected_index, p);
    loaded_polygon_data loaded;
    if(loaded_polygon_try_get(ctx.gpu_poly, editor.selected_index, &loaded)){
      u32 idx = index_table_alloc(ctx.polygons_to_delete);
      u32 * ptr = index_table_lookup(ctx.polygons_to_delete, idx);
      ptr[0] = editor.selected_index;
    }
  }
}

void simple_graphics_editor_render(u64 id){
  u64 index = 0;
  control_pair * child_control = NULL;
  while((child_control = get_control_pair_parent(id, &index))){
    if(child_control == NULL)
      break;
    render_sub(child_control->child_id);
  }
  
  graphics_context gd = get_graphics_context(id);
  {
    u64 cnt = 0;
    u32 * items = index_table_all(gd.polygons_to_delete, &cnt);
    if(cnt > 0){
      loaded_polygon_data loaded;
      for(u64 i = 0; i < cnt; i++){
	if(loaded_polygon_try_get(gd.gpu_poly, items[i], &loaded)){
	  u32 ref = loaded.gl_ref;
	  glDeleteBuffers(1, &ref);
	  loaded_polygon_unset(gd.gpu_poly, items[i]);
	}
      }
      index_table_clear(gd.polygons_to_delete);
    }
  }
  
  {
    vec2 offset = graphics_context_pixel_to_screen(gd, gd.pointer_position);
    UNUSED(offset);
    simple_grid_render_gl(gd, gd.pointer_index, mat4_translate(offset.x, offset.y, 0), true); 
  }
  
  u32 entities[10];
  bool unused[10];
  u64 idx = 0;
  u64 cnt = 0;
  mat4 editor_tform = mat4_identity();
  editor_context ed = get_simple_graphics_editor_context(id);
  if(ed.focused_item_kind == SELECTED_ENTITY){
    if(ed.focused_item != 0){
      entity_data * ed2 = index_table_lookup(gd.entities, ed.focused_item);
      if(ed2 != NULL){
	vec3 p = ed2->position;;
	editor_tform = mat4_translate(-p.x, -p.y, -p.z);
      }
    }
  }else if(ed.focused_item_kind == SELECTED_MODEL){
    model_data * model = index_table_lookup(gd.models, ed.focused_item);
    for(u32 j = 0; j < model->polygons.count; j++){
      u32 index = j + model->polygons.index;
      simple_grid_render_gl(gd, index, mat4_identity(), true);
    }
    return;
  }

  editor_tform = mat4_mul(mat4_scaled(ed.zoom, ed.zoom, ed.zoom), editor_tform);
  
  while(0 != (cnt = active_entities_iter_all(gd.active_entities, entities,unused, array_count(unused), &idx))){
    for(u64 i = 0; i < cnt; i++){
      u32 entity = entities[i];
      entity_data * ed = index_table_lookup(gd.entities, entity);
      vec3 p = ed->position;
      mat4 tform = mat4_translate(p.x, p.y, p.z);
      tform = mat4_mul(editor_tform, tform);
      if(ed->model != 0){
	model_data * model = index_table_lookup(gd.models, ed->model);
	for(u32 j = 0; j < model->polygons.count; j++){
	  u32 index = j + model->polygons.index;
	  simple_grid_render_gl(gd, index, tform, true);
	}
      }
    }
  }

  u32 error = glGetError();
  if(error > 0){
    logd("GL ERROR: %i\n");
  }
}

bool is_whitespace(char c){
  return c == ' ' || c == '\t';
  
}

bool copy_nth(const char * _str, u32 index, char * buffer, u32 buffer_size){
  char * str = (char *) _str;
  while(*str != 0){
    while(is_whitespace(*str))
      str++;
    
    while(*str != 0 && is_whitespace(*str) == false){
      if(index == 0){
	*buffer = *str;
	buffer_size -= 1;
	buffer++;
      }
      str++;
    }
    if(index == 0){
      *buffer = 0;
      return true;
    }
    index--;
  }
  return false;

}

CREATE_TABLE_DECL2(simple_graphics_control, u64, u64);
CREATE_TABLE_NP(simple_graphics_control, u64, u64);

static void command_entered(u64 id, char * command){
  u64 control = get_simple_graphics_control(id);
  editor_context editor = get_simple_graphics_editor_context(control);
  graphics_context ctx = get_graphics_context(control);
  char first_part[100];
  bool first(const char * check){
    return strcmp(first_part, check) == 0;
  }

  char snd_part[100];
  bool snd(const char * check){
    return strcmp(snd_part, check) == 0;
  }
  
  if(copy_nth(command, 0, first_part, array_count(first_part))){
    if(first("exit")){
	set_should_exit(control, true);
    }else if(first("list")){
      logd("Listing: \n");
      u64 cnt = 0;
      entity_data * entities = index_table_all(ctx.entities, &cnt);
      logd("Entities: %i items.\n", cnt);
      for(u64 i = 0; i < cnt; i++){
	u32 entity_id = i + 1;

	entity_data * entity = entities + i;
	logd("Entity: %i\n", entity_id);
	u32 model_id = entity->model;
	if(model_id != 0){
	  logd("Model: %i\n", model_id);
	  model_data * md = index_table_lookup(ctx.models, model_id);
	  for(u32 j = 0; j < md->polygons.count; j++){
	    u32 polygon_id = j + md->polygons.index;
	    logd("Polygon: %i\n", polygon_id);
	    polygon_data * pd = index_table_lookup(ctx.polygon, polygon_id);
	    for(u32 i = 0; i < pd->vertexes.count; i++){
	      vertex_data * vd = index_table_lookup(ctx.vertex, pd->vertexes.index + i);
	      logd("Vertex %i: ", pd->vertexes.index + i);vec2_print(vd->position);logd("\n");
	    }
	  }
	}
      }
    }else if(first("create")){
      copy_nth(command, 1, snd_part, array_count(snd_part));
      logd("SND: %s\n", snd_part);
      if(snd("entity")){
	u32 i1 = index_table_alloc(ctx.entities);
	editor.selection_kind = SELECTED_ENTITY;
	editor.selected_index = i1;
	active_entities_set(ctx.active_entities, i1, true);

	logd("Created entity: %i\n", i1);
      }else if(snd("model") && editor.selection_kind == SELECTED_ENTITY){
	u32 i1 = index_table_alloc(ctx.models);
	entity_data * entity = index_table_lookup(ctx.entities, editor.selected_index);
	editor.selection_kind = SELECTED_MODEL;
	editor.selected_index = i1;
	entity->model = i1;
      }else if(snd("polygon") && editor.selection_kind == SELECTED_MODEL){

	model_data * entity = index_table_lookup(ctx.models, editor.selected_index);
	u32 pcnt = entity->polygons.count;

	index_table_resize_sequence(ctx.polygon, &(entity->polygons), pcnt + 1);
	editor.selection_kind = SELECTED_POLYGON;
	editor.selected_index = entity->polygons.index + entity->polygons.count - 1;
	//logd("creating polygon.. %i %i\n", pcnt, entity->polygons.count);
	//polygon_data * pd = index_table_lookup_sequence(ctx.polygon, entity->polygons);
      }else if(snd("vertex") && editor.selection_kind == SELECTED_POLYGON){
	
	vec2 p = vec2_zero;
	char buf[20];
	for(u32 i = 0 ; i < array_count(p.data); i++){
	  if(false == copy_nth(command, 2 + i, buf, array_count(buf)))
	    goto skip;
	  sscanf(buf,"%f", p.data + i);
	}
	logd("@ ");vec2_print(p);logd("\n");
	polygon_add_vertex2f(&ctx, editor.selected_index, p);
	u32 idx = index_table_alloc(ctx.polygons_to_delete);
	u32 * ptr = index_table_lookup(ctx.polygons_to_delete, idx);
	ptr[0] = editor.selected_index;

      skip:;
	
      }

      set_simple_graphics_editor_context(control, editor);
      logd("%i %i\n", editor.selection_kind, editor.selected_index);
    }else if(first("set")){
	copy_nth(command, 1, snd_part, array_count(snd_part));
	if(snd("color") && editor.selection_kind == SELECTED_POLYGON) {
	  vec4 p = vec4_zero;
	  char buf[20];
	  for(u32 i = 0 ; i < array_count(p.data); i++){
	    if(false == copy_nth(command, 2 + i, buf, array_count(buf)))
	      goto skip;
	    sscanf(buf,"%f", p.data + i);
	  }

	  polygon_data * pd = index_table_lookup(ctx.polygon, editor.selected_index);
	  if(pd->material == 0)
	    pd->material = get_unique_number();
	  logd("Set color: %i", pd->material);vec4_print(p);logd("\n");	  
	  polygon_color_set(ctx.poly_color, pd->material, p);
	}else if(snd("color") && editor.selection_kind == SELECTED_VERTEX) {
	  u32 polygon = vertex_get_polygon(ctx, editor.selected_index);
	  if(polygon == 0)
	    return;
	  vec4 p = vec4_zero;
	  char buf[20];
	  for(u32 i = 0 ; i < array_count(p.data); i++){
	    if(false == copy_nth(command, 2 + i, buf, array_count(buf)))
	      goto skip;
	    sscanf(buf,"%f", p.data + i);
	  }

	  polygon_data * pd = index_table_lookup(ctx.polygon, polygon);
	  if(pd->material == 0)
	    pd->material = get_unique_number();
	  logd("Set color: %i", pd->material);vec4_print(p);logd("\n");	  
	  polygon_color_set(ctx.poly_color, pd->material, p);
	  
	}else if(snd("position")&& editor.selected_index != 0  && editor.selection_kind == SELECTED_ENTITY ){
	  vec3 p = vec3_zero;
	  char buf[20];
	  for(u32 i = 0 ; i < array_count(p.data); i++){
	    if(false == copy_nth(command, 2 + i, buf, array_count(buf)))
	      break;
	    sscanf(buf,"%f", p.data + i);
	  }

	  entity_data * ed = index_table_lookup(ctx.entities, editor.selected_index);
	  if(ed != NULL){
	    ed->position = p;
	  }
	}

	if(snd("zoom")){

	  char buf[20];
	  if(copy_nth(command, 2, buf, array_count(buf))){
	    float v = 1.0;
	    sscanf(buf,"%f", &v);
	    editor.zoom = v;
	  }
	  logd("ZOOM: %f\n", editor.zoom);
	  set_simple_graphics_editor_context(control, editor);
	}
      }
    else if(first("select") || first("focus")){
      bool is_focus = first("focus");
      char id_buffer[100] = {0};
      if(copy_nth(command, 1, snd_part, array_count(snd_part))
	 && copy_nth(command, 2, id_buffer, array_count(id_buffer))){
	u32 i1 = 0;
	sscanf(id_buffer, "%i", &i1);
	index_table * table = NULL;
	selected_kind kind;
	if(snd("entity")){
	  table = ctx.entities;
	  kind = SELECTED_ENTITY;
	}else if(snd("model")){
	  table = ctx.models;
	  kind = SELECTED_MODEL;
	  
	}else if(snd("polygon")){
	  table = ctx.polygon;
	  kind = SELECTED_POLYGON;
	  
	}else if(snd("vertex")){
	  table = ctx.vertex;
	  kind = SELECTED_VERTEX;
	}
	if(table != NULL && index_table_contains(table, i1)){
	  if(is_focus){
	    editor.focused_item_kind = kind;
	    editor.focused_item = i1;
	  }else{
	    editor.selection_kind = kind;
	    editor.selected_index = i1;
	  }
	  
	}else{
	  editor.focused_item_kind = SELECTED_NONE;
	  editor.focused_item = 0;
	}
	
      }
      set_simple_graphics_editor_context(control, editor);
	
    }else if(first("remove")){
      char id_buffer[100] = {0};
      if(copy_nth(command, 1, snd_part, array_count(snd_part))){
	u32 i1 = 0;
	if(copy_nth(command, 2, id_buffer, array_count(id_buffer))){
	  sscanf(id_buffer, "%i", &i1);
	}else{
	  i1 = editor.selected_index;
	}
	logd("Remove: %s, %i\n", snd_part, i1);
	if(snd("vertex")){
	  
	  u32 polygon = vertex_get_polygon(ctx, i1);
	  if(polygon == 0)
	    return;
	  
	  polygon_data * pd = index_table_lookup(ctx.polygon, polygon);
	  vertex_data * vd = index_table_lookup_sequence(ctx.vertex, pd->vertexes);
	  u32 offset = i1 - pd->vertexes.index;
	  for(u32 i = offset; i < pd->vertexes.count - 1; i++)
	    vd[i] = vd[i + 1];
	  index_table_resize_sequence(ctx.vertex, &(pd->vertexes), pd->vertexes.count - 1);
	  u32 idx = index_table_alloc(ctx.polygons_to_delete);
	  u32 * ptr = index_table_lookup(ctx.polygons_to_delete, idx);
	  ptr[0] = polygon;
	    
	}
      }
      set_simple_graphics_editor_context(control, editor);

    }else if(first("optimize")){
      index_table_optimize(ctx.polygon);
      index_table_optimize(ctx.vertex);
      index_table_optimize(ctx.entities);
      index_table_optimize(ctx.models);
      logd("Optimized!");
    }else{
      logd("Unkown command!\n");
    }
  }
  //logd("COMMAND ENTERED %i %s\n", id, command);
}

CREATE_TABLE_DECL2(console_history_index, u64, u32);
CREATE_TABLE2(console_history_index, u64, u32);

CREATE_TABLE_DECL2(alternative_control, u64, u64);
CREATE_TABLE2(alternative_control, u64, u64);

static void game_handle_key(u64 console, int key, int mods, int action){
  if(key == key_tab && action == key_press){

    u64 parent_id = control_pair_get_parent(console);
    control_pair * p = gui_get_control(parent_id, console);
    p->child_id = get_alternative_control(console);
    u64 keybuf[10];
    u64 valuebuf[10];
    u64 idx = 0;
    u64 cnt = 0;
    while(0 != (cnt = iter_all_simple_graphics_control(keybuf, valuebuf, array_count(keybuf), &idx))){
      for(u32 i = 0; i < cnt; i++){
	if(valuebuf[i] == p->child_id){
	  set_focused_element(parent_id, keybuf[i]);
	  return;
	}

      }
      
    }
    
  }
  UNUSED(mods);
}

static void console_handle_key(u64 console, int key, int mods, int action){
  if(action == key_release)
    return;
  
  if(key == key_tab && action == key_press){
    u64 control = get_simple_graphics_control(console);
    u64 parent_id = control_pair_get_parent(control);
    control_pair * p = add_control(parent_id, control);
    p->child_id = get_alternative_control(control);
    set_focused_element(parent_id, p->child_id);
    return;
  }else if(key == key_backspace && mods == mod_ctrl){
    logd("MODS: %i\n", mods);
    u64 control = get_simple_graphics_control(console);
    graphics_context ctx = get_graphics_context(control);
    editor_context editor = get_simple_graphics_editor_context(control);
    if(editor.selection_kind == SELECTED_POLYGON){
      polygon_data * pd = index_table_lookup(ctx.polygon, editor.selected_index);
      u32 pcnt = pd->vertexes.count;
      if(pcnt > 0)
	index_table_resize_sequence(ctx.vertex, &(pd->vertexes), pcnt - 1);
      logd("COUNT: %i\n", pcnt);
      loaded_polygon_data loaded;
      if(loaded_polygon_try_get(ctx.gpu_poly, editor.selected_index, &loaded)){
	u32 idx = index_table_alloc(ctx.polygons_to_delete);
	u32 * ptr = index_table_lookup(ctx.polygons_to_delete, idx);
	ptr[0] = editor.selected_index;
      }
    }
    return;
  }
  
  //logd("%i %i %i\n", console, key, mods);
  u64 histcnt = get_console_history_cnt(console);
  u64 history[histcnt + 1];
  u64 history_cnt = get_console_history(console, history, array_count(history));
  
  u32 idx = get_console_history_index(console);
  if(key == key_up){
    idx += 1;
  }else if(key == key_down){
    if(idx != 0)
      idx -= 1;
  }else{
    unset_console_history_index(console);
    goto skip;
  }

  if(idx > history_cnt){
    idx = 0;
  }
  if(idx == 0)
    unset_console_history_index(console);
  else
    set_console_history_index(console, idx);
  if(idx > 0){
    char buffer[200] = {0};
    if(console_get_history(console, idx - 1, buffer, array_count(buffer))){
      //logd("SETTING TEXT TO '%s'\n", buffer);
      remove_text(console);
      set_text(console, buffer);
    }
  }
  //logd("HIST IDX: %i\n", idx);
  
 skip:;
  CALL_BASE_METHOD(console, key_handler_method, key, mods, action);
}


void simple_grid_measure(u64 id, vec2 * size){
  UNUSED(id);
  *size = shared_size;
  graphics_context gd = get_graphics_context(id);
  gd.render_size = *size;
  set_graphics_context(id, gd);  
}

typedef struct {
  u32 selected_entity;
  vec2 offset;
  bool mouse_state;
  vec2 last_mouse_position;
}game_data;

CREATE_TABLE_DECL2(entity_target, u32, vec2);
CREATE_TABLE2(entity_target, u32, vec2);
CREATE_TABLE_DECL2(simple_game_data, u64, game_data);
CREATE_TABLE2(simple_game_data, u64, game_data);
CREATE_TABLE_DECL2(game_window, u64, u64);
CREATE_TABLE2(game_window, u64, u64);

void detect_collisions(u32 * entities, u32 entitycnt, graphics_context gd, index_table * result_table){
  for(u64 i = 0; i < entitycnt; i++){
    u64 entity = entities[i];
    entity_data * ed = index_table_lookup(gd.entities, entity);
    if(ed == NULL)
      continue;
    if(ed->model == 0) continue;      
    model_data * md = index_table_lookup(gd.models, ed->model);
    u64 pdcnt = md->polygons.count;
    polygon_data * pd = index_table_lookup_sequence(gd.polygon, md->polygons);
    
    if(pd == NULL) continue;
    vec3 position = ed->position;
    
    for(u64 j = i + 1; j < entitycnt; j++){
      u32 entity2 = entities[j];
      entity_data * ed2 = index_table_lookup(gd.entities, entity2);
      if(ed2 == NULL)
	continue;
      if(ed == ed2) continue;
      vec3 position2 = ed2->position;
      model_data * md2 = index_table_lookup(gd.models, ed2->model);
      u64 pd2cnt = md2->polygons.count;
      polygon_data * pd2 = index_table_lookup_sequence(gd.polygon, md2->polygons);
      if(pd2 == NULL) continue;
      for(u32 i = 0; i < pdcnt; i++){
	for(u32 j = 0; j < pd2cnt; j++){
	  if(pd + i == pd2 + j) continue;
	  u64 vcnt1 = pd[i].vertexes.count;
	  u64 vcnt2 = pd2[j].vertexes.count;
	  vertex_data * vd1 = index_table_lookup_sequence(gd.vertex, pd[i].vertexes);
	  vertex_data * vd2 = index_table_lookup_sequence(gd.vertex, pd2[j].vertexes);
	  if(vd1 == NULL || vd2 == NULL) continue;
	  bool cw1 = true;
	  vec3 offset = vec3_sub(position, position2);
	  for(u64 k1 = 2; k1 < vcnt1; k1++){
	    vec2 verts1[3];
	    if(cw1){
	      verts1[0] = vd1[k1 - 2].position;
	      verts1[1] = vd1[k1 - 1].position;
	      verts1[2] = vd1[k1].position;
	      cw1 = false;
	    }else{
	      verts1[0] = vd1[k1].position;
	      verts1[1] = vd1[k1 - 1].position;
	      verts1[2] = vd1[k1 - 2].position;
	      cw1 = true;
	    }
		
	    for(u32 kk = 0; kk < 3; kk++)
	      verts1[kk] = vec2_add(verts1[kk], offset.xy);
		
	    bool cw2 = true;
	    for(u64 k2 = 2; k2 < vcnt2; k2++){
	      vec2 verts2[3];
	      if(cw2){
		verts2[0] = vd2[k2 - 2].position;
		verts2[1] = vd2[k2 - 1].position;
		verts2[2] = vd2[k2].position;
		cw2 = false;
	      }else{
		verts2[0] = vd2[k2].position;
		verts2[1] = vd2[k2 - 1].position;
		verts2[2] = vd2[k2 - 2].position;
		cw2 = true;
	      }
	      // verts1 and verts2 are two triangles.
	      int cols = 0;
	      for(u32 ii = 0; ii < 3; ii++){
		u32 ii2 = ii == 2 ? 0 : ii + 1;
		u32 ii3 = ii == 0 ? 2 : ii - 1;
		vec2 a = verts1[ii];
		vec2 b = verts1[ii2];
		vec2 d = vec2_normalize(vec2_sub(b, a));
		d = vec2_new(d.y, -d.x);
		vec2 e = verts1[ii3];
		float xe = vec2_mul_inner(d, vec2_sub(e, a));
		bool gotcol = false;
		for(u32 jj = 0; jj < 3; jj++){
		  u32 jj2 = jj == 2 ? 0 : jj + 1;
		  vec2 a2 = vec2_sub(verts2[jj], a);
		  vec2 b2 = vec2_sub(verts2[jj2], a);
		  float x = vec2_mul_inner(d, a2);
		  float y = vec2_mul_inner(d, b2);
		  if(x > y)
		    SWAP(x, y);
		  if(x < xe && y > 0)
		    gotcol = true;
		}
		if(gotcol)
		  cols += 1;
	      }

	      for(u32 ii = 0; ii < 3; ii++){
		u32 ii2 = ii == 2 ? 0 : ii + 1;
		u32 ii3 = ii == 0 ? 2 : ii - 1;
		vec2 a = verts2[ii];
		vec2 b = verts2[ii2];
		vec2 d = vec2_normalize(vec2_sub(b, a));
		d = vec2_new(d.y, -d.x);
		vec2 e = verts2[ii3];
		float xe = vec2_mul_inner(d, vec2_sub(e, a));
		bool gotcol = false;
		for(u32 jj = 0; jj < 3; jj++){
		  u32 jj2 = jj == 2 ? 0 : jj + 1;
		  vec2 a2 = vec2_sub(verts1[jj], a);
		  vec2 b2 = vec2_sub(verts1[jj2], a);
		  float x = vec2_mul_inner(d, a2);
		  float y = vec2_mul_inner(d, b2);
		  if(x > y)
		    SWAP(x, y);
		  if(x < xe && y > 0){
		    gotcol = true;
		  }
		}
		if(gotcol)
		  cols += 1;
	      }
		  
	      if(cols == 6){
		collision_data cd;
		cd.entity1.entity = entity;
		cd.entity1.model = ed->model;
		cd.entity1.polygon_offset = md->polygons.index + i;
		cd.entity1.vertex_offset = k1;
		cd.entity2.entity = entity2;
		cd.entity2.model = ed2->model;
		cd.entity2.polygon_offset = md2->polygons.index + i;
		cd.entity2.vertex_offset = k2;
		u32 idx = index_table_alloc(result_table);
		*((collision_data *)index_table_lookup(result_table, idx)) = cd;
	      }
	    }
	  }
	}
      }
    }
  }
}
  



void simple_grid_render(u64 id){
  game_data _gd = get_simple_game_data(id);
  vec2 offset = _gd.offset;
  graphics_context gd = get_graphics_context(get_alternative_control(id));
  {
    u64 cnt = 0;
    u32 * items = index_table_all(gd.polygons_to_delete, &cnt);
    if(cnt > 0){
      loaded_polygon_data loaded;
      for(u64 i = 0; i < cnt; i++){
	if(loaded_polygon_try_get(gd.gpu_poly, items[i], &loaded)){
	  u32 ref = loaded.gl_ref;
	  glDeleteBuffers(1, &ref);
	  loaded_polygon_unset(gd.gpu_poly, items[i]);
	}
      }
      index_table_clear(gd.polygons_to_delete);
    }
  }
  u64 idx = 0;

  u64 count = active_entities_count(gd.active_entities);
  u32 entities[count];
  vec2 prevp[count];
  bool moved[count];
  bool unused[count];
  idx = 0;
  
  active_entities_iter_all(gd.active_entities, entities, unused, count, &idx);
  for(u32 i = 0; i < count ;i ++){
    moved[i] = false;
    u32 entity = entities[i];
    vec2 target;
    if(try_get_entity_target(entity, &target)){
      entity_data * ed = index_table_lookup(gd.entities, entity);
      vec2 d = vec2_sub(target, ed->position.xy);
      float l = vec2_len(d);
      if(l <= 0.01){
	  unset_entity_target(entity);
      }else{
	vec2 p2 = vec2_add(ed->position.xy, vec2_scale(d, 0.01 / l));
	prevp[i] = ed->position.xy;
	ed->position.x = p2.x;
	ed->position.y = p2.y;
	moved[i] = true;
      }
    }
  }

  {
    
    index_table_clear(gd.collision_table);
    detect_collisions(entities, count, gd, gd.collision_table);
    u64 collisions = 0;
    collision_data * cd = index_table_all(gd.collision_table, &collisions);
    u32 e1[collisions];
    u32 e2[collisions];
    for(u32 i = 0; i < collisions; i++){
      e1[i] = cd[i].entity1.entity;
      e2[i] = cd[i].entity2.entity;
    }
    int keycmp32(const u32 * k1,const  u32 * k2){
      if(*k1 > *k2)
	return 1;
      else if(*k1 == *k2)
	return 0;
      else return -1;
    }

    qsort(e1, collisions, sizeof(*e1), (void *)keycmp32);
    qsort(e2, collisions, sizeof(*e2), (void *)keycmp32);
    for(u32 i = 0; i < count; i++){
      if(moved[i] == false) continue;
      if(bsearch(entities + i, e1, collisions, sizeof(entities[i]), (void *) keycmp32)
	 ||bsearch(entities + i, e2, collisions, sizeof(entities[i]), (void *) keycmp32)){
	entity_data * ed = index_table_lookup(gd.entities, entities[i]);
	ed->position.xy = prevp[i];
      }

    }
    u64 count = 0;
    index_table_all(gd.collision_table, &count);
  }
  idx = 0;
  u64 cnt = 0;
  
  while(0 != (cnt = active_entities_iter_all(gd.active_entities, entities,unused, array_count(unused), &idx))){
    for(u64 i = 0; i < cnt; i++){
      u32 entity = entities[i];
      
      entity_data * ed = index_table_lookup(gd.entities, entity);
      
      vec3 p = ed->position;
      mat4 tform = mat4_translate(p.x - offset.x, p.y - offset.y, p.z);
      
      if(ed->model != 0){
	model_data * model = index_table_lookup(gd.models, ed->model);

	for(u32 j = 0; j < model->polygons.count; j++){
	  u32 index = j + model->polygons.index;	
	  simple_grid_render_gl(gd, index, tform, false);
	}
      }
    }
  }

  u32 error = glGetError();
  if(error > 0){
    logd("GL ERROR: %i\n");
  }  
}

void simple_grid_game_mouse_over_func(u64 grid_id, double x, double y, u64 method){
  u64 ctl = get_alternative_control(grid_id);
  graphics_context ctx = get_graphics_context(ctl);
  
  if(method == 0){
    {
      vec2 p = graphics_context_pixel_to_screen(ctx, vec2_new(x, y));
      game_data gd = get_simple_game_data(grid_id);
      if(gd.mouse_state){
	vec2 lastp = ctx.pointer_position;
	vec2 d = vec2_sub(p, graphics_context_pixel_to_screen(ctx, lastp));
	gd.offset = vec2_add(d, gd.offset);
      }
      
      gd.last_mouse_position = p;
      set_simple_game_data(grid_id, gd);
      ctx.pointer_position = vec2_new(x, y);
      set_graphics_context(ctl, ctx);
      return;
    }
  }
  ctx.pointer_position = vec2_new(x, y);
  set_graphics_context(ctl, ctx);
  if(method != 0){
    auto on_mouse_over = get_method(grid_id, method);
    on_mouse_over(grid_id, x, y, 0);
    return;
  }
}

void simple_game_point_collision(graphics_context ctx, vec2 loc, index_table * collisiontable){
  ASSERT(collisiontable->element_size == sizeof(entity_local_data));
  u32 entities[10];
  bool unused[10];
  u64 idx = 0;
  u64 cnt = 0;
  vec2 p = loc;
  while(0 != (cnt = active_entities_iter_all(ctx.active_entities, entities,unused, array_count(unused), &idx))){
    for(u64 i = 0; i < cnt; i++){
      entity_data * ed = index_table_lookup(ctx.entities, entities[i]);
      vec3 position = ed->position;
      UNUSED(position);
      if(ed == NULL)
	continue;
      vec2 p2 = vec2_sub(p, position.xy);
      model_data * md = index_table_lookup(ctx.models, ed->model);
      if(md == NULL)
	continue;
      u64 pdcnt = md->polygons.count;
      polygon_data * pd = index_table_lookup_sequence(ctx.polygon, md->polygons);
      if(pd == NULL) continue;
      for(u64 j = 0; j < pdcnt; j++){
	u64 vcnt = pd[j].vertexes.count;
	vertex_data * vd = index_table_lookup_sequence(ctx.vertex, pd[j].vertexes);
	if(vd == NULL) continue;
	bool cw = true;
	for(u64 k = 2; k < vcnt; k++){
	  vec2 verts[3];
	  if(cw){
	    verts[0] = vd[k-2].position;
	    verts[1] = vd[k-1].position;
	    verts[2] = vd[k].position;
	    cw = false;
	  }else{
	    verts[0] = vd[k].position;
	    verts[1] = vd[k-1].position;
	    verts[2] = vd[k - 2].position;
	    cw = true;
	  }
	  vec2 d0 = vec2_sub(verts[1], verts[0]);
	  vec2 pn = vec2_sub(p2, verts[0]);
	  vec2 n2 = vec2_new(pn.y, -pn.x);
	  float dp0 = vec2_mul_inner(n2, d0);
	  
	  d0 = vec2_sub(verts[2], verts[1]);
	  pn = vec2_sub(p2, verts[1]);
	  n2 = vec2_new(pn.y, -pn.x);
	  float dp1 = vec2_mul_inner(n2, d0);
	  
	  d0 = vec2_sub(verts[0], verts[2]);
	  pn = vec2_sub(p2, verts[2]);
	  n2 = vec2_new(pn.y, -pn.x);
	  float dp2 = vec2_mul_inner(n2, d0);
	  if(dp0 < 0 && dp1 < 0 && dp2 < 0){
	    u32 idx = index_table_alloc(collisiontable);
	    entity_local_data * d =index_table_lookup(collisiontable, idx);
	    d->entity = entities[i];
	    d->model = ed->model;
	    d->polygon_offset = j;
	    d->vertex_offset = k;
	  }
	}
      }
    }
  }
}

void simple_grid_game_mouse_down_func(u64 grid_id, double x, double y, u64 method){
  UNUSED(method);
  UNUSED(grid_id);
  u64 ctl = get_alternative_control(grid_id);
  //if(mouse_button_action != 1) return;
  graphics_context ctx = get_graphics_context(ctl);
  game_data gd = get_simple_game_data(grid_id);
  vec2 p = graphics_context_pixel_to_screen(ctx, vec2_new(x, y));
  if(mouse_button_button == mouse_button_right && mouse_button_action != mouse_button_repeat){
    if(gd.mouse_state){
      vec2 lastp = gd.last_mouse_position;
      vec2 d = vec2_sub(p, lastp);
      gd.offset = vec2_add(d, gd.offset);
    }
    gd.last_mouse_position = p;
    
    gd.mouse_state = mouse_button_action == mouse_button_press;
    u64 win = get_game_window(grid_id);
    if(gd.mouse_state){
      gui_acquire_mouse_capture(win, grid_id);
      gui_set_cursor_mode(win, gui_window_cursor_disabled);
    }else{
      gui_release_mouse_capture(win, grid_id);
      gui_set_cursor_mode(win, gui_window_cursor_normal);
    }
    set_simple_game_data(grid_id, gd);
    return;
  }
  
  else if(mouse_button_button == mouse_button_left && mouse_button_action == mouse_button_press)
  {
    static index_table * tab = NULL;
    if(tab == NULL) tab = index_table_create(NULL, sizeof(entity_local_data));
    index_table_clear(tab);
    simple_game_point_collision(ctx, vec2_add(p, gd.offset), tab);
    u64 cnt = 0;
    entity_local_data * all = index_table_all(tab, &cnt);
    if(cnt > 0){

      gd.selected_entity = all->entity;
      logd("HIT:%i %i %i %i\n", all->entity, all->model, all->polygon_offset, all->vertex_offset);
      set_simple_game_data(grid_id, gd);
    }else{
      // Assumed we did not hit something.
      if(gd.selected_entity != 0)
	set_entity_target(gd.selected_entity, vec2_add(p, gd.offset));
    }
  }
}

void simple_grid_game_measure(u64 id, vec2 * size){
  UNUSED(id);
  *size = shared_size;
  u64 ctl = get_alternative_control(id);
  graphics_context gd = get_graphics_context(ctl);
  gd.render_size = *size;
  set_graphics_context(ctl, gd);  
}

#include "console.h"
void simple_graphics_editor_load(u64 id, u64 win_id){
  define_method(id, render_control_method, (method)simple_graphics_editor_render);
  define_method(id, measure_control_method, (method)simple_grid_measure);
  define_method(id, mouse_over_method, (method)simple_grid_mouse_over_func);
  define_method(id, mouse_down_method, (method) simple_grid_mouse_down_func);

  graphics_context gd;
  graphics_context_load(&gd);
  
  set_graphics_context(id, gd);
  
  u64 console = intern_string("ccconsole!");
  
  set_simple_graphics_control(console, id);
  
  set_console_height(console, 300);
  create_console(console);
  add_control(id, console);
  set_margin(console, (thickness){5,5,5,5});
  set_vertical_alignment(console, VALIGN_BOTTOM);
  set_console_history_cnt(console, 100);
  define_method(console, console_command_entered_method, (method)command_entered);
  define_method(console, key_handler_method, (method) console_handle_key);

  u64 game = intern_string("ggame!");
  define_method(game, render_control_method, (method)simple_grid_render);
  define_method(game, measure_control_method, (method)simple_grid_game_measure);
  define_method(game, key_handler_method, (method) game_handle_key);
  define_method(game, mouse_down_method, (method) simple_grid_game_mouse_down_func);
  define_method(game, mouse_over_method, (method)simple_grid_game_mouse_over_func);
  set_alternative_control(id, game);
  set_alternative_control(game, id);
  set_game_window(game, win_id);
  
  if(gui_get_control(win_id, id) == NULL  && gui_get_control(win_id, game) == NULL){
    add_control(win_id, id);
    set_focused_element(win_id, console);
  }

  
  if(false && once(console)){
    command_entered(console, (char *)"create entity");
    command_entered(console, (char *)"create model");
    command_entered(console, (char *)"create polygon");
    command_entered(console, (char *)"create vertex 0.1 0.1");
    command_entered(console, (char *)"create vertex 0.1 0.3");
    command_entered(console, (char *)"create vertex 0.3 0.1");
    command_entered(console, (char *)"create vertex 0.3 0.3");
    command_entered(console, (char *)"set color 0.3 0.3 0.9 1.0");
    
    command_entered(console, (char *)"create entity");
    command_entered(console, (char *)"create model");
    command_entered(console, (char *)"create polygon");
    command_entered(console, (char *)"create vertex 0.1 0.1");
    command_entered(console, (char *)"create vertex 0.1 -0.3");
    command_entered(console, (char *)"create vertex -0.3 0.1");
    command_entered(console, (char *)"create vertex -0.3 -0.3");
    command_entered(console, (char *)"set color 0.5 0.8 0.3 1.0");

    command_entered(console, (char *)"create entity");
    command_entered(console, (char *)"create model");
    command_entered(console, (char *)"create polygon");
    command_entered(console, (char *)"create vertex 0.1 0.6");
    command_entered(console, (char *)"create vertex 0.1 0.3");
    command_entered(console, (char *)"create vertex -0.3 0.6");
    command_entered(console, (char *)"create vertex -0.3 0.3");
    command_entered(console, (char *)"set color 0.7 0.7 0.3 1.0");
    
  }
  
}


bool simple_graphics_editor_test(){
  
  u64 item = get_unique_number();
  u64 win_id = intern_string("board");
  graphics_context gd;
  graphics_context_load(&gd);
  set_graphics_context(item, gd);
  
  simple_graphics_editor_load(item, win_id);
  u64 console_id = 0;
  {
    u64 keys[100];
    u64 values[100];
    u64 idx = 0;
    iter_all_simple_graphics_control(keys, values, array_count(keys), &idx);
    for(u64 i = 0; i < array_count(keys); i++){
      if(values[i] == item){
	console_id = keys[i];
	break;
      }
    }
  }
  TEST_ASSERT(console_id != 0);
				     
  command_entered(console_id, (char *)"create entity");
  command_entered(console_id, (char *)"create model");
  command_entered(console_id, (char *)"create polygon");
  command_entered(console_id, (char *)"create vertex 0.1 0.1");
  command_entered(console_id, (char *)"create vertex 0.1 1");
  command_entered(console_id, (char *)"create vertex 1 0.1");
  command_entered(console_id, (char *)"create vertex 1 1");
  return TEST_SUCCESS;
}


