lun. 17 août à 11:25
We already talk about it but the first entry point to users should be meta nodes. For now we only have kernels "low level implementation" where the node choose dynamically types and dimensions/rank, which user can still use directly. For me that "kernel_node" low level should never be directly manipulated by users, we should have higher level node that will be used later by meta nodes.
We should have something like "function_node" that describe functionnally what users/artists wants, I want artists drop a node "noise" instead of choosing between perlin_noise_2d, plasma_noise_2d, ... The type of noise should be an option of noise_func_node (functionnal node) the users will see a ComboBox where he can choose. Currently I want to refacto the "dll_metadata.cpp".
Recap that will be metanode -> func_node -> kernel_node.
I want to have metadata containing all informations and refactoring it. I want in the metadata containing the category (potentially multi level per package), display name, synonyme, the list of func_node where a kernel land, which metanode (smooth, generate, ...), ...

You can look at the code 
GitHub
 the_flow2 (main and rendering branch)

Don't give me a final solution let's discuss the design first.

Yes. Looking at the current code, I think the architectural distinction you are making is important, and I would not start by designing the new dll_metadata structure yet. We first need to agree on what each semantic layer owns, otherwise the metadata will fossilize the wrong boundaries.

The current system makes the problem quite visible. kernel_registry knows a kernel by a registry name plus a flat package and category. kernel_menu then reconstructs the entire user-visible catalog from exactly those fields, and explicitly sets operation = kernel_name. kernel_node consequently names itself after that operation and exposes the implementation machinery in its UI.

So today Atelier has almost no semantic distance between:

    “the artist wants Noise”

and

    “instantiate registry entry generation_perlin_2d.”

That is exactly the seam I think your func_node should create.
I think there are actually four layers, not three

Your model is:

meta-node → func-node → kernel-node

I agree from the graph/user perspective. But internally I think we should explicitly acknowledge another layer underneath it because it already exists:
Layer	Meaning	Example
Meta node	user intent	Generate, Smooth, Blend
Functional node	recognizable operation/tool	Noise, Blur, Levels, Distance
Kernel	computational contract/algorithm specialization	generation_perlin_2d, generation_simplex_2d
Implementation	executable realization	f32 2D CUDA group X, AVX2 parallel, serial reference...

That last distinction matters because the current naming is slightly confused. impl_node describes itself as a node that can run a collection of kernels, while actual kernel_impl selection happens farther down through implementation_cache. And function_dll_metadata is very clearly metadata about that fourth level: strategy, target, signature, vector width, schedule, generated function name, etc.

So I would be careful about saying "kernel_node = low-level implementation". A kernel is still above an implementation. I think preserving that distinction will become especially important with auto-scheduling, telemetry, crash fallback and hardware specialization.

Conceptually:

Smooth → Gaussian Blur → gaussian_blur kernel → AVX2/CUDA/etc implementation

or:

Generate → Noise → Perlin kernel → chosen implementation

That vocabulary feels much cleaner to me.
Noise is an excellent test case, but also exposes the difficult part

The current Noise package has 22 kernels, all registered under the single flat category "generation". Yet even package_init.cpp already informally subdivides them into basic, fractal, simple, cellular/Voronoi, domain manipulation and special.

So yes, presenting 22 separate graph nodes is the wrong abstraction for an artist.

But I wouldn't define Noise simply as:

ComboBox → choose one of 22 kernels

because that just moves the kernel menu inside one node.

There is a semantic contract that Noise has to own.

For example, Perlin, Simplex and Value noise are relatively interchangeable. White noise has very different meaningful controls. Voronoi/Worley introduce distance metrics/jitter/cell outputs. fBm is arguably not another primitive noise at all; it is a fractal construction over a primitive. Domain warp is even more questionable as a member of the same functional node—it operates conceptually on another field. Caustics and erosion are farther away again.

So the interesting design question becomes:

    What criterion decides that several kernels are variants of one func_node rather than separate functional nodes?

My current criterion would be stronger than “same package/category” or “artist thinks they are vaguely related.”

They should share a sufficiently stable public semantic contract.

That includes the meaning of inputs, outputs and main parameters, not merely compatible C++ types.

This is important because otherwise Noise will become a gigantic union of every parameter from every noise algorithm, with half the inspector disappearing every time the ComboBox changes. That's not really a functional abstraction.
This also changes what “type” in the Noise ComboBox means

I don't think "Perlin" should merely be the display name of a child kernel.

It is something like a functional variant.

So you might conceptually have:

