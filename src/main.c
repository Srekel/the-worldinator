// TODO
// histogram matching
// gradients
// distance to equator/poles
// distance to water
// rivers
// lighting
// more biomes
// weighted biomes -- DONE

#include <stdio.h>
#if ( ( __STDC_VERSION__ >= 199901L ) || ( _MSC_VER ) )
#include <stdint.h>
#endif
#include <Windows.h>
#include <errno.h>
#include <string.h>

#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

// #define JC_VORONOI_IMPLEMENTATION
// #define JCV_REAL_TYPE double
// #define JCV_FABS fabs
// #define JCV_ATAN2 atan2
// #include "jc_voronoi/jc_voronoi.h"

#include "open-simplex-noise/open-simplex-noise.h"

// #define SINGLE_OCTAVE
#define WIDTH 1024
#define HEIGHT 1024
#define PI 3.14159265

typedef struct {
    const char* name;
    double      height;
    double      height_range;
    uint32_t    color;
} BiomeRule;

static BiomeRule biome_rules[] = {
    { "deepwater", -0.5, 0.5, 0xffaa0000 }, { "shore", -0.1, 0.1, 0xffffa000 },
    { "beach", 0, 0.05, 0xff00ffff },       { "grass", 0.2, 0.2, 0xff00aa00 },
    { "rock", 0.5, 0.2, 0xff555555 },       { "snow", 0.7, 0.3, 0xffffffff }
};

static int num_biome_rules = sizeof( biome_rules ) / sizeof( BiomeRule );

static void
png_write_callback( void* context, void* data, int size ) {
    fwrite( data, 1, size, (FILE*)context );
}

static void
write_png_image( const char* filename, unsigned char* pixels, int w, int h, int has_alpha ) {
    FILE* f;
    fopen_s( &f, filename, "wb" );
    stbi_write_png_to_func( png_write_callback, f, w, h, 4, pixels, 0 );
    fclose( f );
}

double
lerp( double a, double b, double t ) {
    return a + ( b - a ) * t;
}

// double
// ease_in_cubic( double t, double b, double c, double d ) {
//     t /= d;
//     return c * t * t * t + b;
// };

#define CLAMP( a, min, max ) a = ( a < min ? min : ( a > max ? max : a ) );

void
generate_heightmap( int     height,
                    int     width,
                    double  feature_size,
                    double  persistence,
                    int     num_octaves,
                    double* output ) {

    struct osn_context** opensimplexes = malloc( num_octaves * sizeof( void* ) );
    for ( int i = 0; i < num_octaves; ++i ) {
        open_simplex_noise( i * i, opensimplexes + i );
    }

    struct osn_context* opensimplex_roughness;
    open_simplex_noise( 123132, &opensimplex_roughness );

    double* simplex_values = malloc( num_octaves * sizeof( double ) );
    double* biases         = malloc( num_octaves * sizeof( double ) );
    for ( int y = 0; y < height; y++ ) {
        for ( int x = 0; x < width; x++ ) {
            double frequency = 1;
            for ( int i = 0; i < num_octaves; ++i ) {
                double* v = simplex_values + i;
                *v        = open_simplex_noise2( opensimplexes[i],
                                          (double)x / feature_size * frequency,
                                          (double)y / feature_size * frequency );
                frequency *= 4;
            }

            // double roughness_bias = 1;
            double roughness_bias =
              0.5 *
              ( open_simplex_noise2( opensimplex_roughness, x * 4.0 / width, y * 4.0 / height ) +
                1 );

            double bias_sum = 0;
            double amp      = 1;
            for ( int i = 0; i < num_octaves; ++i ) {
                amp *= persistence;
            }

            for ( int i = 0; i < num_octaves; ++i ) {
                amp /= persistence;
                biases[i] = amp * lerp( roughness_bias, 1, i / num_octaves );
                bias_sum += biases[i];
            }

            double value = 0;
            for ( int i = 0; i < num_octaves; ++i ) {
                double v = simplex_values[i];
                value += v * biases[i] / bias_sum;
            }
            // value *= ( 1 + 0.2 * num_octaves );
            // CLAMP( value, -1, 1 );
            output[x + y * HEIGHT] = value;
        }
    }

    open_simplex_noise_free( opensimplex_roughness );
    for ( int i = 0; i < num_octaves; ++i ) {
        open_simplex_noise_free( opensimplexes[i] );
    }
    free( opensimplexes );
    free( simplex_values );
}

