#include <sstream>

#include "random.hh"

Random::Random()
{
    // default random seed
    init(5489);
}

Random::Random(uint32_t s)
{
    init(s);
}

Random::~Random()
{
}

void
Random::init(uint32_t s)
{
    gen.seed(s);
}

Random random_mt;