func: noise
variant/method: perlin | simplex | value | cellular | ...

and then the functional resolver takes:

noise + method=perlin + rank=2 + dtype=f32 + other contracts

and arrives at the kernel.

That distinction becomes very useful later. There may be a perlin functional variant without a dedicated perlin_3d user concept; 2D versus 3D should usually be derived from the graph/type context rather than appearing as separate artist options.

This is exactly where your proposed layer would let you remove _2d, dtype permutations, backend distinctions, etc. from the artist's vocabulary.
Where I disagree somewhat with putting all semantic information on the kernel

You mentioned metadata containing:

    category, display name, synonym, list of func_nodes where a kernel lands, which metanode...

The information absolutely belongs in the metadata system, but I would be cautious about making the kernel record the owner of all of it.

Consider:

generation_perlin_2d
→ Noise
→ Generate

If Noise already declares that it belongs to Generate, then having every Perlin/Simplex/etc kernel separately declare Generate is redundant.

Worse:

kernel → func-node → meta-node

and simultaneously:

kernel → meta-node

give you two sources of truth that can disagree.

I would derive the latter relationship unless there is a concrete case where a kernel participates in a meta-node independently of its functional-node membership.

And the relationship really isn't a hierarchy anyway. A kernel may serve more than one functional concept, and a functional node may use more than one kernel.

That latter case is especially important.

A func_node should not necessarily lower to one kernel.

Imagine Auto Levels, Sharpen, sophisticated Noise, a color operation with conversion around it, or future 3D operations. A functional operation may naturally lower into a little computational graph:

func_node → kernel + kernel + kernel

If we design the metadata now around a strict “func has kernel choices” model, I think we'll regret it.

So I see semantic lowering rather than simple parent/child containment.
The current dll_metadata is probably the wrong semantic boundary

This is where looking at function_dll_metadata changed my view somewhat.

The current structure is very deliberately a binary/runtime artifact. It contains metadata version, layout hash, function ABI ID, DLL build information and an array of implementation metadata. The loader treats this metadata as authoritative for deciding whether a .funcs.dll is compatible with the current runtime.

That is a very different responsibility from:

    “Perlin should be searchable as Perlin Noise, Gradient Noise, coherent noise; displayed as Perlin; belongs to Noise; Noise participates in Generate.”

I'm not saying those cannot eventually be exported in the same DLL. They can.

But conceptually I'd distinguish:

catalog metadata
from
kernel metadata
from
implementation/DLL metadata.

Right now implementation_metadata is generated once per compiled implementation. Putting display_name = "Perlin" and synonyms on every f32/f64/rank/target/schedule implementation would be the wrong cardinality immediately.

There is another practical warning sign: function_package_builder calculates its package hash from kernel names/signatures, returns early when the target DLL already exists, and generates the metadata object as part of the function-DLL build. If semantic catalog data becomes tied directly to that artifact, changing something as harmless as "Voronoi" → "Voronoi / Cellular" potentially becomes entangled with compiled-function invalidation/relinking.

I'd rather decide whether “metadata” means one overall package catalog containing several kinds of records, rather than continuing to grow implementation_metadata.
I think category deserves particular attention

Today category means an engineering/package classification. Basic Math has "algebraic", "rounding", "trigonometry", etc.; Noise puts everything under "generation".

But once functional and meta nodes exist, there are at least two different taxonomies:

Where developers organized the implementation
versus
where users discover the operation.

Those aren't necessarily the same.

A Gaussian implementation might physically live in:

image_processing/filtering

while the functional Blur node could be discoverable under something like:

Image / Filter / Blur

and the meta-node Smooth doesn't really belong to a package hierarchy at all.

So when you say “potentially multi-level per package”, I agree with multi-level navigation, but I'm not convinced yet that package should be the root of the artist's taxonomy.

Packages are ownership/deployment boundaries. Artists care about concepts.

That distinction will matter enormously when third-party packages can contribute another implementation to an existing Atelier functional node.
Display name and synonyms also aren't really kernel properties

This becomes obvious with your Noise example.

Suppose someone searches:

gradient noise

They should probably get Noise, perhaps preconfigured to Perlin, rather than get a low-level generation_perlin_2d node.

Therefore we may eventually need metadata at several semantic granularity levels:

Noise
has its display name and search terms.

Perlin
as a variant has its own display name/search terms.

generation_perlin_2d
has an engineering/debug identity.

Those are three different things.

