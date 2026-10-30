#pragma once
typedef struct { int hit; float score; const char *text,*source,*tone,*style; } Assist;
Assist assistant_route(const char*);
int assistant_tool(const char*,char*,unsigned);
int assistant_direct(const char*,Assist*,char*,unsigned);
const char*assistant_style(const Assist*);
