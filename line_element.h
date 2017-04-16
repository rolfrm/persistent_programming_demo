// This file is auto generated by table_compiler
typedef struct _line_element{
  char ** column_names;
  char ** column_types;
  u64 count;
  const bool is_multi_table;
  const int column_count;
  int (*cmp) (const u32 * k1, const u32 * k2);
  const u64 sizes[3];

  u32 * index;
  index_table_sequence * curve_data;
  bool * is_bezier;
  mem_area * index_area;
  mem_area * curve_data_area;
  mem_area * is_bezier_area;
}line_element;

line_element * line_element_create(const char * optional_name);
void line_element_set(line_element * table, u32 index, index_table_sequence curve_data, bool is_bezier);
void line_element_insert(line_element * table, u32 * index, index_table_sequence * curve_data, bool * is_bezier, u64 count);
void line_element_lookup(line_element * table, u32 * keys, u64 * out_indexes, u64 count);
void line_element_remove(line_element * table, u32 * keys, u64 key_count);
void line_element_get_refs(line_element * table, u32 * keys, u64 ** indexes, u64 count);
void line_element_clear(line_element * table);
void line_element_unset(line_element * table, u32 key);
bool line_element_try_get(line_element * table, u32 * index, index_table_sequence * curve_data, bool * is_bezier);
void line_element_print(line_element * table);