I would also distinguish synonyms from tags. "gradient noise" may genuinely be an alternative term for Perlin-like noise. "procedural" is a search tag, not a synonym. And "old_noise" might be a legacy alias used for document migration, which is something different again.

If we put all three in a generic synonyms[], it will become messy surprisingly quickly.
One existing piece of code particularly validates this refactor

graph_generator.cpp currently has to infer semantic information from package/category/name strings:

it hardcodes things such as Noise + generation = source, BasicMath + arithmetic = binary, ImageProcessing filtering = unary, etc.

That's exactly the sort of code I would expect to disappear once the semantic catalog exists.

The system shouldn't need:

    “if package == noise and category == generation, I guess this is a generator.”

It should know what the functional operation is.

And this ties directly into your inverse-procedural work: policy/search should reason about functional vocabulary much more naturally than raw kernels. The actual kernel remains available when the model needs detailed computational choices, but it shouldn't be the ontology.
The biggest question I'd resolve before touching the metadata structs

I think we should take three concrete examples and determine exactly where the boundaries fall:

Generate → Noise → ?

Smooth → ? → ?

Blend → ? → ?

Especially Noise, because it will tell us whether func_node means “family of interchangeable algorithms”, “artist-facing operation with potentially changing parameter schema”, or “semantic mini-graph that lowers to arbitrary kernels.”

Those three interpretations look similar in the UI, but produce very different metadata models.

My current leaning is toward the third, constrained by a stable functional contract. That makes func_node a genuine abstraction rather than just a prettier kernel_node.

And then the hierarchy you proposed becomes much more powerful:

meta-node = what I want to achieve
func-node = what operation I am manipulating
kernel = how that operation is computationally expressed
implementation = how that kernel runs here

That separation feels consistent with the direction Atelier has been moving toward, rather than an extra UI layer bolted on top.

    graph_generator.cpp is a temporary project don't look at it, but we should have a meta data to distinguish generator, etc. Inspired by Substance Designer
    Should be simpler to have metadata for implementation, kernel, functionnal node and metanode differently
    Look at our packages to understand the scale of the problem


Yes. After looking across the packages, I think your correction is right: trying to make one metadata structure describe implementation + kernel + functional node + meta-node would become a mess very quickly. They have radically different cardinalities and reasons to change.

And I’ll completely exclude graph_generator.cpp from this discussion.
The scale is already large enough that this is not cosmetic

A few representative packages make the problem clear:
Package	Current raw kernel shape	What it tells us
colour	~130 kernels	Huge families of conversions, transfer functions, camera log encoders/decoders, metrics. Raw kernels are absolutely not a usable node vocabulary.
sdf	100 kernels	36 primitives, 9 gradients, 19 patterns, 13 operations, 8 transforms, 15 rasterizers. Some should remain distinct functional nodes; others are clearly methods/options.
image_processing	80+ declared kernels	Mixes grading tools, filters, curves, internal channel-routing utilities, look operations, analysis, etc.
array	43 kernels	Resample, slicing, structural, joining, reduction, accessors. Very technical package where some kernels collapse naturally and others definitely don't.
blend	24 kernels	Almost perfect example of 24 kernels becoming one Blend functional node with a Mode property. All share the same basic (background, foreground, opacity) → result contract.
noise	22 kernels	Current "generation" category hides several genuinely different families: coherent noise, fractal, cellular, domain manipulation, special generators.
normal_map	12 kernels	Generation, compositing, adjustment, analysis; several obvious functional relationships.
flood_fill	12 kernels	Especially revealing: CCL/JFA are algorithmic implementations of flood fill, while Flood Fill to Random/Position/Size/... are actual artist concepts.
neural_net	very large	Backend/context/model nodes plus a huge hierarchy of low-level NN ops. It is already encoding categories such as "ops/activations" and "ops/elementwise" into strings.

And rendering makes the distinction stronger still. The branch adds renderer-neutral scene intent separately from path-tracing execution; path_tracing currently contains trace, a second trace_plain_c implementation path, and a services_probe diagnostic kernel. Those plainly shouldn't become three artist nodes. The renderer-neutral rendering package currently has objects and no computational kernels at all.

So yes: this isn't “better naming around kernel_node”. There is already enough content that Atelier needs a catalog/ontology above its kernel catalog.
Four different metadata records feels much cleaner

I'd now think about them as four independent identities:

Implementation metadata

This is approximately what function_dll_metadata is already good at:

kernel + signature + target + schedule + generated symbol + hashes + ABI/build information

It answers:

    How can I execute this exact kernel specialization on this machine?

