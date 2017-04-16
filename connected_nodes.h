// This file is auto generated by table_compiler
typedef struct _connected_nodes{
  char ** column_names;
  char ** column_types;
  u64 count;
  const bool is_multi_table;
  const int column_count;
  int (*cmp) (const u32 * k1, const u32 * k2);
  const u64 sizes[2];

  u32 * n1;
  u32 * n2;
  mem_area * n1_area;
  mem_area * n2_area;
}connected_nodes;

connected_nodes * connected_nodes_create(const char * optional_name);
void connected_nodes_set(connected_nodes * table, u32 n1, u32 n2);
void connected_nodes_insert(connected_nodes * table, u32 * n1, u32 * n2, u64 count);
void connected_nodes_lookup(connected_nodes * table, u32 * keys, u64 * out_indexes, u64 count);
void connected_nodes_remove(connected_nodes * table, u32 * keys, u64 key_count);
void connected_nodes_get_refs(connected_nodes * table, u32 * keys, u64 ** indexes, u64 count);
void connected_nodes_clear(connected_nodes * table);
void connected_nodes_unset(connected_nodes * table, u32 key);
bool connected_nodes_try_get(connected_nodes * table, u32 * n1, u32 * n2);
void connected_nodes_print(connected_nodes * table);
u64 connected_nodes_iter(connected_nodes * table, u32 * keys, size_t keycnt, u32 * optional_keys_out, u64 * indexes, u64 cnt, u64 * iterator);