extern int weak_alias[4];

int use_weak(void) { return weak_alias[0]; }
