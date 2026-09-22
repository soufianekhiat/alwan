# Neural Layer Kernels

The layers a small colour-science network is made of, run by alwan itself.
Roadmap 3.10, step two: the arithmetic of one output element lives in the core
tier, written against accessors so a shader binds its own buffers; what a
tensor is lives with the backend. The point of running a network here rather
than through a vendor library is determinism: a fixed summation order, no
fused multiply-add, the committed exp, so a deterministic build is bit-exact
across C, CUDA, OpenCL, HLSL and GLSL. A library that picks its algorithm at
run time cannot promise that.

Reference: PyTorch, called from `gendata/tests/nn_layers_reference.py` in
float64 on seeded random tensors, per layer. Suite 176 holds the f64 path to
torch within 1e-12 (measured 9e-16 at worst) and the f32 path to the f64 one
within a few float ULP of the largest value (2e-7 at worst).

---

## Layout

Tensors are channels-last, C order. An image element is
`in[(y * W + x) * C + c]`. Convolution weights are HWIO,
`w[((ky * KW + kx) * Cg + ci) * Cout + o]` with `Cg = Cin / groups` the
channels one group sees; a dense weight is `w[i * n_out + o]`. PyTorch stores
NCHW and OIHW; a model converted for alwan carries its weights already in this
order and nothing is transposed at run time. Shapes are the caller's: every
function states the output it produces and the buffer must hold it.

f32 is the compute type every model in scope was trained in. f64 is the
reference path the f32 one is checked against, and costs nothing given the
two-precision machinery.

---

## The per-element kernels, in the core

`core/alwan_nn_core.h` emits, per precision, the activations as value
functions and the tensor kernels of `core/alwan_nn_reader.inc` bound to
pointers:

```c
alwan_{T} alwan_nn_relu_{T}_v(alwan_{T} x);
alwan_{T} alwan_nn_leaky_relu_{T}_v(alwan_{T} x, alwan_{T} alpha);
alwan_{T} alwan_nn_sigmoid_{T}_v(alwan_{T} x);
alwan_{T} alwan_nn_tanh_{T}_v(alwan_{T} x);
alwan_{T} alwan_nn_gelu_{T}_v(alwan_{T} x);                 /* the tanh form */
alwan_{T} alwan_nn_activation_{T}_v(alwan_{T} x, int kind, alwan_{T} alpha);

alwan_{T} alwan_nn_dense_at_{T}_v(in, w, b, int n_in, int n_out, int has_bias, int o);
alwan_{T} alwan_nn_conv2d_at_{T}_v(in, w, b, int H, int W, int Cin, int KH, int KW, int Cout,
                                   int stride, int pad, int groups, int has_bias, int y, int x, int o);
alwan_{T} alwan_nn_pool2d_at_{T}_v(in, int H, int W, int C, int k, int stride, int pad, int mode, int y, int x, int c);
alwan_{T} alwan_nn_global_avg_at_{T}_v(in, int H, int W, int C, int c);
alwan_{T} alwan_nn_upsample2d_at_{T}_v(in, int H, int W, int C, int scale, int mode, int yo, int xo, int c);
alwan_{T} alwan_nn_softmax_at_{T}_v(in, int n, int i);
```

The saturating activations go through exp alone: tanh is
`1 - 2 / (1 + exp(2x))`, sigmoid `1 / (1 + exp(-x))`, GELU the tanh form
`0.5 x (1 + tanh(sqrt(2/pi) (x + 0.044715 x^3)))`, which PyTorch calls
`approximate='tanh'`; the erf form is not offered because the core has no
erf, and the reference asks torch for the tanh form explicitly.

A shader binds the kernels itself, once per set of tensors, and the tensor
parameters disappear:

```hlsl
StructuredBuffer<float> Act : register(t0);
StructuredBuffer<float> W1  : register(t1);
StructuredBuffer<float> B1  : register(t2);

#define ALWAN_NN_NAME       layer1
#define ALWAN_NN_READ_IN(i) Act[i]
#define ALWAN_NN_READ_W(i)  W1[i]
#define ALWAN_NN_READ_B(i)  B1[i]
#include "core/alwan_nn_reader.inc"

float o = alwan_nn_conv2d_at_layer1(H, W, Cin, 3, 3, Cout, 1, 1, 1, 1, y, x, c);
```

Every accumulate runs in one fixed order (ky, kx, ci for a convolution; i for
a dense layer) into an `ALWAN_DET_PRECISE` local, and touches no intrinsic that
hides an expression. Zero padding is an out-of-range test on the index, not a
read of a padded copy: the accessor is never evaluated outside the image.

