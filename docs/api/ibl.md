# Image-Based Lighting

Precomputation for lighting with an equirectangular environment map: spherical-harmonic
irradiance for the diffuse term, and the GGX prefiltered map with its split-sum table for
the specular term. The inputs are any equirectangular map, such as a sky from
`alwan_sky_bake_equirect` or an HDR photograph. The per-direction math compiles as a
shader, so a GPU evaluates the same arithmetic.

> **Precision variants:** every function shown as `name_{T}` exists as `name_f32` and
> `name_f64`. The sums are formed in double in both, and the f32 forms round the result
> once. `alwan_sh_coefficient_count` takes no float and has no suffix.

## Conventions

The map is alwan's equirectangular convention (see
[importance-sampling.md](importance-sampling.md)): +Y is up and row 0 the zenith, and map
position `(x, y)` is the direction at `theta = pi y` from +Y and `phi = 2 pi x`:

```
d = (sin theta cos phi, cos theta, sin theta sin phi)
```

**Spherical harmonics** are real and orthonormal over the sphere, without the
Condon-Shortley phase (Ramamoorthi and Hanrahan's basis). The polar axis is +Y and the
angles are the map's:

```
Y_l,0  = N_l^0 P_l^0(cos theta)
Y_l,m  = sqrt(2) N_l^m P_l^m(cos theta) cos(m phi)     m > 0
Y_l,-m = sqrt(2) N_l^m P_l^m(cos theta) sin(m phi)     m > 0
N_l^m  = sqrt((2l + 1) / (4 pi) (l - m)! / (l + m)!)
```

In Cartesian terms of an alwan direction `d`, band 1 is `Y_1,-1 = c d.z`, `Y_1,0 = c d.y`,
`Y_1,1 = c d.x` with `c = sqrt(3 / (4 pi))`. A basis value is computed as a polynomial in
`d` (`sin^m theta (cos m phi, sin m phi)` is `(d.x + i d.z)^m`), so no angle is taken and
the poles need no special case.

Coefficient `(l, m)` is at index `l^2 + l + m`. A table for several channels is
coefficient-major: coefficient `k` of channel `c` at `k * channels + c`. Bands 0 to
`ALWAN_SH_MAX_BAND` (8, 81 coefficients) and 1 to 16 channels.

## Spherical harmonics

```c
size_t alwan_sh_coefficient_count(int band);   /* (band + 1)^2, 0 out of range */

alwan_status alwan_sh_project_equirect_{T}(T *coeffs_out, int band, T const *map, size_t row_stride,
                                           size_t width, size_t height, size_t channels);
alwan_status alwan_sh_evaluate_{T}(T *out, alwan_vec3_{T} const *dir, T const *coeffs, int band,
                                   size_t channels);
alwan_status alwan_sh_render_equirect_{T}(T *map_out, size_t row_stride, size_t width, size_t height,
                                          T const *coeffs, int band, size_t channels);
alwan_status alwan_sh_irradiance_coefficients_{T}(T *out, T const *coeffs, int band, size_t channels,
                                                  alwan_sh_window window, T window_width);
alwan_status alwan_sh_irradiance_matrix_{T}(T *out, T const *coeffs, size_t channels,
                                            alwan_sh_window window, T window_width);
```

- **project_equirect**: each pixel's value times the basis at its centre, times its exact
  solid angle `(2 pi / width)(cos theta_top - cos theta_bottom)`, summed in double. The
  solid angles add to 4 pi exactly; the basis is sampled at pixel centres (the midpoint
  rule), so a 256 x 128 map projects a constant to 1.3e-4 and the basis functions back to
  themselves to 2.0e-4 (suite 286).
- **evaluate** / **render_equirect**: the expansion at one direction (any non-zero length)
  or at every pixel centre of a map.
- **irradiance_coefficients**: Ramamoorthi and Hanrahan 2001, `E_l,m = A_l w_l L_l,m`, with
  the clamped cosine's zonal coefficients

  | l | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
  |---|---|---|---|---|---|---|---|---|---|
  | A_l | pi | 2 pi / 3 | pi / 4 | 0 | -pi / 24 | 0 | pi / 64 | 0 | -pi / 128 |

  and `w_l` the window. Evaluating the result at a normal `n` gives the irradiance
  `integral of L(w) max(0, n . w) dw`; divide by pi for the radiance a white Lambertian
  surface reflects. `out` may be `coeffs`.
- **irradiance_matrix**: their equation 12. Per channel a symmetric 4 x 4 `M`, row-major,
  16 values a channel, such that `E(n) = (n, 1)^T M (n, 1)` for a unit `n` in alwan's
  frame (the matrix is permuted from the paper's z-up frame). It reads coefficients 0 to 8
  of any table of band 2 or more.

**Windows.** A truncated expansion rings: on a map with a bright small source, band 2
undershoots on the far side. `ALWAN_SH_WINDOW_HANNING` scales band l by
`0.5 (1 + cos(pi l / width))` and `ALWAN_SH_WINDOW_LANCZOS` by `sinc(l / width)`, both 0
from `l = width` on; `window_width` 0 is `band + 1` (3 for the matrix). Windowing trades
accuracy for less ringing (Sloan 2008).

**Accuracy of band 2.** On the suite's map, a soft sky with a bright lobe, band-2
irradiance sits 2.9% RMS from the brute-force cosine integral over every pixel; band 8 is
at 0.07%. With a Hanning window of width 3, band 2 is 17% off but its lowest value rises
from 0.380 to 0.442. A map linear in the direction (`1 + a . d`, band 1) gives
`pi + (2 pi / 3) a . n`, matched to 2.3e-4 at 192 x 96, the projection's quadrature.

## GGX prefiltering

```c
typedef struct {
    size_t sample_count;   /* 0: 256 */
    int no_mip_filtering;  /* 0: filtered importance sampling */
} alwan_ibl_prefilter_params;

alwan_status alwan_ibl_prefilter_ggx_{T}(T *out, size_t out_row_stride, size_t out_width, size_t out_height,
                                         T roughness, T const *map, size_t row_stride, size_t width,
                                         size_t height, size_t channels,
                                         alwan_ibl_prefilter_params const *params, alwan_ctx *ctx);
```

Karis's split sum (2013), with the view along the normal and the reflection (`n = v = r`).
Each texel of `out` (any size) is the map around the texel's direction `n`, weighed by the
GGX lobe of perceptual roughness `r` (`alpha = r^2`) as Karis's prefilter weighs it:

```
sum over l of L(l) D(h) (n . l)  /  sum over l of D(h) (n . l),     h = normalize(n + l)
```

estimated with `sample_count` Hammersley half vectors drawn with density `D(h)(n . h)`.
By default each sample reads a box pyramid of the map at the level whose texels cover the
sample's solid angle, `0.5 log2(omega_sample / omega_texel) + 1` (Colbert and Krivanek
2007; Karis 2013). That removes the fireflies of few samples at the cost of some blur;
the box pyramid also does not weigh texels by their solid angle near the poles.
`no_mip_filtering` reads the map itself. Roughness 0 is the map resampled bilinearly. x
wraps round the map, and reads do not cross a pole.

Against the brute-force weighted sum over every pixel (suite 286, roughness 0.3, 0.6 and
0.9, a 96 x 48 map), 4096 unfiltered samples land within 0.31% and 256 filtered samples
within 7.1%. The usual chain for a renderer is one call per roughness level, each at half
the size of the last.

## The split-sum table

```c
alwan_status alwan_ibl_brdf_integrate_{T}(T *scale_out, T *bias_out, T nov, T roughness, size_t sample_count);
alwan_status alwan_ibl_brdf_lut_{T}(T *out, size_t row_stride, size_t width, size_t height, size_t sample_count);
alwan_status alwan_ibl_energy_average_{T}(T *out, T const *lut, size_t row_stride, size_t width, size_t height);
```

- **brdf_integrate**: for one `n . v` in (0, 1] and roughness, the scale and bias of `F0`
  in the GGX specular integral with Schlick's Fresnel,
  `integral of f_GGX (n . l) dl = F0 scale + bias`. The masking-shadowing is the
  height-correlated Smith term (Heitz 2014), not Karis's Schlick approximation.
  `sample_count` 0 is 1024 Hammersley samples. At roughness 0 the pair is exactly
  Schlick's, `(1 - (1 - n.v)^5, (1 - n.v)^5)`.
- **brdf_lut**: the table, two values a texel, `width` texels of `n . v` by `height` of
  roughness at the texel centres `((i + 0.5) / width, (j + 0.5) / height)`. A shader reads
  it with `alwan_ibl_lut_lookup` (bilinear, clamp to edge), or as a two-channel texture.
- **energy_average**: per roughness row, `E_avg = 2 integral E(mu) mu dmu` with
  `E = scale + bias`, the directional albedo of a white metal. Kulla and Conty's (2017)
  multiple-scattering lobe is `(1 - E(mu_o))(1 - E(mu_i)) / (pi (1 - E_avg))`, which puts
  back the energy single scattering loses at high roughness (a white metal at roughness
  0.98 keeps E_avg = 0.42).

Against a converged quadrature of the integral, 16384 samples agree to 3e-5 except at
grazing view and low roughness (`n . v` 0.1, roughness 0.4), where the estimator's heavy
tail leaves 1.7e-3 (4 million random samples still sit 6e-4 off). The integral never
exceeds 1; a table of few samples can by its sampling error (1.9e-4 at 256).

## In a shader

`core/alwan_ibl_core.h` holds the basis (`alwan_sh_basis_v`), the zonal coefficients and
windows, `alwan_ibl_ggx_d_v`, `alwan_ibl_smith_g2_v` and `alwan_ibl_ggx_sample_v`.
`core/alwan_ibl_reader.inc` reads a coefficient table and the split-sum table through an
accessor:

```hlsl
#include "alwan_hlsl.h"
#include "core/alwan_ibl_core.h"
StructuredBuffer<float> Sh : register(t0);
#define ALWAN_IBL_NAME     probe
#define ALWAN_IBL_READ(i)  Sh[i]
#include "core/alwan_ibl_reader.inc"
/* alwan_ibl_sh_eval_probe(2, 3, c, n.x, n.y, n.z): irradiance from the irradiance coefficients */
```

Both compile under dxc (SM 6) and fxc (SM 5), fast and deterministic
(`alwan_dev/hlsl_regression/ibl_reader_compile.hlsl`).

## References

- R. Ramamoorthi and P. Hanrahan, "An Efficient Representation for Irradiance Environment
  Maps", SIGGRAPH 2001.
- P.-P. Sloan, "Stupid Spherical Harmonics (SH) Tricks", GDC 2008.
- B. Karis, "Real Shading in Unreal Engine 4", SIGGRAPH 2013 course notes.
- M. Colbert and J. Krivanek, "GPU-Based Importance Sampling", GPU Gems 3, 2007.
- E. Heitz, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs",
  JCGT 3(2), 2014.
- C. Kulla and A. Conty, "Revisiting Physically Based Shading at Imageworks", SIGGRAPH 2017
  course notes.
