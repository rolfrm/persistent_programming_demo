extern u64 console_command_entered_method;
void create_command(u64 id);
CREATE_TABLE_DECL(console_height, u64, float);;
void push_console_history(u64 id, const char * buffer);
CREATE_TABLE_DECL(console_history_cnt, u64, u64);

void create_console(u64 id);
