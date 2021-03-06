
typedef struct{
  u64 id;
}class;

typedef void (* method)(u64 control, ...);

void define_subclass(u64 class, u64 base_class);
u64 get_baseclass(u64 class, u64 * index);
void define_method(u64 class_id, u64 method_id, method handler);
method get_method(u64 class_id, u64 method_id);

class * new_class(u64 id);
method get_command_handler(u64 control_id, u64 command_id);

method * get_command_handler2(u64 control_id, u64 command_id, bool create);
void attach_handler(u64 control_id, u64 command_id, void * handler);

void * find_item(const char * table, u64 itemid, u64 size, bool create);

#define CALL_BASE_METHOD(Item, Method, ...)\
  ({\
    u64 index = 0, base = 0;\
    while(0 != (base = get_baseclass(Item, &index))){	\
      method m = get_method(base, Method);		\
      if(m != NULL) m(Item, __VA_ARGS__);		\
    }							\
  })

typedef struct{
  mem_area * mem;
  size_t value_size;
  size_t key_size;
  bool (* try_get)(u64 * key, void * value);
  void (* remove)(u64 * key);
  void (* add)(u64 * key, void * value);
  void (* reinit)();
  int (* compare)(void * key, void * value);
  bool is_multi_table;
  const char * name;

}u64_table_info;

#define CREATE_TABLE_DECL(Name, KeyType, ValueType)		\
  void set_ ## Name(KeyType key, ValueType value);		\
  ValueType get_ ## Name (KeyType key);				\
  bool try_get_ ## Name (KeyType key, ValueType * value);	\
  u64 iter_all_ ## Name (KeyType * key, ValueType * value, size_t cnt, u64 * idx);

#define CREATE_TABLE(Name, KeyType, ValueType)			\
  CREATE_TABLE_DECL(Name, KeyType, ValueType)			\
  void KeyType ##_table_initialized(KeyType ## _table_info);	\
  void Name ## _set (KeyType * k, ValueType * v){		\
    set_ ## Name(*k, *v);					\
  }								\
  persisted_mem_area * Name ## Initialize(){			\
    static persisted_mem_area * mem = NULL;			\
    if(mem == NULL){						\
      mem = create_mem_area(#Name);				\
      								\
      KeyType ## _table_initialized((KeyType ## _table_info){		\
	  .add = (void *) &Name ## _set, .value_size = sizeof(KeyType), \
	    .key_size = sizeof(KeyType), .mem = mem, .name = #Name			\
	    });								\
    }									\
    									\
    return mem;								\
  }									\
  									\
  void set_ ## Name(KeyType key, ValueType value){			\
    									\
    									\
    persisted_mem_area * mem = Name ## Initialize();			\
    									\
    									\
    struct {								\
      KeyType key;							\
      ValueType value;							\
    } __attribute__((packed)) * data = mem->ptr;			\
    size_t item_size = sizeof(*data);					\
    u64 cnt = mem->size / item_size;					\
    for(size_t i = 0; i < cnt; i++){					\
      if(data[i].key == key){						\
	data[i].value = value;						\
	return;								\
      }									\
    }									\
    mem_area_realloc(mem, mem->size + item_size);			\
    data = mem->ptr;							\
    data[cnt].key = key;						\
    data[cnt].value = value;						\
  }									\
  									\
  ValueType get_ ## Name (KeyType key){					\
    size_t item_size = sizeof(key) + sizeof(ValueType);			\
    persisted_mem_area * mem = Name ## Initialize();			\
    									\
    u64 cnt = mem->size / item_size;					\
    struct {								\
      KeyType key;							\
      ValueType value;							\
    } __attribute__((packed)) * data = mem->ptr;			\
    									\
    for(size_t i = 0; i < cnt; i++){					\
      if(data[i].key == key)						\
	return data[i].value;						\
      									\
    }									\
    									\
    struct {ValueType _default;} _default = {};				\
    return _default._default;						\
    									\
  }									\
  u64 iter_all_ ## Name (KeyType * key, ValueType * value, size_t _cnt, u64 * idx){ \
    size_t item_size = sizeof(KeyType) + sizeof(ValueType);		\
    persisted_mem_area * mem = Name ## Initialize();			\
									\
    u64 cnt = mem->size / item_size;					\
    struct {								\
      KeyType key;							\
      ValueType value;							\
    } __attribute__((packed)) * data = mem->ptr;			\
    u64 i;								\
    for(i = 0; i < _cnt && *idx < cnt; (*idx)++){			\
      if(data[*idx].key == 0) continue;					\
      key[i] = data[*idx].key;						\
      value[i++] = data[*idx].value;					\
    }									\
    return i;								\
  }									\
  bool try_get_ ## Name (KeyType key, ValueType * value){		\
    size_t item_size = sizeof(key) + sizeof(ValueType);			\
    persisted_mem_area * mem = Name ## Initialize();			\
									\
    u64 cnt = mem->size / item_size;					\
    struct {								\
      KeyType key;							\
      ValueType value;							\
    } __attribute__((packed)) * data = mem->ptr;			\
    									\
    for(size_t i = 0; i < cnt; i++){					\
      if(data[i].key == key)						\
	return *value = data[i].value, true;				\
      									\
    }									\
    return false;							\
    									\
  }									\
  									\
  void clear_ ## Name(){						\
    persisted_mem_area * mem = Name ## Initialize();			\
    mem_area_realloc(mem, 1);						\
  }									\
  void unset_## Name(KeyType key){					\
    size_t item_size = sizeof(key) + sizeof(ValueType);			\
    persisted_mem_area * mem = Name ## Initialize();			\
    									\
    u64 cnt = mem->size / item_size;					\
    struct {								\
      KeyType key;							\
      ValueType value;							\
    } __attribute__((packed)) * data = mem->ptr;			\
    									\
    for(size_t i = 0; i < cnt; i++){					\
      if(data[i].key == key){						\
	data[i].key = 0;break;						\
      }									\
      									\
    }									\
  }									

