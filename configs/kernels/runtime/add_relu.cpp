extern "C" void kernel_add_relu(const int *lhs, const int *rhs, int *output, unsigned long count) {
  unsigned long i = 0;
  do {
    int value = lhs[i] + rhs[i];
    output[i] = value > 0 ? value : 0;
  } while (++i != count);
}