void
generate_gradient( double* heightmap, double* gradient_map, int height, int width ) {
    int sobel_filter_x[3][3] = { { -1, 0, 1 }, { -2, 0, 2 }, { -1, 0, 1 } };
    int sobel_filter_y[3][3] = { { -1, -2, -1 }, { 0, 0, 0 }, { 1, 2, 1 } };

    double pixel_value_x;
    double pixel_value_y;
    // double min, max;
    int x, y, x2, y2; /* Loop variable */

    /* Maximum values calculation after filtering*/
    printf( "Now, filtering of input image is performed\n\n" );
    // min = DBL_MAX;
    // max = -DBL_MAX;
    for ( y = 1; y < height - 1; y++ ) {
        for ( x = 1; x < width - 1; x++ ) {
            pixel_value_x = 0.0;
            pixel_value_y = 0.0;
            for ( y2 = -1; y2 <= 1; y2++ ) {
                for ( x2 = -1; x2 <= 1; x2++ ) {
                    pixel_value_x +=
                      sobel_filter_x[x2 + 1][y2 + 1] * heightmap[x + x2 + ( y + y2 ) * width];
                    pixel_value_y +=
                      sobel_filter_y[x2 + 1][y2 + 1] * heightmap[x + x2 + ( y + y2 ) * width];
                }
            }
            gradient_map[x + y * width] =
              sqrt( pixel_value_x * pixel_value_x + pixel_value_y * pixel_value_y );
            // if ( pixel_value < min )
            //     min = pixel_value;
            // if ( pixel_value > max )
            //     max = pixel_value;
        }
    }
}

int
main() {
    int                 x, y;
    uint32_t*           image2d      = malloc( HEIGHT * WIDTH * sizeof( uint32_t ) );
    double*             output       = malloc( HEIGHT * WIDTH * sizeof( double ) );
    double*             gradient_map = malloc( HEIGHT * WIDTH * sizeof( double ) );
    struct osn_context* ctx;
    open_simplex_noise( 46512, &ctx );

    double persistence  = 3;
    int    num_octaves  = 4;
    int    feature_size = 100;
    generate_heightmap( HEIGHT, WIDTH, feature_size, persistence, num_octaves, output );
    generate_gradient( output, gradient_map, HEIGHT, WIDTH );

    for ( y = 0; y < HEIGHT; y++ ) {
        for ( x = 0; x < WIDTH; x++ ) {
            double distance_from_center = sqrt( fabs( x - WIDTH / 2 ) * fabs( x - WIDTH / 2 ) +
                                                fabs( y - WIDTH / 2 ) * fabs( y - WIDTH / 2 ) );
            // double angle                = atan2( y - HEIGHT / 2, x - WIDTH / 2);
            double radius_noise = open_simplex_noise2( ctx, (double)x / 50, (double)y / 50 );
            double radius_threshold_min = HEIGHT * ( radius_noise * 0.1 + 0.3 );
            double radius_threshold_max = HEIGHT * ( radius_noise * 0.1 + 0.4 );
            // output[x + y * HEIGHT]      = radius_noise;

            if ( distance_from_center > radius_threshold_min ) {
                double lerp_t = distance_from_center > radius_threshold_max
                                  ? 1
                                  : ( distance_from_center - radius_threshold_min ) /
                                      ( radius_threshold_max - radius_threshold_min );
                lerp_t *= lerp_t;
                lerp_t *= lerp_t;
                double* value = &output[x + y * HEIGHT];
                *value        = lerp( *value, -1, lerp_t );
            }
        }
    }

    // for ( y = 0; y < HEIGHT; y++ ) {
    //     for ( x = 0; x < WIDTH; x++ ) {
    //         if ( output[x + y * WIDTH] > -0.99 ) {
    //             int x_noise           = 5 * ( open_simplex_noise2( ctx, x, y ) + 1 );
    //             int y_noise           = 5 * ( open_simplex_noise2( ctx, x, y ) + 1 );
    //             output[x + y * WIDTH] = output[x + x_noise + ( y + y_noise ) * WIDTH];
    //         }
    //     }
    // }

    typedef struct  {
        union {
            uint32_t u32;
            struct {
                char a;
                char b;
                char g;
                char r;
            } abgr;
        };
    } Color;

    for ( y = 0; y < HEIGHT; y++ ) {
        for ( x = 0; x < WIDTH; x++ ) {
            double value = output[x + y * HEIGHT];
            // double value      = gradient_map[x + y * HEIGHT];
            double best_score = 0;
            int    best_biome = -1;

            for ( int i = 0; i < num_biome_rules; ++i ) {
                BiomeRule rule = biome_rules[i];

                if ( rule.height - rule.height_range <= value &&
                     value <= rule.height + rule.height_range ) {
                    double lerp_value = fabs( value - rule.height ) / rule.height_range;
                    double score      = lerp( 1, 0.01, lerp_value );
                    if ( score > best_score ) {
                        best_score = score;
                        best_biome = i;
                    }
                }
            }

            BiomeRule rule = biome_rules[best_biome];
            Color     c;
            c.u32 = rule.color;
            c.abgr.b *= ( gradient_map[x + y * HEIGHT] + 1 ) * 1.75;
            c.abgr.g *= ( gradient_map[x + y * HEIGHT] + 1 ) * 1.75;
            c.abgr.a *= ( gradient_map[x + y * HEIGHT] + 1 ) * 1.75;
            uint32_t rgb           = 0x010101 * ( uint32_t )( ( value + 1 ) * 127.5 );
            image2d[x + y * WIDTH] = ( 0x0ff << 24 ) | ( rgb );
            image2d[x + y * WIDTH] = c.u32;
        }
    }

    write_png_image( "test2d.png", (unsigned char*)image2d, WIDTH, HEIGHT, 1 );
    free( image2d );
    free( output );
    open_simplex_noise_free( ctx );
    return 0;
}