It changes when compilation, scheduling or ABI changes. The current structure is explicitly used for DLL compatibility and stale-DLL rejection, so keeping it technically focused makes sense.

Kernel metadata

This describes:

    What computational primitive is this?

generation_perlin_2d, blend_multiply, flood_fill_ccl, conversion_xyz_to_lab, etc.

This is where package ownership, low-level category, capabilities, supported semantic roles, visibility/internal status, functional-node membership and similar things start to make sense.

Functional-node metadata

This answers:

    What tool does the artist think they put in the graph?

Noise
Blend
Flood Fill
Flood Fill to Position
Color Space Convert
Normal From Height
Resample
etc.

Display name, search aliases, discovery categories, modes/options, artist-facing parameterization and presentation semantics belong primarily here.

Meta-node metadata

This answers:

    What is the user's intent?

Generate
Smooth
Blend
Transform
Convert
Analyze
etc.

And then it declares which functional semantics can satisfy that intent under which contracts/context.

The important thing is that these are not four versions of the same struct. They don't even change for the same reasons.
Looking at the packages changes one thing from our previous discussion

I would no longer think of:

metanode → func_node → kernel → implementation

as a strict ownership tree.

It's the resolution direction, yes.

But metadata relationships need to be many-to-many.

For example, Gaussian blur could belong to a Blur functional node, and that functional node could satisfy Smooth, but potentially also be available through another higher-level intent. Likewise one kernel may support two functional presentations.

Conversely, Blend has one functional identity backed by 24 kernels. A future generic Color Space Convert could potentially resolve through dozens of conversion kernels.

So I'd keep:

resolution:
meta → functional → kernel → implementation

but think:

catalog:
a graph of stable IDs and relationships.

That subtle distinction will prevent us from forcing the ontology into the package directory structure.
The package structure demonstrates that category and role must be different

This is probably the next major design point.

Your Substance-inspired Generator classification is not the same information as:

package = noise
category = generation

A category answers something closer to:

    Where should this thing be organized/discovered?

A role answers:

    What kind of operation is this in a graph?

So something along the conceptual axis of:

generator / filter / transform / compositing / conversion / analysis / utility / IO / ...

should exist independently from category.

I wouldn't infer it from arity either.

dist_from_image in Random consumes an image but creates a probability distribution. normal_from_height consumes an image but semantically generates/derives a normal map. flood_fill_to_random has “random” in its name but isn't fundamentally a generator—it maps flood-fill regions to values.

So an explicit semantic classification is much safer.

And I would put the artist-facing role primarily on functional_node_metadata, rather than duplicating it onto every implementation.

A kernel may still need technical roles such as:

internal, surrogate, reference, diagnostic, perhaps automatic_only.

That's another axis.
The existing packages already show why visibility is first-class

Image Processing contains:

extract_rgb
extract_alpha
merge_rgba
unpremul
premul

and explicitly comments that some of those are internal channel-routing kernels and not user-facing. Yet today they live in exactly the same registry/category mechanism as everything else.

Basic Math similarly has differentiable sibling kernels beside ordinary kernels, used for the solver substitution rather than because an artist should choose "differentiable_max" from a palette.

Path tracing has perhaps the strongest example:

trace
trace_plain_c
services_probe

where services_probe is explicitly an acceptance/diagnostics kernel and the two Trace kernels are competing execution paths.

So I think one of the first semantic separations should be:

“exists as a kernel” ≠ “exists as a functional node.”

That alone gets rid of a lot of the current exposure problem.
Some packages compress enormously; others shouldn't

This is where I wouldn't make a generic “kernels with similar names become ComboBox modes” rule.
Blend

This is the easy case.

Twenty-four kernels have the same conceptual operation and contract.

Artist:

Blend
Mode: Multiply

Changing Multiply → Screen doesn't change what the node means. It changes the method/mode.

Excellent func_node.
Colour

Potentially enormous compression.

Today there are separate kernels for:

linear_srgb → OKLab
OKLab → linear_srgb
XYZ → Lab
Lab → XYZ
…

then 33 transfer-function kernels and dozens of camera-log encode/decode kernels.

At the functional level, I can imagine concepts more like:

Convert Color Space
From / To

Transfer Function
Curve / Encode-Decode

Camera Log
System / Direction

Color Difference
Metric

Whether those are exactly the correct four nodes isn't important yet. What matters is that 130 kernels probably represent an order of magnitude fewer artist concepts.
Flood Fill

This gives the opposite lesson.

