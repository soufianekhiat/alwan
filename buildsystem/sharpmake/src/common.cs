using System;
using System.IO;
using Sharpmake;

namespace Alwan
{
    // Whole-tree determinism axis. Det builds compile the alwan lib with
    // ALWAN_DETERMINISTIC=1 so transcendentals route through the portable
    // polynomials and SIMD/FMA fusion is suppressed. Mirrors the CMake
    // Debug_Det / Release_Det configurations and the alwan_dev solution's
    // Determinism fragment. Crossed with Optimization it yields the four
    // configs Debug / Debug_Det / Release / Release_Det.
    [Fragment, Flags]
    public enum Determinism
    {
        NonDet = 1 << 0,
        Det    = 1 << 1,
    }

    // Linkage axis: static library (.lib) vs shared library (.dll). The Dll
    // configs build the same alwan sources as a DLL and export the full public
    // API via a generated .def (see AlwanLib.cs / tools/gen_exports_def.py),
    // mirroring CMake's BUILD_SHARED_LIBS + WINDOWS_EXPORT_ALL_SYMBOLS. Crossed
    // with Determinism and Optimization this yields the eight configs
    // Debug / Release / *_Det / *_Dll / *_Det_Dll.
    [Fragment, Flags]
    public enum LinkType
    {
        Lib = 1 << 0,
        Dll = 1 << 1,
    }

    // Custom target
    [Generate]
    public class AlwanTarget : Target
    {
        public Determinism Determinism = Determinism.NonDet;
        public LinkType    LinkType    = LinkType.Lib;

        public AlwanTarget()
            : base()
        {
        }

        public AlwanTarget(Platform platform, DevEnv devEnv, Optimization optimization,
                           Determinism determinism = Determinism.NonDet,
                           LinkType linkType = LinkType.Lib)
            : base(platform, devEnv, optimization)
        {
            Determinism = determinism;
            LinkType    = linkType;
        }
    }

    // Common project base for all Alwan projects
    public abstract class CommonProject : Project
    {
        public string WorkingDir = Path.Combine("[project.SharpmakeCsPath]", "..", "..", "..", "working_dir");

        public CommonProject() : base(typeof(AlwanTarget))
        {
            IsFileNameToLower = false;
            IsTargetFileNameToLower = false;
            SourceRootPath = @"[project.SharpmakeCsPath]\..\..\..\src";

            AddTargets(new AlwanTarget(
                Platform.win64,
                DevEnv.vs2022,
                Optimization.Debug | Optimization.Release,
                Determinism.NonDet | Determinism.Det,
                LinkType.Lib | LinkType.Dll
            ));
        }

        [Configure()]
        public virtual void ConfigureAll(Configuration conf, AlwanTarget target)
        {
            bool det = (target.Determinism == Determinism.Det);
            bool dll = (target.LinkType == LinkType.Dll);
            conf.Name = target.Optimization.ToString() + (det ? "_Det" : "") + (dll ? "_Dll" : "");

            conf.ProjectFileName = "[project.Name]_[target.DevEnv]_[target.Platform]";
            conf.ProjectPath = Path.Combine("[project.SharpmakeCsPath]", "..", "..", "..", "projects", "[project.Name]");

            conf.IntermediatePath = Path.Combine("[project.SharpmakeCsPath]", "..", "..", "..", "tmp", "[project.Name]", conf.Name);
            conf.TargetPath = WorkingDir;
            conf.TargetLibraryPath = Path.Combine("[project.SharpmakeCsPath]", "..", "..", "..", "tmp", "lib", "win64_" + conf.Name);

            // C11 standard (use compiler option directly)
            conf.AdditionalCompilerOptions.Add("/std:c11");

            // Warning level 4 (all configs). Warnings-as-errors only in Release
            // configs (Release, Release_Det): ship-quality builds must compile
            // clean, while Debug/Debug_Det stay lenient for iteration. (Release
            // optimization is shared by the plain and _Det targets.)
            conf.AdditionalCompilerOptions.Add("/W4");
            if (target.Optimization == Optimization.Release)
            {
                conf.AdditionalCompilerOptions.Add("/WX");
            }

            // Compile as C code (not C++)
            conf.AdditionalCompilerOptions.Add("/TC");

            // Precise floating point for accurate color science calculations
            conf.AdditionalCompilerOptions.Add("/fp:precise");

            // Use 64-bit host cl.exe. The 32-bit host (default on some VS installs)
            // hits C1060 "out of heap space" on large embedded-data TUs such as
            // alwan_spectrum_upsample.c (Jakob2019 LUTs, ~470K float literals/TU).
            // /Zm400 is kept as defense in depth for any fallback to 32-bit cl.
            conf.Options.Add(Options.Vc.General.PreferredToolArchitecture.x64);
            conf.AdditionalCompilerOptions.Add("/Zm400");

            // Basic defines
            conf.Defines.Add("NOMINMAX");
            conf.Defines.Add("WIN32");
            conf.Defines.Add("_CRT_SECURE_NO_WARNINGS");

            // Data embedding (default: embed)
            conf.Defines.Add("ALWAN_EMBED_DATA=1");

            // Disable range normalization for tests (test references use native mathematical ranges).
            // Library header defaults to ALWAN_NORMALIZE_RANGES=1 for end users.
            conf.Defines.Add("ALWAN_NORMALIZE_RANGES=0");

            if (det)
            {
                // Bit-exact polynomial math, no libm transcendentals, no FMA
                // fusion. /fp:precise (added above for all configs) already
                // blocks a*b+c -> FMA contraction on MSVC.
                conf.Defines.Add("ALWAN_DETERMINISTIC=1");

                // In det mode ALWAN_MAP_SIMD_WIDTH collapses to 1, so the SIMD
                // bodies in *_map_kernels.inc are skipped and their precomputed
                // set1/inv_* locals become unreferenced. Suppress C4189/C4101
                // so /WX does not trip (matches the CMake det build).
                conf.AdditionalCompilerOptions.Add("/wd4189");
                conf.AdditionalCompilerOptions.Add("/wd4101");
            }

            // Optimization settings
            if (target.Optimization == Optimization.Debug)
            {
                conf.Defines.Add("_DEBUG");
                conf.Options.Add(Options.Vc.Compiler.Optimization.Disable);
                conf.Options.Add(Options.Vc.Compiler.RuntimeLibrary.MultiThreadedDebug);
            }
            else
            {
                conf.Defines.Add("NDEBUG");
                conf.Options.Add(Options.Vc.Compiler.Optimization.MaximizeSpeed);
                conf.Options.Add(Options.Vc.Compiler.RuntimeLibrary.MultiThreaded);
                conf.Options.Add(Options.Vc.Compiler.Inline.AnySuitable);

                // Enable AVX2 for wider SIMD (f32 x8, f64 x4)
                conf.AdditionalCompilerOptions.Add("/arch:AVX2");
            }

            // Set working directory for Visual Studio debugging
            conf.VcxprojUserFile = new Configuration.VcxprojUserFileSettings();
            conf.VcxprojUserFile.LocalDebuggerWorkingDirectory = "$(SolutionDir)working_dir";
        }
    }
}
