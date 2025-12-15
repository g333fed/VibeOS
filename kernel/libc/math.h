/* VibeOS math.h - floating point math functions for stb_truetype */
#ifndef _MATH_H
#define _MATH_H

/* IEEE 754 special values */
#define INFINITY (__builtin_inf())
#define NAN (__builtin_nan(""))
#define HUGE_VAL (__builtin_huge_val())

/* Classification macros */
#define isnan(x) __builtin_isnan(x)
#define isinf(x) __builtin_isinf(x)
#define isfinite(x) __builtin_isfinite(x)
#define signbit(x) __builtin_signbit(x)

/* Use ARM64 hardware sqrt instruction */
static inline double sqrt(double x) {
    double result;
    __asm__ __volatile__("fsqrt %d0, %d1" : "=w"(result) : "w"(x));
    return result;
}

static inline float sqrtf(float x) {
    float result;
    __asm__ __volatile__("fsqrt %s0, %s1" : "=w"(result) : "w"(x));
    return result;
}

/* Absolute value - clear sign bit */
static inline double fabs(double x) {
    double result;
    __asm__ __volatile__("fabs %d0, %d1" : "=w"(result) : "w"(x));
    return result;
}

static inline float fabsf(float x) {
    float result;
    __asm__ __volatile__("fabs %s0, %s1" : "=w"(result) : "w"(x));
    return result;
}

/* Floor - round toward negative infinity */
static inline double floor(double x) {
    double result;
    __asm__ __volatile__("frintm %d0, %d1" : "=w"(result) : "w"(x));
    return result;
}

static inline float floorf(float x) {
    float result;
    __asm__ __volatile__("frintm %s0, %s1" : "=w"(result) : "w"(x));
    return result;
}

/* Ceil - round toward positive infinity */
static inline double ceil(double x) {
    double result;
    __asm__ __volatile__("frintp %d0, %d1" : "=w"(result) : "w"(x));
    return result;
}

static inline float ceilf(float x) {
    float result;
    __asm__ __volatile__("frintp %s0, %s1" : "=w"(result) : "w"(x));
    return result;
}

/* fmod - floating point remainder */
static inline double fmod(double x, double y) {
    return x - floor(x / y) * y;
}

static inline float fmodf(float x, float y) {
    return x - floorf(x / y) * y;
}

/* Trigonometric constants */
#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923

/* cos using Taylor series
 * cos(x) = 1 - x^2/2! + x^4/4! - x^6/6! + ...
 * Reduce x to [-pi, pi] first for accuracy
 */
static inline double cos(double x) {
    /* Reduce to [0, 2*pi] */
    x = fmod(fabs(x), 2.0 * M_PI);

    /* Reduce to [0, pi] using symmetry */
    int sign = 1;
    if (x > M_PI) {
        x = 2.0 * M_PI - x;
    }

    /* Reduce to [0, pi/2] */
    if (x > M_PI_2) {
        x = M_PI - x;
        sign = -sign;
    }

    /* Taylor series - 6 terms is plenty for float precision */
    double x2 = x * x;
    double result = 1.0;
    double term = 1.0;

    term *= -x2 / (1.0 * 2.0);
    result += term;

    term *= -x2 / (3.0 * 4.0);
    result += term;

    term *= -x2 / (5.0 * 6.0);
    result += term;

    term *= -x2 / (7.0 * 8.0);
    result += term;

    term *= -x2 / (9.0 * 10.0);
    result += term;

    term *= -x2 / (11.0 * 12.0);
    result += term;

    return sign * result;
}

static inline float cosf(float x) {
    return (float)cos((double)x);
}

/* sin using cos(pi/2 - x) */
static inline double sin(double x) {
    return cos(M_PI_2 - x);
}

static inline float sinf(float x) {
    return (float)sin((double)x);
}

/* acos using Newton's method on cos
 * Or use polynomial approximation
 * acos(x) = pi/2 - asin(x)
 * asin(x) ~= x + x^3/6 + 3x^5/40 + ... for small x
 */
