extern "C" void kernel_relu(const int *input, int *output, unsigned long count) {
  unsigned long i = 0;
  do {
    int value = input[i];
    output[i] = value > 0 ? value : 0;
  } while (++i != count);
}
