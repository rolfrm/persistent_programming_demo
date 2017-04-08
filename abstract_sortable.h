typedef struct _abstract_sorttable{
  u64 count;
  u64 capacity;
  bool is_multi_table;
  const u32 column_count;
  int (*cmp) (void * k1, void * k2);
  void * tail;
}abstract_sorttable;

void abstract_sorttable_init(abstract_sorttable * table, const char * table_name, u32 column_count, u32 * column_size, char ** column_name);
void abstract_sorttable_check_sanity(abstract_sorttable * table);
bool abstract_sorttable_keys_sorted(abstract_sorttable * table, void * keys, u64 cnt);
void abstract_sorttable_finds(abstract_sorttable * table, void * keys, u64 * indexes, u64 cnt);
void abstract_sorttable_inserts(abstract_sorttable * table, void ** values, u64 count);
void abstract_sorttable_clear(abstract_sorttable * table);
void abstract_sorttable_remove_indexes(abstract_sorttable * table, u64 * indexes, size_t index_count);