I would probably not collapse:

Flood Fill to Random
Flood Fill to Position
Flood Fill to Size
Flood Fill to Gradient

into a ComboBox just because they are all mapping kernels. They are meaningful Substance-style tools and produce conceptually different information.

But:

flood_fill_ccl
flood_fill_jfa

look much more like two kernels beneath one Flood Fill functional node.

That is almost a textbook demonstration of the boundary we're trying to describe.
SDF

SDF is where a simplistic rule completely falls apart.

A user absolutely wants Star, Box, Disc, Gear, etc. to be readily understandable. Perhaps some could live under a generic Shape functional node, but making every primitive a ComboBox option would make a graph visually opaque.

On the other hand:

union / intersect / subtract / smooth_union / smooth_intersect / smooth_subtract

has a much stronger argument for being a functional Combine SDF operation with meaningful modes/options. Meanwhile twist, bend, displace, onion, round, etc. are different verbs.

So the criterion for func_node grouping is really graph semantics and artist mental model, not implementation similarity.
There is another clue in kernel_node.cpp

Current kernel_node already contains special handling for curve editors, color curves, tone curves, gradients, grid warps and LUT variants. It even identifies color-curve behavior from operation names such as curves_hue_vs_hue.

That's essentially the lower-level node starting to become a functional node accidentally.

The kernel needs something like:

    input 1 is a 1D float LUT with these contracts.

But the artist needs:

    Hue vs Hue curve editor.

Those are deliberately different representations.

So the introduction of func_node also gives us somewhere principled to put the presentation contract, instead of adding more and more if operation == ... behavior to kernel_node.

I don't mean putting DearWidgets state itself into metadata. Rather, functional metadata can describe the artist-facing parameter/presentation; the actual node instance owns its state and lowers it to kernel inputs.

That separation fits this architecture particularly well.
Multi-level categories: the repo is already asking for them

Neural Net is already registering categories like:

ops/activations
ops/elementwise
ops/linear
ops/normalization

inside what is currently just a category string.

So yes, I would definitely stop thinking of category as one char*.

But I also wouldn't necessarily make a category path unique.

For discovery, a functional node can plausibly appear in several places. For example Normal From Height might be discoverable under both Normal Maps and Generate/Derived Maps.

That's where synonyms alone aren't sufficient: synonyms improve search; category paths affect browsing.
One particularly important consequence: package should not define functional identity

The codebase itself is proving this.

Image Processing says crop/pad moved into Array because arrays are the fundamental data type.

SDF says several transforms were migrated into the new data-agnostic transform package. The Transform package now explicitly supports general numeric 2D/3D/4D layouts rather than SDF specifically.

Those are good architectural refactors.

But if user node identity were:

package/category/kernel

then an internal architectural cleanup changes the user's vocabulary/document identity.

That would be bad.

So I think package ownership and semantic identity need to be orthogonal. A function can be supplied by a package, moved between packages, or eventually have kernels contributed by more than one package without becoming a different artist operation.

This will matter even more once external packages/marketplace contributions exist.
Where this leaves the metadata discussion

I think your four-metadata idea is substantially simpler than my previous framing.

I would not turn function_dll_metadata into the universal Atelier metadata blob. Its existing ABI/build responsibility is too useful and too low-level.

Instead, I would think of “Atelier metadata” as an umbrella containing distinct records for:

implementation ⇄ kernel ⇄ functional operation ⇄ meta intent

with generated indexes between them.

And before deciding the exact structs, the package survey suggests we need to settle three semantics: what makes kernels variants of the same functional node, what semantic role (generator, filter, transform, etc.) means independently of browse category, and how much of the artist-facing parameter model belongs to the functional descriptor.

Those three decisions will determine the metadata shape much more than C++ layout will.

Good now give me the full design including the decomposition to func_node, that will be very long that's why make it a *.md file I can download

I created the full design as a self-contained Markdown document: 4,365 lines / ~13,900 words.

It covers the four-layer metadata architecture, func_node semantics and lowering, stable IDs, roles/categories/visibility, serialization and reproducibility, UI/search behavior, package contributions, autodiff/telemetry implications, backward compatibility, and a detailed proposed func_node decomposition across the current package corpus—including blend, noise, colour, image_processing, sdf, tiling, num, statistics, array, normal_map, objective, neural nets, and the rendering/path_tracing branch.

Download Atelier_Meta_Func_Kernel_Metadata_Design.md
Atelier_Meta_Func_Kernel_Metadata_Design.mdDocument


