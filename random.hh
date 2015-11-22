/*
 * Mersenne twister random number generator.
 */

#ifndef __BASE_RANDOM_HH__
#define __BASE_RANDOM_HH__

#include <random>
#include <string>
#include <type_traits>

class Random
{

  private:

    std::mt19937_64 gen;

  public:

    Random();
    Random(uint32_t s);
    ~Random();

    void init(uint32_t s);

    /**
     * Use the SFINAE idiom to choose an implementation based on
     * whether the type is integral or floating point.
     */
    template <typename T>
    typename std::enable_if<std::is_integral<T>::value, T>::type
    random()
    {
        // [0, max_value] for integer types
        std::uniform_int_distribution<T> dist;
        return dist(gen);
    }

    template <typename T>
    typename std::enable_if<std::is_floating_point<T>::value, T>::type
    random()
    {
        // [0, 1) for real types
        std::uniform_real_distribution<T> dist;
        return dist(gen);
    }

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value, T>::type
    random(T min, T max)
    {
        std::uniform_int_distribution<T> dist(min, max);
        return dist(gen);
    }
};

extern Random random_mt;

#endif // __BASE_RANDOM_HH__