Measured, not assumed: `alwan_dev/hlsl_regression/run_nn_parity.py` binds
these kernels exactly as above and runs them on the D3D12 WARP device under
dxc and fxc against the pointer-bound C kernels, 1,616 outputs, with the
reference built ordinary and deterministic. Every tensor kernel is bit-exact
in all four runs; the activations and softmax that go through exp are
bit-exact in the deterministic build, where both sides run the committed
polynomial, and within 0.5 ULP of unity in an ordinary one. One hazard for a
consumer's shader, found there: dxc's default fast-math folds a caller's
arithmetic into a kernel's expression when the two share a statement. Compute
the kernel's input into a variable of its own, or build with `-Gis`, if the
bits matter.

---

## The whole-tensor forms

Each is the per-element kernel in a loop over the output, validated once, so it
agrees with a per-element call to the bit (suite 176 asserts it).

```c
alwan_status alwan_nn_dense_{T}(alwan_{T} *out, alwan_{T} const *in, alwan_{T} const *w, alwan_{T} const *b, int n_in, int n_out);
alwan_status alwan_nn_conv2d_{T}(alwan_{T} *out, alwan_{T} const *in, int H, int W, int Cin,
                                 alwan_{T} const *w, alwan_{T} const *b, int KH, int KW, int Cout,
                                 int stride, int pad, int groups);
alwan_status alwan_nn_activation_{T}(alwan_{T} *out, alwan_{T} const *in, size_t count, alwan_nn_activation_kind kind, alwan_{T} alpha);
alwan_status alwan_nn_pool2d_{T}(alwan_{T} *out, alwan_{T} const *in, int H, int W, int C, int k, int stride, int pad, alwan_nn_pool_kind kind);
alwan_status alwan_nn_global_avg_{T}(alwan_{T} *out, alwan_{T} const *in, int H, int W, int C);
alwan_status alwan_nn_upsample2d_{T}(alwan_{T} *out, alwan_{T} const *in, int H, int W, int C, int scale, alwan_nn_upsample_kind kind);
alwan_status alwan_nn_softmax_{T}(alwan_{T} *out, alwan_{T} const *in, int n);
alwan_status alwan_nn_add_{T}(alwan_{T} *out, alwan_{T} const *a, alwan_{T} const *b, size_t count);
alwan_status alwan_nn_concat_channels_{T}(alwan_{T} *out, alwan_{T} const *a, int Ca, alwan_{T} const *b, int Cb, int H, int W);
```

- **dense**: `out[n_out] = in[n_in] w[n_in x n_out] + b[n_out]`; `b` may be NULL.
- **conv2d**: cross-correlation as every framework means it, of an
  `H x W x Cin` image by `KH x KW x (Cin / groups) x Cout` weights, one stride
  and one zero padding on both axes, `b[Cout]` or NULL. `groups` divides both
  `Cin` and `Cout`; `groups == Cin == Cout` is depthwise. The output is
  `((H + 2 pad - KH) / stride + 1) x ((W + 2 pad - KW) / stride + 1) x Cout`.
  `ALWAN_E_RANGE` if the padded image is smaller than the kernel,
  `ALWAN_E_INVALID` if `groups` does not divide.
- **activation**: elementwise, `out` may be `in`; `alpha` is read by
  `ALWAN_NN_ACTIVATION_LEAKY_RELU` only.