#define CREATE_MULTI_TABLE_DECL(Name, KeyType, ValueType)		\
  void add_ ## Name(KeyType key, ValueType value);			\
  size_t get_ ## Name (KeyType key, ValueType * values, size_t values_cnt); \
  size_t iter_ ## Name (KeyType key, ValueType * values, size_t values_cnt, size_t * i); \
  void clear_item_ ## Name(KeyType key);				\
  void clear_at_ ## Name(u64 idx);					\
  void clear_ ## Name();						
       
#define CREATE_MULTI_TABLE(Name, KeyType, ValueType)\
  void KeyType ##_table_initialized(KeyType ## _table_info);	\
  persisted_mem_area * Name ## Initialize(){   \
    static persisted_mem_area * mem = NULL;   \
    if(mem == NULL){			      \
      mem = create_mem_area(#Name);      \
      KeyType ## _table_initialized((KeyType ## _table_info){.name = #Name});	\
    }					      \
    return mem;				      \
  }					      \
							\
  void add_ ## Name(KeyType key, ValueType value){	\
    persisted_mem_area * mem = Name ## Initialize();		\
    struct {							\
      KeyType key;						\
      ValueType value;						\
    } * data = mem->ptr;					\
    size_t item_size = sizeof(*data);				\
    u64 cnt = mem->size / item_size;				\
    for(size_t i = 0; i < cnt; i++){				\
      if(data[i].key == 0){					\
	data[i].value = value;					\
	data[i].key = key;					\
	return;							\
      }								\
    }								\
    mem_area_realloc(mem, mem->size + item_size);		\
    data = mem->ptr;						\
    data[cnt].key = key;					\
    data[cnt].value = value;					\
  }								\
								\
  size_t get_ ## Name (KeyType key, ValueType * values, size_t values_cnt){	\
    size_t item_size = sizeof(key) + sizeof(ValueType);			\
    persisted_mem_area * mem = Name ## Initialize();			\
									\
    u64 cnt = mem->size / item_size;					\
    struct {								\
      KeyType key;							\
      ValueType value;							\
    } * data = mem->ptr;						\
    size_t index = 0;							\
    for(size_t i = 0; i < cnt; i++){					\
      if(index >= values_cnt) return values_cnt;			\
      if(data[i].key == key){						\
	if(values != NULL)						\
	  values[index] = data[i].value;			        \
	index++;							\
      }									\
    }									\
  									\
    return index;							\
									\
  }									\
  size_t iter_ ## Name (KeyType key, ValueType * values, size_t values_cnt, size_t * i){ \
    size_t item_size = sizeof(key) + sizeof(ValueType);			\
    persisted_mem_area * mem = Name ## Initialize();			\
									\
    u64 cnt = mem->size / item_size;					\
    struct {								\
      KeyType key;							\
      ValueType value;							\
    } * data = mem->ptr;						\
    size_t index = 0;							\
    for(; *i < cnt; (*i)++){						\
      if(index >= values_cnt) return values_cnt;			\
      if(data[*i].key == key){						\
	if(values != NULL)						\
	  values[index] = data[*i].value;	    		        \
	index++;							\
      }									\
    }									\
  									\
    return index;							\
  }									\
  void clear_item_ ## Name(KeyType key){				\
    size_t item_size = sizeof(key) + sizeof(ValueType);			\
    persisted_mem_area * mem = Name ## Initialize();			\
    u64 cnt = mem->size / item_size;					\
    struct {								\
      KeyType key;							\
      ValueType value;							\
    } * data = mem->ptr;						\
    for(size_t i = 0; i < cnt; i++){					\
      if(data[i].key == key) data[i].key = 0;				\
    }									\
  }									\
  void clear_at_ ## Name(u64 idx){				\
    size_t item_size = sizeof(KeyType) + sizeof(ValueType);			\
    persisted_mem_area * mem = Name ## Initialize();			\
    u64 cnt = mem->size / item_size;					\
    ASSERT(idx < cnt);							\
    struct {								\
      KeyType key;							\
      ValueType value;							\
    } * data = mem->ptr;						\
    data[idx].key = 0;							\
  }									\
  void clear_ ## Name(){						\
    persisted_mem_area * mem = Name ## Initialize();			\
    mem_area_realloc(mem, 1);						\
  }