static inline double acos(double x) {
    /* Clamp to valid range */
    if (x <= -1.0) return M_PI;
    if (x >= 1.0) return 0.0;

    /* Use identity: acos(x) = atan2(sqrt(1-x^2), x)
     * But we don't have atan2, so use polynomial for asin then subtract from pi/2
     */

    /* For |x| <= 0.5, use Taylor series for asin */
    if (x >= -0.5 && x <= 0.5) {
        double x2 = x * x;
        double asin_x = x * (1.0 + x2 * (1.0/6.0 + x2 * (3.0/40.0 + x2 * (15.0/336.0 + x2 * 105.0/3456.0))));
        return M_PI_2 - asin_x;
    }

    /* For |x| > 0.5, use identity: acos(x) = 2 * asin(sqrt((1-x)/2)) for x > 0
     * and acos(x) = pi - 2 * asin(sqrt((1+x)/2)) for x < 0
     */
    if (x > 0) {
        double y = sqrt((1.0 - x) / 2.0);
        double y2 = y * y;
        double asin_y = y * (1.0 + y2 * (1.0/6.0 + y2 * (3.0/40.0 + y2 * (15.0/336.0 + y2 * 105.0/3456.0))));
        return 2.0 * asin_y;
    } else {
        double y = sqrt((1.0 + x) / 2.0);
        double y2 = y * y;
        double asin_y = y * (1.0 + y2 * (1.0/6.0 + y2 * (3.0/40.0 + y2 * (15.0/336.0 + y2 * 105.0/3456.0))));
        return M_PI - 2.0 * asin_y;
    }
}

static inline float acosf(float x) {
    return (float)acos((double)x);
}

/* Natural logarithm using series expansion
 * ln(x) = 2 * (y + y^3/3 + y^5/5 + ...) where y = (x-1)/(x+1)
 */
static inline double log(double x) {
    if (x <= 0) return -1e308;  /* -inf substitute */

    /* Reduce x to [1, 2) by extracting exponent */
    int exp = 0;
    while (x >= 2.0) { x /= 2.0; exp++; }
    while (x < 1.0) { x *= 2.0; exp--; }

    /* Now x is in [1, 2), use series */
    double y = (x - 1.0) / (x + 1.0);
    double y2 = y * y;
    double result = y;
    double term = y;

    for (int i = 3; i <= 15; i += 2) {
        term *= y2;
        result += term / i;
    }
    result *= 2.0;

    /* Add back the exponent: ln(x * 2^n) = ln(x) + n * ln(2) */
    return result + exp * 0.693147180559945309;  /* ln(2) */
}

/* Exponential function using Taylor series
 * e^x = 1 + x + x^2/2! + x^3/3! + ...
 */
static inline double exp(double x) {
    /* Handle large values to avoid overflow */
    if (x > 709) return 1e308;
    if (x < -709) return 0;

    /* Reduce x to small range using e^x = 2^(x/ln2) = 2^n * e^r
     * where r is the fractional part */
    double ln2 = 0.693147180559945309;
    int n = (int)(x / ln2);
    double r = x - n * ln2;  /* r is now in [-ln2/2, ln2/2] roughly */

    /* Taylor series for e^r */
    double result = 1.0;
    double term = 1.0;
    for (int i = 1; i <= 20; i++) {
        term *= r / i;
        result += term;
        if (fabs(term) < 1e-15) break;
    }

    /* Multiply by 2^n */
    while (n > 0) { result *= 2.0; n--; }
    while (n < 0) { result /= 2.0; n++; }

    return result;
}

/* Power function: x^y = e^(y * ln(x)) */
static inline double pow(double x, double y) {
    if (x == 0) return 0;
    if (y == 0) return 1;
    if (x < 0) {
        /* For negative base, only works for integer exponents */
        int yi = (int)y;
        if (y == yi) {
            double result = pow(-x, y);
            return (yi & 1) ? -result : result;
        }
        return 0;  /* undefined for non-integer exponent */
    }
    return exp(y * log(x));
}

static inline float powf(float x, float y) {
    return (float)pow((double)x, (double)y);
}

#endif /* _MATH_H */
