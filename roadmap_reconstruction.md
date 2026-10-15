# Reconstruction and tracking: parity with KeenTools, AliceVision / Meshroom, Metashape, RealityScan

Design started 2026-10-13. Owner decisions 2026-10-13 in section 1. Nothing is built yet.

## 0. What parity means here

The four products are applications. suwar is a C library. Parity means the library can do
every computation the products do, with the same inputs and outputs, so that an application
(or a script) built on it can reproduce their results. Out of scope:

- user interfaces, node graphs (Meshroom's editor), project files, render farms, cloud
  processing, licence servers;
- text in any output (the processing report becomes numbers, not a PDF);
- FBX (proprietary SDK).

The products split into two families:

| Family | Products | What they compute |
|---|---|---|
| Photogrammetry | AliceVision / Meshroom, Metashape, RealityScan | cameras and sparse points from many photos, dense depth, point clouds, meshes, textures, orthomosaics, DEMs, georeferencing, laser-scan registration |
| Model-based tracking | KeenTools PinTool, GeoTracker, FaceBuilder, FaceTracker | a camera or a known mesh placed from 2D pins, then tracked through video; a morphable head fitted to photos and tracked through its expressions |

The second family reuses most of the first one's foundations (camera models, solvers,
bundle adjustment, rasterising a mesh, texture baking), so one plan covers both.

## 1. Owner decisions (2026-10-13) and calls still open

Decided:

1. **Home: a new library** on top of suwar and alwan, with its own API and `_dev` repo,
   as the owner decided after first choosing suwar. The name is still open; candidates follow
   alwan's and suwar's Arabic plurals: abaad (dimensions), ajsam (solids), manazir (views),
   ashkal (shapes), zawaya (angles). The 2D pieces the plan needs (features, matching, lens
   maps, seams and blending, inpainting) stay in suwar and are called from there.