- **pool2d**: max or average over `k x k` at one stride with zero padding,
  output `((H + 2 pad - k) / stride + 1)` squared `x C`. An average divides by
  `k * k`, padded positions included (PyTorch's `count_include_pad` default);
  a max skips them. `pad` above `k / 2` is `ALWAN_E_RANGE`, PyTorch's own
  rule.
- **global_avg**: `out[C]`, the mean over the image.
- **upsample2d**: by an integer factor to `(H scale) x (W scale) x C`. Nearest
  takes the source at `floor(dst / scale)`; bilinear is PyTorch's
  `align_corners=False`, source coordinate `(dst + 0.5) / scale - 0.5`
  clamped at zero, the upper neighbour clamped to the last row or column.
- **softmax**: max-subtracted, the max and the sum computed once for the
  vector; `out` may be `in`.
- **add**, **concat_channels**: elementwise sum, and the channel
  concatenation of two `H x W` images into `Ca + Cb` channels.

Batch normalisation is not a layer here on purpose: it folds into the
preceding convolution's weights and bias at conversion time, which is where
every framework's exporter puts it too.

---

## What is not here yet

The layers are the layer set of the first target network, a small
convolution over a chromaticity histogram joined to a dense layer, and the
few others the roadmap lists for the second. Attention arrives when a model in
hand needs it. Quantised int8 inference is a later stage. There is no model
file reader and there will not be one: the converter below turns a trained
model into a generated forward function plus its weights as an `.inc`, so
shapes are static, which is what makes the shading-language backends possible
at all.

---

## Converting a model

`alwan_dev/gendata/nn_convert.py` takes a PyTorch model and writes C:

```
python gendata/nn_convert.py my_models:build_awb awb 64 64 2 awb_model.inc [awb.pt]
```

`my_models.build_awb()` returns the model (the working directory is on the
import path); `awb.pt`, when given, is a `state_dict` or a checkpoint carrying
one under `"state_dict"`, loaded into it; `64 64 2` is the input as H, W, C
(channels-last, which is alwan's layout). A model that is not a Sequential is
routed to the graph converter below. The file it
writes is included after `alwan.h` and holds:

```c
#define AWB_IN_H 64          /* and _IN_W, _IN_C, _OUT_H, _OUT_W, _OUT_C, _OUT_SIZE */
#define AWB_ARENA_FLOATS 32768
static const float awb_w0[...] = { ... };   /* one array per layer, bias as _b<n> */
static alwan_status awb_forward(float *out, float const *in, float *arena);
```

`awb_forward` calls the `alwan_nn_*_f32` layers in the model's order with every
shape a constant, ping-ponging between two halves of `arena`, which is the
caller's and holds two copies of the largest activation. Nothing is allocated,
nothing is transposed at run time, and the function is `static`, so a consumer
includes the `.inc` in one translation unit and owns its symbols.

What the converter does with the model, once, at conversion time:

- Walks a `torch.nn.Sequential` (nested ones are flattened; a module with a
  `forward` of its own is walked through its children, which must be
  sequential). Accepted: `Conv2d` (equal stride and padding on both axes,
  groups, optional bias), `BatchNorm2d` and `BatchNorm1d`, `ReLU`, `LeakyReLU`,
  `Sigmoid`, `Tanh`, `GELU(approximate='tanh')`, `MaxPool2d`, `AvgPool2d`
  (`count_include_pad=True`, PyTorch's default), `AdaptiveAvgPool2d((1, 1))`,
  `Flatten`, `Linear`, `Softmax`, `Upsample` (integer factor, nearest or
  bilinear with `align_corners=False`), `Identity` and `Dropout`. Anything
  else raises, naming the module.
- Folds each batch normalisation into the convolution or dense layer before
  it, per output channel, in float64, then rounds once to float32.
- Permutes convolution weights from OIHW to HWIO and dense weights to
  `w[i * n_out + o]`. The columns of a `Linear` that follows a `Flatten` are
  reordered from torch's `c * H * W + y * W + x` to `(y * W + x) * C + c`,
  because torch flattens c-major and alwan's activation is one H x W x C run.
  That permutation is applied exactly once, on the first `Linear` after the
  `Flatten`.
- Writes every weight as a float32 literal with nine significant digits,
  which reads back to the same float.

The generated function is exactly as faithful as those permutations, so the
suite that proves the converter is an end-to-end one: suite 177 runs two
models from `gendata/tests/nn_demo_reference.py` that between them use every
accepted layer, with batch norm statistics that are not the identity, and
holds the float32 forward to torch in float64 on the same float32 weights with
batch norm applied as a layer. Worst 1.9e-8 and 4.6e-8 on outputs of order
one; a permutation, fold or shape mistake is order one. The models are the
repository's own because the first target network's weights are published
under a non-commercial licence and cannot ship.

The forward function is generated for the f32 API. A model trained in float64
is converted through float32.

A forward that is a graph rather than a sequence, a residual block or a
concatenation, goes through `alwan_dev/gendata/nn_convert_graph.py`
(`convert_graph(model, name, (H, W, C))`, the same output contract), which
traces the model with torch.fx, propagates shapes from a zero input, and emits
the nodes in order: `x + y` and `torch.add` become `alwan_nn_add`,
`torch.cat([x, y], dim=1)` becomes `alwan_nn_concat_channels`, `torch.relu`,
`F.relu`, `torch.softmax` and `torch.flatten` their layer, and every module the
sequential converter accepts is accepted here. Batch normalisation folds into
the convolution or dense layer that feeds it when that layer has no other
consumer. The arena is slots reused as the graph's tensors die (a slot returns
to the free list once its tensor's last consumer is emitted), and an activation
runs in place when its input has no other consumer: suite 177's residual model
is eleven tensors in four slots, 648 floats. Held to torch at 1e-6 like the
other two.

---

## See Also

- [backends.md](backends.md) for the accessor seam the shader binding uses
- [tables.md](tables.md) for the same seam on the table readers
- `docs/determinism.md` for why the summation order is part of the contract
