#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>

using namespace std;

#include "space.hpp"

void qux() {
  space<double> foo;
  const unsigned limit = 52311;
  for(unsigned i = 0; i < limit; ++i) {
    foo.push_back(i);
    assert (foo.size() == i+1);
    for(unsigned j = 0; j <= sqrt(i); ++j) {
      const auto k = rand() % foo.size();
      assert (k == foo.get(k));
    }
    const unsigned little = min(static_cast<unsigned>(sqrt(limit)),i);
    for(unsigned j = 0; j <= little; ++j) {
      foo.pop_back();
    }
    for(unsigned j = i - little; j <= i; ++j) {
      foo.push_back(j);
    }


  }
  //foo.get(10);
}

int main() {
  const auto seed = static_cast<unsigned int>(time(0));
  cerr << "seed: " << seed << endl;
  srand(seed);

  qux();
}
