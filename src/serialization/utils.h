// Common utilities for seriziation


// Compare whether all elements C-style arrays are equal.
// While CATCH2 provides matchers for std::vector, most recob
// classes use C-style arrays so the following is useful
// for comparing them during assertions

template <typename T>
bool compare( T const* a, T const* b, int len) {
  for(auto i=0;i<len;++i)
    if (a[i] != b[i]) return false;
  return true;
}
