float _start(void* arg) {
  unsigned int addr = (unsigned int)arg;
  return *(float*)&addr;
}