2. **Threading: a built-in thread pool.** Optional, behind the context: a pool the caller
   sizes (0 or 1 = today's single thread). Results must equal the single-threaded ones bit
   for bit: work is split into items with fixed boundaries, each item writes its own
   output, and reductions run in item order. The per-item steps stay public too, so a
   caller with its own scheduler can use them.
3. **No dependencies.** Everything is in suwar's own C. Portable code is ported, never
   linked.
4. **Optional formats behind a flag.** Alembic is not a dependency: a caller who has it
   builds with `SUWAR_HAS_ALEMBIC` and suwar compiles its exporter against the caller's
   headers; without the flag nothing about Alembic is built. The same for Partio
   (Disney's particle I/O) as `SUWAR_HAS_PARTIO`. The default build stays dependency-free.
5. **Head models: both.** ICT FaceKit light (MIT) is the shipped model. FLAME 2023 Open
   (CC-BY-4.0) is loaded from a file the caller supplies, never shipped.
6. **Learned features: the models only.** ALIKED, DISK and LightGlue weights converted to
   suwar's nn format and run on the nn layers in C. suwar has no Python API; Python
   (the authors' code, PyTorch) is used locally to produce reference outputs, as now.
   SuperPoint, MASt3R and DUSt3R stay out.
7. **Gaussian splats: in, our own implementation.** Rasteriser and optimiser written in
   suwar from the paper; gsplat (Apache-2.0) and the Inria code (non-commercial) only as
   local references.
8. **Full CRS catalogue.** No PROJ dependency: the projection methods are ported from PROJ
   (MIT) and GeographicLib (MIT), and the parameter catalogue is generated from the EPSG
   dataset by gendata.

Still open:

- **EPSG data terms.** The EPSG dataset's terms of use allow redistribution with
  attribution; to confirm before the generated catalogue ships. Geoid and grid-shift files
  each carry their own terms: proposal is to load them from caller files, ship none.
- **Partio's licence** reads as BSD-style with a Disney trademark clause (to confirm from
  the file when the flag is built).
- **Landmark training data** for automatic face alignment: proposal is renders of the head
  model (random identity, expression, pose, lighting), trained with the existing ERT / LBF
  trainers, measured on real faces locally only.
- **LAZ.** LASzip is LGPL: write our own decoder from the published format, or LAS only?
- **Metashape Pro-only data** (multispectral and thermal, satellite RPC, 4D multi-camera,
  power lines): after everything else, or out?
- **Image access.** A caller-supplied fetch callback (image index to pixels) so a project
  never holds every image: proposed as the default way the pipeline reads images.

## 2. What exists already

In suwar, usable as is:

- Camera: Brown-Conrady `suwar_lens` (k1-k6, p1, p2, s1-s4), projection with Jacobians,
  Rodrigues both ways, undistortion of points, `suwar_rectify_map`, warps through maps.
- Calibration: Zhang with LM and a Schur complement over views (suite 152), stereo
  calibration and Bouguet rectification (155), chessboard, circle grid, ChArUco, ArUco,
  AprilTag 3 (156) with pose.
- Pose: `suwar_solve_pnp` (iterative, planar and DLT starts).
- Robust fitting: RANSAC and LMedS over 2D transforms, `find_homography_opencv`.
- Features: SIFT, AKAZE, ORB, BRISK, good features, brute-force matching (float and
  binary), KLT, ECC, dense flow.
- Stereo: BM and SGBM, WLS, reprojection to 3D.
- Panorama parts: cylindrical and spherical warps, exposure gains, graph-cut and DP seams,
  multi-band blending.
- Others: 2D Delaunay and Voronoi, Boykov-Kolmogorov max-flow (from GrabCut), PCA, k-means,
  FFT, Frankot-Chellappa style integration via the FFT, inpainting, YuNet, ERT / LBF
  landmark trainers, the nn layers.

Missing: everything past two views. No 3D scene type, no sparse solver, no camera model
other than Brown, no minimal solvers past homography and PnP, no image retrieval, no
mesh, no point cloud, no rasteriser, no 3D file formats. The small linear-algebra pieces
(Jacobi eigen, Householder QR, LM loops) are static copies inside their files.

## 3. Product features mapped to work packages

| Feature | Meshroom | Metashape | RealityScan | KeenTools | Package |
|---|---|---|---|---|---|
| Intrinsics from EXIF and a sensor database | CameraInit | yes | yes | | R1 |
| Camera models: Brown, fisheye (equidistant, equisolid), spherical, cylindrical, division, Brown3/4 + tangential | partial | all | division, Brown | focal only | R1 |
| SIFT, DSP-SIFT, AKAZE, upright, RootSIFT | yes | own | own | | R3 |
| Coded targets: CCTag, AprilTag, Metashape circular 12/14/16/20 bit | CCTag, AprilTag | circular | | | R3 |
| Pair preselection: exhaustive, sequential, vocabulary tree, GPS, frustum | yes | generic, reference, sequential | preselector | | R3 |
| Geometric verification (F, E, H; AC-RANSAC, LO-RANSAC) | yes | yes | yes | | R2, R3 |
| Incremental SfM | yes | yes | yes | | R4 |
| Global SfM (rotation averaging, global positioning) | yes | | | | R5 |
| Bundle adjustment with selectable intrinsics, additional corrections | yes | yes | yes | | R0, R4 |
| GCPs, check points, scale bars, camera GPS / orientation priors with accuracies | sfmTransform | yes | yes | | R6 |
| Georeferencing, CRS, geoids | partial | full | yes | | R6 |
| Localising new images in a model | yes | yes | yes | | R4 |
| Depth maps (SGM, PatchMatch), filtering levels | SGM | yes | yes | | R7 |
| Dense point cloud, fusion, cleaning, classification | partial | yes | partial | | R7, R8 |
| Laser scans: import, registration, ICP, meshing | lidar nodes | yes | yes | | R8 |
| Mesh: Delaunay + graph cut, Poisson, height field | Delaunay | yes | yes | | R9 |
| Mesh cleanup, smoothing, decimation, hole filling, refinement | yes | yes | yes | | R10 |
| UV unwrap (Basic, LSCM, ABF++, fixed / adaptive texel), texture blending modes, ghosting filter | yes | yes | yes | yes | R11 |
| Normal, occlusion, displacement maps; vertex colours | | yes | yes | | R11 |
| Orthomosaic, DSM, DTM, seamlines | | yes | yes | | R12 |
| Panorama, HDR fusion, photometric stereo, distortion calibration | yes | | | | R13 |
| Exports: COLMAP, PLY, OBJ, glTF, E57, LAS, camera XML | yes | yes | yes | OBJ, ABC | R14 |
| Camera / object from 2D-3D pins with focal (PinTool) | | | | yes | R15 |
| Rigid mesh tracking through video, zoom, masks, refine modes (GeoTracker) | | | | yes | R15 |
| Camera tracking through video | camera tracking pipeline | | | via GeoTracker | R15 |
| Head from photos with pins and auto-align, texture (FaceBuilder) | | | | yes | R16 |
| Expression tracking, blendshape retargeting (FaceTracker) | | | | yes | R16 |
| AI masks | segmentation | yes | yes | | owner (nn) |
| Gaussian splats | plugin | via COLMAP export | | | R17 |

## 4. Architecture

- **Scene data.** One scene type, plain C structs the caller can read: intrinsics (model id
  plus a parameter array, as COLMAP stores them), rigs, images (intrinsics index, pose,
  priors), 3D points, observations (image, feature index, point), tracks. Arrays with
  counts, not linked objects, so a scene round-trips through COLMAP's binary model exactly.
- **Meshes and clouds.** Indexed triangle meshes (positions, optional normals, colours,
  UVs per corner) with a half-edge view built on demand; point clouds as structure-of-arrays.
- **Cores.** As in suwar: header-only cores that compile as C, HLSL, CUDA and OpenCL, with
  the API looping. The kernel of each loop goes in a core: projection and its Jacobian per
  observation, a residual block, the NCC cost of one pixel and plane, one PatchMatch
  propagation step, one plane-sweep slice, one TSDF voxel update, one rasterised triangle
  per pixel, one texture blend sample. Sorting, queues, graphs and sparse factorisation stay
  CPU.
- **Determinism.** Every random choice through a seeded generator, every unordered set
  sorted before use, so the same inputs give the same scene in Release and Det, whatever the
  order the caller ran the per-item steps in.
- **Precision.** f64 for every solver and the scene; f32 and f64 entry points where the input
  is pixels, f32 checked against f64.
- **Interop as bytes.** Readers and writers take and return byte buffers, as the landmark
  model save / load does, and check every count against the size.

## 5. Work packages

Each package gets a reference suite, a plate and the usual gates. Licences: port only
BSD / MIT / Apache / Boost / zlib / ISC; everything else is an oracle.

| # | Package | Port from (licence) | Oracle | Core kernels |
|---|---|---|---|---|
| R0 | Foundations: shared dense linear algebra (SVD, QR, Cholesky, symmetric eigen) replacing the static copies; sparse matrices and sparse Cholesky with AMD ordering, PCG; nonlinear least squares (LM and dogleg, Schur complement on points, robust losses Huber / Cauchy / soft L1 / Tukey, manifolds SO(3), S2, quaternions); rotations, SE(3), Sim(3), Umeyama; kd-tree (exact kNN and radius) and randomised kd-forest for descriptors; union-find, graph utilities | Ceres (BSD-3) for the solver design, nanoflann (BSD) | Ceres through pyceres, numpy / scipy, Eigen | residual and Jacobian per block |
| R1 | Camera models: COLMAP's set (simple pinhole to full OpenCV, fisheye, FOV, thin prism, division, EUCM, spherical), Metashape frame (f, cx, cy, b1, b2, k1-k4, p1-p4), equidistant and equisolid fisheye, cylindrical, RealityScan Brown3/4 + tangential; project, unproject, Jacobians; undistortion maps; EXIF focal to pixels with a sensor table | COLMAP (BSD-3) | pycolmap, OpenCV, Metashape's documented formulas | project / unproject per point |
| R2 | Minimal solvers and robust estimation: P3P (Lambda Twist), P4Pf, UP2P, generalised P3P, 5-point (Nistér), 7- and 8-point F, 4-point H, 6-point shared focal; E and H decomposition; triangulation (DLT, midpoint, Lindstrom); LO-RANSAC, MSAC, PROSAC, AC-RANSAC (from the paper), MAGSAC++ | PoseLib (BSD-3), COLMAP, OpenCV USAC (Apache-2.0) | PoseLib Python, OpenCV 5, OpenMVG (MPL) for AC-RANSAC | residual per correspondence |
| R3 | Matching at scale: RootSIFT, DSP-SIFT, upright SIFT; vocabulary tree training and query; pair selection (exhaustive, sequential with loop closure, GPS / spatial, vocabulary tree, frustum); ratio and cross checks, guided matching; verification by F / E / H; tracks by union-find; circular coded targets; CCTag from the paper | VLFeat (BSD-2), COLMAP (BSD-3) | COLMAP, VLFeat, AliceVision (MPL) | descriptor distance, tree descent |
| R4 | Incremental SfM: initial pair, next-best view, PnP registration, triangulation, local and global bundle adjustment, filtering, retriangulation, model merging, self-calibration, shared intrinsics, rigs; localising new images | COLMAP's incremental mapper (BSD-3) | pycolmap, synthetic ground truth | reprojection residual |
| R5 | Global SfM: view-graph calibration, rotation averaging, global positioning, final adjustment | COLMAP 4 `global_mapper` (BSD-3, formerly GLOMAP), Theia (BSD-3) | pycolmap, Theia | |
| R6 | Control and georeferencing: markers / GCPs and check points, scale bars, camera position and orientation priors with accuracies as soft constraints, Sim(3) alignment; geodesy and the full CRS catalogue (PROJ's methods, EPSG parameters generated by gendata, geoids and grids from caller files); error summaries per marker and camera | PROJ (MIT), GeographicLib (MIT) | pyproj, GeographicLib | |
| R7 | Dense depth: view selection, PatchMatch with geometric consistency, plane-sweep SGM, multi-resolution, confidence and normals, Metashape-style connected-component filtering, fusion into a point cloud | COLMAP patch-match stereo and fusion (BSD-3) | COLMAP, AliceVision (MPL), OpenMVS (AGPL), rendered depth | NCC per pixel and plane, one propagation step, one sweep slice, fusion test |
| R8 | Point clouds: normals and their orientation, statistical and radius outlier removal, voxel downsampling, ICP (point-to-point, point-to-plane, coloured), FPFH with RANSAC, TSDF fusion, ground classification (grid lowest point plus TIN densification), E57 / LAS input | Open3D (MIT), libE57Format (BSL-1.0) | Open3D, PDAL | nearest-neighbour residual, TSDF voxel update |
| R9 | Surfaces: 3D Delaunay with robust predicates, visibility graph cut (Labatut / Jancosek-Pajdla), screened Poisson, marching cubes, ball pivoting, height-field meshes | COLMAP Delaunay meshing (BSD-3), Geogram (BSD-3), Shewchuk predicates (public domain), PoissonRecon (MIT) | OpenMVS, CGAL (GPL), PoissonRecon binary | in-sphere and orientation tests, Poisson's basis evaluation |
| R10 | Mesh processing: cleaning (non-manifold, degenerate, small parts), Laplacian / Taubin smoothing, hole filling, quadric decimation, isotropic remeshing, quad remeshing, photometric refinement (Vu 2012, from the paper), mesh metrics (Hausdorff, Chamfer) | meshoptimizer (MIT), Instant Meshes (BSD-3), QuadriFlow (BSD-3) | MeshLab (GPL), libigl, CGAL | per-vertex smoothing step |
| R11 | Texturing: triangle rasteriser with depth test (visibility per camera), UV charts and packing, LSCM, ABF++ (paper), fixed and adaptive texel size, view selection by MRF, seam levelling, blending modes (mosaic, average, max / min intensity, natural), ghosting filter, normal / occlusion / displacement baking, vertex colours | xatlas (MIT), mvs-texturing (BSD-3), Geogram LSCM (BSD-3) | OpenMVS, Meshroom | rasterise per pixel, blend per texel |
| R12 | Orthomosaic and DEM: DSM from mesh or cloud in a projection, DTM from ground points, ortho with seamlines and multi-band blending, hole filling, tiling | suwar's own seams and blending | GDAL / Metashape outputs as measurements | height raster per cell, ortho sample |
| R13 | Meshroom extras: HDR merge from brackets, panorama from many images (estimation, warping, seams, compositing), photometric stereo with sphere lighting calibration and normal integration, distortion calibration from lines | suwar parts, papers | AliceVision (MPL) | per pixel |
| R14 | Formats as bytes: COLMAP text and binary, Bundler, NVM, OpenCV YAML, Metashape camera XML, RealityScan XMP, PLY, OBJ + MTL, glTF / GLB, E57, LAS; Alembic and Partio behind `SUWAR_HAS_ALEMBIC` / `SUWAR_HAS_PARTIO` | specs, libE57Format (BSL-1.0) | the products' own readers | |
| R15 | KeenTools rigid: PinTool (pose plus focal, optionally lens, from pins over one or several frames); GeoTracker (keyframes from pins, frame-to-frame tracking of visible model points by KLT and robust PnP, silhouette and edge refinement, smooth and precise refinement over the shot, zoom with focal per frame, 2D and surface masks, solve the object or the camera, texture from frames) | papers (Drummond-Cipolla 2002, Vacchetti 2004) | synthetic renders with known poses | rendered visibility, edge residual |
| R16 | KeenTools faces: ICT FaceKit light shipped, FLAME 2023 Open from a caller file; fitting to landmarks and pins over several photos (shared shape, pose per photo, shared focal, contour landmarks, blendshapes by non-negative least squares); auto-align with a landmark model trained on renders of the head model; texture bake to the model's UV; FaceTracker (pose and expressions per frame from landmarks and flow, temporal smoothing, surface masks, ARKit-named blendshape weights out) | eos (Apache-2.0) | eos Python, synthetic renders | blendshape evaluation per vertex |
| R18 | Appearance capture: per-point radiance samples (normal, view direction, linear value per view, clipped values missing); masked robust PCA with randomised SVD splitting each point's samples into a low-rank, view-independent part (diffuse) and a sparse part (specular); specular samples reflected about the normal into an equirectangular environment, compact lights separated from the broad environment, the diffuse part read as irradiance through spherical harmonics; an HDRI out | papers (Candes 2011 RPCA, Lin 2010 inexact ALM, Halko 2011 randomised SVD, Ramamoorthi-Hanrahan 2001 irradiance) | synthetic renders with a known environment; numpy transcription of the ALM for the iterates | one ALM step per block, one splat per sample |
| R17 | Gaussian splats, our own rasteriser and optimiser from the paper; ALIKED / DISK / LightGlue weights on the nn layers | paper; weights of ALIKED (BSD-3), DISK, LightGlue (Apache-2.0) | gsplat, the Inria code, the authors' PyTorch, locally | splat per pixel, one optimiser step |

## 6. Validation

- **Synthetic scenes with exact truth.** A scene generator in C and Python (as the landmark
  faces are): procedural textured meshes (boxes, spheres, a terrain height field, a building
  block, the head model), cameras on orbits, grids, drone lawnmower paths and handheld video
  paths, rendered by R11's rasteriser. Every pose, intrinsic, depth, normal and visibility is
  known. This is the ground truth for every package; oracles only check that we compute what
  the reference computes.
- **Oracles.** pycolmap (BSD-3) and PoseLib for solvers and SfM, Open3D for clouds and ICP,
  PoissonRecon, xatlas, eos. Copyleft or non-commercial tools (OpenMVS, Meshroom, CGAL,
  MeshLab) only as measured references run locally.
- **Metrics.** Rotation and position error after a Sim(3) fit, focal and distortion error,
  reprojection error; depth error against rendered depth; mesh accuracy and completeness at
  thresholds as ETH3D defines them, Chamfer and Hausdorff; texture PSNR on held-out views.
  Track precision at pins for the KeenTools half.
- **Real data.** BlendedMVS (CC BY 4.0) can be fetched by gendata for measurements; ETH3D
  and DTU only for local measurement, never vendored.
- **Plates.** One per package, rendered by the library: camera frusta and sparse points for
  R4, depth maps for R7, meshes shaded by our rasteriser for R9 to R11, an orthomosaic for
  R12, a tracked object and a fitted head over frames for R15 and R16.

## 7. Order and milestones

| Milestone | Packages | Shows |
|---|---|---|
| M1 sparse | R0, R1, R2, R3, R4, R14 (COLMAP and PLY first) | a synthetic orbit reconstructed, cameras within truth, COLMAP files read by COLMAP |
| M2 pins | R15 PinTool part | camera and focal from pins, the cheapest KeenTools feature |
| M3 dense and mesh | R7, R8, R9, R10 | depth maps, fused cloud, watertight mesh against the generator's mesh |
| M4 textured | R11, R12, R6 | textured mesh, orthomosaic and DEM of a terrain flight with GCPs |
| M5 tracking | R15 GeoTracker part | a mesh tracked through a rendered shot |
| M6 faces | R16 | a head fitted to rendered photos, then to tracked expressions |
| M7 rest | R5, R13, R17, remaining formats | global SfM, Meshroom extras, splats, learned features |

Suite numbers continue from 159, plates from 226.

First steps: R0, including the context's thread pool (shared linear algebra and the sparse LM with
Schur complement, checked against pyceres on bundle problems), then R1's camera models
against pycolmap.

## 8. R18 appearance capture (owner request 2026-10-15)

The goal is an HDRI of the lighting from a multi-view capture. Each surface point seen in
several views gives samples {normal, view direction, value}. Stacked as points x views (one
matrix per channel, views unseen or clipped left out), the diffuse part of a point is the same
in every view and the specular part shows only in a few, so the matrix splits into a low-rank
part and a sparse part (robust PCA on the observed entries; the singular value steps by
randomised SVD). The samples minus the diffuse part are specular; each, reflected about the
normal, is a reading of the environment in one direction. Splatted into an equirectangular map
they give the high frequencies, compact bright regions become lights (direction, size,
radiance) and the rest the environment; the diffuse part, through the normals, constrains the
low frequencies (irradiance in spherical harmonics, up to albedo).

Data: RawNeRF's windowlegovary (104 raw views, two exposures 38x apart, a sunlit window and
glossy plastic), read in abaad_dev only; values are linear raw development divided by exposure
time x ISO, clipped pixels missing. Truth: synthetic renders of a mesh under a known HDRI
(our rasteriser, a diffuse plus GGX material), where every sample's diffuse and specular part
and the map are known.

Steps, each with a suite and a plate: R18a samples (visibility by a rasterised depth test,
abaad_rasterize), R18b masked RPCA, R18c environment from specular samples and lights, R18d
the HDRI on windowlegovary.