typedef struct{
  char chunk[16];
}string_chunk;

#define CREATE_STRING_TABLE(Name, KeyType)				\
  CREATE_MULTI_TABLE(Name ## _chunk, KeyType, string_chunk);		\
									\
  void set_ ## Name(KeyType item, const char * name){			\
    u64 len = strlen(name) + 1;						\
    string_chunk chunks[1 + len / sizeof(string_chunk)];		\
    memcpy(chunks, name, len);						\
    for(u32 x = 0; x < array_count(chunks); x++){			\
      add_ ## Name ## _chunk(item, chunks[x]);				\
    }									\
  }									\
  									\
  u64 get_ ## Name(KeyType item, char * out, u64 buffer_size){		\
    if(buffer_size == 0) return 0;					\
    u64 l = get_##Name##_chunk(item, (void *)out, buffer_size / sizeof(string_chunk)); \
    return l * sizeof(string_chunk);					\
  }									\
  void remove_ ## Name(KeyType item){					\
    clear_item_ ## Name ## _chunk(item);				\
  }						

#define CREATE_STRING_TABLE_DECL(Name, KeyType)				\
  u64 get_ ## Name(KeyType item, char * out, u64 buffer_size);		\
  void set_ ## Name(KeyType item, const char * name);			\
  void remove_ ## Name(KeyType item);					

CREATE_TABLE_DECL2(base_class, u64, u64);
